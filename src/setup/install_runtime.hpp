#pragma once
#include "install_engine.hpp"
namespace sonic::setup {
// Installs only the files enumerated by the release payload manifest. The
// original content store and user profiles are never part of this payload.
std::filesystem::path install_runtime(const std::filesystem::path& source,
    const std::filesystem::path& content,const InstallCallback&,const std::atomic<bool>& cancel);
void start_game(const std::filesystem::path& program);
}
