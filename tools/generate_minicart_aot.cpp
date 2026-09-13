// Port-local supplement: compile the missing MINICART results callback closure
// with the sealed r354 native backend. No SDK/AOT archive is rebuilt or edited.
#include "katana/io/raw_binary_loader.hpp"
#include "katana/io/input_provenance.hpp"
#include "katana/analysis/control_flow_analysis.hpp"
#include "katana/ir/lower.hpp"
#include "katana/ir/optimize.hpp"
#include "katana/codegen/cpp_emitter.hpp"
#include "katana/codegen/native_aot_profile.hpp"
#include "katana/runtime/native_port_texture_asset.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void write(const std::filesystem::path& path, const std::string& data) {
    std::ifstream previous(path,std::ios::binary);
    if(previous && std::string(std::istreambuf_iterator<char>(previous),{})==data)return;
    std::ofstream stream(path, std::ios::binary);
    stream.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!stream) throw std::runtime_error("MINICART supplement output failed");
}
std::string hash(const std::vector<std::uint8_t>& bytes) {
    return katana::io::sha256_bytes({reinterpret_cast<const char*>(bytes.data()),bytes.size()});
}
}
int main(int argc, char** argv) try {
    if (argc != 3) throw std::runtime_error("usage: generator MINICART.PRS output-directory");
    std::ifstream input(argv[1],std::ios::binary);
    std::vector<std::uint8_t> encoded{std::istreambuf_iterator<char>(input),{}};
    if (hash(encoded) != "44d5b16521acee36a392b2ca0e57d9ba2826c83b14983e25f550e01d975747e0")
        throw std::runtime_error("MINICART encoded identity mismatch");
    const auto decoded=katana::runtime::decompress_native_port_prs(encoded);
    if (decoded.size()!=1434052u || hash(decoded)!="2d3ec72d9f62a0ec626155f822a77bac7209999aa89f825b3082a69cf85db2a1")
        throw std::runtime_error("MINICART decoded identity mismatch");
    const std::filesystem::path output(argv[2]);
    std::filesystem::create_directories(output);
    write(output/"minicart.bin", {reinterpret_cast<const char*>(decoded.data()),decoded.size()});
    katana::io::RawBinaryLoadOptions load;
    load.base_address=0x82980000u;load.entry_point=0x8298A81Eu;
    std::vector<katana::ir::Function> program;
    // The resident record writer calls 007A, six bytes before the retained
    // aligned owner 0080. Include the true entry and its complete original
    // body; do not skip the stack/PR prologue by redirecting callers to 0080.
    for(const auto root:{0x8298007Au,0x8298A81Eu}) {
        load.entry_point=root;
        auto image=katana::io::load_raw_binary(output/"minicart.bin",load);
        auto analysis=katana::analysis::analyze_control_flow(image);
        auto discovered=katana::ir::lower_program(analysis);
        program.insert(program.end(),std::make_move_iterator(discovered.begin()),
            std::make_move_iterator(discovered.end()));
    }
    std::sort(program.begin(),program.end(),[](const auto& a,const auto& b){return a.entry_address<b.entry_address;});
    std::set<std::uint32_t> entries;
    for(const auto& function:program)entries.insert(function.entry_address);
    if(program.size()!=3u || entries!=std::set<std::uint32_t>{0x8298007Au,0x8298A81Eu,0x8298AA60u})
        throw std::runtime_error("MINICART results closure changed");
    // Store pre-optimization byte windows; generated switch cases below select
    // exactly which resumable entries are exported from those original blocks.
    std::ostringstream windows;
    for(const auto& function:program)for(const auto& block:function.blocks){
        std::uint32_t end=block.start_address;
        for(const auto& instruction:block.instructions)end=std::max(end,instruction.source_address+2u);
        windows<<std::hex<<function.entry_address<<'\t'<<block.start_address<<'\t'<<end<<'\n';
    }
    write(output/"windows.tsv",windows.str());
    (void)katana::ir::optimize_program(program,{});
    katana::codegen::NativeAotBackendRequestOptions options;
    options.symbol_namespace="katana_port_generated";
    options.emit_run_functions=false;
    options.runtime_binding=katana::codegen::BackendRuntimeBinding::NativePort;
    options.native_bringup_dispatch_validation=true;
    auto request=katana::codegen::make_native_aot_backend_request(
        katana::codegen::NativeAotEmissionProfile::Product,program,0x8298A81Eu,options);
    std::vector<std::uint32_t> checked_calls;
    for(const auto& function:program)for(const auto& block:function.blocks)
        for(const auto& instruction:block.instructions)
            if(katana::codegen::requires_native_bringup_dispatch_validation(instruction))
                checked_calls.push_back(instruction.source_address);
    std::sort(checked_calls.begin(),checked_calls.end());
    checked_calls.erase(std::unique(checked_calls.begin(),checked_calls.end()),checked_calls.end());
    request.native_bringup_dispatch_callsites=checked_calls;
    write(output/"minicart-aot.cpp",katana::codegen::emit_cpp_port_translation_unit(request).joined_text());
    std::cout<<"SONIC_MINICART_AOT_GENERATED functions="<<program.size()<<'\n';
    return 0;
} catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
