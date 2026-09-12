#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sonic::startup {
class Session final {
public:
    Session();
    ~Session();
    Session(const Session&)=delete;
    Session& operator=(const Session&)=delete;
};
void phase(std::string_view label,std::uint64_t completed=0,std::uint64_t total=0) noexcept;
void advance() noexcept;
void finish() noexcept;
void prefetch_program() noexcept;
void frame(std::uint64_t index) noexcept;
std::string digest(std::span<const std::byte> bytes);
inline std::string digest(std::string_view value) {return digest(std::as_bytes(std::span(value.data(),value.size())));}
std::string file_digest(const std::filesystem::path& path) noexcept;
std::vector<std::byte> cache_load(std::string_view domain,std::string_view key,std::size_t maximum) noexcept;
bool cache_save(std::string_view domain,std::string_view key,std::span<const std::byte> data) noexcept;
}
