// Vulkan backend core: instance/device/swapchain ownership, memory, command
// submission and presentation. See VulkanBackend.h for the backend contract.
#include "VkCore.h"
#include <thread>
#include <chrono>
#include <system_error>
#include <iterator>
#include <fstream>
#include <filesystem>
#include "VulkanBackend.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace JudasVulkan {
namespace detail {

Context *g = nullptr;

void Fatal(const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    std::fprintf(stderr, "[Vulkan] fatal: ");
    std::vfprintf(stderr, format, arguments);
    std::fprintf(stderr, "\n");
    va_end(arguments);
    std::abort();
}
void Warn(const char *format, ...) {
    static unsigned count = 0;
    if (++count > 200) return;
    va_list arguments;
    va_start(arguments, format);
    std::fprintf(stderr, "[Vulkan] ");
    std::vfprintf(stderr, format, arguments);
    std::fprintf(stderr, "\n");
    va_end(arguments);
}

#define VK_CHECK(call)                                                                             \
    do {                                                                                           \
        VkResult judasResult = (call);                                                             \
        if (judasResult != VK_SUCCESS) Fatal("%s failed (%d)", #call, int(judasResult));           \
    } while (0)

bool IsDepthFormat(VkFormat format) {
    return format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D32_SFLOAT ||
           format == VK_FORMAT_X8_D24_UNORM_PACK32 || format == VK_FORMAT_D24_UNORM_S8_UINT ||
           format == VK_FORMAT_D32_SFLOAT_S8_UINT;
}
VkImageAspectFlags AspectOf(VkFormat format) {
    if (format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D32_SFLOAT_S8_UINT)
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    return IsDepthFormat(format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
}

// ---------------------------------------------------------------- memory ---
namespace {
constexpr VkDeviceSize kBufferBlockBytes = 32ull << 20;
constexpr VkDeviceSize kImageBlockBytes = 64ull << 20;

std::uint32_t FindMemoryType(std::uint32_t typeBits, VkMemoryPropertyFlags required,
                             VkMemoryPropertyFlags preferred) {
    const auto &memory = g->memoryProperties;
    for (int pass = 0; pass < 2; ++pass) {
        const auto wanted = pass == 0 ? (required | preferred) : required;
        for (std::uint32_t i = 0; i < memory.memoryTypeCount; ++i)
            if ((typeBits & (1u << i)) && (memory.memoryTypes[i].propertyFlags & wanted) == wanted)
                return i;
    }
    Fatal("no compatible device memory type");
    return 0;
}

bool BlockAllocate(MemBlock &block, VkDeviceSize size, VkDeviceSize alignment, VkDeviceSize &offset) {
    for (auto it = block.free.begin(); it != block.free.end(); ++it) {
        const VkDeviceSize start = (it->first + alignment - 1) / alignment * alignment;
        const VkDeviceSize end = it->first + it->second;
        if (start + size > end) continue;
        const VkDeviceSize before = start - it->first, after = end - (start + size);
        const VkDeviceSize origin = it->first;
        block.free.erase(it);
        if (before) block.free[origin] = before;
        if (after) block.free[start + size] = after;
        offset = start;
        return true;
    }
    return false;
}
void BlockFree(MemBlock &block, VkDeviceSize offset, VkDeviceSize size) {
    auto next = block.free.lower_bound(offset);
    if (next != block.free.begin()) {
        auto previous = std::prev(next);
        if (previous->first + previous->second == offset) {
            offset = previous->first;
            size += previous->second;
            block.free.erase(previous);
        }
    }
    if (next != block.free.end() && offset + size == next->first) {
        size += next->second;
        block.free.erase(next);
    }
    block.free[offset] = size;
}

std::unique_ptr<MemBlock> CreateBufferBlock(VkDeviceSize size, bool dedicated, bool deviceLocal) {
    auto block = std::make_unique<MemBlock>();
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = size;
    info.usage = (g->transformFeedback ? VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT : 0) |
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                 VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(g->device, &info, nullptr, &block->buffer));
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(g->device, block->buffer, &requirements);
    VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocate.allocationSize = requirements.size;
    const VkMemoryPropertyFlags mappable = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    // Host blocks: CPU-written/read data, cached for readback. All storage is
    // persistently mapped because the backend writes buffer contents directly.
    const std::uint32_t hostType = FindMemoryType(requirements.memoryTypeBits, mappable, VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
    VkResult result = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    if (deviceLocal && !g->deviceBufferHeapExhausted) {
        const std::uint32_t deviceType =
            FindMemoryType(requirements.memoryTypeBits, mappable, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (g->memoryProperties.memoryTypes[deviceType].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
            allocate.memoryTypeIndex = deviceType;
            result = vkAllocateMemory(g->device, &allocate, nullptr, &block->memory);
            if (result != VK_SUCCESS) {
                g->deviceBufferHeapExhausted = true;
                Warn("device-local host-visible memory exhausted after %llu MiB; GPU-owned buffers now use host memory",
                     static_cast<unsigned long long>(g->deviceBufferBytes >> 20));
            } else
                g->deviceBufferBytes += requirements.size;
        }
    }
    if (result != VK_SUCCESS) {
        allocate.memoryTypeIndex = hostType;
        VK_CHECK(vkAllocateMemory(g->device, &allocate, nullptr, &block->memory));
    }
    block->memoryType = allocate.memoryTypeIndex;
    block->devicePlacement = deviceLocal;
    VK_CHECK(vkBindBufferMemory(g->device, block->buffer, block->memory, 0));
    void *mapped = nullptr;
    VK_CHECK(vkMapMemory(g->device, block->memory, 0, VK_WHOLE_SIZE, 0, &mapped));
    block->mapped = static_cast<std::uint8_t *>(mapped);
    block->size = size;
    block->dedicated = dedicated;
    block->free[0] = size;
    g->bufferBytes += size;
    return block;
}
void DestroyBlock(MemBlock &block) {
    if (block.buffer) {
        vkDestroyBuffer(g->device, block.buffer, nullptr);
        g->bufferBytes -= block.size;
        if (g->memoryProperties.memoryTypes[block.memoryType].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT &&
            block.devicePlacement)
            g->deviceBufferBytes -= std::min(g->deviceBufferBytes, block.size);
    } else
        g->imageBytes -= block.size;
    if (block.memory) vkFreeMemory(g->device, block.memory, nullptr);
    block = MemBlock{};
}
} // namespace

Range AllocateBuffer(VkDeviceSize size, bool deviceLocal) {
    size = std::max<VkDeviceSize>(size, 1);
    const VkDeviceSize rounded = (size + g->bufferAlignment - 1) / g->bufferAlignment * g->bufferAlignment;
    Range range;
    range.size = rounded;
    if (rounded <= kBufferBlockBytes / 4) {
        for (auto &block : g->bufferBlocks)
            if (!block->dedicated && block->devicePlacement == deviceLocal &&
                BlockAllocate(*block, rounded, g->bufferAlignment, range.offset)) {
                range.block = block.get();
                return range;
            }
        g->bufferBlocks.push_back(CreateBufferBlock(kBufferBlockBytes, false, deviceLocal));
    } else
        g->bufferBlocks.push_back(CreateBufferBlock(rounded, true, deviceLocal));
    range.block = g->bufferBlocks.back().get();
    if (!BlockAllocate(*range.block, rounded, g->bufferAlignment, range.offset))
        Fatal("buffer block allocation failed");
    return range;
}

namespace {
void FreeRange(Range range) {
    if (!range.block) return;
    MemBlock &block = *range.block;
    const bool isBuffer = block.buffer != VK_NULL_HANDLE;
    BlockFree(block, range.offset, range.size);
    if (block.dedicated || (block.free.size() == 1 && block.free.begin()->second == block.size)) {
        auto &list = isBuffer ? g->bufferBlocks : g->imageBlocks;
        // Keep one pooled block of each kind to avoid allocation churn.
        std::size_t pooled = 0;
        for (auto &candidate : list)
            if (!candidate->dedicated) ++pooled;
        if (block.dedicated || pooled > 1) {
            for (auto it = list.begin(); it != list.end(); ++it)
                if (it->get() == &block) {
                    DestroyBlock(block);
                    list.erase(it);
                    break;
                }
        }
    }
}
Range AllocateImageMemory(const VkMemoryRequirements &requirements) {
    const std::uint32_t type =
        FindMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);
    Range range;
    range.size = requirements.size;
    const VkDeviceSize alignment = std::max<VkDeviceSize>(requirements.alignment, 1);
    if (requirements.size <= kImageBlockBytes / 4) {
        for (auto &block : g->imageBlocks)
            if (!block->dedicated && block->memoryType == type &&
                BlockAllocate(*block, requirements.size, alignment, range.offset)) {
                range.block = block.get();
                return range;
            }
    }
    const bool dedicated = requirements.size > kImageBlockBytes / 4;
    auto block = std::make_unique<MemBlock>();
    VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocate.allocationSize = dedicated ? requirements.size : kImageBlockBytes;
    allocate.memoryTypeIndex = type;
    VK_CHECK(vkAllocateMemory(g->device, &allocate, nullptr, &block->memory));
    block->size = allocate.allocationSize;
    block->memoryType = type;
    block->dedicated = dedicated;
    block->free[0] = block->size;
    g->imageBytes += block->size;
    g->imageBlocks.push_back(std::move(block));
    range.block = g->imageBlocks.back().get();
    if (!BlockAllocate(*range.block, requirements.size, alignment, range.offset))
        Fatal("image block allocation failed");
    return range;
}
} // namespace

void RetireRange(Range range, std::uint64_t serial) {
    if (!range.block) return;
    Retired entry;
    entry.serial = serial;
    entry.kind = Retired::RangeKind;
    entry.range = range;
    g->retired.push_back(std::move(entry));
}
void Retire(Retired::Kind kind, std::uint64_t handle, std::uint64_t serial) {
    if (!handle) return;
    Retired entry;
    entry.serial = serial;
    entry.kind = kind;
    entry.handle = handle;
    g->retired.push_back(std::move(entry));
}
void RetireImage(std::shared_ptr<Image> image) {
    if (!image) return;
    Retired entry;
    entry.serial = std::max(image->lastUse, g->serial);
    entry.kind = Retired::ImageKind;
    entry.image = std::move(image);
    g->retired.push_back(std::move(entry));
}

Image::~Image() {
    if (!g || external) return;
    if (image) vkDestroyImage(g->device, image, nullptr);
    FreeRange(memory);
}

std::shared_ptr<Image> CreateImage(VkImageType type, VkFormat format, std::uint32_t width,
                                   std::uint32_t height, std::uint32_t depth, std::uint32_t layers,
                                   std::uint32_t levels, bool attachment) {
    auto image = std::make_shared<Image>();
    image->format = format;
    image->aspect = AspectOf(format);
    image->type = type;
    image->width = std::max(1u, width);
    image->height = std::max(1u, height);
    image->depth = std::max(1u, depth);
    image->layers = std::max(1u, layers);
    image->levels = std::max(1u, levels);
    VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    info.imageType = type;
    info.format = format;
    info.extent = {image->width, image->height, image->depth};
    info.mipLevels = image->levels;
    info.arrayLayers = image->layers;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    VkFormatProperties properties;
    vkGetPhysicalDeviceFormatProperties(g->physical, format, &properties);
    if (attachment) {
        if (IsDepthFormat(format)) {
            if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
                info.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        } else if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)
            info.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }
    if (type == VK_IMAGE_TYPE_2D && image->width == image->height && image->layers >= 6)
        info.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image->usage = info.usage;
    VK_CHECK(vkCreateImage(g->device, &info, nullptr, &image->image));
    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(g->device, image->image, &requirements);
    image->memory = AllocateImageMemory(requirements);
    image->bytes = requirements.size;
    VK_CHECK(vkBindImageMemory(g->device, image->image, image->memory.block->memory,
                               image->memory.offset));
    return image;
}

VkImageView CreateView(const Image &image, VkImageViewType type, std::uint32_t baseLevel,
                       std::uint32_t levels, std::uint32_t baseLayer, std::uint32_t layers) {
    VkImageViewCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    info.image = image.image;
    info.viewType = type;
    info.format = image.format;
    // Depth+stencil images are sampled/attached through their depth aspect only
    // when viewed for sampling; attachments use both aspects.
    info.subresourceRange.aspectMask = image.aspect;
    info.subresourceRange.baseMipLevel = baseLevel;
    info.subresourceRange.levelCount = levels;
    info.subresourceRange.baseArrayLayer = baseLayer;
    info.subresourceRange.layerCount = layers;
    VkImageView view = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImageView(g->device, &info, nullptr, &view));
    return view;
}

namespace {
VkAccessFlags AccessForLayout(VkImageLayout layout) {
    switch (layout) {
    case VK_IMAGE_LAYOUT_GENERAL: return VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: return VK_ACCESS_TRANSFER_READ_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return VK_ACCESS_TRANSFER_WRITE_BIT;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return VK_ACCESS_SHADER_READ_BIT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
        return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    default: return 0;
    }
}
} // namespace

void Transition(Image &image, VkImageLayout layout) {
    if (image.layout == layout) return;
    if (g->pass.active) Fatal("image layout transition inside a render pass");
    MarkDirty(DirtyTextures);
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = image.layout;
    barrier.newLayout = layout;
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image.image;
    barrier.subresourceRange = {image.aspect, 0, VK_REMAINING_MIP_LEVELS, 0,
                                VK_REMAINING_ARRAY_LAYERS};
    barrier.srcAccessMask = AccessForLayout(image.layout);
    barrier.dstAccessMask = AccessForLayout(layout);
    vkCmdPipelineBarrier(g->cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    image.layout = layout;
}

void MemoryBarrierAll() {
    if (g->pass.active) EndPass();
    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    vkCmdPipelineBarrier(g->cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
}

// ------------------------------------------------------------ submission ---
namespace {
void BeginCommandBuffer() {
    if (!g->freeCommandBuffers.empty()) {
        g->cmd = g->freeCommandBuffers.back();
        g->freeCommandBuffers.pop_back();
        VK_CHECK(vkResetCommandBuffer(g->cmd, 0));
    } else {
        VkCommandBufferAllocateInfo info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        info.commandPool = g->commandPool;
        info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        info.commandBufferCount = 1;
        VK_CHECK(vkAllocateCommandBuffers(g->device, &info, &g->cmd));
    }
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(g->cmd, &begin));
    g->dynamic = Context::Dynamic{};
    g->dirty = DirtyAll; // nothing is bound in a new command buffer
}

void ReleaseRetired() {
    auto &list = g->retired;
    std::size_t kept = 0;
    for (std::size_t i = 0; i < list.size(); ++i) {
        Retired &entry = list[i];
        if (entry.serial > g->completedSerial) {
            if (kept != i) list[kept] = std::move(entry);
            ++kept;
            continue;
        }
        switch (entry.kind) {
        case Retired::RangeKind: FreeRange(entry.range); break;
        case Retired::ViewKind:
            vkDestroyImageView(g->device, VkImageView(entry.handle), nullptr);
            break;
        case Retired::BufferViewKind:
            vkDestroyBufferView(g->device, VkBufferView(entry.handle), nullptr);
            break;
        case Retired::SamplerKind:
            vkDestroySampler(g->device, VkSampler(entry.handle), nullptr);
            break;
        case Retired::PipelineKind:
            vkDestroyPipeline(g->device, VkPipeline(entry.handle), nullptr);
            break;
        case Retired::ModuleKind:
            vkDestroyShaderModule(g->device, VkShaderModule(entry.handle), nullptr);
            break;
        case Retired::SetLayoutKind:
            vkDestroyDescriptorSetLayout(g->device, VkDescriptorSetLayout(entry.handle), nullptr);
            break;
        case Retired::LayoutKind:
            vkDestroyPipelineLayout(g->device, VkPipelineLayout(entry.handle), nullptr);
            break;
        case Retired::ImageKind: entry.image.reset(); break;
        }
    }
    list.resize(kept);
}
} // namespace

void PollCompleted(bool wait, std::uint64_t untilSerial) {
    while (!g->inFlight.empty()) {
        Submission &oldest = g->inFlight.front();
        const bool mustWait = wait && (untilSerial == 0 || oldest.serial <= untilSerial);
        VkResult status = mustWait
            ? vkWaitForFences(g->device, 1, &oldest.fence, VK_TRUE, UINT64_MAX)
            : vkGetFenceStatus(g->device, oldest.fence);
        if (status != VK_SUCCESS) break;
        g->completedSerial = std::max(g->completedSerial, oldest.serial);
        VK_CHECK(vkResetFences(g->device, 1, &oldest.fence));
        g->freeFences.push_back(oldest.fence);
        g->freeCommandBuffers.push_back(oldest.cmd);
        if (oldest.acquireSemaphore) g->freeAcquireSemaphores.push_back(oldest.acquireSemaphore);
        for (VkDescriptorPool pool : oldest.pools) {
            VK_CHECK(vkResetDescriptorPool(g->device, pool, 0));
            g->freePools.push_back(pool);
        }
        g->inFlight.erase(g->inFlight.begin());
    }
    ReleaseRetired();
}

void EndPass() {
    if (!g->pass.active) return;
    vkCmdEndRendering(g->cmd);
    g->pass.active = false;
}

namespace {
void Submit(VkSemaphore waitSemaphore, VkSemaphore signalSemaphore) {
    EndPass();
    VK_CHECK(vkEndCommandBuffer(g->cmd));
    Submission submission;
    submission.cmd = g->cmd;
    submission.serial = g->serial;
    submission.pools = std::move(g->currentPools);
    g->currentPools.clear();
    g->setCache.clear();
    g->poolSetsLeft = 0;
    if (!g->freeFences.empty()) {
        submission.fence = g->freeFences.back();
        g->freeFences.pop_back();
    } else {
        VkFenceCreateInfo info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        VK_CHECK(vkCreateFence(g->device, &info, nullptr, &submission.fence));
    }
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &g->cmd;
    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    submission.acquireSemaphore = waitSemaphore;
    if (waitSemaphore) {
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &waitSemaphore;
        submit.pWaitDstStageMask = &waitStage;
    }
    if (signalSemaphore) {
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &signalSemaphore;
    }
    VK_CHECK(vkQueueSubmit(g->queue, 1, &submit, submission.fence));
    g->inFlight.push_back(std::move(submission));
    ++g->submissions;
    ++g->serial;
    g->ringCurrent = -1;
    BeginCommandBuffer();
}
} // namespace

void Flush(bool wait) {
    const std::uint64_t submitted = g->serial;
    Submit(VK_NULL_HANDLE, VK_NULL_HANDLE);
    PollCompleted(wait, wait ? submitted : 0);
}

Range RingAllocate(VkDeviceSize size, VkDeviceSize alignment) {
    constexpr VkDeviceSize kChunk = 4ull << 20;
    alignment = std::max<VkDeviceSize>(alignment, 16);
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (g->ringCurrent >= 0) {
            auto &chunk = g->ring[std::size_t(g->ringCurrent)];
            const VkDeviceSize start = (chunk.cursor + alignment - 1) / alignment * alignment;
            if (start + size <= chunk.block->size) {
                chunk.cursor = start + size;
                chunk.serial = g->serial;
                Range range;
                range.block = chunk.block.get();
                range.offset = start;
                range.size = size;
                return range;
            }
        }
        g->ringCurrent = -1;
        for (std::size_t i = 0; i < g->ring.size(); ++i)
            if (g->ring[i].serial <= g->completedSerial && g->ring[i].block->size >= size) {
                g->ring[i].cursor = 0;
                g->ringCurrent = int(i);
                break;
            }
        if (g->ringCurrent < 0) {
            Context::RingChunk chunk;
            chunk.block = CreateBufferBlock(std::max(kChunk, size), true, false);
            g->ring.push_back(std::move(chunk));
            g->ringCurrent = int(g->ring.size() - 1);
        }
    }
    Fatal("uniform ring allocation failed");
    return {};
}

VkDescriptorSet AllocateSet(VkDescriptorSetLayout layout) {
    constexpr std::uint32_t kSetsPerPool = 512;
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (!g->currentPools.empty() && g->poolSetsLeft) {
            VkDescriptorSetAllocateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            info.descriptorPool = g->currentPools.back();
            info.descriptorSetCount = 1;
            info.pSetLayouts = &layout;
            VkDescriptorSet set = VK_NULL_HANDLE;
            const VkResult result = vkAllocateDescriptorSets(g->device, &info, &set);
            if (result == VK_SUCCESS) {
                --g->poolSetsLeft;
                ++g->descriptorSets;
                return set;
            }
        }
        VkDescriptorPool pool = VK_NULL_HANDLE;
        if (!g->freePools.empty()) {
            pool = g->freePools.back();
            g->freePools.pop_back();
        } else {
            const VkDescriptorPoolSize sizes[] = {
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, kSetsPerPool * 3},
                {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, kSetsPerPool * 2},
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kSetsPerPool * 24},
                {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, kSetsPerPool * 2},
            };
            VkDescriptorPoolCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            info.maxSets = kSetsPerPool;
            info.poolSizeCount = 4;
            info.pPoolSizes = sizes;
            VK_CHECK(vkCreateDescriptorPool(g->device, &info, nullptr, &pool));
        }
        g->currentPools.push_back(pool);
        g->poolSetsLeft = kSetsPerPool;
    }
    Fatal("descriptor set allocation failed");
    return VK_NULL_HANDLE;
}

