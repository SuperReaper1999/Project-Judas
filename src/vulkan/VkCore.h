#pragma once
// Internal declarations of the Vulkan backend. Not included by engine code.
#include <vulkan/vulkan.h>
#include <glad/gl.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct SDL_Window;

namespace JudasVulkan::detail {

void Fatal(const char *format, ...);
void Warn(const char *format, ...);

// ---------------------------------------------------------------- memory ---
// One device allocation subdivided by a first-fit free list. Buffer blocks
// also own a VkBuffer covering the whole block (bound with offsets).
struct MemBlock {
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkBuffer buffer = VK_NULL_HANDLE;
    std::uint8_t *mapped = nullptr;
    VkDeviceSize size = 0;
    std::uint32_t memoryType = 0;
    bool dedicated = false;
    std::map<VkDeviceSize, VkDeviceSize> free; // offset -> size
};
struct Range {
    MemBlock *block = nullptr;
    VkDeviceSize offset = 0, size = 0;
    explicit operator bool() const { return block != nullptr; }
    std::uint8_t *Data() const { return block->mapped + offset; }
};

// ---------------------------------------------------------------- images ---
struct Image {
    VkImage image = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    VkImageType type = VK_IMAGE_TYPE_2D;
    std::uint32_t width = 0, height = 0, depth = 1, layers = 1, levels = 1;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageUsageFlags usage = 0;
    Range memory;
    std::uint64_t lastUse = 0;
    bool external = false; // swapchain image
    std::uint64_t bytes = 0;
    ~Image();
};

struct SamplerState {
    GLint minFilter = GL_NEAREST_MIPMAP_LINEAR, magFilter = GL_LINEAR;
    GLint wrapS = GL_REPEAT, wrapT = GL_REPEAT, wrapR = GL_REPEAT;
    GLint compareMode = GL_NONE, compareFunc = GL_LEQUAL;
    float maxAnisotropy = 1.0f;
    float minLod = -1000.0f, maxLod = 1000.0f;
    float border[4] = {0, 0, 0, 0};
};

struct LevelSpec {
    int width = 0, height = 0, depth = 1;
    std::vector<std::uint8_t> data; // already in the VkFormat's layout
    bool specified = false;
};

struct Texture {
    GLenum target = 0; // 0 until first bind
    std::shared_ptr<Image> image;
    GLenum internalFormat = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
    // Requested (GL-side) state before/after realization.
    std::vector<LevelSpec> specs;
    bool wantMipmaps = false, immutable = false;
    int baseLevel = 0, maxLevel = 1000;
    SamplerState sampler;
    // View of another texture's image (glTextureView) or whole image.
    std::uint32_t viewBaseLayer = 0, viewLayers = 0, viewBaseLevel = 0, viewLevels = 0;
    bool isView = false;
    VkImageView view = VK_NULL_HANDLE;
    int viewMaxLevel = -1, viewBaseLevelUsed = -1;
    std::map<std::uint64_t, VkImageView> attachmentViews;
    // Buffer textures.
    GLuint buffer = 0;
    GLenum bufferFormat = 0;
    bool alive = false;
};

struct Renderbuffer {
    std::shared_ptr<Image> image;
    VkImageView view = VK_NULL_HANDLE;
    GLenum internalFormat = 0;
    int width = 0, height = 0;
    bool alive = false, bound = false;
};

struct Attachment {
    enum Kind { None, Tex, Rb } kind = None;
    GLuint name = 0;
    int level = 0, layer = 0;
};
struct Framebuffer {
    std::array<Attachment, 8> color{};
    Attachment depth, stencil;
    std::array<GLenum, 8> drawBuffers{GL_COLOR_ATTACHMENT0, GL_NONE, GL_NONE, GL_NONE,
                                      GL_NONE, GL_NONE, GL_NONE, GL_NONE};
    int drawBufferCount = 1;
    GLenum readBuffer = GL_COLOR_ATTACHMENT0;
    std::uint64_t revision = 1;
    bool alive = false, bound = false;
};

// --------------------------------------------------------------- buffers ---
struct Buffer {
    Range storage;
    VkDeviceSize size = 0;
    std::uint64_t lastUse = 0;      // serial of the last command referencing storage
    std::uint64_t lastGpuWrite = 0; // serial of the last GPU-side write
    bool persistent = false, mapped = false, alive = false, bound = false;
    GLenum usage = GL_STATIC_DRAW;
    GLbitfield storageFlags = 0;
    // Active glMapBufferRange.
    VkDeviceSize mapOffset = 0, mapLength = 0;
    GLbitfield mapAccess = 0;
    // Cached texel-buffer view of the current storage (buffer textures).
    VkBufferView texelView = VK_NULL_HANDLE;
    VkFormat texelFormat = VK_FORMAT_UNDEFINED;
};

struct VertexAttrib {
    bool enabled = false, integer = false, normalized = false;
    GLint size = 4;
    GLenum type = GL_FLOAT;
    GLsizei stride = 0;
    std::uintptr_t offset = 0;
    GLuint buffer = 0;
    GLuint divisor = 0;
    // DSA-style binding override (glVertexArrayVertexBuffer).
};
struct VertexArray {
    std::array<VertexAttrib, 16> attribs{};
    GLuint elementBuffer = 0;
    bool alive = false, bound = false;
};

struct SamplerObject {
    SamplerState state;
    bool alive = false;
};

// --------------------------------------------------------------- shaders ---
struct ShaderObject {
    GLenum type = 0;
    std::string source, log;
    bool compiled = false, alive = false, deletePending = false;
    int attachCount = 0;
};

enum class UniformBase : std::uint8_t { Float, Int, UInt, Bool, Sampler };
struct Uniform {
    std::string name;       // GL-style flattened name without [0]
    GLenum glType = 0;      // GL_FLOAT_VEC3, GL_SAMPLER_2D, ...
    UniformBase base = UniformBase::Float;
    std::uint8_t columns = 1, rows = 1; // vector: columns=1, rows=n
    std::uint32_t offset = 0;      // byte offset in default block
    std::uint32_t arraySize = 1;   // number of elements (1 for non-arrays)
    std::uint32_t arrayStride = 0; // bytes between elements
    std::uint32_t matrixStride = 0;
    GLint location = -1;           // base location; element i = location + i
    bool isArray = false;
    // Samplers.
    std::uint32_t binding = 0;     // Vulkan binding
    GLenum samplerTarget = 0;      // GL_TEXTURE_2D ...
    bool samplerBuffer = false, samplerShadow = false;
    std::vector<GLint> units;      // per array element
};
struct BlockInfo {
    std::string name;
    std::uint32_t binding = 0; // Vulkan binding
    std::uint32_t size = 0;
    GLuint glBinding = 0;      // GL indexed binding point
    bool storage = false;
};
struct AttributeInfo {
    std::uint32_t location = 0;
    UniformBase base = UniformBase::Float;
    std::uint32_t components = 4;
    std::string name;
};
struct Program {
    std::vector<GLuint> attached;
    bool linked = false, alive = false, deletePending = false, compute = false;
    std::string log;
    std::uint64_t generation = 0;
    VkShaderModule modules[3] = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE}; // vs, fs, cs
    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline computePipeline = VK_NULL_HANDLE;
    std::vector<Uniform> uniforms;
    std::unordered_map<std::string, GLint> locations; // exact GL names
    std::vector<std::pair<std::uint32_t, std::uint32_t>> locationTable; // loc -> (uniform, element)
    bool hasDefaultBlock = false;
    std::uint32_t defaultBinding = 0;
    std::vector<std::uint8_t> defaults;
    bool defaultsDirty = true;
    Range defaultsRange;          // last upload (ring chunk + offset)
    std::uint64_t defaultsSerial = 0;
    std::vector<BlockInfo> uniformBlocks, storageBlocks;
    std::vector<AttributeInfo> attributes;
    std::vector<std::uint32_t> samplerUniforms; // indices into uniforms
    std::uint32_t outputMask = 0;
    std::array<int, 3> localSize{0, 0, 0};
    // Dynamic descriptor order: bindings sorted ascending among dynamic buffers.
    std::vector<std::uint32_t> dynamicBindings;
};

