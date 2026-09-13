#pragma once
#include "katana/runtime/runtime.hpp"
#include <array>
#include <string_view>
namespace katana::runtime { class NativePortImmutableWriteGuard; }
namespace sonic::matrix_vectors {
struct Leaf { std::uint32_t entry, size; std::string_view sha256; };
inline constexpr std::array leaves{
    Leaf{0x8C638E0Cu,0x58u,"dc20bddcbd5938d708a7669e769cf3b4b7209f04667e3777c0d6be57370740b8"},
    Leaf{0x8C638E68u,0x68u,"fe97aaf272e15ba9a42a46ccf5f7af51d6ab68112410e02ec79e581dbab839fc"},
    Leaf{0x8C638ED4u,0x2Cu,"229d431e2775b03ed38ef67c482c4e9bea39bac410c897ceb752f01e7e4395d7"},
    Leaf{0x8C638F00u,0x20u,"3f723ba70a79ca1afb5fd248ef863a512bbba44e1239bbf35245320e28e78997"},
};
// Exact complete PAL SDK leaves: point transform, direction transform (including
// the original normalization switch), XMTRX store, and translation extraction.
// False precedes every mutation. Ordinary RAM only; original FPU helpers and
// ordered stores remain authoritative. No cross-call source or matrix cache.
// The live caller owns immutable protection and instruction/cycle accounting.
[[nodiscard]] bool try_execute(katana::runtime::CpuState&,
    const katana::runtime::NativePortImmutableWriteGuard*);
} // namespace sonic::matrix_vectors
