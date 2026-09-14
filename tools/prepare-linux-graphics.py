"""Share current validated Vulkan draw contracts with the SDL window host.

This is a developer build step, not title analysis or end-user code generation.
The retained Windows source is read-only. Extraction fails on moved boundaries.
"""
from pathlib import Path
import re
import sys

source, output = map(Path, sys.argv[1:])
text = source.read_text(encoding='utf-8')
backend = text[text.index('class NativePortGraphicsBackend final {'):text.index('\n#else\n\n#include "../../linux/native_port_graphics_sdl.inc"')]

def section(data, first, last):
    if data.count(first) != 1 or last not in data[data.index(first):]:
        raise RuntimeError('Graphics extraction boundary changed: '+first[:90])
    begin = data.index(first); end = data.index(last, begin)
    return data[begin:end]

def brace_end(data, start):
    # No shader raw strings occur in the extracted C++ methods. Skip quoted
    # diagnostic text and comments so braces in messages cannot affect scope.
    token = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]')
    depth=0
    for match in token.finditer(data,start):
        if match.group()=='{': depth+=1
        elif match.group()=='}':
            depth-=1
            if depth==0: return match.end()
    raise RuntimeError('Unbalanced graphics scope')

def method(name):
    pattern = re.compile(r'^    (?:\[\[nodiscard\]\] )?(?:static )?[\w:<>&]+\s+'+name+r'\s*\(', re.M)
    match=pattern.search(backend)
    if not match: raise RuntimeError('Missing graphics method '+name)
    start=backend.index('{',match.start())
    return backend[match.start():brace_end(backend,start)]+'\n'

def remove_scope(data, marker):
    if data.count(marker)!=1: raise RuntimeError('Graphics scope changed: '+marker)
    start=data.index(marker); brace=data.index('{',start)
    return data[:start]+data[brace_end(data,brace):]

def vulkan_only(data):
    # Specialize only exact backend-selection conditionals, leaving every
    # packet, depth, alpha, clipping and resource check unchanged.
    pattern=re.compile(r'if\s*\(\s*(!?)vulkan_\s*\)\s*')
    while match:=pattern.search(data):
        start=match.end()
        if data[start]=='{':
            end=brace_end(data,start); keep=data[start:end]
        else:
            end=data.index(';',start)+1; keep=data[start:end]
        after=end
        otherwise=re.match(r'\s*else\s*',data[end:])
        if otherwise:
            other=end+otherwise.end()
            if data[other]!='{': raise RuntimeError('Unexpected backend branch')
            after=brace_end(data,other)
            if match.group(1):keep=data[other:after]
        elif match.group(1):keep=''
        data=data[:match.start()]+keep+data[after:]
    return data

constants=section(text,'constexpr std::uint32_t performance_overlay_width','\n#endif\n\n[[nodiscard]] constexpr bool runtime_presentation_rate_choice')
constants+=section(text,'constexpr std::uint32_t draw_flag_vertex_color','[[nodiscard]] ComPtr<ID3DBlob> compile_shader')
types=section(backend,'    struct GeometryCapabilities final {','    enum class GpuTimingState')
types+=section(backend,'    struct DrawConstants final {','    struct TypeTwoFragmentGpu')
types+=section(backend,'    struct TextureSlot final {','    static LRESULT CALLBACK window_proc')
types=re.sub(r'^        ComPtr<[^\n]+\n','',types,flags=re.M)

methods=section(backend,'    [[nodiscard]] GeometryCapabilities validate_geometry(', '    [[nodiscard]] ComPtr<ID3D11Buffer> create_immutable_buffer(')
methods+=section(backend,'    [[nodiscard]] static NativePortTextureHandle make_texture_handle(', '    std::string title_storage_;')
methods+=method('refresh_layout')
methods+=section(backend,'    [[nodiscard]] std::uint64_t environment_unsigned(', '    void capture_completed_frame(')
methods+=section(backend,'    void start_render_submit_telemetry()', '    void disable_gpu_timing()')

textures=section(backend,'    [[nodiscard]] NativePortTextureHandle create_texture(', '    void begin_frame(')
textures=textures.replace(section(textures,'        const auto* const destroyed_view', '        if (vulkan_) vulkan_->destroy_texture(slot.vulkan_texture);'),'')
textures=re.sub(r'^\s*slot\.(texture|view|vertex_buffer|index_buffer)\.Reset\(\);\n','\n',textures,flags=re.M)
methods+=vulkan_only(textures)

draw=method('draw_immediate')
draw=draw.replace(section(draw,'        ID3D11Buffer* bound_vertex_buffer = nullptr;', '        const auto& current_layout = cached_layout_;'),'')
draw=draw.replace(section(draw,'        if ((packet.fog.mode == NativePortFogMode::LookupTable ||','        constants.fog_parameters = {'),'')
draw=remove_scope(draw,'if (!vulkan_ && (!draw_constants_valid_ ||')
methods+=vulkan_only(draw)+method('draw')

for name in ['begin_frame','complete_frame','present','completed_frame_ready','completed_image_available',
             'present_completed_image_on_deadline','abort_frame_after_command_failure','repeat_present',
             'flush_type2_translucency','begin_type2_subpass','present_image','snapshot']:
    value=method(name)
    if name=='abort_frame_after_command_failure': value=remove_scope(value,'if (context_)') if 'if (context_) {' in value else value.replace('        if (context_)\n            context_->OMSetRenderTargets(\n                1u, render_target_.GetAddressOf(), depth_view_.Get());','')
    value=vulkan_only(value)
    value=re.sub(r'^\s*(?:abandon_gpu_timing_frame|retire_gpu_timing_queries_nonblocking|begin_gpu_timing_frame|end_gpu_timing_frame|unbind_type2_subpass|restore_working_frame_attachments|invalidate_draw_state_shadow)\([^;]*;\n','\n',value,flags=re.M)
    value=re.sub(r'^\s*(?:render_texture_|render_target_|render_view_)\.Swap\([^;]*;\n','\n',value,flags=re.M)
    value=re.sub(r'^\s*completed_host_(?:texture|view)_\.Reset\(\);\n','\n',value,flags=re.M)
    value=value.replace('vulkan_ != nullptr || device_ != nullptr','vulkan_ != nullptr')
    methods+=value

admission=method('validate_type2_scene_admission')
admission=admission.replace(section(admission,'        if (!vulkan_ && (feature_level_', '        // PVR Type-2 ordering'),'')
methods+=admission
capture=method('capture_completed_frame')
methods+=vulkan_only(capture)
methods=methods.replace('UINT','std::uint32_t')

members=section(backend,'    bool frame_batch_active_ = false;', '    std::vector<BlendStateSlot> blend_states_;')
members+=section(backend,'    std::vector<TextureSlot> texture_slots_;','    NativePortGraphicsSnapshot snapshot_;')
members+='    NativePortGraphicsSnapshot snapshot_;\n'

output.mkdir(parents=True,exist_ok=True)
for name,value in {'constants':constants,'types':types,'methods':methods,'members':members}.items():
    value='// Generated from the current shared Windows/Vulkan contracts. Do not edit.\n'+value
    path=output/('linux_graphics_'+name+'.inc')
    if not path.exists() or path.read_text(encoding='utf-8')!=value:path.write_text(value,encoding='utf-8')
print('SONIC_LINUX_GRAPHICS_SHARED geometry=retained draw_constants=retained diagnostics=retained')
