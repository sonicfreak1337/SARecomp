// Exercise the actual metadata retirement helpers, including interrupted
// refresh replay and the stale-array-count counterexample from source review.
#define main sonic_provider_refresh_cli_main
#include "refresh_native_provider.cpp"
#undef main

int main(){try{
    using namespace katana::runtime;
    const NativePortHookBinding hook{0x8C028BFEu,0x246u,NativePortHookKind::FunctionEntry,
        NativePortHookRequirement::Required,NativePortHookOriginalPolicy::MayContinueOriginal,
        "sonic_native_near_collision",
        "sha256:1039a254e1a32bfd40deedc30c20cb7e4cb8c082732c926160ba58a95c6992c8",
        "sha256:c2b0ca349bf1523bd47cc1bdd6a5af9f8853a9a4c280cba60a31c5c8d27d061d"};
    const std::string name(hook.symbol);
    unsigned cases=0u;
    const auto check=[&](bool condition){if(!condition)throw std::runtime_error("retirement test failed case "+std::to_string(cases+1u));++cases;std::cerr<<"retirement-case="<<cases<<'\n';};
    const auto rejects=[&](auto operation){bool rejected=false;try{operation();}catch(const std::runtime_error&){rejected=true;}check(rejected);};
    std::vector<NativePortHookBinding> input{hook},remaining;
    NativePortDefinition before,after;before.hooks=input;
    std::optional<NativePortHookBinding> retired;
    const auto filtered=without_retired_near_hook(before,after,remaining,retired);
    check(retired && filtered.hooks.empty() && input.size()==1u);
    input[0].original_policy=NativePortHookOriginalPolicy::ReplacesOriginal;
    retired.reset();rejects([&]{without_retired_near_hook(before,after,remaining,retired);});
    input[0]=hook;input.push_back(hook);before.hooks=input;
    retired.reset();rejects([&]{without_retired_near_hook(before,after,remaining,retired);});

    const std::string row=hook_prefix(hook)+"\""+std::string(hook.provider_implementation_identity)+"\""+hook_suffix(hook);
    auto keep=hook;keep.guest_address=0x8C029B00u;keep.covered_size=0xB6Cu;
    keep.symbol="sonic_native_collision_candidates";
    keep.code_identity="sha256:744ca095c47e07d9b87ed43cf54e013c45349cd472852cf44cee85dd7b946832";
    const std::string keep_row=hook_prefix(keep)+"\""+std::string(keep.provider_implementation_identity)+"\""+hook_suffix(keep);
    const std::string witness="{{0x8C028BFEu, 0x0C028BFEu}, frozen_entry},\n";
    std::string dispatch=witness+
        "constexpr std::array<katana::runtime::NativePortHookBinding, 2u> native_hooks{{\n    "+row+",\n    "+keep_row+",\n}};\n"+
        "extern \"C\" katana::runtime::NativePortHookResult "+name+"(katana::runtime::NativePortContext&) noexcept;\n"+
        "        case 0x8C028BFEu: return HookDispatch{true, HookKind::FunctionEntry, HookRequirement::Required, katana::runtime::NativePortHookOriginalPolicy::MayContinueOriginal, &"+name+", 0x8C028BFEu, 582u};\n"+
        "    if ((source | 0x20000000u) == 0xAC028BFEu) return true;\n"+
        "        if ((source | 0x20000000u) == 0xAC028BFEu) return false;\n";
    std::string audit="constexpr std::array<std::string_view, 2> required_tokens{\n    std::string_view{\"keep\"},\n    std::string_view{\""+name+"\"},\n};\n";
    const auto original_dispatch=dispatch,original_audit=audit;
    retire_near_metadata(dispatch,audit,hook,2u);
    check(dispatch==witness+"constexpr std::array<katana::runtime::NativePortHookBinding, 1u> native_hooks{{\n    "+keep_row+",\n}};\n");
    check(audit=="constexpr std::array<std::string_view, 1> required_tokens{\n    std::string_view{\"keep\"},\n};\n");
    const auto canonical_dispatch=dispatch,canonical_audit=audit;
    retire_near_metadata(dispatch,audit,hook,2u);
    check(dispatch==canonical_dispatch && audit==canonical_audit);
    dispatch.replace(dispatch.find("1u> native_hooks"),2u,"2u");
    rejects([&]{retire_near_metadata(dispatch,audit,hook,2u);});
    dispatch=canonical_dispatch;audit.replace(audit.find("1> required_tokens"),1u,"2");
    rejects([&]{retire_near_metadata(dispatch,audit,hook,2u);});
    dispatch=original_dispatch;audit=original_audit;dispatch.erase(0,witness.size());
    rejects([&]{retire_near_metadata(dispatch,audit,hook,2u);});
    dispatch=original_dispatch;dispatch+= "    if ((source | 0x20000000u) == 0xAC028BFEu) return true;\n";
    rejects([&]{retire_near_metadata(dispatch,audit,hook,2u);});
    std::cout<<"SONIC_PROVIDER_RETIREMENT_TEST_OK cases="<<cases<<'\n';
    return 0;
}catch(const std::exception& e){std::cerr<<"SONIC_PROVIDER_RETIREMENT_TEST_FAILED "<<e.what()<<'\n';return 1;}
}
