#define NOMINMAX
#include <windows.h>
#include "volk.h"
#include "sonic_vulkan.hpp"
#include "renderer_selection.hpp"
#include "sonic_vulkan_shaders.hpp"
#include "../sonic_startup.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace sonic::rendering {
using namespace katana::runtime;
namespace {
void check(VkResult result, const char* operation) {
    if (result == VK_SUCCESS) return;
    std::cerr << "SONIC_VULKAN_ERROR operation=" << operation << " result=" << result << '\n';
    throw NativePortGraphicsError(result == VK_ERROR_DEVICE_LOST ? NativePortGraphicsFailure::DeviceLost :
        NativePortGraphicsFailure::ResourceCreation, static_cast<unsigned>(result), operation);
}
constexpr VkImageSubresourceRange range(VkImageAspectFlags aspect, unsigned levels=1) {
    return {aspect, 0, levels, 0, 1};
}
template<class T> std::span<const std::byte> bytes(const T& object) { return std::as_bytes(std::span(&object,1)); }
constexpr VkBlendFactor factors[] = {VK_BLEND_FACTOR_ZERO,VK_BLEND_FACTOR_ONE,VK_BLEND_FACTOR_SRC_COLOR,
    VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,VK_BLEND_FACTOR_DST_COLOR,VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
    VK_BLEND_FACTOR_SRC_ALPHA,VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,VK_BLEND_FACTOR_DST_ALPHA,VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA};
constexpr VkBlendOp operations[] = {VK_BLEND_OP_ADD,VK_BLEND_OP_SUBTRACT,VK_BLEND_OP_REVERSE_SUBTRACT,VK_BLEND_OP_MIN,VK_BLEND_OP_MAX};
constexpr VkPrimitiveTopology topologies[] = {VK_PRIMITIVE_TOPOLOGY_POINT_LIST,VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
    VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP};
constexpr VkSamplerAddressMode addresses[] = {VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,VK_SAMPLER_ADDRESS_MODE_REPEAT,VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT};
}

struct VulkanRenderer::Impl {
    struct Image {
        VkImage image{}; VkDeviceMemory memory{}; VkImageView view{};
        VkFormat format{}; VkExtent2D extent{}; unsigned levels=1;
        VkImageAspectFlags aspect=VK_IMAGE_ASPECT_COLOR_BIT;
        VkImageLayout layout=VK_IMAGE_LAYOUT_UNDEFINED;
        bool external=false;
    };
    struct Buffer { VkBuffer buffer{}; VkDeviceMemory memory{}; VkDeviceSize size{}; std::byte* mapped{}; };
    struct Mesh { Buffer vertices,indices; unsigned vertex_count{},index_count{}; };
    struct UploadBlock { Buffer buffer; VkDeviceSize used=0; };
    struct Submission {
        VkCommandPool pool{}; VkCommandBuffer command{}; VkFence fence{}; VkSemaphore acquire{};
        std::vector<VkDescriptorPool> descriptors; unsigned descriptor_pool=0, sets=0;
        std::vector<UploadBlock> uploads; unsigned upload_block=0;
        std::vector<std::function<void()>> retired; bool pending=false;
    };
    VkInstance instance{}; VkSurfaceKHR surface{}; VkPhysicalDevice physical{}; VkDevice device{};
    HWND window{};
    bool exclusive_extension=false,exclusive_requested=false,exclusive_controlled=false;
    bool exclusive_acquired=false,exclusive_attempted=false;
    VkDebugUtilsMessengerEXT debug_messenger{};
    VkPhysicalDeviceProperties properties{}; VkPhysicalDeviceMemoryProperties memory_properties{};
    VkQueue queue{}; unsigned queue_family{}; VkSwapchainKHR swapchain{}; VkFormat swap_format{};
    VkExtent2D swap_extent{}; bool swap_dirty=false;
    std::vector<Image> swap_images; std::vector<VkSemaphore> present_semaphores;
    std::array<Submission,3> submissions{}; unsigned submission_index=0;
    Submission* current=nullptr; VkCommandBuffer command{}; bool rendering=false;
    Image working,completed,depth,white,type_base,type_depth,type_heads,type_counts;
    Buffer type_fragments,type_status; bool type_ready=false,completed_ready=false;
    NativePortGraphicsConfig config;
    std::unordered_map<std::uint64_t,Image> textures;
    std::unordered_map<std::uint64_t,Mesh> meshes;
    std::uint64_t next_id=1;
    VkDescriptorSetLayout draw_layout{},capture_layout{},resolve_layout{},composite_layout{},overlay_layout{};
    VkPipelineLayout draw_pipeline_layout{},capture_pipeline_layout{},resolve_pipeline_layout{},composite_pipeline_layout{},overlay_pipeline_layout{};
    VkShaderModule draw_vs{},draw_ps{},capture_ps{},composite_vs{},composite_ps{},resolve_ps{},overlay_ps{};
    VkPipelineCache pipeline_cache{};
    std::string cache_key;
    bool warming_pipelines=false;
    VkFormat depth_format=VK_FORMAT_D32_SFLOAT;
    std::unordered_map<std::string,VkPipeline> pipelines;
    std::unordered_set<std::string> unused_warm_pipelines;
    std::vector<std::pair<NativePortSamplerState,VkSampler>> samplers;

