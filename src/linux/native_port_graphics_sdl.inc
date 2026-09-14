// SDL owns the Linux window on the existing graphics command consumer. The
// generated includes share Windows/Vulkan validation, geometry preparation,
// shader constants, handle generations and bounded diagnostics verbatim.
namespace {
#include "linux_graphics_constants.inc"
std::wstring utf8_to_wide(std::string_view value){
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.from_bytes(value.data(),value.data()+value.size());
}
std::string wide_to_utf8(std::wstring_view value){
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.to_bytes(value.data(),value.data()+value.size());
}
}

class NativePortGraphicsBackend final {
#include "linux_graphics_types.inc"
    std::string title_storage_,development_state_directory_storage_;
    NativePortGraphicsConfig config_;
    std::thread::id owner_thread_;
    NativePortRuntimeOptionsBridge* runtime_options_=nullptr;
    std::optional<NativePortTelemetryWriter> telemetry_writer_;
    std::optional<NativePortTelemetryTimer> render_submit_timer_;
    SDL_Window* window_=nullptr;
    std::unique_ptr<sonic::rendering::VulkanRenderer> vulkan_;
    bool video_initialized_=false,close_requested_=false,minimized_=false,deck_=false;
    bool frame_open_=false,completed_frame_available_=false,completed_host_image_=false;
    bool inject_present_failure_once_=false,inject_present_busy_once_=false,allow_nonblocking_present_=true;
    unsigned present_queue_limit_=2,display_rate_hz_=60;
    NativePortExtent output_extent_,completed_host_extent_;
    NativePortGraphicsLayout cached_layout_;
    NativePortDepthBufferConvention frame_depth_buffer_=NativePortDepthBufferConvention::Forward;
    // These counters are reset by the retained BeginFrame body; the Vulkan
    // upload ring itself lives in VulkanRenderer and owns its own fences.
    unsigned vertex_buffer_write_offset_=0,index_buffer_write_offset_=0;
    bool vertex_buffer_discarded_=false,index_buffer_discarded_=false;
    std::uint64_t runtime_menu_sample_nanoseconds_=0,runtime_menu_output_frames_=0,runtime_menu_simulation_frames_=0;
    double runtime_menu_output_fps_=0,runtime_menu_simulation_fps_=0;
#include "linux_graphics_members.inc"

