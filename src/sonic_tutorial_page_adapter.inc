// SUMMARY page strip leaf only. No global texture replacement, guest texture
// mutation, changed page navigation, or replacement of the character pictures.
void sonic_native_apply_tutorial_page(
    katana::runtime::NativePortContext& context,const SonicGuestReader& reader,
    const SonicNativeRegisteredTextureSelection& selection,std::uint32_t source,
    std::uint32_t count,katana::runtime::NativePortTextureHandle& texture,
    const std::vector<katana::runtime::NativePortVertex>& vertices) noexcept {
    const auto backing=[](std::uint32_t value){return value&0x1fffffffu;};
    if(!context.loaded_aot || !context.graphics || !context.textures || !context.platform ||
        backing(context.cpu->pr)!=0x0c9017beu || backing(source)!=0x0c798718u ||
        count!=4 || vertices.size()!=4)return;
    try {
        const auto module=context.loaded_aot->active_module_for_address(context.cpu->pr);
        if(!module || module->sha256!="sha256:93969e279339fd1a9687e2cee7d842e544a41eb777283f2073a04adf1d531949" ||
            module->source_start!=0x82d61000u || backing(module->runtime_start)!=0x0c900000u ||
            module->byte_size!=695821u || !module->lifecycle_generation)return;
        std::uint32_t controller=0,control_work=0,page_task=0,work1=0,work2=0,root=0,page_index=0;
        std::uint32_t language=0,language_base=0,pages=0,list=0,texlist=0;
        if(!reader.u32(0x8c90e1f4u,controller) || !reader.u32(controller+32,control_work) ||
            !reader.u32(control_work+20,page_task) || !reader.u32(page_task+32,work1) ||
            !reader.u32(page_task+44,work2) || !reader.u32(work1+16,root) ||
            !reader.u32(work2+8,page_index) || !reader.u32(0x8c754b3cu,language) || language>4 ||
            !reader.u32(0x8c90bba8u+language*4,language_base) ||
            root<language_base || (root-language_base)%24 || (root-language_base)/24>=6 ||
            backing(root)!=backing(context.cpu->r[9]) || !reader.u32(root+4,pages))return;
        const auto iterator=context.cpu->r[10];
        if(iterator!=root+12 && iterator!=root+16)return;
        const auto slot=(iterator-root-12)/4;
        std::uint16_t page_count=0,strip_count=0,ordinal=0;
        if(!reader.u16(root,page_count) || !page_count || page_count>32 || page_index>=page_count ||
            !reader.u32(pages+page_index*20+12+slot*4,list) || !list ||
            backing(list)!=backing(context.cpu->r[12]) || !reader.u16(list,strip_count) ||
            strip_count>64 || context.cpu->r[13]>=strip_count ||
            backing(list+2+context.cpu->r[13]*6)!=backing(context.cpu->r[14]) ||
            backing(context.cpu->r[11])!=0x0c07e582u ||
            !reader.u16(list+2+context.cpu->r[13]*6,ordinal) ||
            !reader.u32(root+8+slot*4,texlist) || !texlist)return;
        std::uint32_t current=0,descriptor_address=0,key=0;SonicNativeTextureBinding binding;
        if(!reader.u32(sonic_ninja_texture_list_state,current) || backing(current)!=backing(texlist) ||
            !read_texlist(reader,texlist,binding) || !binding.valid || ordinal>=binding.texture_count ||
            !reader.u32(binding.texname_address+ordinal*12+8,descriptor_address) ||
            backing(descriptor_address)!=backing(selection.source_descriptor) ||
            !reader.u32(descriptor_address,key) || key!=context.cpu->r[6])return;
        const auto* descriptor=find_native_pvm_descriptor(context,reader,selection.source_descriptor);
        if(!descriptor)descriptor=find_native_guest_texture_descriptor(context,reader,selection.source_descriptor);
        if(!descriptor || !descriptor->guest_token || !descriptor->generation ||
            context.textures->resolve(descriptor->guest_token,descriptor->generation)!=texture)return;
        const auto owner=sonic_native_descriptor_catalog_entry(*descriptor);
        if(!owner || owner->second!=ordinal)return;
        const auto archive=sonic_native::texture_archive_binding(owner->first);
        const auto payload=sonic_native_descriptor_payload(*descriptor);
        if(!archive || !payload || archive->entry_count!=binding.texture_count)return;
        const auto* art=sonic::tutorial::find_artwork(archive->content_relative_path,
            archive->byte_identity,ordinal,*payload);
        if(!art)return;
        for(const auto& vertex:vertices){
            if(!std::isfinite(vertex.position[0]) || !std::isfinite(vertex.position[1]) ||
                !std::isfinite(vertex.position[2]) || vertex.position[2]<=0 ||
                !std::isfinite(vertex.texture_coordinate[0]) || !std::isfinite(vertex.texture_coordinate[1]) ||
                vertex.texture_coordinate[0]<-0.001f || vertex.texture_coordinate[0]>1.001f ||
                vertex.texture_coordinate[1]<-0.001f || vertex.texture_coordinate[1]>1.001f)return;
        }
        const auto& settings=sonic::presentation::settings();
        const sonic::tutorial::Controls controls{
            sonic::quit_prompt::effective_language(*context.cpu,settings.text_language),settings.bindings,
            sonic::input::resolved_glyph_style(),settings.camera_style==sonic::camera::Style::Recompiled,
            settings.swap_sticks!=0,settings.mouse_camera!=0};
        auto& state=sonic_native_title_state;
        if(state.tutorial_art_generation!=module->lifecycle_generation || state.tutorial_art_controls!=controls){
            release_native_tutorial_art(context);state.tutorial_art_generation=module->lifecycle_generation;
            state.tutorial_art_controls=controls;
        }
        for(const auto& view:state.tutorial_art_views)if(view.artwork==art){texture=view.texture;return;}
        // At most two character archives are live in a page family. These
        // CPU images are owned with this SUMMARY generation, never per draw.
        auto pixels=std::ranges::find_if(state.tutorial_art_sources,[&](const auto& row){return row.catalog_index==owner->first;});
        if(pixels==state.tutorial_art_sources.end()){
            if(state.tutorial_art_sources.size()>=2 || archive->byte_size>4*1024*1024)return;
            const katana::runtime::NativePortContentFileBinding content{archive->logical_id,
                std::filesystem::path(archive->content_relative_path),archive->byte_identity,0,archive->byte_size};
            auto file=context.platform->open_content_file(content);
            std::vector<std::uint8_t> encoded(std::size_t(archive->byte_size));
            file->read_at(0,std::as_writable_bytes(std::span(encoded)));
            auto decoded=katana::runtime::decode_native_port_prs_pvm_texture_archive(encoded);
            if(decoded.size()!=archive->entry_count)return;
            state.tutorial_art_sources.push_back({owner->first,std::move(decoded)});
            pixels=std::prev(state.tutorial_art_sources.end());
        }
        if(ordinal>=pixels->entries.size() || state.tutorial_art_views.size()>=64)return;
        const auto& original=pixels->entries[ordinal];
        if(original.archive_ordinal!=ordinal || original.extent.width!=art->width ||
            original.extent.height!=art->height)return;
        const auto image=sonic::tutorial::rasterize_artwork(*art,controls,original.rgba8);
        katana::runtime::NativePortTextureConfig config;config.extent={image.width,image.height};
        config.format=katana::runtime::NativePortTextureFormat::Rgba8Unorm;
        config.mip_levels=1;config.shader_resource=true;config.dynamic=false;
        const std::array images{katana::runtime::NativePortImageView{
            config.extent,config.format,image.width*4,false,std::span(image.pixels)}};
        const auto replacement=context.graphics->create_texture(config,images);
        if(!replacement)return;
        try{state.tutorial_art_views.push_back({art,replacement});}
        catch(...){context.graphics->destroy_texture(replacement);throw;}
        texture=replacement;
        std::fprintf(stderr,"SONIC_TUTORIAL_PAGE bound=1 root=%08x page=%u ordinal=%u glyph_regions=%zu archive=%.*s original_transaction=1\n",
            root,page_index,ordinal,art->spans.size(),int(art->archive.size()),art->archive.data());
    }catch(...){
        // Optional presentation failures retain the working original artwork.
    }
}