// ----------------------------------------------------------------- state ---
constexpr int kTextureUnits = 48;
enum TexSlot { Slot2D, Slot2DArray, SlotCube, SlotCubeArray, Slot3D, SlotBuffer, SlotCount };
int SlotForTarget(GLenum target);

struct Query {
    bool alive = false, issued = false;
    std::uint32_t slot = 0;
    std::uint64_t serial = 0;
};
struct Sync {
    std::uint64_t serial = 0;
};

struct GlState {
    // Bindings.
    GLuint arrayBuffer = 0, uniformBuffer = 0, copyRead = 0, copyWrite = 0, pixelPack = 0,
           pixelUnpack = 0, textureBuffer = 0, storageBuffer = 0, indirectBuffer = 0,
           queryBuffer = 0, dispatchIndirect = 0, atomicBuffer = 0, feedbackBuffer = 0;
    struct Indexed { GLuint buffer = 0; VkDeviceSize offset = 0, size = 0; };
    std::array<Indexed, 96> uniformBindings{}, storageBindings{};
    GLuint vao = 0, program = 0, drawFbo = 0, readFbo = 0, renderbuffer = 0;
    int activeUnit = 0;
    std::array<std::array<GLuint, SlotCount>, kTextureUnits> textures{};
    std::array<GLuint, kTextureUnits> samplers{};
    // Raster state.
    bool depthTest = false, blend = false, cull = false, scissorTest = false,
         framebufferSrgb = false, depthClamp = false, stencilTest = false, multisample = true,
         polygonOffsetFill = false, primitiveRestart = false, programPointSize = false,
         dither = true, lineSmooth = false, seamlessCube = false, debugOutput = false,
         debugSync = false, rasterizerDiscard = false, sampleAlpha = false;
    GLenum depthFunc = GL_LESS, cullMode = GL_BACK, frontFace = GL_CCW;
    GLboolean depthMask = GL_TRUE;
    GLboolean colorMask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
    GLenum blendSrcRgb = GL_ONE, blendDstRgb = GL_ZERO, blendSrcAlpha = GL_ONE,
           blendDstAlpha = GL_ZERO, blendEqRgb = GL_FUNC_ADD, blendEqAlpha = GL_FUNC_ADD;
    GLenum polygonMode = GL_FILL;
    float polygonOffsetFactor = 0, polygonOffsetUnits = 0;
    GLint viewport[4] = {0, 0, 0, 0}, scissor[4] = {0, 0, 0, 0};
    float clearColor[4] = {0, 0, 0, 0};
    double clearDepth = 1.0;
    GLint clearStencil = 0;
    float lineWidth = 1.0f;
    // Pixel store.
    GLint unpackAlignment = 4, unpackRowLength = 0, unpackSkipRows = 0, unpackSkipPixels = 0,
          unpackImageHeight = 0, unpackSkipImages = 0;
    GLint packAlignment = 4, packRowLength = 0, packSkipRows = 0, packSkipPixels = 0;
    GLenum error = GL_NO_ERROR;
};

