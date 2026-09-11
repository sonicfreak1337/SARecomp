#include "../src/sonic_native_save_contract.hpp"
#include "katana/runtime/native_port_platform.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
void require(const bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}

int main() try {
    using namespace katana::runtime;
    namespace sdk = sonic::native_save_sdk;
    const auto root = std::filesystem::temp_directory_path() /
        ("sonic-save-sdk-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto content = root / "content";
    const auto data = root / "isolated-user-data";
    std::filesystem::create_directories(content);
    std::filesystem::create_directories(data);
    NativePortPlatformConfig platform_config;
    platform_config.content_root = content;
    platform_config.user_data_root = data;
    platform_config.project_id = "sonic-save-sdk-test";
    platform_config.require_gamepad_backend = false;
    const std::array units{NativePortSaveUnitConfig{{0u, 0u}, "test-a1", true, true, 512u, 200u, 128u}};
    const NativePortSaveProviderConfig config{native_port_save_provider_contract_version,
        "sdk-test", "sdk-test-profile", units};
    const std::array<std::uint8_t, 8u> date{0xEAu, 7u, 9u, 11u, 10u, 21u, 37u, 5u};
    std::vector<std::byte> story(10u * 512u, std::byte{0x53u});
    std::vector<std::byte> chao(28u * 512u);
    for (std::size_t i = 0u; i < chao.size(); ++i) chao[i] = static_cast<std::byte>((i * 17u + i / 512u) & 255u);
    const std::vector<std::byte> game(128u * 512u, std::byte{0xA5u});
    {
        NativePortPlatformServices platform(platform_config);
        NativePortSaveProvider provider(platform, config);
        auto directory = provider.list({{0u, 0u}});
        require(directory.status.free_blocks == 200u && sdk::free_game_blocks(directory) == 128u,
                "empty A1 SDK allocation domains differ");
        // A legacy untagged story save must remain readable and unchanged.
        require(provider.write({{0u, 0u}, {"SONICADV_000", "SONICAD", "Story", "VMU Save", 0u}, story}).completion.error == NativePortSaveError::None,
                "legacy story write failed");
        const auto written = sdk::write(provider, {0u, 0u}, "SONICADV_ALF", chao, date, sdk::verify_flag, false);
        require(written.completion.error == NativePortSaveError::None && !written.verification_failed,
                "Chao write/verify failed");
        const auto partial = sdk::read(provider, {0u, 0u}, "SONICADV_ALF", 3u, 25u);
        require(partial.completion.error == NativePortSaveError::None && partial.payload.size() == 25u * 512u &&
            std::equal(partial.payload.begin(), partial.payload.end(), chao.begin() + 3u * 512u),
            "Chao partial read did not skip the three header blocks");
        require(sdk::read(provider, {0u, 0u}, "SONICADV_ALF", 3u, 0u).payload == partial.payload,
                "zero count no longer means read to EOF");
        require(sdk::read(provider, {0u, 0u}, "SONICADV_ALF", 28u, 1u).completion.error == NativePortSaveError::InvalidArgument,
                "out-of-file read was accepted");
        require(sdk::read(provider, {0u, 1u}, "SONICADV_ALF", 3u, 25u).completion.error == NativePortSaveError::Absent,
                "absent slot aliased A1");
        directory = provider.list({{0u, 0u}});
        require(directory.status.free_blocks == 162u && sdk::free_game_blocks(directory) == 128u,
                "ordinary files consumed the wrong allocation domain");
        const auto installed = sdk::write(provider, {0u, 0u}, "SONICADV__VM", game, date, 0x800000FFu, true);
        require(installed.completion.error == NativePortSaveError::None && !installed.verification_failed,
                "VMU game write/readback failed");
        require(sdk::write(provider, {0u, 0u}, "SONICADV__VM", game, date, 0u, true).completion.error == NativePortSaveError::AlreadyExists,
                "VMU game create-only operation replaced an existing file");
        directory = provider.list({{0u, 0u}});
        require(directory.status.free_blocks == 34u && sdk::free_game_blocks(directory) == 0u,
                "VMU game occupancy was not reflected in both domains");
    }
    // Destroy and reopen platform/provider: this must use persisted volume
    // bytes, never an in-process filename/date/payload cache.
    {
        NativePortPlatformServices platform(platform_config);
        NativePortSaveProvider provider(platform, config);
        require(sdk::read(provider, {0u, 0u}, "SONICADV_000", 0u, 0u).payload == story,
                "Chao/VMU writes changed the story save");
        require(sdk::read(provider, {0u, 0u}, "SONICADV_ALF", 0u, 0u).payload == chao,
                "Chao payload did not survive reopening");
        require(sdk::read(provider, {0u, 0u}, "SONICADV__VM", 0u, 0u).payload == game,
                "VMU game payload did not survive reopening");
        const auto directory = provider.list({{0u, 0u}});
        for (const auto& entry : directory.entries) {
            if (entry.file_id == "SONICADV_000") continue;
            require(sdk::decode_date(entry) == date, "SDK date bytes changed or were not persisted");
            if (entry.file_id == "SONICADV__VM")
                require(sdk::is_game(entry) && (entry.user_flags & 0x800000FFu) == 0x800000FFu,
                        "VMU kind/flags did not survive reopening");
        }
        chao[3u * 512u + 19u] = std::byte{0xEEu};
        require(sdk::write(provider, {0u, 0u}, "SONICADV_ALF", chao, date, 0u, false).completion.error == NativePortSaveError::None,
                "Chao update failed on an existing file");
        const auto updated = sdk::read(provider, {0u, 0u}, "SONICADV_ALF", 3u, 25u);
        require(updated.payload[19u] == std::byte{0xEEu} &&
                sdk::read(provider, {0u, 0u}, "SONICADV_000", 0u, 0u).payload == story,
                "Chao update affected the wrong data");
    }
    // The only recursive cleanup target is the unique directory created above.
    require(std::filesystem::equivalent(root.parent_path(), std::filesystem::temp_directory_path()) &&
            root.filename().string().starts_with("sonic-save-sdk-"), "unsafe test cleanup path");
    std::filesystem::remove_all(root);
    std::cout << "SONIC_NATIVE_SAVE_SDK_PASS story/chao/game reopen; partial read; domains; metadata; verify\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << "SONIC_NATIVE_SAVE_SDK_FAIL " << error.what() << '\n';
    return 1;
}
