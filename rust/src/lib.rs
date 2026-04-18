use std::borrow::Cow;
use std::ffi::c_void;
use std::num::NonZeroIsize;
use std::ptr::NonNull;

use lyon::math::{Box2D, point};
use lyon::tessellation::{
    BuffersBuilder, FillOptions, FillTessellator, FillVertex, FillVertexConstructor, VertexBuffers,
};
use pollster;
use wgpu::util::DeviceExt;

pub struct XpaRenderer {
    _instance: wgpu::Instance,
    _adapter: wgpu::Adapter,
    device: wgpu::Device,
    queue: wgpu::Queue,
    surface: wgpu::Surface<'static>,
    config: wgpu::SurfaceConfiguration,
    pipeline: wgpu::RenderPipeline,
    vertex_buffer: wgpu::Buffer,
    index_buffer: wgpu::Buffer,
    index_count: u32,
}

#[repr(C)]
#[derive(Clone, Copy)]
struct Vertex {
    position: [f32; 2],
    color: [f32; 4],
}

struct SolidColorCtor {
    color: [f32; 4],
}

impl FillVertexConstructor<Vertex> for SolidColorCtor {
    fn new_vertex(&mut self, vertex: FillVertex) -> Vertex {
        let p = vertex.position();
        Vertex {
            position: [p.x, p.y],
            color: self.color,
        }
    }
}

const SHADER_WGSL: &str = r#"
struct VertexIn {
    @location(0) position_ndc: vec2<f32>,
    @location(1) color: vec4<f32>,
};

struct VertexOut {
    @builtin(position) position: vec4<f32>,
    @location(0) color: vec4<f32>,
};

@vertex
fn vs_main(in: VertexIn) -> VertexOut {
    var out: VertexOut;
    out.position = vec4<f32>(in.position_ndc, 0.0, 1.0);
    out.color = in.color;
    return out;
}

@fragment
fn fs_main(in: VertexOut) -> @location(0) vec4<f32> {
    return in.color;
}
"#;

fn slice_as_bytes<T>(values: &[T]) -> &[u8] {
    unsafe {
        std::slice::from_raw_parts(
            values.as_ptr().cast::<u8>(),
            std::mem::size_of_val(values),
        )
    }
}

fn instance_backends() -> wgpu::Backends {
    #[cfg(target_os = "windows")]
    {
        return wgpu::Backends::PRIMARY;
    }

    #[cfg(not(target_os = "windows"))]
    {
        return wgpu::Backends::VULKAN;
    }
}

fn create_device_descriptor() -> wgpu::DeviceDescriptor<'static> {
    wgpu::DeviceDescriptor {
        label: Some("xpa device"),
        required_features: wgpu::Features::empty(),
        required_limits: wgpu::Limits::default(),
        memory_hints: wgpu::MemoryHints::default(),
        experimental_features: wgpu::ExperimentalFeatures::disabled(),
        trace: wgpu::Trace::default(),
    }
}

fn choose_present_mode(caps: &wgpu::SurfaceCapabilities) -> wgpu::PresentMode {
    if caps.present_modes.contains(&wgpu::PresentMode::Mailbox) {
        wgpu::PresentMode::Mailbox
    } else if caps.present_modes.contains(&wgpu::PresentMode::Immediate) {
        wgpu::PresentMode::Immediate
    } else {
        wgpu::PresentMode::Fifo
    }
}

fn choose_surface_format(caps: &wgpu::SurfaceCapabilities) -> wgpu::TextureFormat {
    if caps.formats.contains(&wgpu::TextureFormat::Bgra8UnormSrgb) {
        wgpu::TextureFormat::Bgra8UnormSrgb
    } else {
        caps.formats
            .first()
            .copied()
            .unwrap_or(wgpu::TextureFormat::Bgra8UnormSrgb)
    }
}

fn create_shader(device: &wgpu::Device) -> wgpu::ShaderModule {
    let shader = device.create_shader_module(wgpu::ShaderModuleDescriptor {
        label: Some("xpa vector rect shader"),
        source: wgpu::ShaderSource::Wgsl(Cow::Borrowed(SHADER_WGSL)),
    });
    shader
}

