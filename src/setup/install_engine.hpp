#pragma once
#include <atomic>
#include <filesystem>
#include <functional>
#include <string>
#include <stdexcept>

namespace sonic::setup {
struct InstallProgress { unsigned percent; std::string message; };
using InstallCallback=std::function<void(const InstallProgress&)>;
struct InstallResult { std::filesystem::path content; unsigned files=0; std::uint64_t bytes=0; };
class InstallError : public std::runtime_error { public: using std::runtime_error::runtime_error; };
// Resources are compiled-bound. Installation publishes a new content generation
// only after every output verifies; profile/Story/Chao directories are untouched.
InstallResult install(const std::filesystem::path& gdi,const std::filesystem::path& resources,
                      const std::filesystem::path& user_root,const InstallCallback&,
                      const std::atomic<bool>& cancel);
}
