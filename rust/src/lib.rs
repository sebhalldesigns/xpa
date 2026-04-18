// lib.rs

use std::num::NonZeroIsize;
use std::ptr::NonNull;
use std::time::Instant;
use pollster;
use wgpu;

// ─── GPU Context (one per app) ───

pub struct XpaGpuContext {
    instance: wgpu::Instance,
    adapter: wgpu::Adapter,
    device: wgpu::Device,
    queue: wgpu::Queue,
}

// ─── GPU Surface (one per window/panel) ───

pub struct XpaGpuSurface {
    surface: wgpu::Surface<'static>,
    config: wgpu::SurfaceConfiguration,
}

// ─── Scene ───

pub struct XpaScene {
    _private: (),
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
    } else {
        wgpu::PresentMode::Fifo
    }
}

// ─── Init ───

#[unsafe(no_mangle)]
pub extern "C" fn xpa_rust_init() -> bool {
    // Nothing needed globally for now
    true
}

// ─── Context ───

#[unsafe(no_mangle)]
pub extern "C" fn xpa_gpu_context_create() -> *mut XpaGpuContext {
    let instance = wgpu::Instance::new(&wgpu::InstanceDescriptor {
        backends: instance_backends(),
        ..Default::default()
    });

    let adapter = match pollster::block_on(instance.request_adapter(&wgpu::RequestAdapterOptions {
        power_preference: wgpu::PowerPreference::default(),
        compatible_surface: None,
        force_fallback_adapter: false,
    })) {
        Ok(a) => a,
        Err(e) => {
            eprintln!("xpa: failed to find GPU adapter: {e}");
            return std::ptr::null_mut();
        }
    };

    let device_desc = create_device_descriptor();
    let (device, queue) = match pollster::block_on(adapter.request_device(&device_desc)) {
        Ok(dq) => dq,
        Err(e) => {
            eprintln!("xpa: failed to create device: {e}");
            return std::ptr::null_mut();
        }
    };

    Box::into_raw(Box::new(XpaGpuContext {
        instance,
        adapter,
        device,
        queue,
    }))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn xpa_gpu_context_create_with_surface(
    native_handle: *mut std::ffi::c_void,
    width: u32,
    height: u32,
    out_surface: *mut *mut XpaGpuSurface,
) -> *mut XpaGpuContext {
    let startup = Instant::now();
    let instance = wgpu::Instance::new(&wgpu::InstanceDescriptor {
        backends: instance_backends(),
        ..Default::default()
    });
    eprintln!("[xpa_rust] instance created at +{:.2} ms", startup.elapsed().as_secs_f64() * 1000.0);

    let step = Instant::now();
    let surface = match create_surface(&instance, native_handle) {
        Some(surface) => surface,
        None => {
            eprintln!("xpa: failed to create surface for adapter selection");
            return std::ptr::null_mut();
        }
    };
    eprintln!("[xpa_rust] create_surface took {:.2} ms", step.elapsed().as_secs_f64() * 1000.0);

    let step = Instant::now();
    let adapter = match pollster::block_on(instance.request_adapter(&wgpu::RequestAdapterOptions {
        power_preference: wgpu::PowerPreference::default(),
        compatible_surface: Some(&surface),
        force_fallback_adapter: false,
    })) {
        Ok(a) => a,
        Err(e) => {
            eprintln!("xpa: failed to find GPU adapter: {e}");
            return std::ptr::null_mut();
        }
    };
    eprintln!("[xpa_rust] request_adapter took {:.2} ms", step.elapsed().as_secs_f64() * 1000.0);

    let step = Instant::now();
    let device_desc = create_device_descriptor();
    let (device, queue) = match pollster::block_on(adapter.request_device(&device_desc)) {
        Ok(dq) => dq,
        Err(e) => {
            eprintln!("xpa: failed to create device: {e}");
            return std::ptr::null_mut();
        }
    };
    eprintln!("[xpa_rust] request_device took {:.2} ms", step.elapsed().as_secs_f64() * 1000.0);

    let step = Instant::now();
    let caps = surface.get_capabilities(&adapter);
    let format = caps.formats.first().copied().unwrap_or(wgpu::TextureFormat::Bgra8UnormSrgb);

    let config = wgpu::SurfaceConfiguration {
        usage: wgpu::TextureUsages::RENDER_ATTACHMENT,
        format,
        width,
        height,
        present_mode: choose_present_mode(&caps),
        alpha_mode: caps.alpha_modes.first().copied().unwrap_or(wgpu::CompositeAlphaMode::Auto),
        view_formats: vec![],
        desired_maximum_frame_latency: 2,
    };
    surface.configure(&device, &config);
    eprintln!("[xpa_rust] surface.configure took {:.2} ms", step.elapsed().as_secs_f64() * 1000.0);

    let step = Instant::now();
    let surface = Box::into_raw(Box::new(XpaGpuSurface {
        surface,
        config,
    }));
    eprintln!("[xpa_rust] surface state setup took {:.2} ms", step.elapsed().as_secs_f64() * 1000.0);

    if !out_surface.is_null() {
        unsafe {
            *out_surface = surface;
        }
    }

    eprintln!("[xpa_rust] total first-window gpu bootstrap {:.2} ms", startup.elapsed().as_secs_f64() * 1000.0);

    Box::into_raw(Box::new(XpaGpuContext {
        instance,
        adapter,
        device,
        queue,
    }))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn xpa_gpu_context_destroy(ctx: *mut XpaGpuContext) {
    if !ctx.is_null() {
        unsafe {
            drop(Box::from_raw(ctx));
        }
    }
}

// ─── Surface ───

#[cfg(target_os = "windows")]
fn create_surface(instance: &wgpu::Instance, native_handle: *mut std::ffi::c_void) -> Option<wgpu::Surface<'static>> {
    use raw_window_handle::{RawWindowHandle, RawDisplayHandle, Win32WindowHandle, WindowsDisplayHandle};

    unsafe extern "system" {
        fn GetWindowLongPtrW(h_wnd: *mut std::ffi::c_void, n_index: i32) -> isize;
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

    match unsafe { instance.create_surface_unsafe(target) } {
        Ok(surface) => Some(surface),
        Err(err) => {
            eprintln!("xpa: create_surface_unsafe failed on Win32: {err}");
            None
        }
    }
}

#[cfg(target_os = "macos")]
fn create_surface(instance: &wgpu::Instance, native_handle: *mut std::ffi::c_void) -> Option<wgpu::Surface<'static>> {
    use raw_window_handle::{RawWindowHandle, RawDisplayHandle, AppKitWindowHandle, AppKitDisplayHandle};

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
fn create_surface(instance: &wgpu::Instance, native_handle: *mut std::ffi::c_void) -> Option<wgpu::Surface<'static>> {
    use raw_window_handle::{RawWindowHandle, RawDisplayHandle, XlibWindowHandle, XlibDisplayHandle};

    // native_handle is the X11 Window (a u32 cast to pointer)
    let xlib_window = native_handle as u32;
    let raw_window = RawWindowHandle::Xlib(XlibWindowHandle::new(xlib_window.into()));
    // Display* would need to be passed separately in practice
    let raw_display = RawDisplayHandle::Xlib(XlibDisplayHandle::new(None, 0));

    let target = wgpu::SurfaceTargetUnsafe::RawHandle {
        raw_window_handle: raw_window,
        raw_display_handle: raw_display,
    };

    unsafe { instance.create_surface_unsafe(target).ok() }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn xpa_gpu_surface_create(
    ctx: *mut XpaGpuContext,
    native_handle: *mut std::ffi::c_void,
    width: u32,
    height: u32,
) -> *mut XpaGpuSurface {
    let ctx = unsafe { &*ctx };

    let surface = match create_surface(&ctx.instance, native_handle) {
        Some(s) => s,
        None => {
            eprintln!("xpa: failed to create surface");
            return std::ptr::null_mut();
        }
    };

    let caps = surface.get_capabilities(&ctx.adapter);
    let format = caps.formats.first().copied().unwrap_or(wgpu::TextureFormat::Bgra8UnormSrgb);

    let config = wgpu::SurfaceConfiguration {
        usage: wgpu::TextureUsages::RENDER_ATTACHMENT,
        format,
        width,
        height,
        present_mode: choose_present_mode(&caps),
        alpha_mode: caps.alpha_modes.first().copied().unwrap_or(wgpu::CompositeAlphaMode::Auto),
        view_formats: vec![],
        desired_maximum_frame_latency: 2,
    };
    surface.configure(&ctx.device, &config);

    Box::into_raw(Box::new(XpaGpuSurface {
        surface,
        config,
    }))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn xpa_gpu_surface_destroy(surface: *mut XpaGpuSurface) {
    if !surface.is_null() {
        unsafe {
            drop(Box::from_raw(surface));
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn xpa_gpu_surface_resize(
    ctx: *mut XpaGpuContext,
    surface: *mut XpaGpuSurface,
    width: u32,
    height: u32,
) {
    let ctx = unsafe { &*ctx };
    let surface = unsafe { &mut *surface };

    if width == 0 || height == 0 {
        return;
    }
    if surface.config.width == width && surface.config.height == height {
        return;
    }

    surface.config.width = width;
    surface.config.height = height;
    surface.surface.configure(&ctx.device, &surface.config);
}

// ─── Scene ───

#[unsafe(no_mangle)]
pub extern "C" fn xpa_scene_create() -> *mut XpaScene {
    Box::into_raw(Box::new(XpaScene {
        _private: (),
    }))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn xpa_scene_destroy(scene: *mut XpaScene) {
    if !scene.is_null() {
        unsafe {
            drop(Box::from_raw(scene));
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn xpa_scene_clear(scene: *mut XpaScene) {
    let _scene = unsafe { &mut *scene };
}

// ─── Render ───

#[unsafe(no_mangle)]
pub unsafe extern "C" fn xpa_gpu_render(
    ctx: *mut XpaGpuContext,
    surface: *mut XpaGpuSurface,
    scene: *mut XpaScene,
) {
    let ctx = unsafe { &*ctx };
    let surface = unsafe { &mut *surface };
    let _scene = unsafe { &mut *scene };

    let frame = match surface.surface.get_current_texture() {
        Ok(f) => f,
        Err(wgpu::SurfaceError::Lost | wgpu::SurfaceError::Outdated) => {
            surface.surface.configure(&ctx.device, &surface.config);
            match surface.surface.get_current_texture() {
                Ok(f) => f,
                Err(e) => {
                    eprintln!("xpa: failed to get surface texture after reconfigure: {e}");
                    return;
                }
            }
        }
        Err(wgpu::SurfaceError::Timeout) => {
            return;
        }
        Err(wgpu::SurfaceError::OutOfMemory) => {
            eprintln!("xpa: failed to get surface texture: out of memory");
            return;
        }
        Err(e) => {
            eprintln!("xpa: failed to get surface texture: {e}");
            return;
        }
    };

    let frame_view = frame.texture.create_view(&Default::default());
    let mut encoder = ctx.device.create_command_encoder(&Default::default());
    {
        let _pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
            label: Some("xpa clear pass"),
            color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                view: &frame_view,
                depth_slice: None,
                resolve_target: None,
                ops: wgpu::Operations {
                    load: wgpu::LoadOp::Clear(wgpu::Color {
                        r: 0.16,
                        g: 0.16,
                        b: 0.24,
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
    }
    ctx.queue.submit(std::iter::once(encoder.finish()));

    frame.present();
}