    std::wstring environment_value(const wchar_t* name) const {
        const auto narrow=wide_to_utf8(name);const auto* value=std::getenv(narrow.c_str());
        return value?utf8_to_wide(value):std::wstring{};
    }
    void upload_texture(TextureSlot& slot,const NativePortImageView& image,unsigned mip){
        vulkan_->upload_texture(slot.vulkan_texture,image,mip);
        saturating_add(snapshot_.uploaded_bytes,std::uint64_t(image.extent.width)*image.extent.height*4);
    }
    void resolve_type2_subpass(){vulkan_->resolve_type_two();}
    NativePortPixelRect completed_output_viewport()const noexcept {
        return completed_host_image_?NativePortPixelRect{0,0,output_extent_.width,output_extent_.height}:cached_layout_.output_viewport;
    }
    void update_runtime_options_menu() noexcept {
        const auto now=std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
        const auto simulation=runtime_options_?runtime_options_->simulation_frames.load(std::memory_order_acquire):0;
        if(runtime_menu_sample_nanoseconds_&&now-runtime_menu_sample_nanoseconds_<500'000'000)return;
        if(runtime_menu_sample_nanoseconds_){
            const double seconds=double(now-runtime_menu_sample_nanoseconds_)/1e9;
            runtime_menu_output_fps_=double(snapshot_.presented_frames-runtime_menu_output_frames_)/seconds;
            runtime_menu_simulation_fps_=double(simulation-runtime_menu_simulation_frames_)/seconds;
        }
        runtime_menu_sample_nanoseconds_=now;runtime_menu_output_frames_=snapshot_.presented_frames;
        runtime_menu_simulation_frames_=simulation;
    }
    void refresh_display_rate() noexcept {
        const auto* mode=SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window_));
        display_rate_hz_=mode&&std::isfinite(mode->refresh_rate)&&mode->refresh_rate>=1
            ?unsigned(std::clamp(std::lround(mode->refresh_rate),1l,1000l)):60u;
    }
    bool set_fullscreen(bool fullscreen,bool native=false) noexcept {
        if(background_test_mode_requested())return true;
        if(deck_)fullscreen=true;
        SDL_DisplayMode chosen{};const SDL_DisplayMode* mode=nullptr;
        const auto display=SDL_GetDisplayForWindow(window_);
        if(fullscreen&&(native||sonic::rendering::selected_window_mode==sonic::rendering::WindowMode::Fullscreen)){
            const auto extent=native?NativePortExtent{1280,800}:config_.output_extent;
            if(SDL_GetClosestFullscreenDisplayMode(display,int(extent.width),int(extent.height),0,false,&chosen)&&
                chosen.w==int(extent.width)&&chosen.h==int(extent.height))mode=&chosen;
        }
        if(!SDL_SetWindowFullscreenMode(window_,mode)||!SDL_SetWindowFullscreen(window_,fullscreen))return false;
        return true;
    }
    void destroy_window() noexcept {
        if(window_){sonic::input::window_created(nullptr);SDL_DestroyWindow(window_);window_=nullptr;}
        if(video_initialized_){SDL_QuitSubSystem(SDL_INIT_VIDEO);video_initialized_=false;}
    }
    NativePortBackendPresentOutcome present_completed_frame(const char* operation){
        poll_events();update_runtime_options_menu();
        if(minimized_){snapshot_.occluded=true;stop_render_submit_telemetry();flush_render_telemetry();return NativePortBackendPresentOutcome::Occluded;}
        if(inject_present_failure_once_){inject_present_failure_once_=false;fail(NativePortGraphicsFailure::DeviceLost,1,operation);}
        const bool nonblocking=nonblocking_output_enabled();
        if(nonblocking&&inject_present_busy_once_){inject_present_busy_once_=false;return NativePortBackendPresentOutcome::Deferred;}
        PerformanceOverlayConstants overlay{};
        const bool show_overlay=runtime_options_&&runtime_options_->performance_overlay_enabled.load(std::memory_order_acquire);
        if(show_overlay){
            char output[16]{},simulation[16]{};
            std::snprintf(output,sizeof(output),"FPS %5.1f",std::clamp(runtime_menu_output_fps_,0.0,999.9));
            std::snprintf(simulation,sizeof(simulation),"SIM %5.1f",std::clamp(runtime_menu_simulation_fps_,0.0,999.9));
            rasterize_performance_overlay_text(overlay,output,2u);rasterize_performance_overlay_text(overlay,simulation,17u);
        }
        const auto presented=vulkan_->present(completed_output_viewport(),nonblocking,
            show_overlay?std::as_bytes(std::span(&overlay,1)):std::span<const std::byte>{});
        stop_render_submit_telemetry();flush_render_telemetry();
        if(!presented)return NativePortBackendPresentOutcome::Deferred;
        snapshot_.occluded=false;capture_completed_frame(snapshot_.presented_frames+1);
        saturating_increment(snapshot_.presented_frames);maybe_checkpoint_graphics_breadcrumbs();
        return NativePortBackendPresentOutcome::Presented;
    }
