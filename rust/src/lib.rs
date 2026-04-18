// lib.rs

use std::path::PathBuf;
use std::ptr::NonNull;
use std::num::NonZeroIsize;
use std::time::Instant;
use wgpu;
use vello::{self, AaConfig, AaSupport, RenderParams, RendererOptions, Scene};
use vello::peniko::Color;
use pollster;

// ─── GPU Context (one per app) ───

pub struct XpaGpuContext {
    instance: wgpu::Instance,
    adapter: wgpu::Adapter,
    device: wgpu::Device,
    queue: wgpu::Queue,
    renderer: vello::Renderer,
}

// ─── GPU Surface (one per window/panel) ───

pub struct XpaGpuSurface {
    surface: wgpu::Surface<'static>,
    config: wgpu::SurfaceConfiguration,
    target_texture: Option<wgpu::Texture>,
    blitter: wgpu::util::TextureBlitter,
}

// ─── Scene ───

pub struct XpaScene {
    inner: Scene,
}

fn pipeline_cache_dir() -> Option<PathBuf> {
    let base = std::env::var("LOCALAPPDATA").ok()?;
    let dir = PathBuf::from(base).join("xpa");
    std::fs::create_dir_all(&dir).ok()?;
    Some(dir)
}

fn load_pipeline_cache(device: &wgpu::Device, info: &wgpu::AdapterInfo) -> wgpu::PipelineCache {
    let cached_data = wgpu::util::pipeline_cache_key(info)
        .and_then(|key| pipeline_cache_dir().map(|d| d.join(key)))
        .and_then(|path| std::fs::read(path).ok());

    unsafe {
        device.create_pipeline_cache(&wgpu::PipelineCacheDescriptor {
            label: Some("xpa"),
            data: cached_data.as_deref(),
            fallback: true,
        })
    }
}

fn save_pipeline_cache(cache: &wgpu::PipelineCache, info: &wgpu::AdapterInfo) {
    let Some(data) = cache.get_data() else { return };
    let Some(key) = wgpu::util::pipeline_cache_key(info) else { return };
    let Some(dir) = pipeline_cache_dir() else { return };

    let tmp = dir.join(format!("{}.tmp", key));
    let final_path = dir.join(key);
    if std::fs::write(&tmp, &data).is_ok() {
        let _ = std::fs::rename(&tmp, &final_path);
    }
}