    explicit Impl(void* window,const NativePortGraphicsConfig& settings) : config(settings) {
        try { initialize(window); } catch (...) { cleanup(); throw; }
    }
    ~Impl() { cleanup(); }
    void cleanup() noexcept;
    void initialize(void* window);
    void create_swapchain();
    void destroy_swapchain();
    bool wants_exclusive() const {
        return selected_window_mode==WindowMode::Fullscreen && IsWindowVisible(window) &&
            !(GetWindowLongPtrW(window,GWL_STYLE)&WS_OVERLAPPEDWINDOW);
    }
    unsigned memory_type(unsigned mask,VkMemoryPropertyFlags flags) {
        for(unsigned i=0;i<memory_properties.memoryTypeCount;++i)
            if((mask&(1u<<i)) && (memory_properties.memoryTypes[i].propertyFlags&flags)==flags) return i;
        throw NativePortGraphicsError(NativePortGraphicsFailure::HardwareDeviceUnavailable,0,"vulkan-memory-type");
    }
    Buffer make_buffer(VkDeviceSize size,VkBufferUsageFlags usage,bool host=false) {
        Buffer result; result.size=std::max<VkDeviceSize>(size,4);
        try {
            VkBufferCreateInfo create{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; create.size=result.size; create.usage=usage;
            check(vkCreateBuffer(device,&create,nullptr,&result.buffer),"vulkan-buffer");
            VkMemoryRequirements needs; vkGetBufferMemoryRequirements(device,result.buffer,&needs);
            VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; allocation.allocationSize=needs.size;
            allocation.memoryTypeIndex=memory_type(needs.memoryTypeBits,host ?
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            check(vkAllocateMemory(device,&allocation,nullptr,&result.memory),"vulkan-buffer-memory");
            check(vkBindBufferMemory(device,result.buffer,result.memory,0),"vulkan-buffer-bind");
            if(host) check(vkMapMemory(device,result.memory,0,VK_WHOLE_SIZE,0,reinterpret_cast<void**>(&result.mapped)),"vulkan-buffer-map");
        } catch (...) { destroy(result); throw; }
        return result;
    }
    Image make_image(VkExtent2D extent,VkFormat format,VkImageUsageFlags usage,unsigned levels=1,VkImageAspectFlags aspect=VK_IMAGE_ASPECT_COLOR_BIT) {
        Image result; result.extent=extent; result.format=format; result.levels=levels; result.aspect=aspect;
        try {
            VkImageCreateInfo create{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO}; create.imageType=VK_IMAGE_TYPE_2D;
            create.format=format; create.extent={extent.width,extent.height,1}; create.mipLevels=levels; create.arrayLayers=1;
            create.samples=VK_SAMPLE_COUNT_1_BIT; create.tiling=VK_IMAGE_TILING_OPTIMAL; create.usage=usage;
            check(vkCreateImage(device,&create,nullptr,&result.image),"vulkan-image");
            VkMemoryRequirements needs; vkGetImageMemoryRequirements(device,result.image,&needs);
            VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; allocation.allocationSize=needs.size;
            allocation.memoryTypeIndex=memory_type(needs.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            check(vkAllocateMemory(device,&allocation,nullptr,&result.memory),"vulkan-image-memory");
            check(vkBindImageMemory(device,result.image,result.memory,0),"vulkan-image-bind");
            VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO}; view.image=result.image; view.viewType=VK_IMAGE_VIEW_TYPE_2D;
            view.format=format; view.subresourceRange=range(aspect,levels);
            check(vkCreateImageView(device,&view,nullptr,&result.view),"vulkan-image-view");
        } catch (...) { destroy(result); throw; }
        return result;
    }
    void destroy(Buffer& value) noexcept {
        if(value.mapped) vkUnmapMemory(device,value.memory);
        if(value.buffer) vkDestroyBuffer(device,value.buffer,nullptr);
        if(value.memory) vkFreeMemory(device,value.memory,nullptr);
        value={};
    }
    void destroy(Image& value) noexcept {
        if(value.view) vkDestroyImageView(device,value.view,nullptr);
        if(value.image && !value.external) vkDestroyImage(device,value.image,nullptr);
        if(value.memory) vkFreeMemory(device,value.memory,nullptr);
        value={};
    }
    void start();
    void end_render() { if(rendering) { vkCmdEndRendering(command); rendering=false; } }
    void submit(VkSemaphore wait={},VkSemaphore signal={});
    void finish() { submit(); check(vkQueueWaitIdle(queue),"vulkan-wait-idle"); }
    void barrier(Image& image,VkImageLayout layout) {
        start(); end_render();
        VkImageMemoryBarrier2 b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        b.srcStageMask=VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT; b.srcAccessMask=VK_ACCESS_2_MEMORY_READ_BIT|VK_ACCESS_2_MEMORY_WRITE_BIT;
        b.dstStageMask=VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT; b.dstAccessMask=VK_ACCESS_2_MEMORY_READ_BIT|VK_ACCESS_2_MEMORY_WRITE_BIT;
        b.oldLayout=image.layout; b.newLayout=layout; b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        b.image=image.image; b.subresourceRange=range(image.format==VK_FORMAT_D24_UNORM_S8_UINT ?
            VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT : image.aspect,image.levels);
        VkDependencyInfo info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO}; info.imageMemoryBarrierCount=1; info.pImageMemoryBarriers=&b;
        vkCmdPipelineBarrier2(command,&info); image.layout=layout;
    }
    void memory_barrier() {
        start(); end_render();
        VkMemoryBarrier2 b{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
        b.srcStageMask=b.dstStageMask=VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        b.srcAccessMask=VK_ACCESS_2_MEMORY_WRITE_BIT; b.dstAccessMask=VK_ACCESS_2_MEMORY_READ_BIT|VK_ACCESS_2_MEMORY_WRITE_BIT;
        VkDependencyInfo info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO}; info.memoryBarrierCount=1; info.pMemoryBarriers=&b;
        vkCmdPipelineBarrier2(command,&info);
    }
    void begin_render(Image* color,Image* z,bool clear=false,const float* rgba=nullptr,float clear_z=1) {
        start(); end_render(); memory_barrier();
        if(color) barrier(*color,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        if(z) barrier(*z,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        VkRenderingAttachmentInfo c{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO},d{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        if(color) { c.imageView=color->view; c.imageLayout=color->layout; c.loadOp=clear?VK_ATTACHMENT_LOAD_OP_CLEAR:VK_ATTACHMENT_LOAD_OP_LOAD;
            c.storeOp=VK_ATTACHMENT_STORE_OP_STORE; if(rgba) std::memcpy(c.clearValue.color.float32,rgba,16); }
        if(z) { d.imageView=z->view; d.imageLayout=z->layout; d.loadOp=clear?VK_ATTACHMENT_LOAD_OP_CLEAR:VK_ATTACHMENT_LOAD_OP_LOAD;
            d.storeOp=VK_ATTACHMENT_STORE_OP_STORE; d.clearValue.depthStencil.depth=clear_z; }
        VkRenderingInfo info{VK_STRUCTURE_TYPE_RENDERING_INFO}; info.renderArea.extent=color?color->extent:z->extent; info.layerCount=1;
        info.colorAttachmentCount=color?1:0; info.pColorAttachments=color?&c:nullptr; info.pDepthAttachment=z?&d:nullptr;
        vkCmdBeginRendering(command,&info); rendering=true;
    }
    std::pair<VkBuffer,VkDeviceSize> upload(std::span<const std::byte> data,VkDeviceSize alignment=16);
    VkDescriptorSet descriptor(VkDescriptorSetLayout);
    void uniform(VkDescriptorSet,unsigned,std::span<const std::byte>);
    void image_descriptor(VkDescriptorSet,unsigned,const Image&,VkDescriptorType=VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
    void buffer_descriptor(VkDescriptorSet,unsigned,const Buffer&);
    void sampler_descriptor(VkDescriptorSet,const NativePortSamplerState&);
    VkPipeline pipeline(const NativePortDrawPacket&,NativePortPrimitiveTopology,int kind,VkFormat format);
    void warm_pipelines();
    void save_pipeline_cache() noexcept;
    void viewport(NativePortPixelRect rect) {
        VkViewport viewport{float(rect.x),float(rect.y),float(rect.width),float(rect.height),0,1};
        VkRect2D scissor{{int(rect.x),int(rect.y)},{rect.width,rect.height}};
        vkCmdSetViewport(command,0,1,&viewport); vkCmdSetScissor(command,0,1,&scissor);
    }
    void copy_image(Image& source,Image& destination) {
        barrier(source,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL); barrier(destination,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkImageCopy copy{}; copy.srcSubresource={source.aspect,0,0,1}; copy.dstSubresource={destination.aspect,0,0,1};
        copy.extent={source.extent.width,source.extent.height,1};
        vkCmdCopyImage(command,source.image,source.layout,destination.image,destination.layout,1,&copy);
    }
    void upload_image(Image&,const NativePortImageView&,unsigned);
    void ensure_type_two();
};

void VulkanRenderer::Impl::initialize(void* window) {
    this->window=static_cast<HWND>(window);
    check(volkInitialize(),"vulkan-loader");
    std::vector<const char*> extensions{VK_KHR_SURFACE_EXTENSION_NAME,VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
    unsigned instance_count=0;
    check(vkEnumerateInstanceExtensionProperties(nullptr,&instance_count,nullptr),"vulkan-instance-extensions");
    std::vector<VkExtensionProperties> instance_extensions(instance_count);
    check(vkEnumerateInstanceExtensionProperties(nullptr,&instance_count,instance_extensions.data()),"vulkan-instance-extensions");
    const bool capabilities2=std::any_of(instance_extensions.begin(),instance_extensions.end(),[](const auto& ext){
        return std::strcmp(ext.extensionName,VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME)==0;
    });
    if(capabilities2) extensions.push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
    const auto* request_validation=std::getenv("SARECOMP_VULKAN_VALIDATION");
    const bool validation=request_validation && std::string_view(request_validation)=="1";
    if(validation) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    const char* validation_layer="VK_LAYER_KHRONOS_validation";
    VkDebugUtilsMessengerCreateInfoEXT debug{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    debug.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    debug.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
    debug.pfnUserCallback=[](VkDebugUtilsMessageSeverityFlagBitsEXT severity,VkDebugUtilsMessageTypeFlagsEXT,
                            const VkDebugUtilsMessengerCallbackDataEXT* data,void*)->VkBool32 {
        std::cerr<<"SONIC_VULKAN_VALIDATION "<<data->pMessage<<'\n';
        return (severity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)?VK_TRUE:VK_FALSE;
    };
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO}; app.pApplicationName="Sonic Adventure: Recompiled";
    app.pEngineName="Sonic native renderer"; app.apiVersion=VK_API_VERSION_1_3;
    VkInstanceCreateInfo instance_info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; instance_info.pApplicationInfo=&app;
    instance_info.enabledExtensionCount=unsigned(extensions.size()); instance_info.ppEnabledExtensionNames=extensions.data();
    if(validation) { instance_info.enabledLayerCount=1; instance_info.ppEnabledLayerNames=&validation_layer; instance_info.pNext=&debug; }
    check(vkCreateInstance(&instance_info,nullptr,&instance),"vulkan-instance"); volkLoadInstance(instance);
    if(validation) check(vkCreateDebugUtilsMessengerEXT(instance,&debug,nullptr,&debug_messenger),"vulkan-validation-messenger");
    VkWin32SurfaceCreateInfoKHR surface_info{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    surface_info.hinstance=GetModuleHandleW(nullptr); surface_info.hwnd=static_cast<HWND>(window);
    check(vkCreateWin32SurfaceKHR(instance,&surface_info,nullptr,&surface),"vulkan-surface");
    unsigned count=0; check(vkEnumeratePhysicalDevices(instance,&count,nullptr),"vulkan-gpus");
    std::vector<VkPhysicalDevice> devices(count); check(vkEnumeratePhysicalDevices(instance,&count,devices.data()),"vulkan-gpus");
    int best=-1;
    for(auto candidate:devices) {
        VkPhysicalDeviceProperties p; vkGetPhysicalDeviceProperties(candidate,&p);
        VkPhysicalDeviceVulkan13Features f13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2}; features.pNext=&f13;
        vkGetPhysicalDeviceFeatures2(candidate,&features);
        if(p.apiVersion<VK_API_VERSION_1_3 || !f13.dynamicRendering || !f13.synchronization2 || !f13.shaderDemoteToHelperInvocation ||
           !features.features.fragmentStoresAndAtomics || !features.features.geometryShader ||
           !features.features.fillModeNonSolid || !features.features.depthClamp || !features.features.samplerAnisotropy ||
           p.limits.maxStorageBufferRange < std::uint64_t(config.maximum_type2_fragment_nodes)*24) continue;
        unsigned n=0; vkGetPhysicalDeviceQueueFamilyProperties(candidate,&n,nullptr);
        std::vector<VkQueueFamilyProperties> families(n); vkGetPhysicalDeviceQueueFamilyProperties(candidate,&n,families.data());
        for(unsigned family=0;family<n;++family) {
            VkBool32 supported=false; check(vkGetPhysicalDeviceSurfaceSupportKHR(candidate,family,surface,&supported),"vulkan-present-support");
            if(!supported || !(families[family].queueFlags&VK_QUEUE_GRAPHICS_BIT)) continue;
            int score=p.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU?100:10;
            if(score>best) { best=score; physical=candidate; properties=p; queue_family=family; }
        }
    }
    if(!physical) throw NativePortGraphicsError(NativePortGraphicsFailure::HardwareDeviceUnavailable,0,"vulkan-required-features");
    vkGetPhysicalDeviceMemoryProperties(physical,&memory_properties);
    float priority=1;
    VkDeviceQueueCreateInfo queue_info{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}; queue_info.queueFamilyIndex=queue_family;
    queue_info.queueCount=1; queue_info.pQueuePriorities=&priority;
    VkPhysicalDeviceVulkan13Features f13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES}; f13.dynamicRendering=true; f13.synchronization2=true;
    f13.shaderDemoteToHelperInvocation=true;
    VkPhysicalDeviceFeatures features{}; features.fragmentStoresAndAtomics=true; features.geometryShader=true;
    features.fillModeNonSolid=true; features.depthClamp=true; features.samplerAnisotropy=true; features.robustBufferAccess=true;
    std::vector<const char*> device_extensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    unsigned extension_count=0;
    check(vkEnumerateDeviceExtensionProperties(physical,nullptr,&extension_count,nullptr),"vulkan-device-extensions");
    std::vector<VkExtensionProperties> available(extension_count);
    check(vkEnumerateDeviceExtensionProperties(physical,nullptr,&extension_count,available.data()),"vulkan-device-extensions");
    exclusive_extension=capabilities2 && std::any_of(available.begin(),available.end(),[](const auto& ext){
        return std::strcmp(ext.extensionName,VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME)==0;
    });
    if(exclusive_extension) device_extensions.push_back(VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME);
    VkDeviceCreateInfo info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; info.pNext=&f13; info.pEnabledFeatures=&features;
    info.queueCreateInfoCount=1; info.pQueueCreateInfos=&queue_info;
    info.enabledExtensionCount=unsigned(device_extensions.size()); info.ppEnabledExtensionNames=device_extensions.data();
    check(vkCreateDevice(physical,&info,nullptr,&device),"vulkan-device"); volkLoadDevice(device);
    vkGetDeviceQueue(device,queue_family,0,&queue);
    std::cerr<<"SONIC_VULKAN_DEVICE name="<<properties.deviceName<<" api="<<VK_API_VERSION_MAJOR(properties.apiVersion)
             <<'.'<<VK_API_VERSION_MINOR(properties.apiVersion)<<" type2_nodes="<<config.maximum_type2_fragment_nodes<<'\n';
    for(auto& s:submissions) {
        VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; pool.queueFamilyIndex=queue_family;
        check(vkCreateCommandPool(device,&pool,nullptr,&s.pool),"vulkan-command-pool");
        VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO}; allocate.commandPool=s.pool;
        allocate.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; allocate.commandBufferCount=1;
        check(vkAllocateCommandBuffers(device,&allocate,&s.command),"vulkan-command-buffer");
        VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; check(vkCreateFence(device,&fence,nullptr,&s.fence),"vulkan-fence");
        VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        check(vkCreateSemaphore(device,&semaphore,nullptr,&s.acquire),"vulkan-acquire-semaphore");
    }
    auto layout=[&](std::initializer_list<std::pair<unsigned,VkDescriptorType>> entries,VkDescriptorSetLayout& set,VkPipelineLayout& pipeline) {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        for(auto [binding,type]:entries) bindings.push_back({binding,type,1,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,nullptr});
        VkDescriptorSetLayoutCreateInfo create{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        create.bindingCount=unsigned(bindings.size()); create.pBindings=bindings.data();
        check(vkCreateDescriptorSetLayout(device,&create,nullptr,&set),"vulkan-descriptor-layout");
        VkPipelineLayoutCreateInfo p{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO}; p.setLayoutCount=1; p.pSetLayouts=&set;
        check(vkCreatePipelineLayout(device,&p,nullptr,&pipeline),"vulkan-pipeline-layout");
    };
    constexpr auto ub=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,si=VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        st=VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,sb=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,sa=VK_DESCRIPTOR_TYPE_SAMPLER;
    layout({{0,ub},{1,ub},{4,si},{12,sa}},draw_layout,draw_pipeline_layout);
    layout({{0,ub},{1,ub},{4,si},{12,sa},{9,si},{17,st},{18,sb},{19,st},{20,sb}},capture_layout,capture_pipeline_layout);
    layout({{2,ub},{5,si},{6,si},{7,sb},{8,si},{9,sb}},resolve_layout,resolve_pipeline_layout);
    layout({{4,si},{12,sa}},composite_layout,composite_pipeline_layout);
    layout({{2,ub}},overlay_layout,overlay_pipeline_layout);
    auto shader=[&](std::span<const std::uint32_t> code,VkShaderModule& output) {
        VkShaderModuleCreateInfo create{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO}; create.codeSize=code.size_bytes(); create.pCode=code.data();
        check(vkCreateShaderModule(device,&create,nullptr,&output),"vulkan-shader");
    };
    shader(shaders::draw_vs,draw_vs); shader(shaders::draw_ps,draw_ps); shader(shaders::capture_ps,capture_ps);
    shader(shaders::composite_vs,composite_vs); shader(shaders::composite_ps,composite_ps); shader(shaders::overlay_ps,overlay_ps);
    if(config.maximum_type2_fragments_per_pixel<=32) shader(shaders::resolve_ps_32,resolve_ps);
    else if(config.maximum_type2_fragments_per_pixel<=64) shader(shaders::resolve_ps_64,resolve_ps);
    else if(config.maximum_type2_fragments_per_pixel<=128) shader(shaders::resolve_ps_128,resolve_ps);
    else shader(shaders::resolve_ps_256,resolve_ps);
    std::string contract="sarecomp-vulkan-pipelines-v1:"+std::to_string(properties.vendorID)+":"+
        std::to_string(properties.deviceID)+":"+std::to_string(properties.driverVersion)+":"+
        std::to_string(config.maximum_type2_fragments_per_pixel)+":";
    contract.append(reinterpret_cast<const char*>(properties.pipelineCacheUUID),VK_UUID_SIZE);
    const auto bind_shader=[&](std::span<const std::uint32_t> code){contract+=sonic::startup::digest(std::as_bytes(code));};
    bind_shader(shaders::draw_vs);bind_shader(shaders::draw_ps);bind_shader(shaders::capture_ps);
    bind_shader(shaders::composite_vs);bind_shader(shaders::composite_ps);bind_shader(shaders::overlay_ps);
    bind_shader(shaders::resolve_ps_32);bind_shader(shaders::resolve_ps_64);bind_shader(shaders::resolve_ps_128);bind_shader(shaders::resolve_ps_256);
    cache_key=sonic::startup::digest(contract);
    auto cached=sonic::startup::cache_load("vulkan-driver",cache_key,64u*1024*1024);
    if(cached.size()>=sizeof(VkPipelineCacheHeaderVersionOne)) {
        VkPipelineCacheHeaderVersionOne header{};std::memcpy(&header,cached.data(),sizeof(header));
        if(header.headerSize<sizeof(header) || header.headerSize>cached.size() || header.headerVersion!=VK_PIPELINE_CACHE_HEADER_VERSION_ONE ||
            header.vendorID!=properties.vendorID || header.deviceID!=properties.deviceID || std::memcmp(header.pipelineCacheUUID,properties.pipelineCacheUUID,VK_UUID_SIZE))cached.clear();
    } else cached.clear();
    VkPipelineCacheCreateInfo cache{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};cache.initialDataSize=cached.size();cache.pInitialData=cached.empty()?nullptr:cached.data();
    auto cache_result=vkCreatePipelineCache(device,&cache,nullptr,&pipeline_cache);
    if(cache_result!=VK_SUCCESS && !cached.empty()) {cached.clear();cache.initialDataSize=0;cache.pInitialData=nullptr;cache_result=vkCreatePipelineCache(device,&cache,nullptr,&pipeline_cache);}
    check(cache_result,"vulkan-pipeline-cache");
    std::fprintf(stderr,"SONIC_SHADER_CACHE backend=vulkan driver_hit=%u bytes=%zu\n",unsigned(!cached.empty()),cached.size());
    const VkExtent2D extent{config.render_extent.width,config.render_extent.height};
    VkImageFormatProperties depth_support{};
    if(vkGetPhysicalDeviceImageFormatProperties(physical,VK_FORMAT_D24_UNORM_S8_UINT,VK_IMAGE_TYPE_2D,VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        0,&depth_support)==VK_SUCCESS) depth_format=VK_FORMAT_D24_UNORM_S8_UINT;
    constexpr auto color_usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    working=make_image(extent,VK_FORMAT_R8G8B8A8_UNORM,color_usage);
    completed=make_image(extent,VK_FORMAT_R8G8B8A8_UNORM,color_usage);
    depth=make_image(extent,depth_format,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,1,VK_IMAGE_ASPECT_DEPTH_BIT);
    white=make_image({1,1},VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    const std::array<std::byte,4> pixels{std::byte{255},std::byte{255},std::byte{255},std::byte{255}};
    NativePortImageView image; image.extent={1,1}; image.format=NativePortTextureFormat::Rgba8Unorm; image.stride_bytes=4; image.pixels=pixels;
    upload_image(white,image,0);
    // The same WSI path is used by visible play and hidden/muted tests.
    swap_dirty=true;
    warm_pipelines();
}

void VulkanRenderer::Impl::destroy_swapchain() {
    if(exclusive_acquired && swapchain) vkReleaseFullScreenExclusiveModeEXT(device,swapchain);
    exclusive_acquired=exclusive_attempted=exclusive_controlled=false;
    for(auto& image:swap_images) destroy(image);
    swap_images.clear();
    for(auto sem:present_semaphores) vkDestroySemaphore(device,sem,nullptr);
    present_semaphores.clear();
    if(swapchain) vkDestroySwapchainKHR(device,swapchain,nullptr);
    swapchain={};
}
void VulkanRenderer::Impl::create_swapchain() {
    if(swapchain) { finish(); destroy_swapchain(); }
    exclusive_requested=wants_exclusive();
    VkSurfaceFullScreenExclusiveInfoEXT exclusive{VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT};
    VkSurfaceFullScreenExclusiveWin32InfoEXT monitor{VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT};
    exclusive.fullScreenExclusive=VK_FULL_SCREEN_EXCLUSIVE_APPLICATION_CONTROLLED_EXT;
    exclusive.pNext=&monitor;monitor.hmonitor=MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST);
    if(exclusive_requested && exclusive_extension) {
        VkPhysicalDeviceSurfaceInfo2KHR surface_info{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR};
        surface_info.surface=surface;surface_info.pNext=&exclusive;
        VkSurfaceCapabilitiesFullScreenExclusiveEXT support{VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_FULL_SCREEN_EXCLUSIVE_EXT};
        VkSurfaceCapabilities2KHR properties{VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR};properties.pNext=&support;
        check(vkGetPhysicalDeviceSurfaceCapabilities2KHR(physical,&surface_info,&properties),"vulkan-exclusive-capabilities");
        exclusive_controlled=support.fullScreenExclusiveSupported;
    }
    if(exclusive_requested && !exclusive_controlled)
        std::cerr<<"SONIC_FULLSCREEN_FALLBACK backend=vulkan mode=borderless reason=exclusive-unavailable\n";
    VkSurfaceCapabilitiesKHR caps{};
    const auto caps_result=vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical,surface,&caps);
    if(caps_result!=VK_SUCCESS) {
        RECT client{},outer{};DWORD pid=0;
        const auto client_ok=GetClientRect(window,&client),outer_ok=GetWindowRect(window,&outer);
        const auto owner=GetWindowThreadProcessId(window,&pid);
        std::cerr<<"SONIC_VULKAN_SURFACE_FAILURE valid="<<IsWindow(window)
            <<" visible="<<IsWindowVisible(window)<<" iconic="<<IsIconic(window)
            <<" client_ok="<<client_ok<<" client="<<client.right<<','<<client.bottom
            <<" outer_ok="<<outer_ok<<" outer="<<outer.left<<','<<outer.top<<','<<outer.right<<','<<outer.bottom
            <<" owner_thread="<<owner<<" current_thread="<<GetCurrentThreadId()<<" pid="<<pid<<'\n';
    }
    check(caps_result,"vulkan-surface-caps");
    swap_extent=caps.currentExtent;
    if(swap_extent.width==UINT32_MAX) swap_extent={config.output_extent.width,config.output_extent.height};
    if(!swap_extent.width || !swap_extent.height) { swap_dirty=true; return; }
    unsigned n=0; check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical,surface,&n,nullptr),"vulkan-surface-formats");
    std::vector<VkSurfaceFormatKHR> formats(n); check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical,surface,&n,formats.data()),"vulkan-surface-formats");
    const auto selected=std::find_if(formats.begin(),formats.end(),[](auto f){return f.format==VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace==VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;});
    if(selected==formats.end()) throw NativePortGraphicsError(NativePortGraphicsFailure::HardwareDeviceUnavailable,0,"vulkan-unorm-swapchain");
    swap_format=selected->format;
    check(vkGetPhysicalDeviceSurfacePresentModesKHR(physical,surface,&n,nullptr),"vulkan-present-modes");
    std::vector<VkPresentModeKHR> modes(n); check(vkGetPhysicalDeviceSurfacePresentModesKHR(physical,surface,&n,modes.data()),"vulkan-present-modes");
    // The host owns the 144-Hz deadline. MAILBOX never queues a FIFO backlog.
    VkPresentModeKHR mode=VK_PRESENT_MODE_FIFO_KHR;
    if(std::find(modes.begin(),modes.end(),VK_PRESENT_MODE_MAILBOX_KHR)!=modes.end()) mode=VK_PRESENT_MODE_MAILBOX_KHR;
    else if(!config.synchronize_present && std::find(modes.begin(),modes.end(),VK_PRESENT_MODE_IMMEDIATE_KHR)!=modes.end()) mode=VK_PRESENT_MODE_IMMEDIATE_KHR;
    VkSwapchainCreateInfoKHR create{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR}; create.surface=surface;
    if(exclusive_controlled) create.pNext=&exclusive;
    create.minImageCount=std::max(3u,caps.minImageCount); if(caps.maxImageCount) create.minImageCount=std::min(create.minImageCount,caps.maxImageCount);
    create.imageFormat=swap_format; create.imageColorSpace=selected->colorSpace; create.imageExtent=swap_extent;
    create.imageArrayLayers=1; create.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; create.preTransform=caps.currentTransform;
    create.compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; create.presentMode=mode; create.clipped=true;
    check(vkCreateSwapchainKHR(device,&create,nullptr,&swapchain),"vulkan-swapchain");
    check(vkGetSwapchainImagesKHR(device,swapchain,&n,nullptr),"vulkan-swap-images");
    std::vector<VkImage> images(n); check(vkGetSwapchainImagesKHR(device,swapchain,&n,images.data()),"vulkan-swap-images");
    swap_images.resize(n); present_semaphores.resize(n);
    for(unsigned i=0;i<n;++i) {
        auto& image=swap_images[i]; image.image=images[i]; image.format=swap_format; image.extent=swap_extent; image.external=true;
        VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO}; view.image=image.image; view.viewType=VK_IMAGE_VIEW_TYPE_2D;
        view.format=swap_format; view.subresourceRange=range(VK_IMAGE_ASPECT_COLOR_BIT);
        check(vkCreateImageView(device,&view,nullptr,&image.view),"vulkan-swap-view");
        VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        check(vkCreateSemaphore(device,&semaphore,nullptr,&present_semaphores[i]),"vulkan-present-semaphore");
    }
    swap_dirty=false;
}