struct PipelineKey {
    std::uint64_t program = 0;      // program generation
    std::uint64_t vertexHash = 0;
    std::uint32_t topology = 0;
    std::uint32_t blend = 0;        // packed blend state
    std::uint32_t blendEq = 0;
    std::uint32_t colorFormats[8] = {};
    std::uint32_t colorCount = 0, depthFormat = 0, colorMask = 0xF, polygonMode = 0;
    std::uint32_t depthBias = 0;
    bool operator==(const PipelineKey &o) const { return std::memcmp(this, &o, sizeof(*this)) == 0; }
};
struct PipelineKeyHash {
    std::size_t operator()(const PipelineKey &k) const;
};

struct Submission {
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    std::uint64_t serial = 0;
    std::vector<VkDescriptorPool> pools;
};

struct Retired {
    std::uint64_t serial = 0;
    enum Kind { RangeKind, ViewKind, BufferViewKind, ImageKind, SamplerKind, PipelineKind,
                ModuleKind, SetLayoutKind, LayoutKind } kind = RangeKind;
    Range range;
    std::uint64_t handle = 0;
    std::shared_ptr<Image> image;
};

struct PassState {
    bool active = false;
    GLuint fbo = 0;
    std::uint64_t fboRevision = 0;
    std::uint32_t colorCount = 0;
    std::array<std::shared_ptr<Image>, 8> color{};
    std::array<VkFormat, 8> colorFormats{};
    std::shared_ptr<Image> depth;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    std::uint32_t width = 0, height = 0;
};