fn create_renderer(device: &wgpu::Device, pipeline_cache: Option<wgpu::PipelineCache>) -> Result<vello::Renderer, vello::Error> {
    vello::Renderer::new(
        device,
        RendererOptions {
            use_cpu: false,
            antialiasing_support: AaSupport {
                area: false,
                msaa8: true,
                msaa16: false,
            },
            num_init_threads: None,
            pipeline_cache,
        },
    )
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
        backends: wgpu::Backends::VULKAN,
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

    let (device, queue) = match pollster::block_on(adapter.request_device(
        &wgpu::DeviceDescriptor::default(),
    )) {
        Ok(dq) => dq,
        Err(e) => {
            eprintln!("xpa: failed to create device: {e}");
            return std::ptr::null_mut();
        }
    };

    let info = adapter.get_info();
    let pipeline_cache = load_pipeline_cache(&device, &info);
    let cache_ref = pipeline_cache.clone();

    let renderer = match create_renderer(&device, Some(pipeline_cache)) {
        Ok(r) => r,
        Err(e) => {
            eprintln!("xpa: failed to create vello renderer: {e}");
            return std::ptr::null_mut();
        }
    };

    std::thread::spawn(move || save_pipeline_cache(&cache_ref, &info));

    Box::into_raw(Box::new(XpaGpuContext {
        instance,
        adapter,
        device,
        queue,
        renderer,
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
        backends: wgpu::Backends::VULKAN,
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
    let (device, queue) = match pollster::block_on(adapter.request_device(
        &wgpu::DeviceDescriptor::default(),
    )) {
        Ok(dq) => dq,
        Err(e) => {
            eprintln!("xpa: failed to create device: {e}");
            return std::ptr::null_mut();
        }
    };
    eprintln!("[xpa_rust] request_device took {:.2} ms", step.elapsed().as_secs_f64() * 1000.0);

    let step = Instant::now();
    let info = adapter.get_info();
    let pipeline_cache = load_pipeline_cache(&device, &info);
    let cache_ref = pipeline_cache.clone();
    eprintln!("[xpa_rust] pipeline cache load took {:.2} ms", step.elapsed().as_secs_f64() * 1000.0);

    let step = Instant::now();
    let renderer = match create_renderer(&device, Some(pipeline_cache)) {
        Ok(r) => r,
        Err(e) => {
            eprintln!("xpa: failed to create vello renderer: {e}");
            return std::ptr::null_mut();
        }
    };
    eprintln!("[xpa_rust] Renderer::new took {:.2} ms", step.elapsed().as_secs_f64() * 1000.0);

    std::thread::spawn(move || {
        save_pipeline_cache(&cache_ref, &info);
        eprintln!("[xpa_rust] pipeline cache saved to disk");
    });

    let step = Instant::now();
    let caps = surface.get_capabilities(&adapter);
    let format = caps.formats.first().copied().unwrap_or(wgpu::TextureFormat::Bgra8UnormSrgb);
    let config = wgpu::SurfaceConfiguration {
        usage: wgpu::TextureUsages::RENDER_ATTACHMENT,
        format,
        width,
        height,
        present_mode: wgpu::PresentMode::Fifo,
        alpha_mode: caps.alpha_modes.first().copied().unwrap_or(wgpu::CompositeAlphaMode::Auto),
        view_formats: vec![],
        desired_maximum_frame_latency: 2,
    };
    surface.configure(&device, &config);
    eprintln!("[xpa_rust] surface.configure took {:.2} ms", step.elapsed().as_secs_f64() * 1000.0);

    let step = Instant::now();
    let target_texture = create_target_texture(&device, width, height);
    let blitter = wgpu::util::TextureBlitter::new(&device, format);
    let surface = Box::into_raw(Box::new(XpaGpuSurface {
        surface,
        config,
        target_texture: Some(target_texture),
        blitter,
    }));
    eprintln!("[xpa_rust] target texture + blitter took {:.2} ms", step.elapsed().as_secs_f64() * 1000.0);

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
        renderer,
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

    let hwnd = NonNull::new(native_handle)?;
    let hwnd = NonZeroIsize::new(hwnd.as_ptr() as isize)?;
    let raw_window = RawWindowHandle::Win32(Win32WindowHandle::new(hwnd));
    let raw_display = RawDisplayHandle::Windows(WindowsDisplayHandle::new());

    let target = wgpu::SurfaceTargetUnsafe::RawHandle {
        raw_window_handle: raw_window,
        raw_display_handle: raw_display,
    };

    unsafe { instance.create_surface_unsafe(target).ok() }
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

fn create_target_texture(device: &wgpu::Device, width: u32, height: u32) -> wgpu::Texture {
    device.create_texture(&wgpu::TextureDescriptor {
        label: Some("vello target"),
        size: wgpu::Extent3d { width, height, depth_or_array_layers: 1 },
        mip_level_count: 1,
        sample_count: 1,
        dimension: wgpu::TextureDimension::D2,
        format: wgpu::TextureFormat::Rgba8Unorm,
        usage: wgpu::TextureUsages::STORAGE_BINDING
             | wgpu::TextureUsages::TEXTURE_BINDING,
        view_formats: &[],
    })
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
        present_mode: wgpu::PresentMode::Fifo,
        alpha_mode: caps.alpha_modes.first().copied().unwrap_or(wgpu::CompositeAlphaMode::Auto),
        view_formats: vec![],
        desired_maximum_frame_latency: 2,
    };
    surface.configure(&ctx.device, &config);

    let target_texture = create_target_texture(&ctx.device, width, height);
    let blitter = wgpu::util::TextureBlitter::new(&ctx.device, format);

    Box::into_raw(Box::new(XpaGpuSurface {
        surface,
        config,
        target_texture: Some(target_texture),
        blitter,
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

    surface.config.width = width;
    surface.config.height = height;
    surface.surface.configure(&ctx.device, &surface.config);
    surface.target_texture = Some(create_target_texture(&ctx.device, width, height));
}

// ─── Scene ───

#[unsafe(no_mangle)]
pub extern "C" fn xpa_scene_create() -> *mut XpaScene {
    Box::into_raw(Box::new(XpaScene {
        inner: Scene::new(),
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
    let scene = unsafe { &mut *scene };
    scene.inner.reset();
}

// ─── Render ───

#[unsafe(no_mangle)]
pub unsafe extern "C" fn xpa_gpu_render(
    ctx: *mut XpaGpuContext,
    surface: *mut XpaGpuSurface,
    scene: *mut XpaScene,
) {
    let ctx = unsafe { &mut *ctx };
    let surface = unsafe { &mut *surface };
    let scene = unsafe { &*scene };

    let width = surface.config.width;
    let height = surface.config.height;

    // Get the target texture vello will render into
    let target = match surface.target_texture.as_ref() {
        Some(t) => t,
        None => return,
    };

    // Draw a test circle into the scene if it's empty (demo)
    // In production, the C side builds the scene before calling render

    // Render scene to offscreen texture
    let target_view = target.create_view(&Default::default());

    if let Err(e) = ctx.renderer.render_to_texture(
        &ctx.device,
        &ctx.queue,
        &scene.inner,
        &target_view,
        &RenderParams {
            base_color: Color::from_rgb8(40, 40, 60),
            width,
            height,
            antialiasing_method: AaConfig::Msaa8,
        },
    ) {
        eprintln!("xpa: vello render failed: {e}");
        return;
    }

    // Acquire swapchain texture and blit
    let frame = match surface.surface.get_current_texture() {
        Ok(f) => f,
        Err(e) => {
            eprintln!("xpa: failed to get surface texture: {e}");
            return;
        }
    };

    let frame_view = frame.texture.create_view(&Default::default());

    let mut encoder = ctx.device.create_command_encoder(&Default::default());
    surface.blitter.copy(&ctx.device, &mut encoder, &target_view, &frame_view);
    ctx.queue.submit(std::iter::once(encoder.finish()));

    frame.present();
}
