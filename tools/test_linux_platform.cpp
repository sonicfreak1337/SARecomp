#include "katana/runtime/native_port_platform.hpp"
#include "katana/io/input_provenance.hpp"
#include <SDL3/SDL.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
using namespace katana::runtime;
namespace fs = std::filesystem;
void require(bool okay, const char* detail) { if (!okay) throw std::runtime_error(detail); }
template<class F> void fails(F run, NativePortPlatformFailure expected) {
    try { run(); } catch (const NativePortPlatformError& e) { require(e.failure() == expected, e.what()); return; }
    throw std::runtime_error("Expected typed platform failure");
}
std::span<const std::byte> bytes(std::string_view text) { return {reinterpret_cast<const std::byte*>(text.data()), text.size()}; }
std::string read(const fs::path& path) { std::ifstream f(path, std::ios::binary); return {std::istreambuf_iterator<char>(f), {}}; }
int main(int argc, char** argv) {
    try {
        require(argc == 2 && std::getenv("KATANA_PORT_BACKGROUND_TEST"), "Isolated test root required");
        const auto root = fs::canonical(argv[1]);
        NativePortPlatformConfig config;
        config.content_root = root / "content"; config.user_data_root = root / "state"; config.project_id = "sonic-linux-contract";
        { auto bad = config; bad.user_data_root = config.content_root / "should-not-exist";
          fails([&] { NativePortPlatformServices rejected(bad); }, NativePortPlatformFailure::InvalidConfig);
          require(!fs::exists(bad.user_data_root), "Overlap created writable content"); }
        NativePortPlatformServices platform(config);
        fails([&] { NativePortPlatformServices second(config); }, NativePortPlatformFailure::SaveConflict);
        const auto original = read(config.content_root / "source.bin");
        const auto identity = "sha256:" + katana::io::sha256_bytes(original);
        NativePortContentFileBinding binding{"fixture", "source.bin", identity, 0, original.size()};
        auto file = platform.open_content_file(binding);
        std::ofstream(config.content_root / "source.bin", std::ios::binary | std::ios::trunc) << "changed by another process";
        std::vector<std::byte> content(original.size()); file->read_at(0, content);
        require(std::equal(content.begin(), content.end(), bytes(original).begin()), "Verified content changed after open");
        fails([&] { (void)platform.open_content_file(binding); }, NativePortPlatformFailure::ContentIdentity);
        auto outside = binding; outside.content_relative_path = "../outside.bin";
        fails([&] { (void)platform.open_content_file(outside); }, NativePortPlatformFailure::InvalidConfig);
        fs::create_symlink(root / "outside.bin", config.content_root / "linked.bin");
        outside.content_relative_path = "linked.bin";
        fails([&] { (void)platform.open_content_file(outside); }, NativePortPlatformFailure::ContentBoundary);
        bool thread_rejected = false;
        std::thread alien([&] { try { (void)platform.snapshot(); } catch (const NativePortPlatformError& e) { thread_rejected = e.failure() == NativePortPlatformFailure::ThreadViolation; } });
        alien.join(); require(thread_rejected, "Cross-thread platform access allowed");
        const NativePortSaveKey story{"story", 7}, chao{"chao", 7}, future{"future", 7}, broken{"broken", 7};
        const auto loaded = platform.load_save(story);
        require(loaded.status == NativePortSaveLoadStatus::Loaded && loaded.generation == 41, "Independent Windows save fixture rejected");
        const std::string new_story = "STORY_FULL_CLEAR_FIXTURE_42";
        require(platform.store_save(story, bytes(new_story)) == 42, "Save generation did not advance");
        const auto save_dir = config.user_data_root / config.project_id / "saves";
        fs::copy_file(save_dir / "story.ksave", root / "linux-output.ksave");
        fs::copy_file(save_dir / "story.ksave.bak", root / "linux-backup.ksave");
        require(platform.store_save(chao, bytes("CHAO_FIXTURE")) == 1, "Chao save failed");
        require(platform.load_save(story).generation == 42, "Chao save changed Story");
        std::ofstream(save_dir / "story.ksave", std::ios::binary | std::ios::trunc) << "corrupt-primary";
        const auto recovered = platform.load_save(story);
        require(recovered.status == NativePortSaveLoadStatus::RecoveredFromBackup && recovered.generation == 41, "Backup recovery failed");
        require(platform.store_save(story, bytes(new_story)) == 42, "Recovered save could not be committed");
        require(read(save_dir / "story.ksave.bak") == read(root / "linux-backup.ksave"), "Recovery overwrote valid backup");
        require(platform.load_save(future).status == NativePortSaveLoadStatus::IncompatibleSchema, "Newer schema fell back to an old save");
        fails([&] { (void)platform.store_save(future, bytes("bad")); }, NativePortPlatformFailure::SaveIncompatible);
        require(platform.load_save(broken).status == NativePortSaveLoadStatus::Corrupt, "Invalid checksum treated as a newer schema");
        fails([&] { (void)platform.store_save(broken, bytes("bad")); }, NativePortPlatformFailure::SaveCorrupt);
        fs::create_symlink(root / "outside.bin", save_dir / "linked.ksave");
        fails([&] { (void)platform.store_save({"linked", 7}, bytes("bad")); }, NativePortPlatformFailure::SaveBoundary);

        SDL_VirtualJoystickDesc description{}; SDL_INIT_INTERFACE(&description);
        description.type = SDL_JOYSTICK_TYPE_GAMEPAD; description.naxes = 6; description.nbuttons = 15;
        description.axis_mask = (1u << 6) - 1; description.button_mask = (1u << 15) - 1;
        description.vendor_id = 0x054c; description.product_id = 0x0ce6; description.name = "SARecomp isolated virtual pad";
        const auto device = SDL_AttachVirtualJoystick(&description); require(device != 0, SDL_GetError());
        auto* joystick = SDL_OpenJoystick(device); require(joystick != nullptr, SDL_GetError());
        // SDL opens/maps the gamepad first and initializes trigger zero points.
        // Apply test input only after that normal connection boundary.
        require(platform.poll_gamepads().gamepads[0].connected, "Virtual controller was not discovered");
        SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_RIGHTX, 16384);
        SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_RIGHTY, -16384);
        SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 32767);
        SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, -32768);
        SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, true); SDL_UpdateJoysticks();
        const auto input = platform.poll_gamepads(); const auto& pad = input.gamepads[0];
        std::cout << "SDL_VIRTUAL_INPUT connected=" << pad.connected << " buttons=" << pad.buttons
                  << " rx=" << pad.right_stick_x_raw << " ry=" << pad.right_stick_y_raw
                  << " lt=" << unsigned(pad.left_trigger_raw) << " rt=" << unsigned(pad.right_trigger_raw) << '\n';
        require(pad.connected && (pad.buttons & (1u << 10)) && pad.right_stick_x_raw == 16384 && pad.right_stick_y_raw == 16384 &&
            pad.left_trigger_raw == 255 && pad.right_trigger_raw == 0, "SDL axes/buttons do not match native input contract");
        require(platform.poll_gamepads().gamepads[0].packet_number == pad.packet_number, "Unchanged input advanced packet");
        SDL_CloseJoystick(joystick); require(SDL_DetachVirtualJoystick(device), SDL_GetError()); SDL_UpdateJoysticks();
        const auto disconnected = platform.poll_gamepads();
        require(!disconnected.gamepads[0].connected && disconnected.connection_generation > input.connection_generation, "Disconnect retained stale input");
        platform.finalize_clean_shutdown();
        std::cout << "SONIC_LINUX_PLATFORM_OK content=sealed boundaries=ok save=windows-v2 backup=ok schema=ok sdl_axes=ok disconnect=ok\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