// ---------------------------------------------------------- disk caches ---
namespace {
std::string PipelineCachePath() {
    if (g->cacheDirectory.empty()) return {};
    char name[96];
    std::snprintf(name, sizeof(name), "/pipelines-%04x-%04x-%08x.bin", g->properties.vendorID, g->properties.deviceID,
                  g->properties.driverVersion);
    return g->cacheDirectory + name;
}
void LoadPipelineCache() {
    g->cacheDirectory.clear();
    if (g->diskCache) {
        std::string base;
        if (const char *xdg = std::getenv("XDG_CACHE_HOME"); xdg && *xdg) base = xdg;
        else if (const char *home = std::getenv("HOME"); home && *home) base = std::string(home) + "/.cache";
        if (!base.empty()) {
            std::error_code error;
            const std::filesystem::path directory = std::filesystem::path(base) / "judas-engine" / "vulkan";
            std::filesystem::create_directories(directory, error);
            if (!error) g->cacheDirectory = directory.string();
        }
    }
    std::vector<char> data;
    if (const std::string path = PipelineCachePath(); !path.empty()) {
        std::ifstream file(path, std::ios::binary);
        if (file) data.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }
    // The driver validates the header (vendor, device, cache UUID) and ignores
    // data it does not recognise.
    VkPipelineCacheCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    info.initialDataSize = data.size();
    info.pInitialData = data.data();
    if (vkCreatePipelineCache(g->device, &info, nullptr, &g->pipelineCache) != VK_SUCCESS) {
        info.initialDataSize = 0;
        info.pInitialData = nullptr;
        data.clear();
        VK_CHECK(vkCreatePipelineCache(g->device, &info, nullptr, &g->pipelineCache));
    }
    g->pipelineCacheLoadedBytes = data.size();
}
void SavePipelineCache() {
    if (!g->pipelineCache) return;
    const std::string path = PipelineCachePath();
    std::size_t size = 0;
    if (!path.empty() && g->pipelinesCreated && vkGetPipelineCacheData(g->device, g->pipelineCache, &size, nullptr) == VK_SUCCESS &&
        size > g->pipelineCacheLoadedBytes) {
        std::vector<char> data(size);
        if (vkGetPipelineCacheData(g->device, g->pipelineCache, &size, data.data()) == VK_SUCCESS) {
            // Write then rename so a concurrent or interrupted process never leaves a torn file.
            const std::string temporary = path + "." + std::to_string(std::uint64_t(std::chrono::steady_clock::now().time_since_epoch().count())) + ".tmp";
            std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
            file.write(data.data(), std::streamsize(size));
            file.close();
            std::error_code error;
            if (file.good()) std::filesystem::rename(temporary, path, error);
            if (!file.good() || error) std::filesystem::remove(temporary, error);
        }
    }
    vkDestroyPipelineCache(g->device, g->pipelineCache, nullptr);
    g->pipelineCache = VK_NULL_HANDLE;
}
} // namespace

