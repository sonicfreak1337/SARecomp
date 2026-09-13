#pragma once
#include "sonic_recovery_status.hpp"
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>
namespace sonic::profiles {
struct Preview {std::string id,digest;std::uint64_t generation=0,bytes=0;std::vector<std::string> files;};
struct Profile {std::string id;std::optional<Preview> preview;bool available=true;};
struct BackupStatus {
    recovery::BackupState state=recovery::BackupState::Waiting;
    std::uint64_t saved=0,failures=0,cleanup_failures=0;
};
BackupStatus backup_status() noexcept;
void initialize(const std::filesystem::path& base);
std::filesystem::path data_root(std::string_view profile);
const std::filesystem::path& library_root();
std::vector<std::string> list();
std::string create();
Preview inspect(std::span<const std::byte>);
std::vector<Preview> snapshots(bool imports=false);
Preview active_preview();
std::optional<Preview> profile_preview(std::string_view profile);
// Preview failures belong to their row; never repair or modify data here.
std::vector<Profile> catalog();
Preview import_candidate(const std::filesystem::path& file);
std::filesystem::path backup(bool automatic=false);
std::filesystem::path export_save(std::string_view expected_digest={});
// Called at completed guest frames; regular VMU commits remain authoritative.
void poll(std::uint64_t time_ns) noexcept;
// Import/restore is explicitly confirmed by the menu and committed only after
// the old game/VMU provider has been destroyed. Includes a versioned backup.
void stage_restore(std::string_view id,bool imported,std::string_view expected_digest={});
void apply_pending_restore();
std::filesystem::path export_diagnostics();
}