public:
    NativePortGraphicsBackend(const NativePortGraphicsConfig& config,NativePortRuntimeOptionsBridge* bridge)
        :title_storage_(copy_validated_graphics_title(config.title)),
         development_state_directory_storage_(copy_development_state_directory(config.development_state_directory)),
         config_(config),owner_thread_(std::this_thread::get_id()),runtime_options_(bridge){
        config_.title=title_storage_;config_.development_state_directory=development_state_directory_storage_;
        validate_graphics_config(config_);initialize_present_policy();initialize_frame_capture();initialize_graphics_diagnostics();
        deck_=sonic::linux_host::steam_deck();
        inject_present_failure_once_=present_failure_test_requested();inject_present_busy_once_=present_busy_test_requested();
        if(!SDL_InitSubSystem(SDL_INIT_VIDEO))fail(NativePortGraphicsFailure::WindowCreation,1,SDL_GetError());
        video_initialized_=true;
        try{
            auto flags=SDL_WINDOW_VULKAN|SDL_WINDOW_HIDDEN|SDL_WINDOW_HIGH_PIXEL_DENSITY;
            if(config_.resizable&&!deck_)flags|=SDL_WINDOW_RESIZABLE;
            window_=SDL_CreateWindow(title_storage_.c_str(),int(config_.output_extent.width),int(config_.output_extent.height),flags);
            if(!window_)fail(NativePortGraphicsFailure::WindowCreation,1,SDL_GetError());
            sonic::input::window_created(window_);
            if((deck_||sonic::rendering::selected_window_mode!=sonic::rendering::WindowMode::Windowed)&&!set_fullscreen(true))
                fail(NativePortGraphicsFailure::WindowCreation,1,"fullscreen-window");
            int width=0,height=0;SDL_GetWindowSizeInPixels(window_,&width,&height);
            output_extent_={unsigned(std::max(1,width)),unsigned(std::max(1,height))};refresh_display_rate();refresh_layout();
            vulkan_=std::make_unique<sonic::rendering::VulkanRenderer>(window_,config_);
            config_.maximum_type2_fragment_nodes=vulkan_->type_two_node_capacity();
            if(config_.telemetry)telemetry_writer_.emplace(config_.telemetry->make_writer());
            if(config_.initially_visible)show();
        }catch(...){vulkan_.reset();destroy_window();throw;}
    }
    ~NativePortGraphicsBackend() noexcept {
        if(std::this_thread::get_id()!=owner_thread_)std::terminate();
        stop_render_submit_telemetry();flush_render_telemetry();flush_graphics_breadcrumbs();vulkan_.reset();destroy_window();
    }
    void show(){require_owner_thread();if(!background_test_mode_requested())SDL_ShowWindow(window_);}
    void publish_telemetry() noexcept {flush_render_telemetry();}
    void poll_events(){
        require_owner_thread();bool display_changed=false;SDL_Event event{};
        while(SDL_PollEvent(&event)){
            if(event.type==SDL_EVENT_QUIT)close_requested_=true;
            if(event.type==SDL_EVENT_DISPLAY_REMOVED||event.type==SDL_EVENT_DISPLAY_ADDED||event.type==SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED)display_changed=true;
            if(event.type==SDL_EVENT_WINDOW_CLOSE_REQUESTED&&event.window.windowID==SDL_GetWindowID(window_))close_requested_=true;
            if(event.type==SDL_EVENT_KEY_DOWN&&event.key.windowID==SDL_GetWindowID(window_)&&!event.key.repeat){
                if(event.key.key==SDLK_RETURN&&(event.key.mod&SDL_KMOD_ALT)){
                    if(!set_fullscreen(!(SDL_GetWindowFlags(window_)&SDL_WINDOW_FULLSCREEN)))std::fprintf(stderr,"SONIC_DISPLAY_RECOVERY detail=%s\n",SDL_GetError());
                    continue;
                }
                if(event.key.key==SDLK_F1&&(event.key.mod&SDL_KMOD_CTRL)&&runtime_options_){
                    runtime_options_->performance_overlay_enabled.store(!runtime_options_->performance_overlay_enabled.load());continue;
                }
            }
            sonic::input::window_event(window_,event);
        }
        if(display_changed){
            int count=0;auto* displays=SDL_GetDisplays(&count);
            // SDL relocates an orphaned window. Restore a supported native
            // fullscreen mode when the Deck has only its internal panel left.
            if(deck_&&count==1&&!background_test_mode_requested()){
                SDL_SetWindowPosition(window_,SDL_WINDOWPOS_CENTERED_DISPLAY(displays[0]),SDL_WINDOWPOS_CENTERED_DISPLAY(displays[0]));
                if(!set_fullscreen(true,true))std::fprintf(stderr,"SONIC_DISPLAY_RECOVERY detail=%s\n",SDL_GetError());
            }
            SDL_free(displays);refresh_display_rate();
        }
        const auto flags=SDL_GetWindowFlags(window_);minimized_=(flags&SDL_WINDOW_MINIMIZED)!=0;
        int width=0,height=0;SDL_GetWindowSizeInPixels(window_,&width,&height);
        if(width>0&&height>0&&(display_changed||unsigned(width)!=output_extent_.width||unsigned(height)!=output_extent_.height)){
            if(width>int(maximum_graphics_dimension)||height>int(maximum_graphics_dimension))fail(NativePortGraphicsFailure::InvalidConfig,1,"window-size");
            output_extent_={unsigned(width),unsigned(height)};
            if(vulkan_)vulkan_->resize(output_extent_);refresh_layout();refresh_display_rate();saturating_increment(snapshot_.swap_chain_resizes);
        }
        sonic::input::window_update(window_);update_runtime_options_menu();
    }
    NativePortLifecycleState lifecycle_state()const {require_owner_thread();return close_requested_?NativePortLifecycleState::Shutdown:minimized_?NativePortLifecycleState::Paused:NativePortLifecycleState::Running;}
    NativePortGraphicsLayout layout()const {require_owner_thread();return cached_layout_;}
    unsigned effective_presentation_rate(unsigned manual)const noexcept {return config_.synchronize_present?display_rate_hz_:manual;}
    bool nonblocking_output_enabled()const noexcept {return allow_nonblocking_present_&&runtime_options_&&runtime_options_->independent_presentation_enabled.load(std::memory_order_acquire);}
    bool driver_paces_output()const noexcept {return config_.synchronize_present&&!nonblocking_output_enabled()&&!minimized_&&window_&&!(SDL_GetWindowFlags(window_)&SDL_WINDOW_HIDDEN);}
#include "linux_graphics_methods.inc"
};