// ------------------------------------------------------------- swapchain ---
namespace {
void DestroySwapchain() {
    if (g->swapchain) vkDestroySwapchainKHR(g->device, g->swapchain, nullptr);
    g->swapchain = VK_NULL_HANDLE;
    g->swapchainImages.clear();
    g->swapchainLayouts.clear();
}

void CreateSwapchain() {
    g->swapchainDirty = false;
    if (!g->surface) return;
    vkDeviceWaitIdle(g->device);
    VkSurfaceCapabilitiesKHR caps;
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g->physical, g->surface, &caps) != VK_SUCCESS) {
        DestroySwapchain();
        return;
    }
    int width = 0, height = 0;
    SDL_Vulkan_GetDrawableSize(g->window, &width, &height);
    VkExtent2D extent = caps.currentExtent;
    if (extent.width == 0xFFFFFFFFu) extent = {std::uint32_t(width), std::uint32_t(height)};
    extent.width = std::clamp(extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
    extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    if (!extent.width || !extent.height) {
        DestroySwapchain();
        return;
    }
    std::uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(g->physical, g->surface, &count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(g->physical, g->surface, &count, formats.data());
    if (formats.empty()) {
        DestroySwapchain();
        return;
    }
    // The renderer writes display-encoded values, exactly as it does to the
    // OpenGL default framebuffer without GL_FRAMEBUFFER_SRGB: use UNORM.
    VkSurfaceFormatKHR chosen = formats[0];
    for (const auto &format : formats)
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM || format.format == VK_FORMAT_R8G8B8A8_UNORM) {
            chosen = format;
            break;
        }
    vkGetPhysicalDeviceSurfacePresentModesKHR(g->physical, g->surface, &count, nullptr);
    std::vector<VkPresentModeKHR> modes(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(g->physical, g->surface, &count, modes.data());
    VkPresentModeKHR mode = VK_PRESENT_MODE_FIFO_KHR;
    if (g->swapInterval == 0) {
        for (auto candidate : modes)
            if (candidate == VK_PRESENT_MODE_MAILBOX_KHR) mode = candidate;
        for (auto candidate : modes)
            if (candidate == VK_PRESENT_MODE_IMMEDIATE_KHR) mode = candidate;
    }
    VkSwapchainCreateInfoKHR info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    info.surface = g->surface;
    info.minImageCount = std::max(caps.minImageCount, 2u);
    if (caps.maxImageCount && info.minImageCount > caps.maxImageCount)
        info.minImageCount = caps.maxImageCount;
    info.imageFormat = chosen.format;
    info.imageColorSpace = chosen.colorSpace;
    info.imageExtent = extent;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.preTransform = caps.currentTransform;
    info.compositeAlpha = (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
                              ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
                              : VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    info.presentMode = mode;
    info.clipped = VK_TRUE;
    info.oldSwapchain = g->swapchain;
    VkSwapchainKHR created = VK_NULL_HANDLE;
    const VkResult result = vkCreateSwapchainKHR(g->device, &info, nullptr, &created);
    DestroySwapchain();
    if (result != VK_SUCCESS) {
        Warn("swapchain creation failed (%d); presentation disabled until the next resize", int(result));
        return;
    }
    g->swapchain = created;
    g->swapchainFormat = chosen.format;
    g->presentMode = mode;
    g->swapchainExtent = extent;
    vkGetSwapchainImagesKHR(g->device, g->swapchain, &count, nullptr);
    g->swapchainImages.resize(count);
    vkGetSwapchainImagesKHR(g->device, g->swapchain, &count, g->swapchainImages.data());
    g->swapchainLayouts.assign(count, VK_IMAGE_LAYOUT_UNDEFINED);
    // The previous swapchain is destroyed, releasing its presents' semaphore waits.
    while (g->presentSemaphores.size() < count) {
        VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkSemaphore semaphore = VK_NULL_HANDLE;
        VK_CHECK(vkCreateSemaphore(g->device, &semaphoreInfo, nullptr, &semaphore));
        g->presentSemaphores.push_back(semaphore);
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL DebugMessage(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                            VkDebugUtilsMessageTypeFlagsEXT,
                                            const VkDebugUtilsMessengerCallbackDataEXT *data, void *) {
    if (g) ++g->validationMessages;
    static unsigned printed = 0;
    if (++printed <= 60)
        std::fprintf(stderr, "[Vulkan validation] %s: %s\n",
                     severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT ? "error" : "warning",
                     data && data->pMessage ? data->pMessage : "");
    return VK_FALSE;
}
} // namespace

void RecreateDefaultFramebuffer(int width, int height) {
    width = std::max(1, width);
    height = std::max(1, height);
    if (g->defaultColor && g->drawableWidth == width && g->drawableHeight == height) return;
    EndPass();
    if (g->defaultColorView) Retire(Retired::ViewKind, std::uint64_t(g->defaultColorView), g->serial);
    if (g->defaultDepthView) Retire(Retired::ViewKind, std::uint64_t(g->defaultDepthView), g->serial);
    RetireImage(g->defaultColor);
    RetireImage(g->defaultDepth);
    g->defaultColor = CreateImage(VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, std::uint32_t(width),
                                  std::uint32_t(height), 1, 1, 1, true);
    g->defaultDepth = CreateImage(VK_IMAGE_TYPE_2D, g->depth24Format, std::uint32_t(width),
                                  std::uint32_t(height), 1, 1, 1, true);
    g->defaultColorView = CreateView(*g->defaultColor, VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1);
    g->defaultDepthView = CreateView(*g->defaultDepth, VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1);
    g->drawableWidth = width;
    g->drawableHeight = height;
    // An OpenGL default framebuffer starts with undefined contents; start black.
    Transition(*g->defaultColor, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkClearColorValue black{};
    black.float32[3] = 1.0f;
    VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdClearColorImage(g->cmd, g->defaultColor->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         &black, 1, &range);
    Transition(*g->defaultDepth, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkClearDepthStencilValue depth{1.0f, 0};
    VkImageSubresourceRange depthRange{g->defaultDepth->aspect, 0, 1, 0, 1};
    vkCmdClearDepthStencilImage(g->cmd, g->defaultDepth->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                &depth, 1, &depthRange);
}

} // namespace detail

using namespace detail;

bool Compiled() { return true; }
std::uint32_t WindowFlag() { return SDL_WINDOW_VULKAN; }
bool Active() { return g != nullptr; }
std::uintptr_t ContextIdentity() { return reinterpret_cast<std::uintptr_t>(g); }

namespace {
std::unordered_map<std::string, Proc> &EntryTable() {
    static std::unordered_map<std::string, Proc> table;
    return table;
}
} // namespace

Proc ProcAddress(const char *name) {
    auto &table = EntryTable();
    if (table.empty()) {
        InstallShaderEntries(table);
        InstallTextureEntries(table);
        InstallDrawEntries(table);
        InstallStateEntries(table);
    }
    const auto found = table.find(name ? name : "");
    return found == table.end() ? nullptr : found->second;
}

bool Init(SDL_Window *window, int glMajor, int glMinor, std::string &error) {
    if (g) {
        error = "The Vulkan backend supports one device per process.";
        return false;
    }
    auto context = std::make_unique<Context>();
    context->window = window;
    context->glMajor = glMajor;
    context->glMinor = glMinor;
    if (const char *value = std::getenv("JUDAS_VULKAN_VALIDATION"))
        context->validation = std::strcmp(value, "0") != 0;
    if (const char *value = std::getenv("JUDAS_VULKAN_SHADER_CACHE")) context->diskCache = std::strcmp(value, "0") != 0;
    if (const char *value = std::getenv("JUDAS_VULKAN_DRAW_CACHE")) context->drawCache = std::strcmp(value, "0") != 0;
    if (const char *value = std::getenv("JUDAS_VULKAN_TRACE"))
        context->trace = std::strcmp(value, "0") != 0;

    // --- instance ---
    unsigned extensionCount = 0;
    std::vector<const char *> instanceExtensions;
    const bool hasSurface = window && (SDL_GetWindowFlags(window) & SDL_WINDOW_VULKAN);
    if (hasSurface) {
        if (!SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr)) {
            error = std::string("SDL cannot enumerate Vulkan surface extensions: ") + SDL_GetError();
            return false;
        }
        instanceExtensions.resize(extensionCount);
        SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, instanceExtensions.data());
    }
    std::vector<const char *> layers;
    if (context->validation) {
        std::uint32_t count = 0;
        vkEnumerateInstanceLayerProperties(&count, nullptr);
        std::vector<VkLayerProperties> available(count);
        vkEnumerateInstanceLayerProperties(&count, available.data());
        bool found = false;
        for (const auto &layer : available)
            found |= std::strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0;
        if (found) {
            layers.push_back("VK_LAYER_KHRONOS_validation");
            instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        } else {
            std::fprintf(stderr, "[Vulkan] JUDAS_VULKAN_VALIDATION requested but the Khronos "
                                 "validation layer is not installed\n");
            context->validation = false;
        }
    }
    VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    application.pApplicationName = "Judas";
    application.pEngineName = "Judas Engine";
    application.apiVersion = VK_API_VERSION_1_3;
    VkInstanceCreateInfo instanceInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instanceInfo.pApplicationInfo = &application;
    instanceInfo.enabledExtensionCount = std::uint32_t(instanceExtensions.size());
    instanceInfo.ppEnabledExtensionNames = instanceExtensions.data();
    instanceInfo.enabledLayerCount = std::uint32_t(layers.size());
    instanceInfo.ppEnabledLayerNames = layers.data();
    VkResult result = vkCreateInstance(&instanceInfo, nullptr, &context->instance);
    if (result != VK_SUCCESS) {
        error = "vkCreateInstance failed (" + std::to_string(int(result)) +
                "). A Vulkan 1.3 loader and driver are required.";
        return false;
    }
    g = context.release();
    const auto fail = [&](const std::string &message) {
        error = message;
        Shutdown();
        return false;
    };
    if (g->validation) {
        VkDebugUtilsMessengerCreateInfoEXT info{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
        info.pfnUserCallback = &DebugMessage;
        auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(g->instance, "vkCreateDebugUtilsMessengerEXT"));
        if (create) create(g->instance, &info, nullptr, &g->messenger);
    }
    if (hasSurface && !SDL_Vulkan_CreateSurface(window, g->instance, &g->surface))
        return fail(std::string("SDL cannot create a Vulkan surface: ") + SDL_GetError());

    // --- physical device ---
    std::uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(g->instance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(g->instance, &deviceCount, devices.data());
    const char *wanted = std::getenv("JUDAS_VULKAN_DEVICE");
    int bestScore = -1;
    std::string rejected;
    for (VkPhysicalDevice device : devices) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device, &properties);
        std::string reason;
        VkPhysicalDeviceVulkan13Features features13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        VkPhysicalDeviceVulkan12Features features12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        features12.pNext = &features13;
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        features.pNext = &features12;
        std::uint32_t family = UINT32_MAX;
        if (properties.apiVersion < VK_API_VERSION_1_3)
            reason = "Vulkan 1.3 is required";
        else {
            vkGetPhysicalDeviceFeatures2(device, &features);
            if (!features13.dynamicRendering) reason = "dynamic rendering unavailable";
            else if (!features12.hostQueryReset) reason = "host query reset unavailable";
            else if (!features.features.imageCubeArray) reason = "cube array images unavailable";
            std::uint32_t familyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);
            std::vector<VkQueueFamilyProperties> families(familyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, families.data());
            for (std::uint32_t i = 0; i < familyCount && family == UINT32_MAX; ++i) {
                if (!(families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) ||
                    !(families[i].queueFlags & VK_QUEUE_COMPUTE_BIT))
                    continue;
                VkBool32 presentable = VK_TRUE;
                if (g->surface)
                    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, g->surface, &presentable);
                if (presentable) family = i;
            }
            if (reason.empty() && family == UINT32_MAX) reason = "no graphics+compute+present queue";
        }
        if (!reason.empty()) {
            rejected += std::string(properties.deviceName) + ": " + reason + "; ";
            continue;
        }
        int score = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU     ? 4
                    : properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 3
                    : properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU    ? 2
                                                                                      : 1;
        if (wanted && *wanted && std::strstr(properties.deviceName, wanted)) score += 100;
        if (score > bestScore) {
            bestScore = score;
            g->physical = device;
            g->properties = properties;
            g->queueFamily = family;
            g->features = features.features;
        }
    }
    if (!g->physical)
        return fail("No Vulkan 1.3 device can run Judas. " +
                    (rejected.empty() ? std::string("No Vulkan devices were found.") : rejected));
    vkGetPhysicalDeviceMemoryProperties(g->physical, &g->memoryProperties);
    g->bufferAlignment = std::max<VkDeviceSize>({256, g->properties.limits.minUniformBufferOffsetAlignment,
                                                 g->properties.limits.minStorageBufferOffsetAlignment,
                                                 g->properties.limits.minTexelBufferOffsetAlignment});

    // --- device ---
    VkPhysicalDeviceVulkan13Features enable13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    enable13.dynamicRendering = VK_TRUE;
    VkPhysicalDeviceVulkan12Features enable12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    enable12.hostQueryReset = VK_TRUE;
    enable12.pNext = &enable13;
    VkPhysicalDeviceFeatures2 enable{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    enable.pNext = &enable12;
    enable.features.imageCubeArray = VK_TRUE;
    enable.features.samplerAnisotropy = g->features.samplerAnisotropy;
    enable.features.textureCompressionBC = g->features.textureCompressionBC;
    enable.features.multiDrawIndirect = g->features.multiDrawIndirect;
    enable.features.fillModeNonSolid = g->features.fillModeNonSolid;
    enable.features.depthClamp = g->features.depthClamp;
    enable.features.depthBiasClamp = g->features.depthBiasClamp;
    enable.features.independentBlend = g->features.independentBlend;
    enable.features.fragmentStoresAndAtomics = g->features.fragmentStoresAndAtomics;
    enable.features.vertexPipelineStoresAndAtomics = g->features.vertexPipelineStoresAndAtomics;
    enable.features.shaderClipDistance = g->features.shaderClipDistance;
    enable.features.shaderCullDistance = g->features.shaderCullDistance;
    enable.features.sampleRateShading = g->features.sampleRateShading;
    enable.features.shaderImageGatherExtended = g->features.shaderImageGatherExtended;
    // GLSL 4.x kernels may use double and 64-bit integer arithmetic.
    enable.features.shaderFloat64 = g->features.shaderFloat64;
    enable.features.shaderInt64 = g->features.shaderInt64;
    enable.features.shaderInt16 = g->features.shaderInt16;
    g->anisotropy = g->features.samplerAnisotropy;
    g->bcSupported = g->features.textureCompressionBC;
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueInfo.queueFamilyIndex = g->queueFamily;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;
    std::vector<const char *> deviceExtensions;
    if (g->surface) deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    // Prefer the device's own OpenGL-convention depth transform: it reproduces
    // OpenGL's depth arithmetic exactly. Without it every vertex shader remaps
    // z after main(), which is equivalent up to floating-point rounding.
    VkPhysicalDeviceDepthClipControlFeaturesEXT clipControl{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_CONTROL_FEATURES_EXT};
    VkPhysicalDeviceTransformFeedbackFeaturesEXT feedback{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT};
    {
        std::uint32_t count = 0;
        vkEnumerateDeviceExtensionProperties(g->physical, nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> available(count);
        vkEnumerateDeviceExtensionProperties(g->physical, nullptr, &count, available.data());
        bool present = false;
        for (const auto &extension : available)
            present |= std::strcmp(extension.extensionName, VK_EXT_DEPTH_CLIP_CONTROL_EXTENSION_NAME) == 0;
        bool feedbackPresent = false;
        for (const auto &extension : available)
            feedbackPresent |= std::strcmp(extension.extensionName, VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME) == 0;
        if (feedbackPresent) {
            VkPhysicalDeviceFeatures2 query{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
            query.pNext = &feedback;
            vkGetPhysicalDeviceFeatures2(g->physical, &query);
            if (feedback.transformFeedback) {
                g->transformFeedback = true;
                deviceExtensions.push_back(VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME);
                feedback.geometryStreams = VK_FALSE;
                feedback.pNext = nullptr;
                enable13.pNext = &feedback;
            }
        }
        const char *force = std::getenv("JUDAS_VULKAN_SHADER_DEPTH");
        if (present && !(force && std::strcmp(force, "1") == 0)) {
            VkPhysicalDeviceFeatures2 query{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
            query.pNext = &clipControl;
            vkGetPhysicalDeviceFeatures2(g->physical, &query);
            if (clipControl.depthClipControl) {
                g->depthClipControl = true;
                deviceExtensions.push_back(VK_EXT_DEPTH_CLIP_CONTROL_EXTENSION_NAME);
                clipControl.pNext = const_cast<void *>(enable13.pNext);
                enable13.pNext = &clipControl;
            }
        }
    }
    VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceInfo.pNext = &enable;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.enabledExtensionCount = std::uint32_t(deviceExtensions.size());
    deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
    // A device can be reported lost at creation while the driver is still
    // releasing another process's context (observed twice on NVIDIA 580 when a
    // run started in the same second the previous one exited). Retry briefly.
    for (int attempt = 0; attempt < 4; ++attempt) {
        result = vkCreateDevice(g->physical, &deviceInfo, nullptr, &g->device);
        if (result != VK_ERROR_DEVICE_LOST) break;
        std::fprintf(stderr, "[Vulkan] device creation reported a lost device (attempt %d); retrying\n", attempt + 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(250 << attempt));
    }
    if (result != VK_SUCCESS) return fail("vkCreateDevice failed (" + std::to_string(int(result)) + ")");
    vkGetDeviceQueue(g->device, g->queueFamily, 0, &g->queue);
    if (g->transformFeedback) {
        // Extension commands are not exported by the loader library.
        g->bindFeedbackBuffers = reinterpret_cast<PFN_vkCmdBindTransformFeedbackBuffersEXT>(
            vkGetDeviceProcAddr(g->device, "vkCmdBindTransformFeedbackBuffersEXT"));
        g->beginFeedback = reinterpret_cast<PFN_vkCmdBeginTransformFeedbackEXT>(
            vkGetDeviceProcAddr(g->device, "vkCmdBeginTransformFeedbackEXT"));
        g->endFeedback = reinterpret_cast<PFN_vkCmdEndTransformFeedbackEXT>(
            vkGetDeviceProcAddr(g->device, "vkCmdEndTransformFeedbackEXT"));
        g->transformFeedback = g->bindFeedbackBuffers && g->beginFeedback && g->endFeedback;
    }

    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = g->queueFamily;
    VK_CHECK(vkCreateCommandPool(g->device, &poolInfo, nullptr, &g->commandPool));
    LoadPipelineCache();
    VkQueryPoolCreateInfo queryInfo{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    queryInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    queryInfo.queryCount = Context::kQuerySlots;
    VK_CHECK(vkCreateQueryPool(g->device, &queryInfo, nullptr, &g->queryPool));
    vkResetQueryPool(g->device, g->queryPool, 0, Context::kQuerySlots);

    // The OpenGL paths use 24-bit fixed-point depth. Prefer the same
    // representation; devices without it use 32-bit float depth.
    g->depth24Format = VK_FORMAT_D32_SFLOAT;
    {
        const VkFormatFeatureFlags needed = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                            VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
                                            VK_FORMAT_FEATURE_BLIT_SRC_BIT;
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(g->physical, VK_FORMAT_X8_D24_UNORM_PACK32, &properties);
        const char *force = std::getenv("JUDAS_VULKAN_DEPTH32F");
        if ((properties.optimalTilingFeatures & needed) == needed && !(force && std::strcmp(force, "1") == 0))
            g->depth24Format = VK_FORMAT_X8_D24_UNORM_PACK32;
    }

    char version[160];
    std::snprintf(version, sizeof(version), "%d.%d Judas Vulkan %u.%u.%u", glMajor, glMinor,
                  VK_API_VERSION_MAJOR(g->properties.apiVersion),
                  VK_API_VERSION_MINOR(g->properties.apiVersion),
                  VK_API_VERSION_PATCH(g->properties.apiVersion));
    g->versionString = version;
    g->rendererString = std::string("Vulkan / ") + g->properties.deviceName;
    char vendor[64];
    std::snprintf(vendor, sizeof(vendor), "Judas Vulkan backend (vendor 0x%04x)", g->properties.vendorID);
    g->vendorString = vendor;
    if (glMajor > 4 || (glMajor == 4 && glMinor >= 5))
        g->extensions = {"GL_ARB_buffer_storage", "GL_ARB_compute_shader", "GL_ARB_direct_state_access",
                         "GL_ARB_multi_draw_indirect", "GL_ARB_shader_storage_buffer_object",
                         "GL_ARB_texture_view", "GL_ARB_uniform_buffer_object"};
    else
        g->extensions = {"GL_ARB_uniform_buffer_object"};
    if (g->bcSupported) g->extensions.push_back("GL_EXT_texture_compression_s3tc");

    BeginCommandBuffer();
    int width = 1, height = 1;
    if (g->surface) SDL_Vulkan_GetDrawableSize(window, &width, &height);
    else if (window) SDL_GetWindowSize(window, &width, &height);
    RecreateDefaultFramebuffer(width, height);
    g->gl.viewport[2] = g->gl.scissor[2] = g->drawableWidth;
    g->gl.viewport[3] = g->gl.scissor[3] = g->drawableHeight;
    g->dummyVertex = AllocateBuffer(64);
    {
        // Generic vertex attribute default (0,0,0,1) for inputs without an array.
        const float floats[4] = {0, 0, 0, 1};
        std::memcpy(g->dummyVertex.Data(), floats, sizeof(floats));
        std::memset(g->dummyVertex.Data() + 16, 0, 48);
    }
    CreateDummyTextures();
    CreateSwapchain();
    std::fprintf(stderr, "[Graphics] Vulkan backend: %s (API %u.%u.%u, driver 0x%x), depth %s, BC %d%s\n",
                 g->properties.deviceName, VK_API_VERSION_MAJOR(g->properties.apiVersion),
                 VK_API_VERSION_MINOR(g->properties.apiVersion),
                 VK_API_VERSION_PATCH(g->properties.apiVersion), g->properties.driverVersion,
                 g->depth24Format == VK_FORMAT_X8_D24_UNORM_PACK32
                     ? (g->depthClipControl ? "D24, device clip control" : "D24, shader clip remap")
                     : (g->depthClipControl ? "D32F, device clip control" : "D32F, shader clip remap"),
                 int(g->bcSupported), g->validation ? ", validation on" : "");
    return true;
}

void Shutdown() {
    if (!g) return;
    if (g->device) {
        PrintDispatchTiming();
        SavePipelineCache();
        vkDeviceWaitIdle(g->device);
        if (g->cmd) {
            EndPass();
            vkEndCommandBuffer(g->cmd);
        }
        g->completedSerial = UINT64_MAX;
        for (auto &submission : g->inFlight) {
            vkDestroyFence(g->device, submission.fence, nullptr);
            for (auto pool : submission.pools) vkDestroyDescriptorPool(g->device, pool, nullptr);
        }
        g->inFlight.clear();
        g->pass = PassState{};
        ClearDrawScratch();
        if (g->dummyTexelView) vkDestroyBufferView(g->device, g->dummyTexelView, nullptr);
        for (auto &buffer : g->buffers)
            if (buffer.texelView) vkDestroyBufferView(g->device, buffer.texelView, nullptr);
        for (auto &program : g->programs) DestroyProgramResources(program);
        for (auto &texture : g->textures) DestroyTextureObject(texture);
        for (auto &renderbuffer : g->renderbuffers) {
            if (renderbuffer.view) vkDestroyImageView(g->device, renderbuffer.view, nullptr);
            renderbuffer.view = VK_NULL_HANDLE;
            renderbuffer.image.reset();
        }
        for (auto &buffer : g->buffers) {
            RetireRange(buffer.storage, 0);
            buffer.storage = {};
        }
        RetireRange(g->dummyVertex, 0);
        if (g->defaultColorView) vkDestroyImageView(g->device, g->defaultColorView, nullptr);
        if (g->defaultDepthView) vkDestroyImageView(g->device, g->defaultDepthView, nullptr);
        g->defaultColor.reset();
        g->defaultDepth.reset();
        ReleaseRetired();
        for (auto &entry : g->pipelines) vkDestroyPipeline(g->device, entry.second, nullptr);
        for (auto &entry : g->samplerCache) vkDestroySampler(g->device, entry.second, nullptr);
        for (auto pool : g->freePools) vkDestroyDescriptorPool(g->device, pool, nullptr);
        for (auto pool : g->currentPools) vkDestroyDescriptorPool(g->device, pool, nullptr);
        for (auto fence : g->freeFences) vkDestroyFence(g->device, fence, nullptr);
        for (auto &chunk : g->ring) DestroyBlock(*chunk.block);
        for (auto &block : g->bufferBlocks) DestroyBlock(*block);
        for (auto &block : g->imageBlocks) DestroyBlock(*block);
        g->bufferBlocks.clear();
        g->imageBlocks.clear();
        DestroySwapchain();
        if (g->queryPool) vkDestroyQueryPool(g->device, g->queryPool, nullptr);
        for (VkSemaphore semaphore : g->acquireSemaphores) vkDestroySemaphore(g->device, semaphore, nullptr);
        for (VkSemaphore semaphore : g->presentSemaphores) vkDestroySemaphore(g->device, semaphore, nullptr);
        if (g->commandPool) vkDestroyCommandPool(g->device, g->commandPool, nullptr);
        vkDestroyDevice(g->device, nullptr);
    }
    if (g->surface) vkDestroySurfaceKHR(g->instance, g->surface, nullptr);
    if (g->messenger) {
        auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(g->instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (destroy) destroy(g->instance, g->messenger, nullptr);
    }
    if (g->trace)
        std::fprintf(stderr, "[Vulkan trace] shader programs: %llu from disk, %llu compiled, %.1f ms; pipelines created: %llu, %.1f ms\n",
                     static_cast<unsigned long long>(g->shaderCacheHits), static_cast<unsigned long long>(g->shaderCacheMisses),
                     g->shaderSeconds * 1e3, static_cast<unsigned long long>(g->pipelinesCreated), g->pipelineSeconds * 1e3);
    if (g->trace)
        std::fprintf(stderr, "[Vulkan trace] draws reusing state: %llu, rebuilding: %llu\n",
                     static_cast<unsigned long long>(g->drawCacheHits), static_cast<unsigned long long>(g->drawCacheMisses));
    if (g->validation)
        std::fprintf(stderr, "[Vulkan] validation messages: %llu\n",
                     static_cast<unsigned long long>(g->validationMessages));
    if (g->instance) vkDestroyInstance(g->instance, nullptr);
    delete g;
    g = nullptr;
    ShutdownShaderCompiler();
}

void SetSwapInterval(int interval) {
    if (!g || g->swapInterval == interval) return;
    g->swapInterval = interval;
    g->swapchainDirty = true;
}

int SwapInterval() {
    if (!g) return -1;
    if (!g->swapchain || g->swapchainDirty) return g->swapInterval;
    return g->presentMode == VK_PRESENT_MODE_IMMEDIATE_KHR || g->presentMode == VK_PRESENT_MODE_MAILBOX_KHR ? 0 : 1;
}

void DrawableSize(int &width, int &height) {
    width = g ? g->drawableWidth : 0;
    height = g ? g->drawableHeight : 0;
}

void Present() {
    if (!g) return;
    EndPass();
    ++g->frames;
    int width = g->drawableWidth, height = g->drawableHeight;
    if (g->surface) SDL_Vulkan_GetDrawableSize(g->window, &width, &height);
    else if (g->window) SDL_GetWindowSize(g->window, &width, &height);
    if (g->surface && (g->swapchainDirty || !g->swapchain ||
                       g->swapchainExtent.width != std::uint32_t(width) ||
                       g->swapchainExtent.height != std::uint32_t(height)))
        CreateSwapchain();
    bool presented = false;
    if (g->swapchain) {
        std::uint32_t index = 0;
        VkSemaphore acquireSemaphore = VK_NULL_HANDLE;
        if (!g->freeAcquireSemaphores.empty()) {
            acquireSemaphore = g->freeAcquireSemaphores.back();
            g->freeAcquireSemaphores.pop_back();
        } else {
            VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
            VK_CHECK(vkCreateSemaphore(g->device, &semaphoreInfo, nullptr, &acquireSemaphore));
            g->acquireSemaphores.push_back(acquireSemaphore);
        }
        VkResult acquired = vkAcquireNextImageKHR(g->device, g->swapchain, UINT64_MAX,
                                                  acquireSemaphore, VK_NULL_HANDLE, &index);
        if (acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR)
            g->freeAcquireSemaphores.push_back(acquireSemaphore); // not signalled
        if (acquired == VK_SUCCESS || acquired == VK_SUBOPTIMAL_KHR) {
            VkSemaphore presentSemaphore = g->presentSemaphores[index];
            Transition(*g->defaultColor, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = g->swapchainImages[index];
            barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(g->cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                 &barrier);
            // The default framebuffer keeps OpenGL's bottom-left origin: row 0
            // is the bottom of the picture, so presentation flips vertically.
            VkImageBlit blit{};
            blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            blit.srcOffsets[0] = {0, g->drawableHeight, 0};
            blit.srcOffsets[1] = {g->drawableWidth, 0, 1};
            blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            blit.dstOffsets[1] = {int(g->swapchainExtent.width), int(g->swapchainExtent.height), 1};
            vkCmdBlitImage(g->cmd, g->defaultColor->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           g->swapchainImages[index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                           VK_FILTER_NEAREST);
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = 0;
            vkCmdPipelineBarrier(g->cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                 &barrier);
            Submit(acquireSemaphore, presentSemaphore);
            VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
            present.waitSemaphoreCount = 1;
            present.pWaitSemaphores = &presentSemaphore;
            present.swapchainCount = 1;
            present.pSwapchains = &g->swapchain;
            present.pImageIndices = &index;
            const VkResult result = vkQueuePresentKHR(g->queue, &present);
            if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
                g->swapchainDirty = true;
            presented = true;
        } else
            g->swapchainDirty = true;
    }
    if (!presented) Submit(VK_NULL_HANDLE, VK_NULL_HANDLE);
    // Bound latency and memory: at most two frames may be in flight. Count
    // frames, not submissions: a frame that also flushed mid-way (fences,
    // readbacks) must not force the CPU to wait for the previous frame, which
    // would serialise CPU and GPU work.
    g->frameSerials.push_back(g->serial - 1);
    while (g->frameSerials.size() > 2) {
        PollCompleted(true, g->frameSerials.front());
        g->frameSerials.pop_front();
    }
    PollCompleted(false);
    ResolveDispatchTiming(false);
    if (width != g->drawableWidth || height != g->drawableHeight)
        RecreateDefaultFramebuffer(width, height);
}

Statistics Stats() {
    Statistics stats;
    if (!g) return stats;
    stats.frames = g->frames;
    stats.submissions = g->submissions;
    stats.renderPasses = g->renderPasses;
    stats.draws = g->draws;
    stats.pipelines = g->pipelines.size();
    stats.descriptorSets = g->descriptorSets;
    stats.validationMessages = g->validationMessages;
    stats.bufferBytes = g->bufferBytes;
    stats.imageBytes = g->imageBytes;
    return stats;
}

std::string DeviceSummary() {
    if (!g) return "Vulkan backend inactive";
    char text[256];
    std::snprintf(text, sizeof(text), "Vulkan %u.%u.%u / %s",
                  VK_API_VERSION_MAJOR(g->properties.apiVersion),
                  VK_API_VERSION_MINOR(g->properties.apiVersion),
                  VK_API_VERSION_PATCH(g->properties.apiVersion), g->properties.deviceName);
    return text;
}

} // namespace JudasVulkan
