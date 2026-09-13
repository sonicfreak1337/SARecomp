#define NOMINMAX
#include <windows.h>
#include "sonic_render_completion.hpp"
#include "sonic_presentation.hpp"
#include <array>
#include <iostream>
#include <thread>
#include <vector>
using namespace katana::runtime;
using namespace sonic::render_completion;
namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void channel_contract() {
    Channel channel;
    require(!capture(), "host acquired guest ticket");
    {
        Submission outer(&channel);
        auto a = capture();
        { Submission masked(nullptr); require(!capture(), "nested host scope inherited guest"); }
        auto b = capture();
        require(!b.complete(), "out-of-order completion accepted");
        require(!channel.acknowledge(), "submitted-only ticket delivered");
        require(a.complete() && !a.complete(), "duplicate completion accepted");
        require(channel.acknowledge() && !channel.acknowledge(), "duplicate guest dispatch accepted");
        require(b.complete() && channel.acknowledge(), "ordered completion lost");
    }
    require(!capture(), "scope leaked into host UI");
    auto old = channel.reserve();
    channel = {};
    require(old.complete() && channel.counters().completed == 0, "old lifetime notified restored guest");
    std::vector<Ticket> tickets;
    for (unsigned i=0; i<4096; ++i) tickets.push_back(channel.reserve());
    bool ordered=true;
    std::atomic<bool> done=false;
    std::thread consumer([&] {
        for(const auto& ticket:tickets) ordered &= ticket.complete();
        done.store(true,std::memory_order_release);
    });
    while (!done.load(std::memory_order_acquire)) {
        if (!channel.acknowledge()) std::this_thread::yield();
    }
    consumer.join();
    while(channel.acknowledge()) {}
    require(ordered && channel.counters().dispatched==tickets.size(), "concurrent publication lost or duplicated");
}
}
int main(int argc, char** argv) {
    try {
        require(argc==3,"renderer and private cache directory required");
        channel_contract();
        SetEnvironmentVariableW(L"KATANA_PORT_BACKGROUND_TEST",L"1");
        const auto cache=std::filesystem::absolute(argv[2]);std::filesystem::create_directories(cache);
        SetEnvironmentVariableW(L"SARECOMP_CACHE_ROOT",cache.c_str());
        sonic::rendering::selected_renderer=std::string_view(argv[1])=="vulkan"
            ?sonic::rendering::Renderer::Vulkan:sonic::rendering::Renderer::D3D11;
        NativePortGraphicsConfig config;config.output_extent=config.render_extent={128,128};
        config.initially_visible=false;config.synchronize_present=false;config.maximum_type2_fragment_nodes=131072;
        NativePortGraphicsDevice device(config);
        Channel channel;
        std::uint64_t expected=0;
        std::array<std::byte,16> pixels{};
        NativePortImageView image;image.extent={2,2};image.format=NativePortTextureFormat::Rgba8Unorm;
        image.stride_bytes=8;image.pixels=pixels;
        for (unsigned frame=0; frame<48; ++frame) {
            device.begin_frame({});
            // A synchronous resource prefix can precede Present in the same
            // guest frame. Its queue sequence must not acquire a completion.
            NativePortTextureConfig texture;texture.extent={2,2};
            const auto handle=device.create_texture(texture,&image);
            {
                Submission submission(frame%3==0?&channel:nullptr);
                device.present();
                // Even under a guest submission scope these host commands do
                // not represent a new guest render and must not reserve IDs.
                device.repeat_present();
                device.present_image(image,NativePortViewportTarget::Game,NativePortImageFit::Contain);
            }
            expected += frame%3==0;
            device.destroy_texture(handle);
        }
        device.finish();
        auto c=channel.counters();
        require(c.submitted==expected && c.completed==expected,"queue confused guest renders and host images");
        for(unsigned i=0;i<expected;++i)require(channel.acknowledge(),"completion lost");
        require(!channel.acknowledge(),"duplicate dispatch");
        bool rejected=false;
        { Submission scope(&channel);try{device.present();}catch(const NativePortGraphicsError&){rejected=true;} }
        require(rejected && channel.counters().submitted==expected,"invalid frame allocated completion");
        device.begin_frame({});
        {Submission scope(&channel);device.present();}
        channel={};device.finish();
        require(channel.counters().completed==0,"in-flight render leaked across restore");
        std::cout<<"SONIC_RENDER_COMPLETION_OK renderer="<<argv[1]<<" guest="<<expected
            <<" frames=48 scopes=ok prefix=ok repeats=excluded movies=excluded restore=ok concurrent=4096\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"SONIC_RENDER_COMPLETION_FAIL "<<error.what()<<'\n';return 1;
    }
}