fn create_pipeline(
    device: &wgpu::Device,
    format: wgpu::TextureFormat,
    shader: &wgpu::ShaderModule,
) -> wgpu::RenderPipeline {
    let pipeline_layout = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
        label: Some("xpa vector pipeline layout"),
        bind_group_layouts: &[],
        immediate_size: 0,
    });

    let vertex_layout = wgpu::VertexBufferLayout {
        array_stride: std::mem::size_of::<Vertex>() as wgpu::BufferAddress,
        step_mode: wgpu::VertexStepMode::Vertex,
        attributes: &[
            wgpu::VertexAttribute {
                offset: 0,
                shader_location: 0,
                format: wgpu::VertexFormat::Float32x2,
            },
            wgpu::VertexAttribute {
                offset: std::mem::size_of::<[f32; 2]>() as wgpu::BufferAddress,
                shader_location: 1,
                format: wgpu::VertexFormat::Float32x4,
            },
        ],
    };

    device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
        label: Some("xpa vector pipeline"),
        layout: Some(&pipeline_layout),
        vertex: wgpu::VertexState {
            module: shader,
            entry_point: Some("vs_main"),
            compilation_options: wgpu::PipelineCompilationOptions::default(),
            buffers: &[vertex_layout],
        },
        primitive: wgpu::PrimitiveState {
            topology: wgpu::PrimitiveTopology::TriangleList,
            strip_index_format: None,
            front_face: wgpu::FrontFace::Ccw,
            cull_mode: None,
            unclipped_depth: false,
            polygon_mode: wgpu::PolygonMode::Fill,
            conservative: false,
        },
        depth_stencil: None,
        multisample: wgpu::MultisampleState::default(),
        fragment: Some(wgpu::FragmentState {
            module: shader,
            entry_point: Some("fs_main"),
            compilation_options: wgpu::PipelineCompilationOptions::default(),
            targets: &[Some(wgpu::ColorTargetState {
                format,
                blend: Some(wgpu::BlendState::ALPHA_BLENDING),
                write_mask: wgpu::ColorWrites::ALL,
            })],
        }),
        multiview_mask: None,
        cache: None,
    })
}

fn tessellate_rect(
    tessellator: &mut FillTessellator,
    geometry: &mut VertexBuffers<Vertex, u32>,
    rect: Box2D,
    color: [f32; 4],
) {
    let ctor = SolidColorCtor { color };
    if let Err(err) = tessellator.tessellate_rectangle(
        &rect,
        &FillOptions::default(),
        &mut BuffersBuilder::new(geometry, ctor),
    ) {
        eprintln!("xpa: vector tessellation failed: {err}");
    }
}

fn to_ndc_x(x: f32) -> f32 {
    x * 2.0 - 1.0
}

fn to_ndc_y(y: f32) -> f32 {
    1.0 - y * 2.0
}

fn build_vector_geometry() -> VertexBuffers<Vertex, u32> {
    let mut geometry: VertexBuffers<Vertex, u32> = VertexBuffers::new();
    let mut tess = FillTessellator::new();

    // Main panel
    tessellate_rect(
        &mut tess,
        &mut geometry,
        Box2D::new(point(0.06, 0.06), point(0.94, 0.94)),
        [0.10, 0.15, 0.24, 1.0],
    );

    // Top bar
    tessellate_rect(
        &mut tess,
        &mut geometry,
        Box2D::new(point(0.12, 0.12), point(0.56, 0.22)),
        [0.24, 0.50, 0.86, 0.95],
    );

    // Content card
    tessellate_rect(
        &mut tess,
        &mut geometry,
        Box2D::new(point(0.42, 0.42), point(0.88, 0.76)),
        [0.18, 0.33, 0.58, 0.90],
    );

    // Bottom accent bar
    tessellate_rect(
        &mut tess,
        &mut geometry,
        Box2D::new(point(0.12, 0.74), point(0.44, 0.84)),
        [0.39, 0.73, 0.93, 0.90],
    );

    for vertex in &mut geometry.vertices {
        vertex.position[0] = to_ndc_x(vertex.position[0]);
        vertex.position[1] = to_ndc_y(vertex.position[1]);
    }

    geometry
}

fn create_geometry_buffers(device: &wgpu::Device) -> (wgpu::Buffer, wgpu::Buffer, u32) {
    let mut geometry = build_vector_geometry();
    if geometry.vertices.is_empty() || geometry.indices.is_empty() {
        geometry.vertices = vec![
            Vertex {
                position: [-1.0, 1.0],
                color: [0.10, 0.15, 0.24, 1.0],
            },
            Vertex {
                position: [1.0, 1.0],
                color: [0.10, 0.15, 0.24, 1.0],
            },
            Vertex {
                position: [1.0, -1.0],
                color: [0.10, 0.15, 0.24, 1.0],
            },
            Vertex {
                position: [-1.0, -1.0],
                color: [0.10, 0.15, 0.24, 1.0],
            },
        ];
        geometry.indices = vec![0, 1, 2, 0, 2, 3];
    }

    let vertex_buffer = device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
        label: Some("xpa vector vertices"),
        contents: slice_as_bytes(&geometry.vertices),
        usage: wgpu::BufferUsages::VERTEX,
    });

    let index_buffer = device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
        label: Some("xpa vector indices"),
        contents: slice_as_bytes(&geometry.indices),
        usage: wgpu::BufferUsages::INDEX,
    });

    (vertex_buffer, index_buffer, geometry.indices.len() as u32)
}