void VulkanRenderer::Impl::start() {
    if(current) return;
    auto& s=submissions[submission_index];
    if(s.pending) { check(vkWaitForFences(device,1,&s.fence,true,UINT64_MAX),"vulkan-frame-fence"); s.pending=false; }
    for(auto& retire:s.retired) retire(); s.retired.clear();
    check(vkResetCommandPool(device,s.pool,0),"vulkan-reset-command-pool");
    for(auto pool:s.descriptors) check(vkResetDescriptorPool(device,pool,0),"vulkan-reset-descriptors");
    s.descriptor_pool=0; s.sets=0; s.upload_block=0;
    for(auto& block:s.uploads) block.used=0;
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check(vkBeginCommandBuffer(s.command,&begin),"vulkan-begin-commands");
    current=&s; command=s.command;
}
void VulkanRenderer::Impl::submit(VkSemaphore wait,VkSemaphore signal) {
    if(!current) return;
    end_render(); check(vkEndCommandBuffer(command),"vulkan-end-commands");
    VkCommandBufferSubmitInfo buffer{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO}; buffer.commandBuffer=command;
    VkSemaphoreSubmitInfo acquire{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO},present{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    acquire.semaphore=wait; acquire.stageMask=VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    present.semaphore=signal; present.stageMask=VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    VkSubmitInfo2 submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2}; submit.commandBufferInfoCount=1; submit.pCommandBufferInfos=&buffer;
    submit.waitSemaphoreInfoCount=wait?1:0; submit.pWaitSemaphoreInfos=wait?&acquire:nullptr;
    submit.signalSemaphoreInfoCount=signal?1:0; submit.pSignalSemaphoreInfos=signal?&present:nullptr;
    check(vkResetFences(device,1,&current->fence),"vulkan-reset-fence");
    check(vkQueueSubmit2(queue,1,&submit,current->fence),"vulkan-submit"); current->pending=true;
    current=nullptr; command={}; submission_index=(submission_index+1)%submissions.size();
}
std::pair<VkBuffer,VkDeviceSize> VulkanRenderer::Impl::upload(std::span<const std::byte> data,VkDeviceSize alignment) {
    start();
    for(;;) {
        if(current->upload_block==current->uploads.size())
            current->uploads.push_back({make_buffer(std::max<VkDeviceSize>(4*1024*1024,data.size()+alignment),
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT|VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_INDEX_BUFFER_BIT,true),0});
        auto& block=current->uploads[current->upload_block];
        auto offset=(block.used+alignment-1)&~(alignment-1);
        if(offset+data.size()<=block.buffer.size) {
            std::memcpy(block.buffer.mapped+offset,data.data(),data.size()); block.used=offset+data.size();
            return {block.buffer.buffer,offset};
        }
        ++current->upload_block;
    }
}
VkDescriptorSet VulkanRenderer::Impl::descriptor(VkDescriptorSetLayout layout) {
    start();
    if(current->sets==4096) { ++current->descriptor_pool; current->sets=0; }
    if(current->descriptor_pool==current->descriptors.size()) {
        const VkDescriptorPoolSize sizes[]={{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,12288},{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,24576},
            {VK_DESCRIPTOR_TYPE_SAMPLER,4096},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,8192},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,8192}};
        VkDescriptorPoolCreateInfo create{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO}; create.maxSets=4096;
        create.poolSizeCount=std::size(sizes); create.pPoolSizes=sizes;
        VkDescriptorPool pool; check(vkCreateDescriptorPool(device,&create,nullptr,&pool),"vulkan-descriptor-pool");
        current->descriptors.push_back(pool);
    }
    VkDescriptorSetAllocateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    info.descriptorPool=current->descriptors[current->descriptor_pool]; info.descriptorSetCount=1; info.pSetLayouts=&layout;
    VkDescriptorSet result; check(vkAllocateDescriptorSets(device,&info,&result),"vulkan-descriptor-set"); ++current->sets;
    return result;
}
void VulkanRenderer::Impl::uniform(VkDescriptorSet set,unsigned binding,std::span<const std::byte> data) {
    auto [buffer,offset]=upload(data,std::max<VkDeviceSize>(16,properties.limits.minUniformBufferOffsetAlignment));
    VkDescriptorBufferInfo info{buffer,offset,data.size()};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}; write.dstSet=set; write.dstBinding=binding;
    write.descriptorCount=1; write.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; write.pBufferInfo=&info;
    vkUpdateDescriptorSets(device,1,&write,0,nullptr);
}
void VulkanRenderer::Impl::image_descriptor(VkDescriptorSet set,unsigned binding,const Image& image,VkDescriptorType type) {
    VkDescriptorImageInfo info{{},image.view,image.layout};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}; write.dstSet=set; write.dstBinding=binding;
    write.descriptorCount=1; write.descriptorType=type; write.pImageInfo=&info;
    vkUpdateDescriptorSets(device,1,&write,0,nullptr);
}
void VulkanRenderer::Impl::buffer_descriptor(VkDescriptorSet set,unsigned binding,const Buffer& buffer) {
    VkDescriptorBufferInfo info{buffer.buffer,0,buffer.size};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}; write.dstSet=set; write.dstBinding=binding;
    write.descriptorCount=1; write.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; write.pBufferInfo=&info;
    vkUpdateDescriptorSets(device,1,&write,0,nullptr);
}
void VulkanRenderer::Impl::sampler_descriptor(VkDescriptorSet set,const NativePortSamplerState& state) {
    auto found=std::find_if(samplers.begin(),samplers.end(),[&](const auto& value){return value.first==state;});
    VkSampler sampler{};
    if(found!=samplers.end()) sampler=found->second;
    else {
        if(samplers.size()>=config.maximum_pipeline_states) throw NativePortGraphicsError(NativePortGraphicsFailure::ResourceLimit,0,"vulkan-samplers");
        VkSamplerCreateInfo create{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        create.magFilter=create.minFilter=state.filter==NativePortTextureFilter::Point?VK_FILTER_NEAREST:VK_FILTER_LINEAR;
        create.mipmapMode=(state.filter==NativePortTextureFilter::Trilinear || state.filter==NativePortTextureFilter::Anisotropic)?VK_SAMPLER_MIPMAP_MODE_LINEAR:VK_SAMPLER_MIPMAP_MODE_NEAREST;
        create.addressModeU=addresses[unsigned(state.address_u)]; create.addressModeV=addresses[unsigned(state.address_v)]; create.addressModeW=addresses[unsigned(state.address_w)];
        create.mipLodBias=state.mip_lod_bias; create.minLod=state.minimum_lod; create.maxLod=state.maximum_lod;
        create.anisotropyEnable=state.filter==NativePortTextureFilter::Anisotropic; create.maxAnisotropy=std::min(float(state.maximum_anisotropy),properties.limits.maxSamplerAnisotropy);
        check(vkCreateSampler(device,&create,nullptr,&sampler),"vulkan-sampler"); samplers.emplace_back(state,sampler);
    }
    VkDescriptorImageInfo info{sampler,{},{}}; VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet=set; write.dstBinding=12; write.descriptorCount=1; write.descriptorType=VK_DESCRIPTOR_TYPE_SAMPLER; write.pImageInfo=&info;
    vkUpdateDescriptorSets(device,1,&write,0,nullptr);
}
VkPipeline VulkanRenderer::Impl::pipeline(const NativePortDrawPacket& packet,NativePortPrimitiveTopology topology,int kind,VkFormat format) {
    // Semantic keys, never struct padding or per-draw material/clip constants.
    std::string key;
    auto add=[&](unsigned value) { for(unsigned shift=0;shift<32;shift+=8) key.push_back(char(value>>shift)); };
    add(kind); add(format); add(unsigned(topology));
    const auto& blend=packet.blend; const auto& z=packet.depth; const auto& raster=packet.rasterizer;
    add(blend.enabled); add(unsigned(blend.source_color)); add(unsigned(blend.destination_color)); add(unsigned(blend.color_operation));
    add(unsigned(blend.source_alpha)); add(unsigned(blend.destination_alpha)); add(unsigned(blend.alpha_operation)); add(blend.color_write_mask);
    add(z.test_enabled); add(z.write_enabled); add(unsigned(z.compare)); add(unsigned(raster.cull)); add(unsigned(raster.fill)); add(raster.front_counter_clockwise); add(raster.depth_clip_enabled);
    if(auto it=pipelines.find(key);it!=pipelines.end()) {if(!warming_pipelines)unused_warm_pipelines.erase(it->first);return it->second;}
    if(pipelines.size()>=config.maximum_pipeline_states) {
        // Historical warmup must never take budget away from this playthrough.
        // These objects have never been bound, so no in-flight work uses them.
        if(!warming_pipelines && !unused_warm_pipelines.empty()) {
            const auto victim=unused_warm_pipelines.begin();const auto old=pipelines.find(*victim);
            if(old!=pipelines.end()){vkDestroyPipeline(device,old->second,nullptr);pipelines.erase(old);}
            unused_warm_pipelines.erase(victim);
        } else throw NativePortGraphicsError(NativePortGraphicsFailure::ResourceLimit,0,"vulkan-pipelines");
    }
    const bool geometry=kind<=1;
    VkPipelineShaderStageCreateInfo stages[2]{};
    for(auto& stage:stages) stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT; stages[0].module=geometry?draw_vs:composite_vs;
    stages[0].pName=geometry?"draw_vertex_main":"composite_vertex_main";
    stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module=kind==0?draw_ps:kind==1?capture_ps:kind==2?resolve_ps:kind==3?composite_ps:overlay_ps;
    stages[1].pName=kind==0?"draw_pixel_main":kind==1?"draw_type_two_capture_main":kind==2?"type_two_resolve_pixel_main":kind==3?"composite_pixel_main":"performance_overlay_pixel_main";
    VkVertexInputBindingDescription binding{0,sizeof(NativePortVertex),VK_VERTEX_INPUT_RATE_VERTEX};
    const VkVertexInputAttributeDescription attributes[]={
        {0,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(NativePortVertex,position)},
        {1,0,VK_FORMAT_R32_SFLOAT,offsetof(NativePortVertex,position_w)},
        {2,0,VK_FORMAT_R32G32_SFLOAT,offsetof(NativePortVertex,texture_coordinate)},
        {3,0,VK_FORMAT_R32G32B32A32_SFLOAT,offsetof(NativePortVertex,color)},
        {4,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(NativePortVertex,normal)},
        {5,0,VK_FORMAT_R32G32B32A32_SFLOAT,offsetof(NativePortVertex,secondary_color)},
        {6,0,VK_FORMAT_R32_SFLOAT,offsetof(NativePortVertex,fog_coordinate)},
        {7,0,VK_FORMAT_R32_SFLOAT,offsetof(NativePortVertex,depth_coordinate)}};
    VkPipelineVertexInputStateCreateInfo vertex{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    if(geometry) { vertex.vertexBindingDescriptionCount=1; vertex.pVertexBindingDescriptions=&binding; vertex.vertexAttributeDescriptionCount=std::size(attributes); vertex.pVertexAttributeDescriptions=attributes; }
    VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO}; assembly.topology=topologies[unsigned(topology)];
    VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO}; viewport.viewportCount=1; viewport.scissorCount=1;
    VkPipelineRasterizationStateCreateInfo rasterizer{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizer.polygonMode=raster.fill==NativePortFillMode::Wireframe?VK_POLYGON_MODE_LINE:VK_POLYGON_MODE_FILL;
    rasterizer.cullMode=raster.cull==NativePortCullMode::None?VK_CULL_MODE_NONE:raster.cull==NativePortCullMode::Back?VK_CULL_MODE_BACK_BIT:VK_CULL_MODE_FRONT_BIT;
    rasterizer.frontFace=raster.front_counter_clockwise?VK_FRONT_FACE_COUNTER_CLOCKWISE:VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthClampEnable=!raster.depth_clip_enabled; rasterizer.lineWidth=1;
    VkPipelineMultisampleStateCreateInfo samples{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO}; samples.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo depth_state{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depth_state.depthTestEnable=kind==0 && z.test_enabled; depth_state.depthWriteEnable=kind==0 && z.write_enabled;
    depth_state.depthCompareOp=z.test_enabled?VkCompareOp(z.compare):VK_COMPARE_OP_ALWAYS;
    VkPipelineColorBlendAttachmentState attachment{}; attachment.blendEnable=blend.enabled; attachment.colorWriteMask=blend.color_write_mask;
    attachment.srcColorBlendFactor=factors[unsigned(blend.source_color)]; attachment.dstColorBlendFactor=factors[unsigned(blend.destination_color)]; attachment.colorBlendOp=operations[unsigned(blend.color_operation)];
    // D3D maps color factors in its separate alpha channel to the matching alpha factors.
    auto alpha_factor=[](NativePortBlendFactor f) { switch(f) {
        case NativePortBlendFactor::SourceColor:return VK_BLEND_FACTOR_SRC_ALPHA;
        case NativePortBlendFactor::InverseSourceColor:return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case NativePortBlendFactor::DestinationColor:return VK_BLEND_FACTOR_DST_ALPHA;
        case NativePortBlendFactor::InverseDestinationColor:return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        default:return factors[unsigned(f)]; }};
    attachment.srcAlphaBlendFactor=alpha_factor(blend.source_alpha); attachment.dstAlphaBlendFactor=alpha_factor(blend.destination_alpha); attachment.alphaBlendOp=operations[unsigned(blend.alpha_operation)];
    VkPipelineColorBlendStateCreateInfo blending{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO}; blending.attachmentCount=kind==1?0:1; blending.pAttachments=&attachment;
    const VkDynamicState dynamic_states[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO}; dynamic.dynamicStateCount=2; dynamic.pDynamicStates=dynamic_states;
    VkPipelineRenderingCreateInfo render{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO}; render.colorAttachmentCount=kind==1?0:1;
    render.pColorAttachmentFormats=&format; render.depthAttachmentFormat=geometry?depth_format:VK_FORMAT_UNDEFINED;
    VkGraphicsPipelineCreateInfo create{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO}; create.pNext=&render;
    create.stageCount=2; create.pStages=stages; create.pVertexInputState=&vertex; create.pInputAssemblyState=&assembly;
    create.pViewportState=&viewport; create.pRasterizationState=&rasterizer; create.pMultisampleState=&samples;
    create.pDepthStencilState=&depth_state; create.pColorBlendState=&blending; create.pDynamicState=&dynamic;
    create.layout=kind==0?draw_pipeline_layout:kind==1?capture_pipeline_layout:kind==2?resolve_pipeline_layout:kind==3?composite_pipeline_layout:overlay_pipeline_layout;
    const auto started=std::chrono::steady_clock::now();
    VkPipeline result{};
    const auto status=vkCreateGraphicsPipelines(device,pipeline_cache,1,&create,nullptr,&result);
    if(status!=VK_SUCCESS) {if(result)vkDestroyPipeline(device,result,nullptr);check(status,"vulkan-pipeline");}
    try {
        const auto inserted=pipelines.emplace(key,result);
        if(warming_pipelines)unused_warm_pipelines.insert(inserted.first->first);
    }catch(...) {pipelines.erase(key);vkDestroyPipeline(device,result,nullptr);throw;}
    std::fprintf(stderr,"SONIC_VULKAN_PIPELINE kind=%d warmup=%u elapsed_ms=%.3f\n",kind,unsigned(warming_pipelines),std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count());
    return result;
}

void VulkanRenderer::Impl::warm_pipelines() {
    constexpr std::size_t stride=18*sizeof(std::uint32_t);
    const auto recipes=sonic::startup::cache_load("vulkan-recipes",cache_key,std::min<std::size_t>(config.maximum_pipeline_states,4096)*stride);
    if(recipes.size()%stride)return;
    warming_pipelines=true;std::size_t warmed=0;
    sonic::startup::phase("Preparing graphics...",0,recipes.size()/stride);
    const auto discard_warmup=[&] {
        for(const auto& key:unused_warm_pipelines) {
            const auto entry=pipelines.find(key);
            if(entry!=pipelines.end()){vkDestroyPipeline(device,entry->second,nullptr);pipelines.erase(entry);}
        }
        unused_warm_pipelines.clear();warmed=0;
    };
    try {for(std::size_t offset=0;offset<recipes.size();offset+=stride) {
        std::array<std::uint32_t,18> k{};std::memcpy(k.data(),recipes.data()+offset,stride);
        if(k[0]>4 || (k[1]!=VK_FORMAT_R8G8B8A8_UNORM && k[1]!=VK_FORMAT_B8G8R8A8_UNORM) || k[2]>=std::size(topologies) ||
            k[3]>1 || k[4]>=std::size(factors) || k[5]>=std::size(factors) || k[6]>=std::size(operations) ||
            k[7]>=std::size(factors) || k[8]>=std::size(factors) || k[9]>=std::size(operations) || k[10]>15 ||
            k[11]>1 || k[12]>1 || k[13]>7 || k[14]>2 || k[15]>1 || k[16]>1 || k[17]>1)continue;
        NativePortDrawPacket packet;
        packet.blend.enabled=k[3];packet.blend.source_color=NativePortBlendFactor(k[4]);packet.blend.destination_color=NativePortBlendFactor(k[5]);
        packet.blend.color_operation=NativePortBlendOperation(k[6]);packet.blend.source_alpha=NativePortBlendFactor(k[7]);
        packet.blend.destination_alpha=NativePortBlendFactor(k[8]);packet.blend.alpha_operation=NativePortBlendOperation(k[9]);packet.blend.color_write_mask=std::uint8_t(k[10]);
        packet.depth.test_enabled=k[11];packet.depth.write_enabled=k[12];packet.depth.compare=NativePortCompareOperation(k[13]);
        packet.rasterizer.cull=NativePortCullMode(k[14]);packet.rasterizer.fill=NativePortFillMode(k[15]);
        packet.rasterizer.front_counter_clockwise=k[16];packet.rasterizer.depth_clip_enabled=k[17];
        pipeline(packet,NativePortPrimitiveTopology(k[2]),int(k[0]),VkFormat(k[1]));++warmed;sonic::startup::advance();
    }}catch(const NativePortGraphicsError& error) {
        warming_pipelines=false;discard_warmup();
        if(error.failure()==NativePortGraphicsFailure::DeviceLost)throw;
        std::fprintf(stderr,"SONIC_VULKAN_WARMUP fallback=lazy platform_error=%u\n",error.platform_error_code());
    }catch(const std::bad_alloc&) {
        discard_warmup();std::fprintf(stderr,"SONIC_VULKAN_WARMUP fallback=lazy host_memory=1\n");
    }
    warming_pipelines=false;
    std::fprintf(stderr,"SONIC_VULKAN_WARMUP pipelines=%zu\n",warmed);
}
void VulkanRenderer::Impl::save_pipeline_cache() noexcept {
    if(!device || !pipeline_cache || cache_key.empty() || pipelines.empty())return;
    try {
        std::size_t count=0;
        if(vkGetPipelineCacheData(device,pipeline_cache,&count,nullptr)==VK_SUCCESS && count && count<=64u*1024*1024) {
            std::vector<std::byte> bytes(count);
            if(vkGetPipelineCacheData(device,pipeline_cache,&count,bytes.data())==VK_SUCCESS)sonic::startup::cache_save("vulkan-driver",cache_key,std::span(bytes.data(),count));
        }
        std::vector<std::string> keys;for(const auto& [key,_]:pipelines)keys.push_back(key);std::sort(keys.begin(),keys.end());
        std::string recipes;for(const auto& key:keys)recipes+=key;
        sonic::startup::cache_save("vulkan-recipes",cache_key,std::as_bytes(std::span(recipes.data(),recipes.size())));
    }catch(...){}
}

void VulkanRenderer::Impl::upload_image(Image& image,const NativePortImageView& source,unsigned level) {
    const auto row=std::size_t(source.extent.width)*4;
    std::vector<std::byte> packed(row*source.extent.height);
    for(unsigned y=0;y<source.extent.height;++y)
        std::memcpy(packed.data()+y*row,source.pixels.data()+std::size_t(source.bottom_up?source.extent.height-1-y:y)*source.stride_bytes,row);
    auto [buffer,offset]=upload(packed,4);
    barrier(image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkBufferImageCopy copy{}; copy.bufferOffset=offset; copy.imageSubresource={image.aspect,level,0,1};
    copy.imageExtent={source.extent.width,source.extent.height,1};
    vkCmdCopyBufferToImage(command,buffer,image.image,image.layout,1,&copy);
    barrier(image,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}
void VulkanRenderer::Impl::ensure_type_two() {
    if(type_ready) return;
    const auto extent=working.extent;
    type_base=make_image(extent,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    type_depth=make_image(extent,depth_format,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,1,VK_IMAGE_ASPECT_DEPTH_BIT);
    constexpr auto usage=VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    type_heads=make_image(extent,VK_FORMAT_R32_UINT,usage); type_counts=make_image(extent,VK_FORMAT_R32_UINT,usage);
    type_fragments=make_buffer(std::uint64_t(config.maximum_type2_fragment_nodes)*24,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    type_status=make_buffer(16,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    type_ready=true;
}

VulkanRenderer::VulkanRenderer(void* window,const NativePortGraphicsConfig& config):impl_(std::make_unique<Impl>(window,config)) {}
VulkanRenderer::~VulkanRenderer()=default;
std::uint64_t VulkanRenderer::create_texture(const NativePortTextureConfig& config) {
    auto& p=*impl_;
    auto image=p.make_image({config.extent.width,config.extent.height},config.format==NativePortTextureFormat::Rgba8Unorm?VK_FORMAT_R8G8B8A8_UNORM:VK_FORMAT_B8G8R8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,config.mip_levels);
    const auto id=p.next_id++;
    try { p.textures.emplace(id,image); } catch(...) { p.destroy(image); throw; }
    return id;
}
void VulkanRenderer::upload_texture(std::uint64_t id,const NativePortImageView& image,unsigned level) {
    impl_->upload_image(impl_->textures.at(id),image,level);
}
void VulkanRenderer::destroy_texture(std::uint64_t id) {
    auto& p=*impl_; auto it=p.textures.find(id); if(it==p.textures.end()) return;
    p.start(); auto resource=it->second;
    // This fence is submitted after every earlier use on the same queue.
    p.current->retired.emplace_back([&p,resource]() mutable {p.destroy(resource);}); p.textures.erase(it);
}
std::uint64_t VulkanRenderer::create_mesh(std::span<const NativePortVertex> vertices,std::span<const std::uint32_t> indices) {
    auto& p=*impl_; Impl::Mesh mesh;
    auto copy=[&](std::span<const std::byte> data,VkBufferUsageFlags usage) {
        auto result=p.make_buffer(data.size(),usage|VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        try { auto [buffer,offset]=p.upload(data,4); p.end_render(); VkBufferCopy copy{offset,0,data.size()};
            vkCmdCopyBuffer(p.command,buffer,result.buffer,1,&copy); }
        catch(...) {p.destroy(result); throw;} return result;
    };
    try { mesh.vertices=copy(std::as_bytes(vertices),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        if(!indices.empty()) mesh.indices=copy(std::as_bytes(indices),VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
        mesh.vertex_count=unsigned(vertices.size()); mesh.index_count=unsigned(indices.size());
        auto id=p.next_id++; p.meshes.emplace(id,mesh); return id;
    } catch(...) {
        // Copies may already be recorded. Retire their queue use before freeing.
        try { p.finish(); } catch(...) { vkDeviceWaitIdle(p.device); }
        p.destroy(mesh.vertices); p.destroy(mesh.indices); throw;
    }
}
void VulkanRenderer::destroy_mesh(std::uint64_t id) {
    auto& p=*impl_; auto it=p.meshes.find(id); if(it==p.meshes.end()) return;
    p.start(); auto resource=it->second;
    p.current->retired.emplace_back([&p,resource]() mutable {p.destroy(resource.vertices); p.destroy(resource.indices);}); p.meshes.erase(it);
}
void VulkanRenderer::begin_frame(const NativePortFrameConfig& config) {
    impl_->begin_render(&impl_->working,&impl_->depth,true,config.clear_color.data(),config.clear_depth);
}
void VulkanRenderer::draw(const NativePortDrawPacket& packet,std::span<const NativePortVertex> vertices,
    std::span<const std::uint32_t> indices,NativePortPrimitiveTopology topology,std::uint64_t mesh,std::uint64_t texture,
    NativePortPixelRect rect,std::span<const std::byte> constants,bool type_two) {
    auto& p=*impl_; p.start();
    auto set=p.descriptor(type_two?p.capture_layout:p.draw_layout);
    p.uniform(set,0,constants); p.uniform(set,1,bytes(packet.fog.lookup_table));
    p.image_descriptor(set,4,texture?p.textures.at(texture):p.white); p.sampler_descriptor(set,packet.sampler);
    if(type_two) {
        p.image_descriptor(set,9,p.type_depth);
        p.image_descriptor(set,17,p.type_heads,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        p.image_descriptor(set,19,p.type_counts,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        p.buffer_descriptor(set,18,p.type_fragments); p.buffer_descriptor(set,20,p.type_status);
    }
    VkBuffer vb{},ib{}; VkDeviceSize vo=0,io=0; unsigned vertex_count,index_count;
    if(mesh) { const auto& source=p.meshes.at(mesh); vb=source.vertices.buffer; ib=source.indices.buffer;
        vertex_count=source.vertex_count; index_count=source.index_count; }
    else { std::tie(vb,vo)=p.upload(std::as_bytes(vertices)); vertex_count=unsigned(vertices.size()); index_count=unsigned(indices.size());
        if(index_count) std::tie(ib,io)=p.upload(std::as_bytes(indices),4); }
    if(!p.rendering) p.begin_render(type_two?nullptr:&p.working,&p.depth);
    p.viewport(rect);
    vkCmdBindPipeline(p.command,VK_PIPELINE_BIND_POINT_GRAPHICS,p.pipeline(packet,topology,type_two?1:0,p.working.format));
    vkCmdBindDescriptorSets(p.command,VK_PIPELINE_BIND_POINT_GRAPHICS,type_two?p.capture_pipeline_layout:p.draw_pipeline_layout,0,1,&set,0,nullptr);
    vkCmdBindVertexBuffers(p.command,0,1,&vb,&vo);
    if(index_count) { vkCmdBindIndexBuffer(p.command,ib,io,VK_INDEX_TYPE_UINT32); vkCmdDrawIndexed(p.command,index_count,1,0,0,0); }
    else vkCmdDraw(p.command,vertex_count,1,0,0);
}
void VulkanRenderer::begin_type_two() {
    auto& p=*impl_; p.ensure_type_two();
    p.copy_image(p.working,p.type_base); p.copy_image(p.depth,p.type_depth);
    p.barrier(p.type_base,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL); p.barrier(p.type_depth,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkClearColorValue empty{}; for(auto& x:empty.uint32) x=0xffffffffu;
    VkClearColorValue zero{}; const auto r=range(VK_IMAGE_ASPECT_COLOR_BIT);
    p.barrier(p.type_heads,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL); vkCmdClearColorImage(p.command,p.type_heads.image,p.type_heads.layout,&empty,1,&r);
    p.barrier(p.type_counts,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL); vkCmdClearColorImage(p.command,p.type_counts.image,p.type_counts.layout,&zero,1,&r);
    p.memory_barrier(); vkCmdFillBuffer(p.command,p.type_status.buffer,0,VK_WHOLE_SIZE,0);
    p.barrier(p.type_heads,VK_IMAGE_LAYOUT_GENERAL); p.barrier(p.type_counts,VK_IMAGE_LAYOUT_GENERAL);
    p.begin_render(nullptr,&p.depth);
}
void VulkanRenderer::resolve_type_two() {
    auto& p=*impl_; p.memory_barrier();
    p.barrier(p.type_heads,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL); p.barrier(p.type_counts,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    auto set=p.descriptor(p.resolve_layout);
    const std::array<unsigned,4> constants{p.config.maximum_type2_fragments_per_pixel,0,0,p.config.maximum_type2_fragment_nodes};
    p.uniform(set,2,bytes(constants)); p.image_descriptor(set,5,p.type_base); p.image_descriptor(set,6,p.type_heads);
    p.buffer_descriptor(set,7,p.type_fragments); p.image_descriptor(set,8,p.type_counts); p.buffer_descriptor(set,9,p.type_status);
    p.begin_render(&p.working,nullptr); p.viewport({0,0,p.working.extent.width,p.working.extent.height});
    NativePortDrawPacket packet; packet.depth.test_enabled=packet.depth.write_enabled=false; packet.rasterizer.cull=NativePortCullMode::None;
    vkCmdBindPipeline(p.command,VK_PIPELINE_BIND_POINT_GRAPHICS,p.pipeline(packet,NativePortPrimitiveTopology::TriangleList,2,p.working.format));
    vkCmdBindDescriptorSets(p.command,VK_PIPELINE_BIND_POINT_GRAPHICS,p.resolve_pipeline_layout,0,1,&set,0,nullptr); vkCmdDraw(p.command,3,1,0,0);
    p.end_render();
}
void VulkanRenderer::complete_frame() {
    auto& p=*impl_; p.end_render(); p.barrier(p.working,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL); p.submit();
    std::swap(p.working,p.completed); p.completed_ready=true;
}
void VulkanRenderer::abort_frame() {
    auto& p=*impl_; p.end_render(); p.submit(); p.completed_ready=false;
}
void VulkanRenderer::resize(NativePortExtent extent) { impl_->config.output_extent=extent; impl_->swap_dirty=true; }
bool VulkanRenderer::present(NativePortPixelRect rect,bool nonblocking,std::span<const std::byte> overlay) {
    auto& p=*impl_; if(!p.completed_ready) return false;
    if(p.wants_exclusive()!=p.exclusive_requested) p.swap_dirty=true;
    if(p.swap_dirty) p.create_swapchain();
    if(!p.swapchain || p.swap_dirty) return false;
    if(p.exclusive_controlled) {
        if(GetForegroundWindow()!=p.window) {
            if(p.exclusive_acquired) vkReleaseFullScreenExclusiveModeEXT(p.device,p.swapchain);
            p.exclusive_acquired=p.exclusive_attempted=false;
        } else if(!p.exclusive_acquired && !p.exclusive_attempted) {
            const auto result=vkAcquireFullScreenExclusiveModeEXT(p.device,p.swapchain);
            p.exclusive_acquired=result==VK_SUCCESS;p.exclusive_attempted=true;
            if(result!=VK_SUCCESS && result!=VK_ERROR_INITIALIZATION_FAILED) check(result,"vulkan-exclusive-acquire");
            std::cerr<<"SONIC_VULKAN_FULLSCREEN exclusive="<<p.exclusive_acquired<<'\n';
        }
    }
    // Finish any partial simulation submission without resolving its live OIT list.
    p.submit(); p.start();
    unsigned index=0;
    auto acquired=vkAcquireNextImageKHR(p.device,p.swapchain,nonblocking?0:UINT64_MAX,p.current->acquire,{},&index);
    if(acquired==VK_NOT_READY || acquired==VK_TIMEOUT) return false;
    if(acquired==VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT) {p.exclusive_acquired=p.exclusive_attempted=false;return false;}
    if(acquired==VK_ERROR_OUT_OF_DATE_KHR) {p.swap_dirty=true; return false;}
    if(acquired==VK_SUBOPTIMAL_KHR) p.swap_dirty=true; else check(acquired,"vulkan-acquire");
    auto set=p.descriptor(p.composite_layout); p.image_descriptor(set,4,p.completed); p.sampler_descriptor(set,NativePortSamplerState{});
    const float black[]={0,0,0,1}; auto& image=p.swap_images[index];
    p.begin_render(&image,nullptr,true,black); p.viewport(rect);
    NativePortDrawPacket packet; packet.depth.test_enabled=packet.depth.write_enabled=false; packet.rasterizer.cull=NativePortCullMode::None;
    vkCmdBindPipeline(p.command,VK_PIPELINE_BIND_POINT_GRAPHICS,p.pipeline(packet,NativePortPrimitiveTopology::TriangleList,3,p.swap_format));
    vkCmdBindDescriptorSets(p.command,VK_PIPELINE_BIND_POINT_GRAPHICS,p.composite_pipeline_layout,0,1,&set,0,nullptr); vkCmdDraw(p.command,3,1,0,0);
    if(!overlay.empty() && p.swap_extent.width>12 && p.swap_extent.height>12) {
        auto os=p.descriptor(p.overlay_layout); p.uniform(os,2,overlay);
        p.viewport({12,12,std::min(128u,p.swap_extent.width-12),std::min(32u,p.swap_extent.height-12)});
        packet.blend.enabled=true; packet.blend.source_color=NativePortBlendFactor::SourceAlpha;
        packet.blend.destination_color=packet.blend.destination_alpha=NativePortBlendFactor::InverseSourceAlpha;
        vkCmdBindPipeline(p.command,VK_PIPELINE_BIND_POINT_GRAPHICS,p.pipeline(packet,NativePortPrimitiveTopology::TriangleList,4,p.swap_format));
        vkCmdBindDescriptorSets(p.command,VK_PIPELINE_BIND_POINT_GRAPHICS,p.overlay_pipeline_layout,0,1,&os,0,nullptr); vkCmdDraw(p.command,3,1,0,0);
    }
    p.barrier(image,VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    const auto sem=p.present_semaphores[index]; p.submit(p.current->acquire,sem);
    VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR}; present.waitSemaphoreCount=1; present.pWaitSemaphores=&sem;
    present.swapchainCount=1; present.pSwapchains=&p.swapchain; present.pImageIndices=&index;
    const auto result=vkQueuePresentKHR(p.queue,&present);
    if(result==VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT) {p.exclusive_acquired=p.exclusive_attempted=false;return false;}
    if(result==VK_ERROR_OUT_OF_DATE_KHR || result==VK_SUBOPTIMAL_KHR) p.swap_dirty=true; else check(result,"vulkan-present");
    return true;
}
std::vector<std::uint8_t> VulkanRenderer::capture_bgra_bottom_up() {
    auto& p=*impl_; const auto width=p.completed.extent.width,height=p.completed.extent.height;
    const auto size=std::size_t(width)*height*4;
    auto readback=p.make_buffer(size,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
    try {
        p.barrier(p.completed,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        VkBufferImageCopy copy{}; copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; copy.imageExtent={width,height,1};
        vkCmdCopyImageToBuffer(p.command,p.completed.image,p.completed.layout,readback.buffer,1,&copy);
        p.barrier(p.completed,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL); p.finish();
        std::vector<std::uint8_t> pixels(size);
        const auto* source=reinterpret_cast<const std::uint8_t*>(readback.mapped);
        for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
            const auto from=(std::size_t(height-1-y)*width+x)*4,to=(std::size_t(y)*width+x)*4;
            pixels[to]=source[from+2]; pixels[to+1]=source[from+1]; pixels[to+2]=source[from]; pixels[to+3]=source[from+3];
        }
        p.destroy(readback); return pixels;
    } catch(...) { vkDeviceWaitIdle(p.device); p.destroy(readback); throw; }
}

void VulkanRenderer::Impl::cleanup() noexcept {
    if(device) {
        vkDeviceWaitIdle(device);
        save_pipeline_cache();
        for(auto& s:submissions) {
            for(auto& retired:s.retired) retired();
            for(auto& b:s.uploads) destroy(b.buffer);
            for(auto pool:s.descriptors) vkDestroyDescriptorPool(device,pool,nullptr);
            if(s.acquire) vkDestroySemaphore(device,s.acquire,nullptr);
            if(s.fence) vkDestroyFence(device,s.fence,nullptr);
            if(s.pool) vkDestroyCommandPool(device,s.pool,nullptr);
        }
        for(auto& [_,image]:textures) destroy(image);
        for(auto& [_,mesh]:meshes) { destroy(mesh.vertices); destroy(mesh.indices); }
        for(auto* image:{&working,&completed,&depth,&white,&type_base,&type_depth,&type_heads,&type_counts}) destroy(*image);
        destroy(type_fragments); destroy(type_status); destroy_swapchain();
        for(auto [_,sampler]:samplers) vkDestroySampler(device,sampler,nullptr);
        for(auto [_,pipeline]:pipelines) vkDestroyPipeline(device,pipeline,nullptr);
        for(auto shader:{draw_vs,draw_ps,capture_ps,composite_vs,composite_ps,resolve_ps,overlay_ps}) if(shader) vkDestroyShaderModule(device,shader,nullptr);
        for(auto layout:{draw_pipeline_layout,capture_pipeline_layout,resolve_pipeline_layout,composite_pipeline_layout,overlay_pipeline_layout}) if(layout) vkDestroyPipelineLayout(device,layout,nullptr);
        for(auto layout:{draw_layout,capture_layout,resolve_layout,composite_layout,overlay_layout}) if(layout) vkDestroyDescriptorSetLayout(device,layout,nullptr);
        if(pipeline_cache) vkDestroyPipelineCache(device,pipeline_cache,nullptr);
        vkDestroyDevice(device,nullptr);
    }
    if(surface) vkDestroySurfaceKHR(instance,surface,nullptr);
    if(debug_messenger) vkDestroyDebugUtilsMessengerEXT(instance,debug_messenger,nullptr);
    if(instance) vkDestroyInstance(instance,nullptr);
}
}
