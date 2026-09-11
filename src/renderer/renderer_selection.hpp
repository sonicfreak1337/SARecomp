#pragma once
namespace sonic::rendering {
enum class Renderer { D3D11, Vulkan };
enum class WindowMode { Windowed, Borderless, Fullscreen };
// Set before constructing the host; thereafter immutable on both threads.
inline Renderer selected_renderer = Renderer::D3D11;
inline WindowMode selected_window_mode = WindowMode::Windowed;
inline const char* name(Renderer value) noexcept {
    return value == Renderer::Vulkan ? "vulkan" : "d3d11";
}
}
