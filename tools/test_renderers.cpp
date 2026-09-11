#include "katana/runtime/native_port_graphics.hpp"
#include "renderer/renderer_selection.hpp"
#include <array>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <vector>
#define NOMINMAX
#include <windows.h>
using namespace katana::runtime;

int main(int argc,char** argv) {
    try {
        if(argc!=3) return 2;
        sonic::rendering::selected_renderer=std::string_view(argv[1])=="vulkan"?sonic::rendering::Renderer::Vulkan:sonic::rendering::Renderer::D3D11;
        std::filesystem::create_directories(argv[2]);
        _putenv_s("KATANA_PORT_BACKGROUND_TEST","1");
        _putenv_s("KATANA_NATIVE_GRAPHICS_CAPTURE_DIRECTORY",argv[2]);
        _putenv_s("KATANA_NATIVE_GRAPHICS_CAPTURE_START_FRAME","1");
        _putenv_s("KATANA_NATIVE_GRAPHICS_CAPTURE_END_FRAME","13");
        _putenv_s("KATANA_NATIVE_GRAPHICS_CAPTURE_INTERVAL","1");
        NativePortGraphicsConfig config; config.output_extent=config.render_extent={128,128};
        config.initially_visible=false; config.synchronize_present=false; config.maximum_type2_fragment_nodes=131072;
        NativePortGraphicsDevice device(config);
        std::array<NativePortVertex,4> v;
        v[0].position={-.8f,.8f,.5f}; v[1].position={.8f,.8f,.5f}; v[2].position={-.8f,-.8f,.5f}; v[3].position={.8f,-.8f,.5f};
        v[0].texture_coordinate={0,0}; v[1].texture_coordinate={1,0}; v[2].texture_coordinate={0,1}; v[3].texture_coordinate={1,1};
        for(auto& x:v) x.depth_coordinate=1;
        const std::array<unsigned,6> indices{0,1,2,2,1,3};
        std::array<std::byte,16> pixels{};
        const std::array<unsigned char,16> colors{255,0,0,255,0,255,0,255,0,0,255,255,255,255,255,255};
        for(unsigned i=0;i<16;++i) pixels[i]=std::byte(colors[i]);
        NativePortTextureConfig texture; texture.extent={2,2}; texture.dynamic=true;
        NativePortImageView image; image.extent={2,2}; image.format=NativePortTextureFormat::Rgba8Unorm; image.stride_bytes=8; image.pixels=pixels;
        auto handle=device.create_texture(texture,&image);
        NativePortDrawPacket packet; packet.vertices=v; packet.indices=indices; packet.batch.identity=1;
        packet.rasterizer.cull=NativePortCullMode::None; packet.texture=handle; packet.texture_stage=NativePortTextureStage::RequiredResolved;
        packet.sampler.filter=NativePortTextureFilter::Point;
        device.begin_frame(); device.draw(packet); device.present();
        // Update and immutable geometry share the same API and queue lifetime.
        image.bottom_up=true; device.update_texture(handle,image);
        NativePortMeshConfig mesh; mesh.vertices=v; mesh.indices=indices;
        auto mh=device.create_mesh(mesh); packet.mesh=mh; packet.vertices={}; packet.indices={};
        device.begin_frame(); device.draw(packet); device.present();
        device.destroy_mesh(mh); device.destroy_texture(handle);
        packet.mesh={}; packet.vertices=v; packet.indices=indices; packet.texture={}; packet.texture_stage=NativePortTextureStage::Disabled;
        packet.material.diffuse={.4f,.8f,.2f,1};
        packet.rasterizer.logical_clip.mode=NativePortLogicalClipMode::Outside;
        packet.rasterizer.logical_clip.logical_extent={128,128}; packet.rasterizer.logical_clip.bounds={30,40,80,90};
        device.begin_frame(); device.draw(packet); device.present();
        packet.rasterizer.logical_clip={}; packet.material.diffuse={1,1,1,1};
        // Reverse/logarithmic depth, two ordered layers, then secondary accumulation.
        NativePortFrameConfig reverse; reverse.depth_buffer=NativePortDepthBufferConvention::ReciprocalPositive; reverse.clear_depth=0;
        packet.vertex_space=NativePortVertexSpace::ClipHomogeneous;
        packet.depth_mapping.mode=NativePortDepthCoordinateMode::ReciprocalPositiveHomogeneousClip;
        packet.depth.compare=NativePortCompareOperation::GreaterEqual;
        for(unsigned frame=0;frame<2;++frame) {
            device.begin_frame(reverse);
            packet.draw_class=NativePortDrawClass::Opaque; packet.translucency=NativePortTranslucencyPolicy::NotApplicable;
            packet.blend={}; packet.batch.submission_order=0;
            for(auto& x:v) {x.color={0,0,1,1}; x.depth_coordinate=1;}
            device.draw(packet);
            packet.draw_class=NativePortDrawClass::Translucent; packet.translucency=NativePortTranslucencyPolicy::Type2AutoSorted;
            packet.blend.enabled=true; packet.blend.source_color=packet.blend.source_alpha=NativePortBlendFactor::SourceAlpha;
            packet.blend.destination_color=packet.blend.destination_alpha=NativePortBlendFactor::InverseSourceAlpha;
            packet.batch.submission_order=1;
            if(frame) packet.blend.destination_buffer=NativePortBlendDestination::SecondaryAccumulation;
            for(auto& x:v) {x.color={1,0,0,.5f}; x.depth_coordinate=2;}
            device.draw(packet);
            packet.batch.submission_order=2;
            if(frame) {packet.blend.destination_buffer=NativePortBlendDestination::Framebuffer; packet.blend.source_buffer=NativePortBlendSource::SecondaryAccumulation;}
            for(auto& x:v) {x.color={0,1,0,.5f}; x.depth_coordinate=3;}
            device.draw(packet); device.present();
        }
        // The movie/UI path uses the same texture upload and aspect-fit composite.
        device.present_image(image,NativePortViewportTarget::Game,NativePortImageFit::Contain);
        packet={}; packet.vertices=v; packet.indices=indices; packet.batch.identity=1;
        for(auto& x:v) x.color={1,.4f,.2f,1};
        for(bool ccw:{false,true}) {
            packet.rasterizer.front_counter_clockwise=ccw;
            device.begin_frame(); device.draw(packet); device.present();
        }
        packet.rasterizer.cull=NativePortCullMode::None; packet.rasterizer.front_counter_clockwise=false;
        v[0].color={1,0,0,1}; v[1].color={0,1,0,1}; v[2].color={0,0,1,1}; v[3].color={1,1,0,1};
        packet.rasterizer.shading=NativePortShadingMode::FlatLastVertex;
        device.begin_frame(); device.draw(packet); device.present();
        packet.rasterizer.shading=NativePortShadingMode::Smooth;
        packet.fog.mode=NativePortFogMode::LookupTable; packet.fog.color={.1f,.3f,.7f,1};
        for(unsigned i=0;i<128;++i) packet.fog.lookup_table[i]=float(i)/127;
        for(unsigned i=0;i<4;++i) v[i].fog_coordinate=1+30*float(i);
        device.begin_frame(); device.draw(packet); device.present();
        packet.fog={}; packet.indices={};
        for(auto topology:{NativePortPrimitiveTopology::LineList,NativePortPrimitiveTopology::LineStrip,NativePortPrimitiveTopology::PointList}) {
            packet.topology=topology;
            device.begin_frame(); device.draw(packet); device.present();
        }
        device.finish();
        std::cout<<"SONIC_RENDERER_TEST_OK backend="<<argv[1]<<" frames="<<device.snapshot().begun_frames<<'\n';
        if (sonic::rendering::selected_renderer==sonic::rendering::Renderer::Vulkan) {
            HWND window=nullptr;
            EnumWindows([](HWND candidate,LPARAM context)->BOOL {
                DWORD pid=0; GetWindowThreadProcessId(candidate,&pid);
                wchar_t name[128]{};
                if (pid==GetCurrentProcessId() && GetClassNameW(candidate,name,128) &&
                    std::wstring_view(name)==L"KatanaRecompNativeGraphicsV1") {
                    *reinterpret_cast<HWND*>(context)=candidate; return FALSE;
                }
                return TRUE;
            },reinterpret_cast<LPARAM>(&window));
            if (!window) throw std::runtime_error("Fullscreen test window missing");
            RECT before{},after{},client{};
            GetWindowRect(window,&before);
            const auto menu=GetMenu(window);
            const auto visible=IsWindowVisible(window);
            const auto style=GetWindowLongPtrW(window,GWL_STYLE);
            MONITORINFO monitor{sizeof(monitor)};
            GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor);
            constexpr LPARAM alt=LPARAM{1}<<29;
            SendMessageW(window,WM_SYSKEYDOWN,VK_RETURN,alt);
            SendMessageW(window,WM_SYSKEYDOWN,VK_RETURN,alt|(LPARAM{1}<<30)); // repeat must not toggle back
            GetWindowRect(window,&after); GetClientRect(window,&client);
            if (!EqualRect(&after,&monitor.rcMonitor) || GetMenu(window) ||
                client.right!=after.right-after.left || client.bottom!=after.bottom-after.top ||
                IsWindowVisible(window)!=visible)
                throw std::runtime_error("Vulkan fullscreen did not fill the monitor invisibly");
            static_cast<void>(device.poll_events()); device.begin_frame(); device.draw(packet); device.present(); device.finish();
            SendMessageW(window,WM_SYSKEYDOWN,VK_RETURN,alt);
            GetWindowRect(window,&after);
            if (!EqualRect(&before,&after) || GetMenu(window)!=menu ||
                GetWindowLongPtrW(window,GWL_STYLE)!=style || IsWindowVisible(window)!=visible)
                throw std::runtime_error("Vulkan fullscreen did not restore its window/menu");
            static_cast<void>(device.poll_events()); device.begin_frame(); device.draw(packet); device.present(); device.finish();
            std::cout<<"SONIC_VULKAN_FULLSCREEN_TEST_OK enter=1 repeat_ignored=1 restore=1 swapchain_resizes="
                     <<device.snapshot().swap_chain_resizes<<'\n';
        }
        return 0;
    } catch(const NativePortGraphicsError& e) {
        std::cerr<<e.what()<<" failure="<<unsigned(e.failure())<<" operation="<<e.operation_id()<<'\n'; return 1;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n'; return 1;}
}