fn create_surface_config(
    surface: &wgpu::Surface<'static>,
    adapter: &wgpu::Adapter,
    width: u32,
    height: u32,
) -> wgpu::SurfaceConfiguration {
    let caps = surface.get_capabilities(adapter);
    wgpu::SurfaceConfiguration {
        usage: wgpu::TextureUsages::RENDER_ATTACHMENT,
        format: choose_surface_format(&caps),
        width: width.max(1),
        height: height.max(1),
        present_mode: choose_present_mode(&caps),
        alpha_mode: caps
            .alpha_modes
            .first()
            .copied()
            .unwrap_or(wgpu::CompositeAlphaMode::Auto),
        view_formats: vec![],
        desired_maximum_frame_latency: 1,
    }
}

fn acquire_frame(renderer: &mut XpaRenderer) -> Option<wgpu::SurfaceTexture> {
    match renderer.surface.get_current_texture() {
        Ok(frame) => Some(frame),
        Err(wgpu::SurfaceError::Lost | wgpu::SurfaceError::Outdated) => {
            renderer.surface.configure(&renderer.device, &renderer.config);
            match renderer.surface.get_current_texture() {
                Ok(frame) => Some(frame),
                Err(err) => {
                    eprintln!("xpa: failed to acquire frame after reconfigure: {err}");
                    None
                }
            }
        }
        Err(wgpu::SurfaceError::Timeout) => None,
        Err(wgpu::SurfaceError::OutOfMemory) => {
            eprintln!("xpa: failed to acquire frame: out of memory");
            None
        }
        Err(wgpu::SurfaceError::Other) => {
            eprintln!("xpa: failed to acquire frame: surface error");
            None
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn xpa_rust_init() -> bool {
    true
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn xpa_renderer_init(
    native_handle: *mut c_void,
    width: u32,
    height: u32,
) -> *mut XpaRenderer {
    let instance = wgpu::Instance::new(&wgpu::InstanceDescriptor {
        backends: instance_backends(),
        ..Default::default()
    });

    let surface = match create_surface(&instance, native_handle) {
        Some(surface) => surface,
        None => {
            eprintln!("xpa: failed to create surface");
            return std::ptr::null_mut();
        }
    };

    let adapter = match pollster::block_on(instance.request_adapter(&wgpu::RequestAdapterOptions {
        power_preference: wgpu::PowerPreference::default(),
        compatible_surface: Some(&surface),
        force_fallback_adapter: false,
    })) {
        Ok(adapter) => adapter,
        Err(err) => {
            eprintln!("xpa: failed to find adapter: {err}");
            return std::ptr::null_mut();
        }
    };

    let device_desc = create_device_descriptor();
    let (device, queue) = match pollster::block_on(adapter.request_device(&device_desc)) {
        Ok(dq) => dq,
        Err(err) => {
            eprintln!("xpa: failed to create device: {err}");
            return std::ptr::null_mut();
        }
    };

    let config = create_surface_config(&surface, &adapter, width, height);
    surface.configure(&device, &config);

    let shader = create_shader(&device);
    let pipeline = create_pipeline(&device, config.format, &shader);
    let (vertex_buffer, index_buffer, index_count) = create_geometry_buffers(&device);

    Box::into_raw(Box::new(XpaRenderer {
        _instance: instance,
        _adapter: adapter,
        device,
        queue,
        surface,
        config,
        pipeline,
        vertex_buffer,
        index_buffer,
        index_count,
    }))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn xpa_renderer_resize(renderer: *mut XpaRenderer, width: u32, height: u32) {
    if renderer.is_null() || width == 0 || height == 0 {
        return;
    }

    let renderer = unsafe { &mut *renderer };
    if renderer.config.width == width && renderer.config.height == height {
        return;
    }

    renderer.config.width = width;
    renderer.config.height = height;
    renderer.surface.configure(&renderer.device, &renderer.config);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn xpa_renderer_render(renderer: *mut XpaRenderer) {
    if renderer.is_null() {
        return;
    }

    let renderer = unsafe { &mut *renderer };
    if renderer.config.width == 0 || renderer.config.height == 0 {
        return;
    }

    let Some(frame) = acquire_frame(renderer) else {
        return;
    };

    let frame_view = frame
        .texture
        .create_view(&wgpu::TextureViewDescriptor::default());

    let mut encoder = renderer
        .device
        .create_command_encoder(&wgpu::CommandEncoderDescriptor {
            label: Some("xpa render encoder"),
        });

    {
        let mut pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
            label: Some("xpa vector render pass"),
            color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                view: &frame_view,
                depth_slice: None,
                resolve_target: None,
                ops: wgpu::Operations {
                    load: wgpu::LoadOp::Clear(wgpu::Color {
                        r: 0.05,
                        g: 0.08,
                        b: 0.13,
                        a: 1.0,
                    }),
                    store: wgpu::StoreOp::Store,
                },
            })],
            depth_stencil_attachment: None,
            timestamp_writes: None,
            occlusion_query_set: None,
            multiview_mask: None,
        });

        /*pass.set_pipeline(&renderer.pipeline);
        pass.set_vertex_buffer(0, renderer.vertex_buffer.slice(..));
        pass.set_index_buffer(renderer.index_buffer.slice(..), wgpu::IndexFormat::Uint32);
        pass.draw_indexed(0..renderer.index_count, 0, 0..1);*/
    }

    renderer.queue.submit(std::iter::once(encoder.finish()));
    frame.present();
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn xpa_renderer_destroy(renderer: *mut XpaRenderer) {
    if !renderer.is_null() {
        unsafe {
            drop(Box::from_raw(renderer));
        }
    }
}

#[cfg(target_os = "windows")]
fn create_surface(
    instance: &wgpu::Instance,
    native_handle: *mut c_void,
) -> Option<wgpu::Surface<'static>> {
    use raw_window_handle::{
        RawDisplayHandle, RawWindowHandle, Win32WindowHandle, WindowsDisplayHandle,
    };

    unsafe extern "system" {
        fn GetWindowLongPtrW(h_wnd: *mut c_void, n_index: i32) -> isize;
    }

    const GWLP_HINSTANCE: i32 = -6;

    let hwnd = NonNull::new(native_handle)?;
    let hwnd = NonZeroIsize::new(hwnd.as_ptr() as isize)?;
    let mut window_handle = Win32WindowHandle::new(hwnd);
    window_handle.hinstance = NonZeroIsize::new(unsafe { GetWindowLongPtrW(native_handle, GWLP_HINSTANCE) });

    let raw_window = RawWindowHandle::Win32(window_handle);
    let raw_display = RawDisplayHandle::Windows(WindowsDisplayHandle::new());

    let target = wgpu::SurfaceTargetUnsafe::RawHandle {
        raw_window_handle: raw_window,
        raw_display_handle: raw_display,
    };

    unsafe { instance.create_surface_unsafe(target).ok() }
}

#[cfg(target_os = "macos")]
fn create_surface(
    instance: &wgpu::Instance,
    native_handle: *mut c_void,
) -> Option<wgpu::Surface<'static>> {
    use raw_window_handle::{
        AppKitDisplayHandle, AppKitWindowHandle, RawDisplayHandle, RawWindowHandle,
    };

    let ns_view = NonNull::new(native_handle)?;
    let raw_window = RawWindowHandle::AppKit(AppKitWindowHandle::new(ns_view));
    let raw_display = RawDisplayHandle::AppKit(AppKitDisplayHandle::new());

    let target = wgpu::SurfaceTargetUnsafe::RawHandle {
        raw_window_handle: raw_window,
        raw_display_handle: raw_display,
    };

    unsafe { instance.create_surface_unsafe(target).ok() }
}

#[cfg(target_os = "linux")]
fn create_surface(
    instance: &wgpu::Instance,
    native_handle: *mut c_void,
) -> Option<wgpu::Surface<'static>> {
    use raw_window_handle::{RawDisplayHandle, RawWindowHandle, XlibDisplayHandle, XlibWindowHandle};

    let xlib_window = native_handle as u32;
    let raw_window = RawWindowHandle::Xlib(XlibWindowHandle::new(xlib_window.into()));
    let raw_display = RawDisplayHandle::Xlib(XlibDisplayHandle::new(None, 0));

    let target = wgpu::SurfaceTargetUnsafe::RawHandle {
        raw_window_handle: raw_window,
        raw_display_handle: raw_display,
    };

    unsafe { instance.create_surface_unsafe(target).ok() }
}
