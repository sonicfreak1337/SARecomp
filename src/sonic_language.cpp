#include "sonic_language.hpp"
#include "katana/runtime/runtime.hpp"
#include <array>
#include <iostream>
#include <stdexcept>

namespace sonic::language {
std::uint8_t save_options(std::uint8_t original,const presentation::Settings& settings) noexcept {
    unsigned value=original;
    if(settings.text_language>=0)
        value=(value&~0x70u)|((unsigned(settings.text_language)+1u)<<4u);
    if(settings.voice_language>=0)
        value=(value&~0x0Cu)|((unsigned(settings.voice_language)+1u)<<2u);
    if(settings.subtitles>=0)
        value=(value&~0x02u)|(settings.subtitles?0u:2u);
    return std::uint8_t(value);
}
}

namespace {
using namespace katana::runtime;
using namespace sonic::language;
using Action=NativePortHookAction;
constexpr std::uint32_t error_language=0x53414E01u;
bool enabled() noexcept {
    const auto& settings=sonic::presentation::settings();
    return settings.text_language>=0 || settings.voice_language>=0 || settings.subtitles>=0;
}
NativePortHookResult failure(const char* detail) noexcept {
    std::cerr<<"SONIC_LANGUAGE_ERROR "<<detail<<'\n';
    return {Action::Abort,0u,error_language};
}
void apply_record(CpuState& cpu) {
    const auto index=cpu.memory.read_u32(canonical_physical_address(selected_record));
    // Never infer an extra slot from adjacent title memory.
    if(index>=3u) return;
    const auto address=canonical_physical_address(records+index*record_bytes+options_offset);
    const auto previous=cpu.memory.read_u8(address);
    const auto next=save_options(previous,sonic::presentation::settings());
    if(next!=previous) {
        const std::array bytes{next};
        cpu.memory.write_bytes(address,bytes,CodeWriteSource::Copy);
        std::cerr<<"SONIC_LANGUAGE_SAVE_OPTIONS record="<<index
            <<" before="<<unsigned(previous)<<" after="<<unsigned(next)<<'\n';
    }
}
NativePortHookResult getter(NativePortContext& context,int preference) noexcept {
    if(preference<0) return {};
    if(!context.cpu || !context.aot.invoke_original) return failure("getter-context");
    const auto return_pc=context.cpu->pr;
    // Keep all original scratch-register, MACL, T and stack effects. Loaded
    // module bridges require the current runtime PC, never the source alias.
    const auto result=context.aot.invoke_original(context,context.cpu->pc);
    if(result.action!=Action::Return) return result;
    if(context.stop_reason!=NativePortStopReason::None || context.cpu->pc!=return_pc)
        return failure("getter-return");
    context.cpu->r[0]=std::uint32_t(preference);
    return result;
}
}

extern "C" NativePortHookResult sonic_language_initial(NativePortContext& context) noexcept {
    if(!enabled()) return {};
    if(!context.cpu) return failure("initial-context");
    if(context.cpu->pr!=0x8C054872u) return {};
    try {
        const auto& settings=sonic::presentation::settings();
        std::array<std::array<std::uint8_t,4>,3> values{};
        std::array<LinearMemoryTransactionWrite,3> writes{};
        std::size_t count=0;
        const auto add=[&](std::uint32_t address,int value,std::size_t bytes) {
            if(value<0) return;
            values[count][0]=std::uint8_t(value);
            writes[count]={canonical_physical_address(address),
                std::span<const std::uint8_t>(values[count].data(),bytes)};
            ++count;
        };
        add(text_global,settings.text_language,4);
        add(voice_global,settings.voice_language,4);
        add(subtitles_global,settings.subtitles,1);
        if(!context.cpu->memory.commit_linear_transaction_batch(
            std::span<const LinearMemoryTransactionWrite>(writes.data(),count),CodeWriteSource::Copy))
            return failure("initial-globals");
        return {};
    } catch(...) { return failure("initial-memory"); }
}
extern "C" NativePortHookResult sonic_language_text(NativePortContext& context) noexcept {
    return getter(context,sonic::presentation::settings().text_language);
}
extern "C" NativePortHookResult sonic_language_voice(NativePortContext& context) noexcept {
    return getter(context,sonic::presentation::settings().voice_language);
}
extern "C" NativePortHookResult sonic_language_subtitles(NativePortContext& context) noexcept {
    // The Options getter uses 0=on, 1=off; the global and host UI use 1=on.
    const auto preference=sonic::presentation::settings().subtitles;
    return getter(context,preference<0?-1:1-preference);
}
extern "C" NativePortHookResult sonic_language_subtitles_loaded(NativePortContext& context) noexcept {
    const auto preference=sonic::presentation::settings().subtitles;
    if(preference<0) return {};
    if(!context.cpu) return failure("subtitle-context");
    if(context.cpu->pr!=0x8C04C9D8u) return {};
    try {
        const std::array bytes{std::uint8_t(preference)};
        context.cpu->memory.write_bytes(canonical_physical_address(subtitles_global),bytes,CodeWriteSource::Copy);
        return {};
    } catch(...) { return failure("subtitle-memory"); }
}
extern "C" NativePortHookResult sonic_language_save(NativePortContext& context) noexcept {
    if(!enabled()) return {};
    if(!context.cpu) return failure("save-context");
    try {
        // Common retail serializer: merge before it copies the selected
        // record, adds current progress, computes its checksum and returns it.
        // The normal save pipeline owns publication; no extra VMU write here.
        apply_record(*context.cpu);
        return {};
    } catch(...) { return failure("save-memory"); }
}
extern "C" NativePortHookResult sonic_language_loaded(NativePortContext& context) noexcept {
    if(!enabled()) return {};
    if(!context.cpu) return failure("load-context");
    // This caller follows a successful SDK read, original checksum validation
    // and the complete 0xDE0-byte save copy. Other callers remain untouched.
    if(context.cpu->pr!=0x8C011750u) return {};
    return sonic_language_save(context);
}
