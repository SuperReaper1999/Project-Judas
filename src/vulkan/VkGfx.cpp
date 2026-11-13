// Explicit Vulkan renderer interface: see VulkanGfx.h and docs/VULKAN_NATIVE.md.
//
// This file records Vulkan commands directly from explicit descriptions. It
// shares the device, command buffer, transient ring and resource tables with
// the OpenGL-named entry points so that migrated and unmigrated passes can
// coexist in one frame; it never reads OpenGL-style binding or raster state.
#include "VkCore.h"
#include "VulkanGfx.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <vector>

namespace JudasVulkan::Gfx {
using namespace JudasVulkan::detail;

// ------------------------------------------------------------------ objects ---
struct Shader {
    GLuint program = 0;
    std::uint64_t generation = 0;
    struct TextureSlotInfo { std::uint32_t uniform = 0, element = 0; };
    std::vector<TextureSlotInfo> textures; // flattened sampler elements, in samplerUniforms order
    enum class BufferKind : std::uint8_t { Uniform, Storage, Texel };
    struct BufferSlotInfo {
        std::string name;
        std::uint32_t binding = 0, blockSize = 0;
        BufferKind kind = BufferKind::Uniform;
    };
    std::vector<BufferSlotInfo> buffers;
    // Dynamic descriptor order (ascending binding): what supplies each offset.
    struct Dynamic { std::uint32_t binding = 0; int slot = -1; bool defaults = false; };
    std::vector<Dynamic> dynamic;
    std::vector<std::uint32_t> zeroBindings; // declared but inactive blocks
    std::vector<bool> zeroStorage;
};
struct VertexLayout {
    VertexLayoutDesc desc;
    std::uint32_t id = 0;
};
struct Pipeline {
    PipelineDesc desc;
    struct Variant {
        std::uint32_t colorCount = 0;
        VkFormat color[8] = {};
        VkFormat depth = VK_FORMAT_UNDEFINED;
        VkPipeline pipeline = VK_NULL_HANDLE;
    };
    std::vector<Variant> variants;
    std::uint32_t streamMask = 0; // streams with at least one attribute the shader reads
};

namespace {
struct State {
    std::vector<std::unique_ptr<Shader>> shaders;
    std::vector<std::unique_ptr<VertexLayout>> layouts;
    std::vector<std::unique_ptr<Pipeline>> pipelines;
    Statistics stats;
    // Explicit pass.
    bool passActive = false;
    PassDesc pass;
    // What this interface last bound in the current command buffer. Reset when
    // the command buffer changes or an OpenGL-named draw bound its own state.
    std::uint64_t serial = ~0ull, foreignDraws = ~0ull;
    std::uint64_t groupKey = 0;
    VkDescriptorSet set = VK_NULL_HANDLE;
    VkPipelineLayout setLayout = VK_NULL_HANDLE;
    std::vector<std::uint32_t> offsets, boundOffsets;
    VkBuffer vertexBuffers[4] = {};
    VkDeviceSize vertexOffsets[4] = {};
    std::uint32_t vertexMask = 0;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceSize indexOffset = 0;
    Range zeroBlock;
    std::uint64_t zeroSerial = ~0ull;
};
State &S() {
    static State state;
    return state;
}

Program *ProgramOf(const Shader *shader) {
    if (!g || !shader) return nullptr;
    Program *program = Lookup(g->programs, shader->program);
    return program && program->linked && program->generation == shader->generation ? program : nullptr;
}

constexpr std::uint32_t kExplicitPassMarker = 0xFFFFFFFFu;

bool StartExplicitPass(const PassDesc &desc, bool clear) {
    detail::EndPass();
    Attachment attachment;
    attachment.kind = Attachment::Tex;
    attachment.name = desc.depthTexture;
    attachment.layer = int(desc.depthLayer);
    ResolvedAttachment resolved;
    if (!ResolveAttachment(attachment, resolved) || !(resolved.image->usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) return false;
    Transition(*resolved.image, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    resolved.image->lastUse = g->serial;
    VkRenderingAttachmentInfo depth{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    depth.imageView = resolved.view;
    depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depth.clearValue.depthStencil = {desc.clearDepthValue, 0};
    PassState &pass = g->pass;
    pass.fbo = kExplicitPassMarker;
    pass.fboRevision = 0;
    pass.colorCount = 0;
    for (auto &color : pass.color) color.reset();
    pass.depth = resolved.image;
    pass.depthFormat = resolved.image->format;
    pass.width = resolved.width;
    pass.height = resolved.height;
    VkRenderingInfo rendering{VK_STRUCTURE_TYPE_RENDERING_INFO};
    rendering.renderArea = {{0, 0}, {resolved.width, resolved.height}};
    rendering.layerCount = 1;
    rendering.pDepthAttachment = &depth;
    if (AspectOf(pass.depthFormat) & VK_IMAGE_ASPECT_STENCIL_BIT) rendering.pStencilAttachment = &depth;
    vkCmdBeginRendering(g->cmd, &rendering);
    pass.active = true;
    MarkDirty(DirtyPipeline);
    ++g->renderPasses;
    return true;
}

// Makes sure a pass is recording: the explicit one, or the frame's current target.
bool EnsureRecording() {
    State &s = S();
    if (s.passActive) {
        if (g->pass.active && g->pass.fbo == kExplicitPassMarker) return true;
        return StartExplicitPass(s.pass, false);
    }
    EnsurePass();
    return g->pass.active;
}

VkFormat VulkanFormat(Format format) {
    switch (format) {
    case Format::Float2: return VK_FORMAT_R32G32_SFLOAT;
    case Format::Float3: return VK_FORMAT_R32G32B32_SFLOAT;
    case Format::UInt4: return VK_FORMAT_R32G32B32A32_UINT;
    default: return VK_FORMAT_R32G32B32A32_SFLOAT;
    }
}

VkPipeline BuildPipeline(Pipeline &pipeline, std::uint32_t colorCount, const VkFormat *colorFormats, VkFormat depthFormat) {
    for (const auto &variant : pipeline.variants)
        if (variant.colorCount == colorCount && variant.depth == depthFormat &&
            std::memcmp(variant.color, colorFormats, sizeof(VkFormat) * colorCount) == 0)
            return variant.pipeline;
    Program *program = ProgramOf(pipeline.desc.shader);
    if (!program || !pipeline.desc.layout) return VK_NULL_HANDLE;
    const VertexLayoutDesc &layout = pipeline.desc.layout->desc;
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = program->modules[0];
    stages[0].pName = "main";
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = program->modules[1];
    stages[1].pName = "main";
    std::array<VkVertexInputBindingDescription, 4> bindings{};
    std::array<VkVertexInputAttributeDescription, 16> attributes{};
    std::uint32_t bindingCount = 0, attributeCount = 0;
    pipeline.streamMask = 0;
    for (std::uint32_t i = 0; i < layout.attributeCount; ++i) {
        const VertexAttribute &attribute = layout.attributes[i];
        bool consumed = false;
        for (const AttributeInfo &input : program->attributes) consumed |= input.location == attribute.location;
        if (!consumed) continue;
        auto &out = attributes[attributeCount++];
        out.location = attribute.location;
        out.binding = attribute.stream;
        out.format = VulkanFormat(attribute.format);
        out.offset = attribute.offset;
        pipeline.streamMask |= 1u << attribute.stream;
    }
    for (std::uint32_t stream = 0; stream < layout.streamCount; ++stream) {
        if (!(pipeline.streamMask & (1u << stream))) continue;
        auto &out = bindings[bindingCount++];
        out.binding = stream;
        const auto kind = layout.streams[stream].kind;
        out.stride = kind == StreamKind::PerVertex || kind == StreamKind::PerInstance ? layout.streams[stream].stride : 0;
        out.inputRate = kind == StreamKind::PerVertex ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE;
    }
    VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInput.vertexBindingDescriptionCount = bindingCount;
    vertexInput.pVertexBindingDescriptions = bindings.data();
    vertexInput.vertexAttributeDescriptionCount = attributeCount;
    vertexInput.pVertexAttributeDescriptions = attributes.data();
    VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;
    VkPipelineViewportDepthClipControlCreateInfoEXT clipControl{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT};
    clipControl.negativeOneToOne = VK_TRUE;
    if (g->depthClipControl) viewport.pNext = &clipControl;
    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depth.maxDepthBounds = 1.0f;
    std::array<VkPipelineColorBlendAttachmentState, 8> blendAttachments{};
    for (std::uint32_t i = 0; i < colorCount; ++i) {
        auto &a = blendAttachments[i];
        a.colorWriteMask = 0xF;
        if (pipeline.desc.blend == Blend::Alpha) {
            a.blendEnable = VK_TRUE;
            a.srcColorBlendFactor = a.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            a.dstColorBlendFactor = a.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            a.colorBlendOp = a.alphaBlendOp = VK_BLEND_OP_ADD;
        }
    }
    VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blend.attachmentCount = colorCount;
    blend.pAttachments = blendAttachments.data();
    // The same dynamic state set as the OpenGL-named path, so one dynamic-state
    // tracker (Context::dynamic) serves both within a command buffer.
    const VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_CULL_MODE,
                                            VK_DYNAMIC_STATE_FRONT_FACE, VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
                                            VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE, VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,
                                            VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY, VK_DYNAMIC_STATE_DEPTH_BIAS,
                                            VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE};
    VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic.dynamicStateCount = std::uint32_t(std::size(dynamicStates));
    dynamic.pDynamicStates = dynamicStates;
    VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    rendering.colorAttachmentCount = colorCount;
    rendering.pColorAttachmentFormats = colorFormats;
    rendering.depthAttachmentFormat = depthFormat;
    if (AspectOf(depthFormat) & VK_IMAGE_ASPECT_STENCIL_BIT) rendering.stencilAttachmentFormat = depthFormat;
    VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    info.pNext = &rendering;
    info.stageCount = program->modules[1] ? 2u : 1u;
    info.pStages = stages;
    info.pVertexInputState = &vertexInput;
    info.pInputAssemblyState = &assembly;
    info.pViewportState = &viewport;
    info.pRasterizationState = &raster;
    info.pMultisampleState = &multisample;
    info.pDepthStencilState = &depth;
    info.pColorBlendState = &blend;
    info.pDynamicState = &dynamic;
    info.layout = program->layout;
    Pipeline::Variant variant;
    variant.colorCount = colorCount;
    std::memcpy(variant.color, colorFormats, sizeof(VkFormat) * colorCount);
    variant.depth = depthFormat;
    const auto started = std::chrono::steady_clock::now();
    const VkResult created = vkCreateGraphicsPipelines(g->device, g->pipelineCache, 1, &info, nullptr, &variant.pipeline);
    g->pipelineSeconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    ++g->pipelinesCreated;
    ++S().stats.pipelinesCreated;
    if (created != VK_SUCCESS) {
        Warn("explicit pipeline creation failed (%d)", int(created));
        variant.pipeline = VK_NULL_HANDLE;
    }
    pipeline.variants.push_back(variant);
    return variant.pipeline;
}

bool ImageInCurrentPass(const Image *image) {
    if (!g->pass.active) return false;
    for (std::uint32_t i = 0; i < g->pass.colorCount; ++i)
        if (g->pass.color[i].get() == image) return true;
    return g->pass.depth.get() == image;
}

// Resolves one texture binding to a view, sampler and layout. May end the
// current pass to change an image layout (the caller restarts it).
VkDescriptorImageInfo ResolveTexture(const Uniform &uniform, const TextureBinding &binding) {
    const int slot = SlotForTarget(uniform.samplerTarget);
    Texture *texture = slot >= 0 ? GetTexture(binding.texture) : nullptr;
    // A texture's dimensionality is fixed by its first use. Engine textures that
    // were only ever created (texture views, for example) get it from the slot.
    if (texture && !texture->target) texture->target = uniform.samplerTarget;
    VkImageView view = texture ? SamplingView(*texture) : VK_NULL_HANDLE;
    if (view && uniform.samplerTarget == GL_TEXTURE_CUBE_MAP_ARRAY && texture->image->layers - texture->viewBaseLayer < 6) view = VK_NULL_HANDLE;
    if (view && SlotForTarget(texture->target) != slot) view = VK_NULL_HANDLE; // wrong dimensionality for this slot
    if (view && ImageInCurrentPass(texture->image.get())) view = VK_NULL_HANDLE;
    if (!view) {
        texture = GetTexture(g->dummyTextures[std::size_t(std::max(slot, 0))]);
        view = SamplingView(*texture);
    }
    Image &image = *texture->image;
    const VkImageLayout wanted = IsDepthFormat(image.format) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                                             : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (image.layout != wanted) {
        detail::EndPass();
        Transition(image, wanted);
    }
    image.lastUse = g->serial;
    const SamplerObject *object = Lookup(g->samplerObjects, binding.sampler);
    const std::uint32_t levels = texture->maxLevel >= 0 ? std::min<std::uint32_t>(image.levels, std::uint32_t(texture->maxLevel) + 1) : image.levels;
    VkDescriptorImageInfo info{};
    info.sampler = GetSampler(object ? object->state : texture->sampler, levels > 1, uniform.samplerShadow);
    info.imageView = view;
    info.imageLayout = wanted;
    return info;
}

const Range &ZeroBlock() {
    State &s = S();
    if (s.zeroSerial != g->serial || !s.zeroBlock) {
        s.zeroBlock = RingAllocate(256, g->bufferAlignment);
        std::memset(s.zeroBlock.Data(), 0, 256);
        s.zeroSerial = g->serial;
    }
    return s.zeroBlock;
}

struct ResolvedBuffer { VkBuffer buffer = VK_NULL_HANDLE; VkDeviceSize range = 0; std::uint32_t offset = 0; };
ResolvedBuffer ResolveBuffer(const Shader::BufferSlotInfo &slot, const BufferBinding &binding) {
    ResolvedBuffer out;
    if (binding.transient) {
        if (binding.transient.serial != g->serial) RingKeepAlive(VkBuffer(binding.transient.buffer));
        out.buffer = VkBuffer(binding.transient.buffer);
        out.range = binding.transient.size;
        out.offset = binding.transient.offset;
    } else if (Buffer *buffer = GetBuffer(binding.buffer); buffer && buffer->storage && binding.offset < buffer->size) {
        const VkDeviceSize available = binding.size ? std::min<VkDeviceSize>(binding.size, buffer->size - binding.offset) : buffer->size - binding.offset;
        out.buffer = buffer->storage.block->buffer;
        out.range = available;
        out.offset = std::uint32_t(buffer->storage.offset + binding.offset);
        buffer->lastUse = g->serial;
    } else {
        const Range &zero = ZeroBlock();
        out.buffer = zero.block->buffer;
        out.range = std::min<VkDeviceSize>(256, std::max<std::uint32_t>(slot.blockSize, 16));
        out.offset = std::uint32_t(zero.offset);
    }
    if (slot.kind == Shader::BufferKind::Uniform) out.range = std::min<VkDeviceSize>(out.range, std::max<std::uint32_t>(slot.blockSize, 16));
    if (slot.kind == Shader::BufferKind::Texel) out.range = std::max<VkDeviceSize>(out.range / 16 * 16, 16);
    return out;
}

void ApplyDynamic(const DrawDesc &draw) {
    auto &dynamic = g->dynamic;
    VkViewport viewport;
    if (S().passActive) viewport = {0.0f, 0.0f, float(g->pass.width), float(g->pass.height), 0.0f, 1.0f};
    else {
        const GLint *v = g->gl.viewport; // the frame's current target and viewport (see VulkanGfx.h)
        viewport = {float(v[0]), float(v[1]), float(v[2]), float(v[3]), 0.0f, 1.0f};
    }
    const VkRect2D scissor{{0, 0}, {g->pass.width, g->pass.height}};
    const VkCullModeFlags cull = draw.cull ? VkCullModeFlags(VK_CULL_MODE_BACK_BIT) : VkCullModeFlags(VK_CULL_MODE_NONE);
    // Vulkan's signed-area convention is the negative of OpenGL's.
    const VkFrontFace front = draw.frontCounterClockwise ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
    const bool hasDepth = g->pass.depthFormat != VK_FORMAT_UNDEFINED;
    const VkBool32 depthTest = draw.depthTest && hasDepth;
    const VkBool32 depthWrite = depthTest && draw.depthWrite;
    const VkCompareOp depthOp = VK_COMPARE_OP_LESS;
    const VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    const bool all = !dynamic.valid;
    if (all || std::memcmp(&dynamic.viewport, &viewport, sizeof(viewport)) != 0) { vkCmdSetViewport(g->cmd, 0, 1, &viewport); dynamic.viewport = viewport; }
    if (all || std::memcmp(&dynamic.scissor, &scissor, sizeof(scissor)) != 0) { vkCmdSetScissor(g->cmd, 0, 1, &scissor); dynamic.scissor = scissor; }
    if (all || dynamic.cull != cull) { vkCmdSetCullMode(g->cmd, cull); dynamic.cull = cull; }
    if (all || dynamic.front != front) { vkCmdSetFrontFace(g->cmd, front); dynamic.front = front; }
    if (all || dynamic.depthTest != depthTest) { vkCmdSetDepthTestEnable(g->cmd, depthTest); dynamic.depthTest = depthTest; }
    if (all || dynamic.depthWrite != depthWrite) { vkCmdSetDepthWriteEnable(g->cmd, depthWrite); dynamic.depthWrite = depthWrite; }
    if (all || dynamic.depthOp != depthOp) { vkCmdSetDepthCompareOp(g->cmd, depthOp); dynamic.depthOp = depthOp; }
    if (all || dynamic.topology != topology) { vkCmdSetPrimitiveTopology(g->cmd, topology); dynamic.topology = topology; }
    if (all || dynamic.depthBiasConstant != 0.0f || dynamic.depthBiasSlope != 0.0f) {
        vkCmdSetDepthBias(g->cmd, 0.0f, 0.0f, 0.0f);
        dynamic.depthBiasConstant = dynamic.depthBiasSlope = 0.0f;
    }
    if (all || dynamic.rasterizerDiscard != VK_FALSE) { vkCmdSetRasterizerDiscardEnable(g->cmd, VK_FALSE); dynamic.rasterizerDiscard = VK_FALSE; }
    dynamic.valid = true;
}
} // namespace

// ------------------------------------------------------------------ shaders ---
Shader *AdoptShader(Handle name) {
    if (!g) return nullptr;
    Program *program = Lookup(g->programs, name);
    if (!program || !program->linked || program->compute) return nullptr;
    auto shader = std::make_unique<Shader>();
    shader->program = name;
    shader->generation = program->generation;
    for (const auto index : program->samplerUniforms)
        for (std::uint32_t element = 0; element < program->uniforms[index].arraySize; ++element) shader->textures.push_back({index, element});
    for (const auto &block : program->uniformBlocks) shader->buffers.push_back({block.name, block.binding, block.size, Shader::BufferKind::Uniform});
    for (const auto &block : program->storageBlocks) shader->buffers.push_back({block.name, block.binding, block.size, Shader::BufferKind::Storage});
    for (const auto index : program->texelUniforms)
        shader->buffers.push_back({program->uniforms[index].name, program->uniforms[index].binding, 16, Shader::BufferKind::Texel});
    for (const auto &block : program->inactiveBlocks) {
        shader->zeroBindings.push_back(block.binding);
        shader->zeroStorage.push_back(block.storage);
    }
    if (shader->textures.size() > std::size_t(Bindings::kTextures) || shader->buffers.size() > std::size_t(Bindings::kBuffers)) {
        Warn("shader has more resources than the explicit binding group holds");
        return nullptr;
    }
    for (const std::uint32_t binding : program->dynamicBindings) {
        Shader::Dynamic entry;
        entry.binding = binding;
        entry.defaults = program->hasDefaultBlock && binding == program->defaultBinding;
        for (std::size_t i = 0; i < shader->buffers.size(); ++i)
            if (shader->buffers[i].binding == binding) entry.slot = int(i);
        shader->dynamic.push_back(entry);
    }
    S().shaders.push_back(std::move(shader));
    return S().shaders.back().get();
}
void ReleaseShader(Shader *) {} // owned by the device; freed with it

int Param(const Shader *shader, const char *name) {
    Program *program = ProgramOf(shader);
    if (!program || !name) return -1;
    const auto found = program->locations.find(name);
    if (found == program->locations.end()) return -1;
    const auto [index, element] = program->locationTable[std::size_t(found->second)];
    (void)element;
    return program->uniforms[index].base == UniformBase::Sampler ? -1 : found->second;
}
int TextureSlot(const Shader *shader, const char *name) {
    Program *program = ProgramOf(shader);
    if (!program || !name) return -1;
    for (std::size_t i = 0; i < shader->textures.size(); ++i)
        if (shader->textures[i].element == 0 && program->uniforms[shader->textures[i].uniform].name == name) return int(i);
    return -1;
}
int BufferSlot(const Shader *shader, const char *name) {
    if (!shader || !name) return -1;
    for (std::size_t i = 0; i < shader->buffers.size(); ++i)
        if (shader->buffers[i].name == name) return int(i);
    return -1;
}
void SetInt(Shader *shader, int param, int value) {
    if (param >= 0) WriteUniform(ProgramOf(shader), param, 1, 1, 1, &value);
}
void SetFloats(Shader *shader, int param, const float *values, unsigned components, unsigned count) {
    if (param >= 0) WriteUniform(ProgramOf(shader), param, 0, int(components), GLsizei(count), values);
}
void SetMat3(Shader *shader, int param, const float *values) {
    if (param >= 0) WriteUniformMatrix(ProgramOf(shader), param, 3, 1, values);
}
void SetMat4(Shader *shader, int param, const float *values) {
    if (param >= 0) WriteUniformMatrix(ProgramOf(shader), param, 4, 1, values);
}

// ---------------------------------------------------------------- transient ---
Transient AllocateTransient(std::uint32_t bytes) {
    Transient out;
    if (!g || !bytes) return out;
    const Range range = RingAllocate(bytes, std::max<VkDeviceSize>(g->bufferAlignment, 16));
    out.data = range.Data();
    out.buffer = std::uint64_t(range.block->buffer);
    out.offset = std::uint32_t(range.offset);
    out.size = bytes;
    out.serial = g->serial;
    return out;
}

// ------------------------------------------------------- layouts, pipelines ---
VertexLayout *CreateVertexLayout(const VertexLayoutDesc &desc) {
    if (!g) return nullptr;
    auto layout = std::make_unique<VertexLayout>();
    layout->desc = desc;
    layout->id = std::uint32_t(S().layouts.size() + 1);
    S().layouts.push_back(std::move(layout));
    return S().layouts.back().get();
}
Pipeline *CreatePipeline(const PipelineDesc &desc) {
    if (!g || !desc.shader || !desc.layout) return nullptr;
    auto pipeline = std::make_unique<Pipeline>();
    pipeline->desc = desc;
    S().pipelines.push_back(std::move(pipeline));
    return S().pipelines.back().get();
}
void Prewarm(Pipeline *pipeline, TargetFormat color, bool depth) {
    if (!g || !pipeline) return;
    const VkFormat format = color == TargetFormat::Rgba8 ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R16G16B16A16_SFLOAT;
    BuildPipeline(*pipeline, color == TargetFormat::None ? 0u : 1u, &format, depth ? g->depth24Format : VK_FORMAT_UNDEFINED);
}

// ------------------------------------------------------------------- passes ---
bool BeginPass(const PassDesc &desc) {
    if (!g) return false;
    State &s = S();
    if (!StartExplicitPass(desc, desc.clearDepth)) return false;
    s.passActive = true;
    s.pass = desc;
    ++s.stats.passes;
    return true;
}
void EndPass() {
    if (!g) return;
    S().passActive = false;
    detail::EndPass();
}

// -------------------------------------------------------------------- draws ---
void Draw(const DrawDesc &draw) {
    if (!g || !draw.pipeline || !draw.bindings || !draw.count) return;
    State &s = S();
    Shader *shader = draw.pipeline->desc.shader;
    Program *program = ProgramOf(shader);
    if (!program) return;
    if (!EnsureRecording()) return;
    if (s.serial != g->serial || s.foreignDraws != g->draws) {
        // New command buffer, or an OpenGL-named draw bound its own state since.
        s.serial = g->serial;
        s.foreignDraws = g->draws;
        s.groupKey = 0;
        s.set = VK_NULL_HANDLE;
        s.vertexMask = 0;
        s.indexBuffer = VK_NULL_HANDLE;
        s.boundOffsets.clear();
    }
    RefreshProgramDefaults(*program);
    const VkBuffer defaultsBuffer = program->hasDefaultBlock ? program->defaultsRange.block->buffer : VK_NULL_HANDLE;

    // Binding group: identified by what is bound, not by where in a buffer.
    const Bindings &bindings = *draw.bindings;
    std::uint64_t key = 0xcbf29ce484222325ull ^ 0x6a75646173676678ull;
    const auto mix = [&](std::uint64_t value) { key = (key ^ value) * 1099511628211ull; };
    mix(program->generation);
    mix(g->resourceEpoch);
    mix(std::uint64_t(defaultsBuffer));
    for (std::size_t i = 0; i < shader->textures.size(); ++i) mix((std::uint64_t(bindings.textures[i].texture) << 32) | bindings.textures[i].sampler);
    for (std::size_t i = 0; i < shader->buffers.size(); ++i) {
        const BufferBinding &binding = bindings.buffers[i];
        mix(binding.transient ? binding.transient.buffer : std::uint64_t(binding.buffer) | (1ull << 40));
        mix(binding.transient ? binding.transient.size : (std::uint64_t(binding.size) << 32) | binding.offset);
    }
    bool rebind = false;
    if (key != s.groupKey || !s.set) {
        const auto cached = g->setCache.find(key);
        if (cached != g->setCache.end()) {
            s.set = cached->second;
            ++s.stats.bindingGroupsReused;
        } else {
            std::vector<VkDescriptorImageInfo> images(shader->textures.size());
            for (std::size_t i = 0; i < shader->textures.size(); ++i)
                images[i] = ResolveTexture(program->uniforms[shader->textures[i].uniform], bindings.textures[i]);
            if (!EnsureRecording()) return; // a layout change ended the pass
            std::vector<VkDescriptorBufferInfo> buffers;
            buffers.reserve(shader->buffers.size() + shader->zeroBindings.size() + 1);
            std::vector<VkWriteDescriptorSet> writes;
            const VkDescriptorSet set = AllocateSet(program->setLayout);
            const auto bufferWrite = [&](std::uint32_t binding, VkDescriptorType type, VkBuffer buffer, VkDeviceSize range) {
                buffers.push_back({buffer, 0, range});
                VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                write.dstSet = set;
                write.dstBinding = binding;
                write.descriptorCount = 1;
                write.descriptorType = type;
                write.pBufferInfo = reinterpret_cast<const VkDescriptorBufferInfo *>(buffers.size()); // index + 1, fixed below
                writes.push_back(write);
            };
            if (program->hasDefaultBlock)
                bufferWrite(program->defaultBinding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, defaultsBuffer,
                            std::max<VkDeviceSize>(program->defaults.size(), 16));
            for (std::size_t i = 0; i < shader->buffers.size(); ++i) {
                const ResolvedBuffer resolved = ResolveBuffer(shader->buffers[i], bindings.buffers[i]);
                bufferWrite(shader->buffers[i].binding,
                            shader->buffers[i].kind == Shader::BufferKind::Uniform ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
                                                                                   : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                            resolved.buffer, resolved.range);
            }
            for (std::size_t i = 0; i < shader->zeroBindings.size(); ++i)
                bufferWrite(shader->zeroBindings[i],
                            shader->zeroStorage[i] ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                            ZeroBlock().block->buffer, 16);
            for (auto &write : writes) write.pBufferInfo = &buffers[reinterpret_cast<std::uintptr_t>(write.pBufferInfo) - 1];
            for (std::size_t i = 0; i < shader->textures.size(); ++i) {
                if (shader->textures[i].element != 0) continue;
                const Uniform &uniform = program->uniforms[shader->textures[i].uniform];
                VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                write.dstSet = set;
                write.dstBinding = uniform.binding;
                write.descriptorCount = uniform.arraySize;
                write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                write.pImageInfo = &images[i];
                writes.push_back(write);
            }
            vkUpdateDescriptorSets(g->device, std::uint32_t(writes.size()), writes.data(), 0, nullptr);
            g->setCache.emplace(key, set);
            s.set = set;
            ++s.stats.bindingGroupsBuilt;
        }
        s.groupKey = key;
        rebind = true;
    }

    // Dynamic offsets, in ascending binding order.
    s.offsets.clear();
    for (const Shader::Dynamic &entry : shader->dynamic) {
        if (entry.defaults) s.offsets.push_back(std::uint32_t(program->defaultsRange.offset));
        else if (entry.slot >= 0) s.offsets.push_back(ResolveBuffer(shader->buffers[std::size_t(entry.slot)], bindings.buffers[entry.slot]).offset);
        else s.offsets.push_back(std::uint32_t(ZeroBlock().offset));
    }

    // Pipeline for this pass's attachment formats.
    const VkPipeline pipeline = BuildPipeline(*draw.pipeline, g->pass.colorCount, g->pass.colorFormats.data(), g->pass.depthFormat);
    if (!pipeline) return;
    if (g->dynamic.pipeline != pipeline) {
        vkCmdBindPipeline(g->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        g->dynamic.pipeline = pipeline;
    }
    if (rebind || s.setLayout != program->layout || s.offsets != s.boundOffsets) {
        vkCmdBindDescriptorSets(g->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, program->layout, 0, 1, &s.set,
                                std::uint32_t(s.offsets.size()), s.offsets.data());
        s.boundOffsets = s.offsets;
        s.setLayout = program->layout;
    }

    // Vertex streams.
    const VertexLayoutDesc &layout = draw.pipeline->desc.layout->desc;
    for (std::uint32_t stream = 0; stream < layout.streamCount; ++stream) {
        if (!(draw.pipeline->streamMask & (1u << stream))) continue;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceSize offset = 0;
        const StreamKind kind = layout.streams[stream].kind;
        if (kind == StreamKind::ConstantFloat || kind == StreamKind::ConstantUInt) {
            buffer = g->dummyVertex.block->buffer;
            offset = g->dummyVertex.offset + (kind == StreamKind::ConstantUInt ? 16 : 0);
        } else if (draw.streams[stream].transient) {
            if (draw.streams[stream].transient.serial != g->serial) RingKeepAlive(VkBuffer(draw.streams[stream].transient.buffer));
            buffer = VkBuffer(draw.streams[stream].transient.buffer);
            offset = draw.streams[stream].transient.offset + draw.streams[stream].offset;
        } else if (Buffer *source = GetBuffer(draw.streams[stream].buffer); source && source->storage) {
            buffer = source->storage.block->buffer;
            offset = source->storage.offset + draw.streams[stream].offset;
            source->lastUse = g->serial;
        } else
            return; // a required stream is missing
        if (!(s.vertexMask & (1u << stream)) || s.vertexBuffers[stream] != buffer || s.vertexOffsets[stream] != offset) {
            vkCmdBindVertexBuffers(g->cmd, stream, 1, &buffer, &offset);
            s.vertexBuffers[stream] = buffer;
            s.vertexOffsets[stream] = offset;
            s.vertexMask |= 1u << stream;
        }
    }
    ApplyDynamic(draw);
    if (draw.indexBuffer) {
        Buffer *indices = GetBuffer(draw.indexBuffer);
        if (!indices || !indices->storage) return;
        indices->lastUse = g->serial;
        if (s.indexBuffer != indices->storage.block->buffer || s.indexOffset != indices->storage.offset) {
            vkCmdBindIndexBuffer(g->cmd, indices->storage.block->buffer, indices->storage.offset, VK_INDEX_TYPE_UINT32);
            s.indexBuffer = indices->storage.block->buffer;
            s.indexOffset = indices->storage.offset;
        }
        vkCmdDrawIndexed(g->cmd, draw.count, draw.instances, draw.first, 0, 0);
    } else
        vkCmdDraw(g->cmd, draw.count, draw.instances, draw.first, 0);
    ++s.stats.draws;
    // The OpenGL-named path must rebuild whatever it had bound.
    g->dirty = DirtyAll;
}

Statistics Stats() { return S().stats; }

} // namespace JudasVulkan::Gfx

namespace JudasVulkan::detail {
// Called while the device is idle, before it is destroyed.
void ShutdownGfx() {
    auto &s = Gfx::S();
    for (auto &pipeline : s.pipelines)
        for (auto &variant : pipeline->variants)
            if (variant.pipeline) vkDestroyPipeline(g->device, variant.pipeline, nullptr);
    if (g->trace)
        std::fprintf(stderr, "[Vulkan trace] explicit interface: %llu draws, %llu binding groups built, %llu reused, %llu pipelines, %llu passes\n",
                     static_cast<unsigned long long>(s.stats.draws), static_cast<unsigned long long>(s.stats.bindingGroupsBuilt),
                     static_cast<unsigned long long>(s.stats.bindingGroupsReused), static_cast<unsigned long long>(s.stats.pipelinesCreated),
                     static_cast<unsigned long long>(s.stats.passes));
    s = Gfx::State{};
}
} // namespace JudasVulkan::detail