struct Context {
    SDL_Window *window = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties properties{};
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    VkPhysicalDeviceFeatures features{};
    std::uint32_t queueFamily = 0;
    VkQueue queue = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swapchainFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent{};
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageLayout> swapchainLayouts;
    VkSemaphore acquireSemaphore = VK_NULL_HANDLE, presentSemaphore = VK_NULL_HANDLE;
    int swapInterval = 1;
    bool swapchainDirty = true;
    VkFormat depth24Format = VK_FORMAT_D32_SFLOAT;
    bool bcSupported = false, anisotropy = false;
    int glMajor = 3, glMinor = 3;
    std::string versionString, rendererString, vendorString;
    std::vector<std::string> extensions;

    // Default framebuffer.
    std::shared_ptr<Image> defaultColor, defaultDepth;
    VkImageView defaultColorView = VK_NULL_HANDLE, defaultDepthView = VK_NULL_HANDLE;
    int drawableWidth = 0, drawableHeight = 0;

    // Recording.
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    std::uint64_t serial = 1, completedSerial = 0;
    std::vector<Submission> inFlight;
    std::vector<VkCommandBuffer> freeCommandBuffers;
    std::vector<VkFence> freeFences;
    std::vector<Retired> retired;
    PassState pass;

    // Memory.
    std::vector<std::unique_ptr<MemBlock>> bufferBlocks, imageBlocks;
    struct RingChunk { std::unique_ptr<MemBlock> block; VkDeviceSize cursor = 0; std::uint64_t serial = 0; };
    std::vector<RingChunk> ring;
    int ringCurrent = -1;
    Range dummyVertex;
    VkBufferView dummyTexelView = VK_NULL_HANDLE;
    VkDeviceSize bufferAlignment = 256;

    // Descriptors.
    std::vector<VkDescriptorPool> freePools, currentPools;
    std::unordered_map<std::uint64_t, VkDescriptorSet> setCache;
    std::uint32_t poolSetsLeft = 0;

    // Objects (index 0 is reserved as the GL "zero" object).
    std::vector<Buffer> buffers{1};
    std::vector<Texture> textures{1};
    std::vector<Renderbuffer> renderbuffers{1};
    std::vector<Framebuffer> framebuffers{1};
    std::vector<VertexArray> vertexArrays{1};
    std::vector<SamplerObject> samplerObjects{1};
    std::vector<ShaderObject> shaders{1};
    std::vector<Program> programs{1};
    std::vector<Query> queries{1};
    std::uint64_t programGenerations = 0;

    std::unordered_map<std::uint64_t, VkSampler> samplerCache;
    std::unordered_map<PipelineKey, VkPipeline, PipelineKeyHash> pipelines;
    std::array<GLuint, SlotCount> dummyTextures{};
    GLuint dummyShadow2D = 0;

    // Queries.
    VkQueryPool queryPool = VK_NULL_HANDLE;
    std::uint32_t queryCursor = 0;
    static constexpr std::uint32_t kQuerySlots = 8192;
    std::array<std::uint64_t, kQuerySlots> querySlotSerial{};

    GlState gl;

    // Dynamic state cache (per command buffer).
    struct Dynamic {
        bool valid = false;
        VkViewport viewport{};
        VkRect2D scissor{};
        VkCullModeFlags cull = 0;
        VkFrontFace front = VK_FRONT_FACE_CLOCKWISE;
        VkBool32 depthTest = 0, depthWrite = 0;
        VkCompareOp depthOp = VK_COMPARE_OP_LESS;
        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipeline pipeline = VK_NULL_HANDLE;
        float depthBiasConstant = 0, depthBiasSlope = 0;
    } dynamic;

