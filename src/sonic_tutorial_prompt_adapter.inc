// Included at the single immediate textured-stream owner. All guest work,
// texture publication and prepare/finish transactions remain the original path.
void sonic_native_apply_tutorial_prompt(
    katana::runtime::NativePortContext& context, const SonicGuestReader& reader,
    const SonicNativeRegisteredTextureSelection& selection, std::uint32_t source,
    std::uint32_t count, katana::runtime::NativePortTextureHandle& texture,
    const std::vector<katana::runtime::NativePortVertex>& vertices) noexcept {
    const auto backing=[](std::uint32_t value){return value&0x1fffffffu;};
    if (!context.loaded_aot || !context.graphics || !context.textures ||
        backing(context.cpu->pr)!=0x0c9014ceu || backing(source)!=0x0c798718u ||
        count!=4 || vertices.size()!=4) return;
    try {
        const auto module=context.loaded_aot->active_module_for_address(context.cpu->pr);
        if (!module || module->sha256!="sha256:93969e279339fd1a9687e2cee7d842e544a41eb777283f2073a04adf1d531949" ||
            module->source_start!=0x82d61000u || backing(module->runtime_start)!=0x0c900000u ||
            module->byte_size!=695821u || !module->lifecycle_generation) return;
        std::uint32_t address=0,descriptor_address=0,key=0;
        SonicNativeTextureBinding binding;
        if (!reader.u32(sonic_ninja_texture_list_state,address) || backing(address)!=0x0c9097d4u ||
            !read_texlist(reader,address,binding) || !binding.valid || binding.texture_count!=3 ||
            !reader.u32(binding.texname_address+8,descriptor_address) ||
            backing(descriptor_address)!=backing(selection.source_descriptor) ||
            !reader.u32(descriptor_address,key) || key!=context.cpu->r[6]) return;
        const auto* descriptor=find_native_pvm_descriptor(context,reader,selection.source_descriptor);
        if (!descriptor) descriptor=find_native_guest_texture_descriptor(context,reader,selection.source_descriptor);
        if (!descriptor || !descriptor->guest_token || !descriptor->generation ||
            context.textures->resolve(descriptor->guest_token,descriptor->generation)!=texture) return;
        const auto owner=sonic_native_descriptor_catalog_entry(*descriptor);
        if (!owner || owner->second!=0) return;
        const auto archive=sonic_native::texture_archive_binding(owner->first);
        const auto payload=sonic_native_descriptor_payload(*descriptor);
        if (!archive || !payload || archive->entry_count!=3) return;
        const bool japanese=archive->content_relative_path=="SONICAD/TUTO_CMN.PRS" &&
            archive->byte_identity=="sha256:eafda6f0a68c370d99ed6689a5c41f3ae04e8b1b06d83bc764962907ea9c6da9" &&
            *payload=="sha256:aa1618e787d68b67cffde8ed00757eb8606e35ccb0faedda14a2f9faef206dbe";
        const bool english=archive->content_relative_path=="SONICAD/TUTO_CMN_E.PRS" &&
            archive->byte_identity=="sha256:5a33c0c55481b393316ff94c3eaa1dd1aa12ee5115585b6513df87614989deb3" &&
            *payload=="sha256:3530dc4c613dcc05dc7272b3d8af13f7edf4faf2f641e2dc4b7d5fc9b51a2c30";
        if (!japanese && !english) return;
        float left=640,right=0,top=480,bottom=0;
        for (const auto& vertex:vertices) {
            if (!std::isfinite(vertex.position[0]) || !std::isfinite(vertex.position[1]) ||
                !std::isfinite(vertex.position[2]) || vertex.position[2]<=0 ||
                !std::isfinite(vertex.texture_coordinate[0]) || !std::isfinite(vertex.texture_coordinate[1]) ||
                vertex.texture_coordinate[0]<-0.001f || vertex.texture_coordinate[0]>1.001f ||
                vertex.texture_coordinate[1]<-0.001f || vertex.texture_coordinate[1]>1.001f) return;
            left=std::min(left,vertex.position[0]);right=std::max(right,vertex.position[0]);
            top=std::min(top,vertex.position[1]);bottom=std::max(bottom,vertex.position[1]);
        }
        if (std::abs((right-left)-512.f)>0.01f || std::abs((bottom-top)-32.f)>0.01f ||
            left<0 || left>512 || top<0 || bottom>480) return;
        const unsigned visible_width=unsigned(std::clamp(640.f-left,128.f,512.f));
        const auto& settings=sonic::presentation::settings();
        auto labels=sonic::tutorial::labels(
            sonic::quit_prompt::effective_language(*context.cpu,settings.text_language),
            settings.bindings,sonic::input::resolved_glyph_style());
        auto& state=sonic_native_title_state;
        if (!state.tutorial_prompt_texture || state.tutorial_prompt_labels!=labels ||
            state.tutorial_prompt_generation!=module->lifecycle_generation ||
            state.tutorial_prompt_visible_width!=visible_width) {
            const auto image=sonic::tutorial::rasterize(labels,visible_width);
            katana::runtime::NativePortTextureConfig config;
            config.extent={image.width,image.height};
            config.format=katana::runtime::NativePortTextureFormat::Rgba8Unorm;
            config.mip_levels=1;config.shader_resource=true;config.dynamic=false;
            const std::array images{katana::runtime::NativePortImageView{
                config.extent,config.format,image.width*4,false,std::span(image.pixels)}};
            const auto replacement=context.graphics->create_texture(config,images);
            if (!replacement) return;
            try {retire_or_destroy_native_dynamic_texture(context,state.tutorial_prompt_texture,
                katana::runtime::NativePortTextureAssetPixelFormat::Argb4444);}
            catch (...) {context.graphics->destroy_texture(replacement);throw;}
            state.tutorial_prompt_texture=replacement;state.tutorial_prompt_labels=std::move(labels);
            state.tutorial_prompt_generation=module->lifecycle_generation;
            state.tutorial_prompt_visible_width=visible_width;
            std::fprintf(stderr,"SONIC_TUTORIAL_PROMPT bound=1 language=%d visible_width=%u original_transaction=1\n",
                state.tutorial_prompt_labels.language,visible_width);
        }
        texture=state.tutorial_prompt_texture;
    } catch (...) {
        // Optional host artwork must never turn a working guest tutorial into
        // a missing texture, altered transaction or a process abort.
    }
}