    // Statistics.
    std::uint64_t frames = 0, submissions = 0, renderPasses = 0, draws = 0, descriptorSets = 0,
                  validationMessages = 0, bufferBytes = 0, imageBytes = 0;
    bool validation = false, trace = false;
};

extern Context *g;

// ---- core (VkCore.cpp) ----
Range AllocateBuffer(VkDeviceSize size);
void RetireRange(Range range, std::uint64_t serial);
void Retire(Retired::Kind kind, std::uint64_t handle, std::uint64_t serial);
void RetireImage(std::shared_ptr<Image> image);
std::shared_ptr<Image> CreateImage(VkImageType type, VkFormat format, std::uint32_t width,
                                   std::uint32_t height, std::uint32_t depth, std::uint32_t layers,
                                   std::uint32_t levels, bool attachment);
VkImageView CreateView(const Image &image, VkImageViewType type, std::uint32_t baseLevel,
                       std::uint32_t levels, std::uint32_t baseLayer, std::uint32_t layers);
void Transition(Image &image, VkImageLayout layout);
void EndPass();
void Flush(bool wait);
void PollCompleted(bool wait = false, std::uint64_t untilSerial = 0);
Range RingAllocate(VkDeviceSize size, VkDeviceSize alignment);
VkDescriptorSet AllocateSet(VkDescriptorSetLayout layout);
bool IsDepthFormat(VkFormat format);
VkImageAspectFlags AspectOf(VkFormat format);
void RecreateDefaultFramebuffer(int width, int height);
void MemoryBarrierAll();

// ---- shaders (VkShader.cpp) ----
void InstallShaderEntries(std::unordered_map<std::string, void (*)(void)> &table);
void DestroyProgramResources(Program &program);
Program *CurrentProgram();
void ShutdownShaderCompiler();

// ---- textures / framebuffers (VkTexture.cpp) ----
void InstallTextureEntries(std::unordered_map<std::string, void (*)(void)> &table);
Texture *GetTexture(GLuint name);
bool RealizeTexture(Texture &texture);
VkImageView SamplingView(Texture &texture);
VkSampler GetSampler(const SamplerState &state, bool mipmapped, bool shadow);
void CreateDummyTextures();
struct ResolvedAttachment {
    std::shared_ptr<Image> image;
    VkImageView view = VK_NULL_HANDLE;
    std::uint32_t width = 0, height = 0, layer = 0, level = 0;
};
bool ResolveAttachment(const Attachment &attachment, ResolvedAttachment &out);
bool ResolveFramebufferColor(GLuint fbo, int index, ResolvedAttachment &out);
bool ResolveFramebufferDepth(GLuint fbo, ResolvedAttachment &out);
void DestroyTextureObject(Texture &texture);

// ---- draw / buffers / state (VkDraw.cpp) ----
void InstallDrawEntries(std::unordered_map<std::string, void (*)(void)> &table);
Buffer *GetBuffer(GLuint name);
GLuint *BufferBinding(GLenum target);
void PrepareBufferWrite(Buffer &buffer, bool discard);
void BeginPass(bool clearColor, bool clearDepth);
void EnsurePass();
void ClearDrawScratch();

// ---- queries (VkInstall.cpp) ----
void InstallStateEntries(std::unordered_map<std::string, void (*)(void)> &table);

template <class T> GLuint AllocateName(std::vector<T> &objects) {
    for (std::size_t i = 1; i < objects.size(); ++i)
        if (!objects[i].alive) {
            objects[i] = T{};
            objects[i].alive = true;
            return GLuint(i);
        }
    objects.emplace_back();
    objects.back().alive = true;
    return GLuint(objects.size() - 1);
}
template <class T> T *Lookup(std::vector<T> &objects, GLuint name) {
    return name && name < objects.size() && objects[name].alive ? &objects[name] : nullptr;
}

} // namespace JudasVulkan::detail
