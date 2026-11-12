// Vulkan backend: shader and program objects. The renderer's GLSL is compiled
// unchanged to SPIR-V with glslang's relaxed Vulkan rules, which gather loose
// uniforms into one block. That block, the opaque uniforms and the interface
// blocks are then reflected from the generated SPIR-V itself, so the layout
// used on the CPU is exactly the layout the driver executes.
#include "VkCore.h"

#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>

#include <algorithm>
#include <cstdio>
#include <regex>

namespace JudasVulkan::detail {
namespace {

constexpr const char *kDefaultBlockName = "JudasDefaults";
constexpr unsigned kUniformShift = 0, kStorageShift = 64, kSamplerShift = 128, kImageShift = 256;

bool glslangReady = false;

EShLanguage StageOf(GLenum type) {
    return type == GL_VERTEX_SHADER ? EShLangVertex : type == GL_FRAGMENT_SHADER ? EShLangFragment
                                                                                 : EShLangCompute;
}
int ModuleIndex(GLenum type) { return type == GL_VERTEX_SHADER ? 0 : type == GL_FRAGMENT_SHADER ? 1 : 2; }

// Vulkan clip space has z in [0, w]; OpenGL has z in [-w, w]. The renderer's
// matrices and shaders are written for OpenGL. Devices with
// VK_EXT_depth_clip_control apply OpenGL's convention themselves. Otherwise
// every vertex shader's result is remapped after its unmodified main() has
// run: clipping and window depth are then mathematically identical to
// OpenGL's, differing only in floating-point rounding.
std::string AdaptSource(const std::string &source, GLenum type) {
    std::string text = source;
    static const std::regex version(R"(^\s*#version[^\n]*)");
    if (std::regex_search(text, version))
        text = std::regex_replace(text, version, "#version 450 core", std::regex_constants::format_first_only);
    else
        text = "#version 450 core\n" + text;
    if (type == GL_VERTEX_SHADER) {
        // The unmodified main() runs first. OpenGL rasterizes points at size 1
        // unless the shader's own size is enabled; Vulkan requires the vertex
        // shader to write it.
        static const std::regex entry(R"(\bvoid\s+main\s*\(\s*(void)?\s*\))");
        text = std::regex_replace(text, entry, "void judas_gl_main()");
        text += "\nvoid main(){judas_gl_main();";
        if (text.find("gl_PointSize") == std::string::npos) text += "gl_PointSize=1.0;";
        if (!g->depthClipControl) text += "gl_Position.z=(gl_Position.z+gl_Position.w)*0.5;";
        text += "}\n";
    }
    return text;
}

void ConfigureShader(glslang::TShader &shader, EShLanguage stage) {
    shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 100);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);
    shader.setEnvInputVulkanRulesRelaxed();
    shader.setAutoMapBindings(true);
    shader.setAutoMapLocations(true);
    shader.setGlobalUniformBlockName(kDefaultBlockName);
    shader.setGlobalUniformSet(0);
    shader.setGlobalUniformBinding(0);
    shader.setShiftBinding(glslang::EResUbo, kUniformShift);
    shader.setShiftBinding(glslang::EResSsbo, kStorageShift);
    shader.setShiftBinding(glslang::EResSampler, kSamplerShift);
    shader.setShiftBinding(glslang::EResTexture, kSamplerShift);
    shader.setShiftBinding(glslang::EResImage, kImageShift);
}

constexpr EShMessages kMessages = EShMessages(EShMsgSpvRules | EShMsgVulkanRules);

// ------------------------------------------------------------ reflection ---
struct SpvType {
    std::uint32_t op = 0;
    std::uint32_t width = 0, sign = 0;        // int/float
    std::uint32_t element = 0, count = 0;     // vector/matrix/array (count resolved)
    std::uint32_t dim = 0, depth = 0, arrayed = 0, sampled = 0; // image
    std::uint32_t storage = 0;                // pointer
    std::vector<std::uint32_t> members;       // struct
    bool runtime = false;
};
struct SpvModule {
    std::unordered_map<std::uint32_t, SpvType> types;
    std::unordered_map<std::uint32_t, std::string> names;
    std::unordered_map<std::uint64_t, std::string> memberNames;
    std::unordered_map<std::uint32_t, std::uint32_t> constants, location, binding, arrayStride;
    std::unordered_map<std::uint32_t, std::size_t> bindingWord; // id -> index of the Binding operand
    std::unordered_map<std::uint32_t, std::size_t> locationWord;
    std::unordered_map<std::uint32_t, bool> block, bufferBlock, builtin;
    std::unordered_map<std::uint64_t, std::uint32_t> memberOffset, memberMatrixStride;
    struct Variable { std::uint32_t id, type, storage; };
    std::vector<Variable> variables;
    std::array<int, 3> localSize{0, 0, 0};
};
std::uint64_t MemberKey(std::uint32_t id, std::uint32_t member) { return (std::uint64_t(id) << 32) | member; }

std::string SpvString(const std::uint32_t *words, std::size_t count) {
    std::string text;
    for (std::size_t i = 0; i < count; ++i)
        for (int byte = 0; byte < 4; ++byte) {
            const char c = char((words[i] >> (8 * byte)) & 0xFF);
            if (!c) return text;
            text.push_back(c);
        }
    return text;
}

void ParseSpirv(const std::vector<std::uint32_t> &code, SpvModule &module) {
    std::size_t cursor = 5;
    while (cursor < code.size()) {
        const std::uint32_t word = code[cursor];
        const std::uint32_t count = word >> 16, op = word & 0xFFFF;
        if (!count || cursor + count > code.size()) break;
        const std::uint32_t *w = &code[cursor];
        switch (op) {
        case 5: module.names[w[1]] = SpvString(w + 2, count - 2); break;
        case 6: module.memberNames[MemberKey(w[1], w[2])] = SpvString(w + 3, count - 3); break;
        case 16:
            if (count >= 6 && w[2] == 17) module.localSize = {int(w[3]), int(w[4]), int(w[5])};
            break;
        case 71:
            if (w[2] == 30) { module.location[w[1]] = w[3]; module.locationWord[w[1]] = cursor + 3; }
            else if (w[2] == 33) { module.binding[w[1]] = w[3]; module.bindingWord[w[1]] = cursor + 3; }
            else if (w[2] == 6) module.arrayStride[w[1]] = w[3];
            else if (w[2] == 2) module.block[w[1]] = true;
            else if (w[2] == 3) module.bufferBlock[w[1]] = true;
            else if (w[2] == 11) module.builtin[w[1]] = true;
            break;
        case 72:
            if (w[3] == 35) module.memberOffset[MemberKey(w[1], w[2])] = w[4];
            else if (w[3] == 7) module.memberMatrixStride[MemberKey(w[1], w[2])] = w[4];
            else if (w[3] == 11) module.builtin[w[1]] = true;
            break;
        case 20: module.types[w[1]].op = 20; break;
        case 21: { auto &t = module.types[w[1]]; t.op = 21; t.width = w[2]; t.sign = w[3]; break; }
        case 22: { auto &t = module.types[w[1]]; t.op = 22; t.width = w[2]; break; }
        case 23: case 24: { auto &t = module.types[w[1]]; t.op = op; t.element = w[2]; t.count = w[3]; break; }
        case 25: {
            auto &t = module.types[w[1]];
            t.op = 25; t.element = w[2]; t.dim = w[3]; t.depth = w[4]; t.arrayed = w[5]; t.sampled = w[7];
            break;
        }
        case 27: { auto &t = module.types[w[1]]; t.op = 27; t.element = w[2]; break; }
        case 28: {
            auto &t = module.types[w[1]];
            t.op = 28; t.element = w[2];
            const auto found = module.constants.find(w[3]);
            t.count = found == module.constants.end() ? 1 : found->second;
            break;
        }
        case 29: { auto &t = module.types[w[1]]; t.op = 28; t.element = w[2]; t.count = 1; t.runtime = true; break; }
        case 30: { auto &t = module.types[w[1]]; t.op = 30; t.members.assign(w + 2, w + count); break; }
        case 32: { auto &t = module.types[w[1]]; t.op = 32; t.storage = w[2]; t.element = w[3]; break; }
        case 43: if (count >= 4) module.constants[w[2]] = w[3]; break;
        case 59: module.variables.push_back({w[2], w[1], w[3]}); break;
        default: break;
        }
        cursor += count;
    }
}

struct Basic { UniformBase base; std::uint8_t columns, rows; };
bool BasicOf(const SpvModule &m, std::uint32_t id, Basic &out) {
    const auto found = m.types.find(id);
    if (found == m.types.end()) return false;
    const SpvType &t = found->second;
    if (t.op == 20) { out = {UniformBase::Bool, 1, 1}; return true; }
    if (t.op == 21) { out = {t.sign ? UniformBase::Int : UniformBase::UInt, 1, 1}; return true; }
    if (t.op == 22) { out = {UniformBase::Float, 1, 1}; return true; }
    if (t.op == 23) {
        if (!BasicOf(m, t.element, out)) return false;
        out.rows = std::uint8_t(t.count);
        return true;
    }
    if (t.op == 24) {
        if (!BasicOf(m, t.element, out)) return false;
        out.columns = std::uint8_t(t.count);
        return true;
    }
    return false;
}

std::uint32_t TypeBytes(const SpvModule &m, std::uint32_t id, std::uint32_t matrixStride) {
    const SpvType &t = m.types.at(id);
    Basic basic;
    switch (t.op) {
    case 20: case 21: case 22: return 4;
    case 23: return 4 * t.count;
    case 24:
        BasicOf(m, id, basic);
        return matrixStride ? matrixStride * (basic.columns - 1) + 4u * basic.rows : 4u * basic.rows * basic.columns;
    case 28: {
        const auto stride = m.arrayStride.find(id);
        return (stride == m.arrayStride.end() ? TypeBytes(m, t.element, matrixStride) : stride->second) * t.count;
    }
    case 30: {
        std::uint32_t end = 0;
        for (std::uint32_t i = 0; i < t.members.size(); ++i) {
            const auto offset = m.memberOffset.find(MemberKey(id, i));
            const auto stride = m.memberMatrixStride.find(MemberKey(id, i));
            const std::uint32_t base = offset == m.memberOffset.end() ? 0 : offset->second;
            end = std::max(end, base + TypeBytes(m, t.members[i], stride == m.memberMatrixStride.end() ? 0 : stride->second));
        }
        return (end + 15) / 16 * 16;
    }
    default: return 0;
    }
}

// Minimum buffer size of an interface block: the end of its last member, with
// a trailing unsized array counted as one element (GL_BUFFER_DATA_SIZE).
std::uint32_t BlockDataBytes(const SpvModule &m, std::uint32_t id) {
    const SpvType &t = m.types.at(id);
    std::uint32_t end = 0;
    for (std::uint32_t i = 0; i < t.members.size(); ++i) {
        const auto offset = m.memberOffset.find(MemberKey(id, i));
        const auto stride = m.memberMatrixStride.find(MemberKey(id, i));
        end = std::max(end, (offset == m.memberOffset.end() ? 0 : offset->second) +
                                TypeBytes(m, t.members[i], stride == m.memberMatrixStride.end() ? 0 : stride->second));
    }
    return end;
}

GLenum GlTypeOf(const Basic &b) {
    if (b.columns > 1) {
        if (b.columns == 4 && b.rows == 4) return GL_FLOAT_MAT4;
        if (b.columns == 3 && b.rows == 3) return GL_FLOAT_MAT3;
        if (b.columns == 2 && b.rows == 2) return GL_FLOAT_MAT2;
        return GL_FLOAT_MAT4;
    }
    static const GLenum floats[] = {GL_FLOAT, GL_FLOAT_VEC2, GL_FLOAT_VEC3, GL_FLOAT_VEC4};
    static const GLenum ints[] = {GL_INT, GL_INT_VEC2, GL_INT_VEC3, GL_INT_VEC4};
    static const GLenum uints[] = {GL_UNSIGNED_INT, GL_UNSIGNED_INT_VEC2, GL_UNSIGNED_INT_VEC3, GL_UNSIGNED_INT_VEC4};
    static const GLenum bools[] = {GL_BOOL, GL_BOOL_VEC2, GL_BOOL_VEC3, GL_BOOL_VEC4};
    const int index = std::clamp(int(b.rows), 1, 4) - 1;
    switch (b.base) {
    case UniformBase::Int: return ints[index];
    case UniformBase::UInt: return uints[index];
    case UniformBase::Bool: return bools[index];
    default: return floats[index];
    }
}

void AddUniform(Program &program, Uniform uniform) {
    for (const auto &existing : program.uniforms)
        if (existing.name == uniform.name) return;
    program.uniforms.push_back(std::move(uniform));
}

void FlattenBlock(const SpvModule &m, Program &program, const std::string &name, std::uint32_t typeId,
                  std::uint32_t offset, std::uint32_t matrixStride) {
    const SpvType &t = m.types.at(typeId);
    if (t.op == 30) {
        for (std::uint32_t i = 0; i < t.members.size(); ++i) {
            const auto memberName = m.memberNames.find(MemberKey(typeId, i));
            const auto memberOffset = m.memberOffset.find(MemberKey(typeId, i));
            const auto stride = m.memberMatrixStride.find(MemberKey(typeId, i));
            if (memberName == m.memberNames.end()) continue;
            FlattenBlock(m, program, name.empty() ? memberName->second : name + "." + memberName->second,
                         t.members[i], offset + (memberOffset == m.memberOffset.end() ? 0 : memberOffset->second),
                         stride == m.memberMatrixStride.end() ? 0 : stride->second);
        }
        return;
    }
    if (t.op == 28) {
        const SpvType &element = m.types.at(t.element);
        const auto strideFound = m.arrayStride.find(typeId);
        const std::uint32_t stride = strideFound == m.arrayStride.end() ? TypeBytes(m, t.element, matrixStride)
                                                                        : strideFound->second;
        if (element.op == 30 || element.op == 28) {
            for (std::uint32_t i = 0; i < t.count; ++i)
                FlattenBlock(m, program, name + "[" + std::to_string(i) + "]", t.element, offset + i * stride,
                             matrixStride);
            return;
        }
        Basic basic;
        if (!BasicOf(m, t.element, basic)) return;
        Uniform uniform;
        uniform.name = name;
        uniform.base = basic.base;
        uniform.columns = basic.columns;
        uniform.rows = basic.rows;
        uniform.glType = GlTypeOf(basic);
        uniform.offset = offset;
        uniform.arraySize = t.count;
        uniform.arrayStride = stride;
        uniform.matrixStride = matrixStride;
        uniform.isArray = true;
        AddUniform(program, std::move(uniform));
        return;
    }
    Basic basic;
    if (!BasicOf(m, typeId, basic)) return;
    Uniform uniform;
    uniform.name = name;
    uniform.base = basic.base;
    uniform.columns = basic.columns;
    uniform.rows = basic.rows;
    uniform.glType = GlTypeOf(basic);
    uniform.offset = offset;
    uniform.matrixStride = matrixStride;
    AddUniform(program, std::move(uniform));
}

void SamplerKind(const SpvModule &m, const SpvType &image, Uniform &uniform) {
    Basic sampled{UniformBase::Float, 1, 1};
    BasicOf(m, image.element, sampled);
    const bool shadow = image.depth == 1;
    uniform.samplerShadow = shadow;
    switch (image.dim) {
    case 1:
        uniform.samplerTarget = image.arrayed ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;
        uniform.glType = image.arrayed ? (shadow ? GL_SAMPLER_2D_ARRAY_SHADOW : GL_SAMPLER_2D_ARRAY)
                                       : (shadow ? GL_SAMPLER_2D_SHADOW : GL_SAMPLER_2D);
        break;
    case 2: uniform.samplerTarget = GL_TEXTURE_3D; uniform.glType = GL_SAMPLER_3D; break;
    case 3:
        uniform.samplerTarget = image.arrayed ? GL_TEXTURE_CUBE_MAP_ARRAY : GL_TEXTURE_CUBE_MAP;
        uniform.glType = image.arrayed ? GL_SAMPLER_CUBE_MAP_ARRAY : (shadow ? GL_SAMPLER_CUBE_SHADOW : GL_SAMPLER_CUBE);
        break;
    case 5:
        uniform.samplerTarget = GL_TEXTURE_BUFFER;
        uniform.samplerBuffer = true;
        uniform.glType = GL_SAMPLER_BUFFER;
        break;
    default: uniform.samplerTarget = GL_TEXTURE_2D; uniform.glType = GL_SAMPLER_2D; break;
    }
}

// glslang assigns automatic bindings independently per stage, so the same
// number can name different resources in the vertex and fragment modules.
// One descriptor set serves the whole program: give every resource a single
// program-wide binding (by name) and patch the generated decorations.
struct BindingPlan {
    std::unordered_map<std::string, std::uint32_t> assigned;
    std::uint32_t nextUniform = 1, nextStorage = kStorageShift, nextSampler = kSamplerShift;
    // Vertex-to-fragment varyings matched by name, as OpenGL links them.
    std::unordered_map<std::string, std::uint32_t> varyings;
    std::uint32_t nextVarying = 0;
};
std::uint32_t LocationSlots(const SpvModule &m, std::uint32_t id) {
    const auto found = m.types.find(id);
    if (found == m.types.end()) return 1;
    const SpvType &t = found->second;
    if (t.op == 24) return t.count;
    if (t.op == 28) return t.count * LocationSlots(m, t.element);
    if (t.op == 30) {
        std::uint32_t slots = 0;
        for (auto member : t.members) slots += LocationSlots(m, member);
        return std::max(slots, 1u);
    }
    return 1;
}
void UnifyVaryings(std::vector<std::uint32_t> &code, GLenum stage, BindingPlan &plan) {
    if (stage != GL_VERTEX_SHADER && stage != GL_FRAGMENT_SHADER) return;
    SpvModule m;
    ParseSpirv(code, m);
    const std::uint32_t wanted = stage == GL_VERTEX_SHADER ? 3u : 1u; // Output : Input
    for (const auto &variable : m.variables) {
        if (variable.storage != wanted) continue;
        const auto word = m.locationWord.find(variable.id);
        const auto pointer = m.types.find(variable.type);
        const auto name = m.names.find(variable.id);
        if (word == m.locationWord.end() || pointer == m.types.end() || name == m.names.end()) continue;
        auto found = plan.varyings.find(name->second);
        if (found == plan.varyings.end()) {
            found = plan.varyings.emplace(name->second, plan.nextVarying).first;
            plan.nextVarying += LocationSlots(m, pointer->second.element);
        }
        code[word->second] = found->second;
    }
}
void UnifyBindings(std::vector<std::uint32_t> &code, BindingPlan &plan) {
    SpvModule m;
    ParseSpirv(code, m);
    for (const auto &variable : m.variables) {
        const auto word = m.bindingWord.find(variable.id);
        const auto pointer = m.types.find(variable.type);
        if (word == m.bindingWord.end() || pointer == m.types.end()) continue;
        const std::uint32_t pointee = pointer->second.element;
        const auto type = m.types.find(pointee);
        if (type == m.types.end()) continue;
        std::string key;
        std::uint32_t *next = &plan.nextSampler;
        if (type->second.op == 30 && (variable.storage == 2 || variable.storage == 12)) {
            const auto name = m.names.find(pointee);
            key = "B:" + (name == m.names.end() ? std::to_string(pointee) : name->second);
            const bool storage = variable.storage == 12 || m.bufferBlock.count(pointee);
            if (!storage && key == std::string("B:") + kDefaultBlockName) {
                code[word->second] = 0;
                continue;
            }
            next = storage ? &plan.nextStorage : &plan.nextUniform;
        } else {
            const auto name = m.names.find(variable.id);
            key = "S:" + (name == m.names.end() ? std::to_string(variable.id) : name->second);
        }
        auto found = plan.assigned.find(key);
        if (found == plan.assigned.end()) found = plan.assigned.emplace(key, (*next)++).first;
        code[word->second] = found->second;
    }
}

// Transform feedback capture: decorate the requested vertex outputs with an
// interleaved layout in buffer 0 and declare the Xfb execution mode.
std::uint32_t PatchTransformFeedback(std::vector<std::uint32_t> &code, const std::vector<std::string> &names) {
    SpvModule m;
    ParseSpirv(code, m);
    std::vector<std::pair<std::uint32_t, std::uint32_t>> captured; // id, offset
    std::uint32_t stride = 0;
    for (const auto &name : names)
        for (const auto &variable : m.variables) {
            const auto found = m.names.find(variable.id);
            if (variable.storage != 3 || found == m.names.end() || found->second != name) continue;
            Basic basic;
            if (!BasicOf(m, m.types.at(variable.type).element, basic)) continue;
            captured.emplace_back(variable.id, stride);
            stride += 4u * basic.rows * basic.columns;
        }
    if (captured.empty()) return 0;
    std::size_t cursor = 5, entryEnd = 0, firstDecoration = 0;
    std::uint32_t entry = 0;
    while (cursor < code.size()) {
        const std::uint32_t count = code[cursor] >> 16, op = code[cursor] & 0xFFFF;
        if (!count) break;
        if (op == 15) { entry = code[cursor + 2]; entryEnd = cursor + count; }
        if ((op == 71 || op == 72) && !firstDecoration) firstDecoration = cursor;
        cursor += count;
    }
    if (!entryEnd || !firstDecoration) return 0;
    std::vector<std::uint32_t> decorations;
    for (const auto &[id, offset] : captured) {
        decorations.insert(decorations.end(), {(4u << 16) | 71u, id, 36u, 0u});      // XfbBuffer 0
        decorations.insert(decorations.end(), {(4u << 16) | 71u, id, 37u, stride});  // XfbStride
        decorations.insert(decorations.end(), {(4u << 16) | 71u, id, 35u, offset});  // Offset
    }
    code.insert(code.begin() + std::ptrdiff_t(firstDecoration), decorations.begin(), decorations.end());
    const std::uint32_t mode[] = {(3u << 16) | 16u, entry, 11u}; // OpExecutionMode Xfb
    code.insert(code.begin() + std::ptrdiff_t(entryEnd), mode, mode + 3);
    // TransformFeedback capability (53) must precede the other sections.
    const std::uint32_t capability[] = {(2u << 16) | 17u, 53u};
    code.insert(code.begin() + 5, capability, capability + 2);
    return stride;
}

void ReflectStage(const std::vector<std::uint32_t> &code, GLenum stage, Program &program,
                  const std::unordered_map<std::string, int> &declaredBindings) {
    SpvModule m;
    ParseSpirv(code, m);
    if (stage == GL_COMPUTE_SHADER) program.localSize = m.localSize;
    for (const auto &variable : m.variables) {
        const auto pointer = m.types.find(variable.type);
        if (pointer == m.types.end()) continue;
        const std::uint32_t pointee = pointer->second.element;
        const auto typeFound = m.types.find(pointee);
        if (typeFound == m.types.end()) continue;
        const SpvType &type = typeFound->second;
        const auto nameFound = m.names.find(variable.id);
        const std::string variableName = nameFound == m.names.end() ? std::string() : nameFound->second;
        const auto bindingFound = m.binding.find(variable.id);
        const std::uint32_t binding = bindingFound == m.binding.end() ? 0 : bindingFound->second;
        if (variable.storage == 2 || variable.storage == 12) { // Uniform / StorageBuffer
            if (type.op != 30) continue;
            const auto typeName = m.names.find(pointee);
            const std::string blockName = typeName == m.names.end() ? variableName : typeName->second;
            const bool storage = variable.storage == 12 || m.bufferBlock.count(pointee);
            if (!storage && blockName == kDefaultBlockName) {
                program.hasDefaultBlock = true;
                program.defaultBinding = binding;
                FlattenBlock(m, program, "", pointee, 0, 0);
                program.defaults.resize(std::max<std::size_t>(program.defaults.size(), TypeBytes(m, pointee, 0)));
                continue;
            }
            auto &list = storage ? program.storageBlocks : program.uniformBlocks;
            bool known = false;
            for (const auto &existing : list) known |= existing.name == blockName;
            if (known) continue;
            if (!storage) {
                // Introspection of named-block members (glGetUniformIndices).
                Program members;
                FlattenBlock(m, members, "", pointee, 0, 0);
                for (auto &member : members.uniforms) {
                    member.binding = std::uint32_t(list.size()); // owning block index
                    program.blockUniforms.push_back(std::move(member));
                }
            }
            BlockInfo info;
            info.name = blockName;
            info.binding = binding;
            info.size = storage ? BlockDataBytes(m, pointee) : TypeBytes(m, pointee, 0);
            info.storage = storage;
            const auto declared = declaredBindings.find(blockName);
            info.glBinding = declared != declaredBindings.end() && declared->second >= 0 ? GLuint(declared->second) : 0;
            list.push_back(std::move(info));
        } else if (variable.storage == 0) { // UniformConstant: opaque
            const SpvType *opaque = &type;
            std::uint32_t count = 1;
            bool array = false;
            if (opaque->op == 28) {
                count = opaque->count;
                array = true;
                opaque = &m.types.at(opaque->element);
            }
            if (opaque->op != 27) continue;
            const SpvType &image = m.types.at(opaque->element);
            Uniform uniform;
            uniform.name = variableName;
            uniform.base = UniformBase::Sampler;
            uniform.binding = binding;
            uniform.arraySize = count;
            uniform.isArray = array;
            uniform.units.assign(count, 0);
            SamplerKind(m, image, uniform);
            AddUniform(program, std::move(uniform));
        } else if (variable.storage == 1 && stage == GL_VERTEX_SHADER) { // Input
            if (m.builtin.count(variable.id) || m.builtin.count(pointee)) continue;
            const auto location = m.location.find(variable.id);
            if (location == m.location.end()) continue;
            Basic basic;
            if (!BasicOf(m, pointee, basic)) continue;
            for (std::uint32_t column = 0; column < basic.columns; ++column) {
                AttributeInfo attribute;
                attribute.location = location->second + column;
                attribute.base = basic.base;
                attribute.components = basic.rows;
                attribute.name = variableName;
                program.attributes.push_back(std::move(attribute));
            }
        } else if (variable.storage == 3 && stage == GL_FRAGMENT_SHADER) { // Output
            const auto location = m.location.find(variable.id);
            if (location != m.location.end() && location->second < 8) program.outputMask |= 1u << location->second;
        }
    }
}

void FinishReflection(Program &program) {
    GLint next = 0;
    program.locations.clear();
    program.locationTable.clear();
    program.samplerUniforms.clear();
    for (std::uint32_t index = 0; index < program.uniforms.size(); ++index) {
        Uniform &uniform = program.uniforms[index];
        uniform.location = next;
        next += GLint(uniform.arraySize);
        for (std::uint32_t element = 0; element < uniform.arraySize; ++element) {
            program.locationTable.emplace_back(index, element);
            if (uniform.isArray)
                program.locations[uniform.name + "[" + std::to_string(element) + "]"] = uniform.location + GLint(element);
        }
        program.locations[uniform.name] = uniform.location;
        if (uniform.base == UniformBase::Sampler) program.samplerUniforms.push_back(index);
    }
}

bool BuildLayouts(Program &program, std::string &error) {
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    const VkShaderStageFlags stages = program.compute ? VkShaderStageFlags(VK_SHADER_STAGE_COMPUTE_BIT)
                                                      : VkShaderStageFlags(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    program.dynamicBindings.clear();
    const auto add = [&](std::uint32_t binding, VkDescriptorType type, std::uint32_t count) {
        for (const auto &existing : bindings)
            if (existing.binding == binding) return false;
        VkDescriptorSetLayoutBinding entry{};
        entry.binding = binding;
        entry.descriptorType = type;
        entry.descriptorCount = count;
        entry.stageFlags = stages;
        bindings.push_back(entry);
        return true;
    };
    bool ok = true;
    if (program.hasDefaultBlock) {
        ok &= add(program.defaultBinding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1);
        program.dynamicBindings.push_back(program.defaultBinding);
    }
    for (const auto &block : program.uniformBlocks) {
        ok &= add(block.binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1);
        program.dynamicBindings.push_back(block.binding);
    }
    for (const auto &block : program.storageBlocks) {
        ok &= add(block.binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1);
        program.dynamicBindings.push_back(block.binding);
    }
    for (const auto index : program.samplerUniforms) {
        const Uniform &uniform = program.uniforms[index];
        ok &= add(uniform.binding, uniform.samplerBuffer ? VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER
                                                         : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                  uniform.arraySize);
    }
    if (!ok) {
        error = "shader resources were assigned overlapping Vulkan bindings";
        return false;
    }
    std::sort(program.dynamicBindings.begin(), program.dynamicBindings.end());
    VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.bindingCount = std::uint32_t(bindings.size());
    layoutInfo.pBindings = bindings.data();
    if (vkCreateDescriptorSetLayout(g->device, &layoutInfo, nullptr, &program.setLayout) != VK_SUCCESS) {
        error = "cannot create the descriptor set layout";
        return false;
    }
    VkPipelineLayoutCreateInfo pipelineLayout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayout.setLayoutCount = 1;
    pipelineLayout.pSetLayouts = &program.setLayout;
    if (vkCreatePipelineLayout(g->device, &pipelineLayout, nullptr, &program.layout) != VK_SUCCESS) {
        error = "cannot create the pipeline layout";
        return false;
    }
    return true;
}

void ResetLink(Program &program) {
    const std::uint64_t serial = g->serial;
    for (auto &module : program.modules) {
        Retire(Retired::ModuleKind, std::uint64_t(module), serial);
        module = VK_NULL_HANDLE;
    }
    Retire(Retired::PipelineKind, std::uint64_t(program.computePipeline), serial);
    Retire(Retired::LayoutKind, std::uint64_t(program.layout), serial);
    Retire(Retired::SetLayoutKind, std::uint64_t(program.setLayout), serial);
    program.computePipeline = VK_NULL_HANDLE;
    program.layout = VK_NULL_HANDLE;
    program.setLayout = VK_NULL_HANDLE;
    // Graphics pipelines of the old link are unreachable through its generation;
    // retire them now rather than at device shutdown.
    for (auto it = g->pipelines.begin(); it != g->pipelines.end();) {
        if (program.generation && it->first.program == program.generation) {
            Retire(Retired::PipelineKind, std::uint64_t(it->second), serial);
            it = g->pipelines.erase(it);
        } else
            ++it;
    }
    program.uniforms.clear();
    program.locations.clear();
    program.locationTable.clear();
    program.uniformBlocks.clear();
    program.blockUniforms.clear();
    program.feedbackStride = 0;
    program.storageBlocks.clear();
    program.attributes.clear();
    program.samplerUniforms.clear();
    program.defaults.clear();
    program.hasDefaultBlock = false;
    program.outputMask = 0;
    program.linked = false;
    program.defaultsDirty = true;
}

bool Link(Program &program) {
    ResetLink(program);
    program.log.clear();
    if (!glslangReady) {
        glslang::InitializeProcess();
        glslangReady = true;
    }
    std::vector<std::unique_ptr<glslang::TShader>> shaders;
    std::vector<std::string> sources;
    std::vector<GLenum> types;
    for (GLuint name : program.attached) {
        ShaderObject *shader = Lookup(g->shaders, name);
        if (!shader || !shader->compiled) {
            program.log = "an attached shader is not successfully compiled";
            return false;
        }
        sources.push_back(AdaptSource(shader->source, shader->type));
        types.push_back(shader->type);
    }
    if (sources.empty()) {
        program.log = "no shaders attached";
        return false;
    }
    program.compute = types.size() == 1 && types[0] == GL_COMPUTE_SHADER;
    glslang::TProgram linker;
    for (std::size_t i = 0; i < sources.size(); ++i) {
        auto shader = std::make_unique<glslang::TShader>(StageOf(types[i]));
        const char *text = sources[i].c_str();
        shader->setStrings(&text, 1);
        ConfigureShader(*shader, StageOf(types[i]));
        if (!shader->parse(GetDefaultResources(), 450, false, kMessages)) {
            program.log = shader->getInfoLog();
            return false;
        }
        linker.addShader(shader.get());
        shaders.push_back(std::move(shader));
    }
    if (!linker.link(kMessages)) {
        program.log = linker.getInfoLog();
        return false;
    }
    // Declared interface-block bindings are OpenGL binding points; record them
    // before Vulkan binding assignment rewrites the qualifiers.
    std::unordered_map<std::string, int> declared;
    if (linker.buildReflection(EShReflectionSeparateBuffers | EShReflectionAllBlockVariables)) {
        for (int i = 0; i < linker.getNumUniformBlocks(); ++i)
            declared[linker.getUniformBlock(i).name] = linker.getUniformBlock(i).getBinding();
        for (int i = 0; i < linker.getNumBufferBlocks(); ++i)
            declared[linker.getBufferBlock(i).name] = linker.getBufferBlock(i).getBinding();
    }
    if (!linker.mapIO()) {
        program.log = linker.getInfoLog();
        return false;
    }
    BindingPlan plan;
    for (std::size_t i = 0; i < types.size(); ++i) {
        std::vector<std::uint32_t> code;
        glslang::SpvOptions options;
        options.disableOptimizer = true;
        glslang::GlslangToSpv(*linker.getIntermediate(StageOf(types[i])), code, &options);
        if (code.empty()) {
            program.log = "SPIR-V generation produced no code";
            return false;
        }
        UnifyBindings(code, plan);
        UnifyVaryings(code, types[i], plan);
        if (types[i] == GL_VERTEX_SHADER && !program.feedbackVaryings.empty()) {
            if (!g->transformFeedback) {
                program.log = "transform feedback capture needs VK_EXT_transform_feedback on this device";
                return false;
            }
            program.feedbackStride = PatchTransformFeedback(code, program.feedbackVaryings);
        }
        VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        info.codeSize = code.size() * sizeof(std::uint32_t);
        info.pCode = code.data();
        if (vkCreateShaderModule(g->device, &info, nullptr, &program.modules[ModuleIndex(types[i])]) != VK_SUCCESS) {
            program.log = "vkCreateShaderModule failed";
            return false;
        }
        ReflectStage(code, types[i], program, declared);
        if (g->trace) {
            static int dumped = 0;
            char path[64];
            std::snprintf(path, sizeof(path), "/tmp/judas-vk-%d.spv", dumped++);
            if (FILE *file = std::fopen(path, "wb")) {
                std::fwrite(code.data(), 4, code.size(), file);
                std::fclose(file);
            }
        }
    }
    FinishReflection(program);
    std::string error;
    if (!BuildLayouts(program, error)) {
        program.log = error;
        return false;
    }
    if (program.compute) {
        VkComputePipelineCreateInfo info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        info.stage.module = program.modules[2];
        info.stage.pName = "main";
        info.layout = program.layout;
        if (vkCreateComputePipelines(g->device, VK_NULL_HANDLE, 1, &info, nullptr, &program.computePipeline) != VK_SUCCESS) {
            program.log = "vkCreateComputePipelines failed";
            return false;
        }
    }
    program.generation = ++g->programGenerations;
    program.linked = true;
    program.defaultsDirty = true;
    return true;
}

// ----------------------------------------------------------- entry points ---
GLuint GLAD_API_PTR CreateShader(GLenum type) {
    const GLuint name = AllocateName(g->shaders);
    g->shaders[name].type = type;
    return name;
}
void GLAD_API_PTR ShaderSource(GLuint shader, GLsizei count, const GLchar *const *strings, const GLint *lengths) {
    ShaderObject *object = Lookup(g->shaders, shader);
    if (!object) return;
    object->source.clear();
    for (GLsizei i = 0; i < count; ++i) {
        if (!strings[i]) continue;
        if (lengths && lengths[i] >= 0) object->source.append(strings[i], std::size_t(lengths[i]));
        else object->source.append(strings[i]);
    }
}
void GLAD_API_PTR CompileShader(GLuint shader) {
    ShaderObject *object = Lookup(g->shaders, shader);
    if (!object) return;
    if (!glslangReady) {
        glslang::InitializeProcess();
        glslangReady = true;
    }
    const std::string adapted = AdaptSource(object->source, object->type);
    glslang::TShader parser(StageOf(object->type));
    const char *text = adapted.c_str();
    parser.setStrings(&text, 1);
    ConfigureShader(parser, StageOf(object->type));
    object->compiled = parser.parse(GetDefaultResources(), 450, false, kMessages);
    object->log = object->compiled ? std::string() : std::string(parser.getInfoLog());
}
void GLAD_API_PTR GetShaderiv(GLuint shader, GLenum pname, GLint *value) {
    ShaderObject *object = Lookup(g->shaders, shader);
    if (!object || !value) return;
    switch (pname) {
    case GL_COMPILE_STATUS: *value = object->compiled; break;
    case GL_INFO_LOG_LENGTH: *value = object->log.empty() ? 0 : GLint(object->log.size() + 1); break;
    case GL_SHADER_TYPE: *value = GLint(object->type); break;
    case GL_DELETE_STATUS: *value = object->deletePending; break;
    case GL_SHADER_SOURCE_LENGTH: *value = GLint(object->source.size() + 1); break;
    default: *value = 0; break;
    }
}
void CopyLog(const std::string &log, GLsizei capacity, GLsizei *length, GLchar *out) {
    const GLsizei written = capacity > 0 ? std::min<GLsizei>(capacity - 1, GLsizei(log.size())) : 0;
    if (out && capacity > 0) {
        std::memcpy(out, log.data(), std::size_t(written));
        out[written] = 0;
    }
    if (length) *length = written;
}
void GLAD_API_PTR GetShaderInfoLog(GLuint shader, GLsizei capacity, GLsizei *length, GLchar *out) {
    ShaderObject *object = Lookup(g->shaders, shader);
    CopyLog(object ? object->log : std::string(), capacity, length, out);
}
void ReleaseShader(GLuint shader) {
    ShaderObject *object = Lookup(g->shaders, shader);
    if (object && object->deletePending && object->attachCount <= 0) *object = ShaderObject{};
}
void GLAD_API_PTR DeleteShader(GLuint shader) {
    ShaderObject *object = Lookup(g->shaders, shader);
    if (!object) return;
    object->deletePending = true;
    ReleaseShader(shader);
}
GLboolean GLAD_API_PTR IsShader(GLuint shader) { return Lookup(g->shaders, shader) != nullptr; }

GLuint GLAD_API_PTR CreateProgram() { return AllocateName(g->programs); }
void GLAD_API_PTR AttachShader(GLuint program, GLuint shader) {
    Program *object = Lookup(g->programs, program);
    ShaderObject *attached = Lookup(g->shaders, shader);
    if (!object || !attached) return;
    object->attached.push_back(shader);
    ++attached->attachCount;
}
void GLAD_API_PTR DetachShader(GLuint program, GLuint shader) {
    Program *object = Lookup(g->programs, program);
    if (!object) return;
    const auto found = std::find(object->attached.begin(), object->attached.end(), shader);
    if (found == object->attached.end()) return;
    object->attached.erase(found);
    if (ShaderObject *attached = Lookup(g->shaders, shader)) --attached->attachCount;
    ReleaseShader(shader);
}
void GLAD_API_PTR LinkProgram(GLuint program) {
    Program *object = Lookup(g->programs, program);
    if (!object) return;
    if (!Link(*object)) {
        ResetLink(*object);
        if (object->log.empty()) object->log = "link failed";
    }
}
void GLAD_API_PTR ValidateProgram(GLuint) {}
void GLAD_API_PTR GetProgramiv(GLuint program, GLenum pname, GLint *value) {
    Program *object = Lookup(g->programs, program);
    if (!object || !value) return;
    switch (pname) {
    case GL_LINK_STATUS: case GL_VALIDATE_STATUS: *value = object->linked; break;
    case GL_INFO_LOG_LENGTH: *value = object->log.empty() ? 0 : GLint(object->log.size() + 1); break;
    case GL_DELETE_STATUS: *value = object->deletePending; break;
    case GL_ATTACHED_SHADERS: *value = GLint(object->attached.size()); break;
    case GL_ACTIVE_UNIFORMS: *value = GLint(object->uniforms.size()); break;
    case GL_ACTIVE_UNIFORM_BLOCKS: *value = GLint(object->uniformBlocks.size()); break;
    case GL_ACTIVE_ATTRIBUTES: *value = GLint(object->attributes.size()); break;
    case GL_COMPUTE_WORK_GROUP_SIZE:
        value[0] = object->localSize[0]; value[1] = object->localSize[1]; value[2] = object->localSize[2];
        break;
    case GL_ACTIVE_UNIFORM_MAX_LENGTH: {
        std::size_t longest = 0;
        for (const auto &uniform : object->uniforms) longest = std::max(longest, uniform.name.size() + 4);
        *value = GLint(longest);
        break;
    }
    default: *value = 0; break;
    }
}
void GLAD_API_PTR GetProgramInfoLog(GLuint program, GLsizei capacity, GLsizei *length, GLchar *out) {
    Program *object = Lookup(g->programs, program);
    CopyLog(object ? object->log : std::string(), capacity, length, out);
}
void GLAD_API_PTR DeleteProgram(GLuint program) {
    Program *object = Lookup(g->programs, program);
    if (!object) return;
    if (g->gl.program == program) {
        object->deletePending = true; // deleted when no longer current
        return;
    }
    for (GLuint shader : object->attached) {
        if (ShaderObject *attached = Lookup(g->shaders, shader)) --attached->attachCount;
        ReleaseShader(shader);
    }
    ResetLink(*object);
    *object = Program{};
}
void GLAD_API_PTR UseProgram(GLuint program) {
    const GLuint previous = g->gl.program;
    g->gl.program = program;
    if (previous && previous != program) {
        Program *old = Lookup(g->programs, previous);
        if (old && old->deletePending) DeleteProgram(previous);
    }
}
GLboolean GLAD_API_PTR IsProgram(GLuint program) { return Lookup(g->programs, program) != nullptr; }

GLint GLAD_API_PTR GetUniformLocation(GLuint program, const GLchar *name) {
    Program *object = Lookup(g->programs, program);
    if (!object || !object->linked || !name) return -1;
    const auto found = object->locations.find(name);
    return found == object->locations.end() ? -1 : found->second;
}
GLint GLAD_API_PTR GetAttribLocation(GLuint program, const GLchar *name) {
    Program *object = Lookup(g->programs, program);
    if (!object || !name) return -1;
    for (const auto &attribute : object->attributes)
        if (attribute.name == name) return GLint(attribute.location);
    return -1;
}

// Writes `count` elements of `components` scalars. `kind`: 0 float, 1 int, 2 uint.
void SetUniform(Program *program, GLint location, int kind, int components, GLsizei count, const void *data) {
    if (!program || location < 0 || std::size_t(location) >= program->locationTable.size() || !data) return;
    const auto [index, element] = program->locationTable[std::size_t(location)];
    Uniform &uniform = program->uniforms[index];
    const std::uint32_t available = uniform.arraySize - element;
    const std::uint32_t elements = std::min<std::uint32_t>(std::uint32_t(std::max<GLsizei>(count, 0)), available);
    if (uniform.base == UniformBase::Sampler) {
        for (std::uint32_t i = 0; i < elements; ++i) {
            GLint unit = 0;
            if (kind == 0) unit = GLint(static_cast<const float *>(data)[i * components]);
            else unit = static_cast<const GLint *>(data)[i * components];
            uniform.units[element + i] = unit;
        }
        return;
    }
    const int rows = std::min<int>(components, uniform.rows);
    const std::uint32_t stride = uniform.arrayStride ? uniform.arrayStride : 4u * uniform.rows;
    for (std::uint32_t i = 0; i < elements; ++i) {
        const std::size_t destination = uniform.offset + std::size_t(element + i) * stride;
        if (destination + std::size_t(rows) * 4 > program->defaults.size()) return;
        std::uint8_t *out = program->defaults.data() + destination;
        for (int c = 0; c < rows; ++c) {
            const std::size_t source = std::size_t(i) * components + c;
            std::uint32_t bits = 0;
            float f = 0;
            std::int64_t integer = 0;
            if (kind == 0) { f = static_cast<const float *>(data)[source]; integer = std::int64_t(f); }
            else if (kind == 1) { integer = static_cast<const GLint *>(data)[source]; f = float(integer); }
            else { integer = static_cast<const GLuint *>(data)[source]; f = float(integer); }
            switch (uniform.base) {
            case UniformBase::Float: std::memcpy(&bits, &f, 4); break;
            case UniformBase::Bool: bits = (kind == 0 ? f != 0.0f : integer != 0) ? 1u : 0u; break;
            default: bits = std::uint32_t(integer); break;
            }
            std::memcpy(out + c * 4, &bits, 4);
        }
    }
    program->defaultsDirty = true;
}
void SetMatrix(Program *program, GLint location, int dimension, GLsizei count, GLboolean transpose, const GLfloat *data) {
    if (!program || location < 0 || std::size_t(location) >= program->locationTable.size() || !data) return;
    const auto [index, element] = program->locationTable[std::size_t(location)];
    Uniform &uniform = program->uniforms[index];
    if (uniform.columns != dimension || uniform.rows != dimension) return;
    const std::uint32_t columnStride = uniform.matrixStride ? uniform.matrixStride : 4u * dimension;
    const std::uint32_t stride = uniform.arrayStride ? uniform.arrayStride : columnStride * dimension;
    const std::uint32_t elements = std::min<std::uint32_t>(std::uint32_t(std::max<GLsizei>(count, 0)), uniform.arraySize - element);
    for (std::uint32_t i = 0; i < elements; ++i) {
        const std::size_t base = uniform.offset + std::size_t(element + i) * stride;
        if (base + std::size_t(columnStride) * (dimension - 1) + 4u * dimension > program->defaults.size()) return;
        for (int column = 0; column < dimension; ++column)
            for (int row = 0; row < dimension; ++row) {
                const float value = transpose ? data[i * dimension * dimension + row * dimension + column]
                                              : data[i * dimension * dimension + column * dimension + row];
                std::memcpy(program->defaults.data() + base + column * columnStride + row * 4, &value, 4);
            }
    }
    program->defaultsDirty = true;
}
Program *Active() { return Lookup(g->programs, g->gl.program); }

void GLAD_API_PTR Uniform1i(GLint l, GLint a) { SetUniform(Active(), l, 1, 1, 1, &a); }
void GLAD_API_PTR Uniform2i(GLint l, GLint a, GLint b) { const GLint v[] = {a, b}; SetUniform(Active(), l, 1, 2, 1, v); }
void GLAD_API_PTR Uniform3i(GLint l, GLint a, GLint b, GLint c) { const GLint v[] = {a, b, c}; SetUniform(Active(), l, 1, 3, 1, v); }
void GLAD_API_PTR Uniform4i(GLint l, GLint a, GLint b, GLint c, GLint d) { const GLint v[] = {a, b, c, d}; SetUniform(Active(), l, 1, 4, 1, v); }
void GLAD_API_PTR Uniform1ui(GLint l, GLuint a) { SetUniform(Active(), l, 2, 1, 1, &a); }
void GLAD_API_PTR Uniform2ui(GLint l, GLuint a, GLuint b) { const GLuint v[] = {a, b}; SetUniform(Active(), l, 2, 2, 1, v); }
void GLAD_API_PTR Uniform3ui(GLint l, GLuint a, GLuint b, GLuint c) { const GLuint v[] = {a, b, c}; SetUniform(Active(), l, 2, 3, 1, v); }
void GLAD_API_PTR Uniform4ui(GLint l, GLuint a, GLuint b, GLuint c, GLuint d) { const GLuint v[] = {a, b, c, d}; SetUniform(Active(), l, 2, 4, 1, v); }
void GLAD_API_PTR Uniform1f(GLint l, GLfloat a) { SetUniform(Active(), l, 0, 1, 1, &a); }
void GLAD_API_PTR Uniform2f(GLint l, GLfloat a, GLfloat b) { const GLfloat v[] = {a, b}; SetUniform(Active(), l, 0, 2, 1, v); }
void GLAD_API_PTR Uniform3f(GLint l, GLfloat a, GLfloat b, GLfloat c) { const GLfloat v[] = {a, b, c}; SetUniform(Active(), l, 0, 3, 1, v); }
void GLAD_API_PTR Uniform4f(GLint l, GLfloat a, GLfloat b, GLfloat c, GLfloat d) { const GLfloat v[] = {a, b, c, d}; SetUniform(Active(), l, 0, 4, 1, v); }
void GLAD_API_PTR Uniform1iv(GLint l, GLsizei n, const GLint *v) { SetUniform(Active(), l, 1, 1, n, v); }
void GLAD_API_PTR Uniform2iv(GLint l, GLsizei n, const GLint *v) { SetUniform(Active(), l, 1, 2, n, v); }
void GLAD_API_PTR Uniform3iv(GLint l, GLsizei n, const GLint *v) { SetUniform(Active(), l, 1, 3, n, v); }
void GLAD_API_PTR Uniform4iv(GLint l, GLsizei n, const GLint *v) { SetUniform(Active(), l, 1, 4, n, v); }
void GLAD_API_PTR Uniform1uiv(GLint l, GLsizei n, const GLuint *v) { SetUniform(Active(), l, 2, 1, n, v); }
void GLAD_API_PTR Uniform2uiv(GLint l, GLsizei n, const GLuint *v) { SetUniform(Active(), l, 2, 2, n, v); }
void GLAD_API_PTR Uniform3uiv(GLint l, GLsizei n, const GLuint *v) { SetUniform(Active(), l, 2, 3, n, v); }
void GLAD_API_PTR Uniform4uiv(GLint l, GLsizei n, const GLuint *v) { SetUniform(Active(), l, 2, 4, n, v); }
void GLAD_API_PTR Uniform1fv(GLint l, GLsizei n, const GLfloat *v) { SetUniform(Active(), l, 0, 1, n, v); }
void GLAD_API_PTR Uniform2fv(GLint l, GLsizei n, const GLfloat *v) { SetUniform(Active(), l, 0, 2, n, v); }
void GLAD_API_PTR Uniform3fv(GLint l, GLsizei n, const GLfloat *v) { SetUniform(Active(), l, 0, 3, n, v); }
void GLAD_API_PTR Uniform4fv(GLint l, GLsizei n, const GLfloat *v) { SetUniform(Active(), l, 0, 4, n, v); }
void GLAD_API_PTR UniformMatrix2fv(GLint l, GLsizei n, GLboolean t, const GLfloat *v) { SetMatrix(Active(), l, 2, n, t, v); }
void GLAD_API_PTR UniformMatrix3fv(GLint l, GLsizei n, GLboolean t, const GLfloat *v) { SetMatrix(Active(), l, 3, n, t, v); }
void GLAD_API_PTR UniformMatrix4fv(GLint l, GLsizei n, GLboolean t, const GLfloat *v) { SetMatrix(Active(), l, 4, n, t, v); }

void GLAD_API_PTR ProgramUniform1i(GLuint p, GLint l, GLint a) { SetUniform(Lookup(g->programs, p), l, 1, 1, 1, &a); }
void GLAD_API_PTR ProgramUniform1ui(GLuint p, GLint l, GLuint a) { SetUniform(Lookup(g->programs, p), l, 2, 1, 1, &a); }
void GLAD_API_PTR ProgramUniform1f(GLuint p, GLint l, GLfloat a) { SetUniform(Lookup(g->programs, p), l, 0, 1, 1, &a); }
void GLAD_API_PTR ProgramUniform2ui(GLuint p, GLint l, GLuint a, GLuint b) { const GLuint v[] = {a, b}; SetUniform(Lookup(g->programs, p), l, 2, 2, 1, v); }
void GLAD_API_PTR ProgramUniform2f(GLuint p, GLint l, GLfloat a, GLfloat b) { const GLfloat v[] = {a, b}; SetUniform(Lookup(g->programs, p), l, 0, 2, 1, v); }
void GLAD_API_PTR ProgramUniform3f(GLuint p, GLint l, GLfloat a, GLfloat b, GLfloat c) { const GLfloat v[] = {a, b, c}; SetUniform(Lookup(g->programs, p), l, 0, 3, 1, v); }
void GLAD_API_PTR ProgramUniform4f(GLuint p, GLint l, GLfloat a, GLfloat b, GLfloat c, GLfloat d) { const GLfloat v[] = {a, b, c, d}; SetUniform(Lookup(g->programs, p), l, 0, 4, 1, v); }
void GLAD_API_PTR ProgramUniformMatrix4fv(GLuint p, GLint l, GLsizei n, GLboolean t, const GLfloat *v) { SetMatrix(Lookup(g->programs, p), l, 4, n, t, v); }

GLuint GLAD_API_PTR GetUniformBlockIndex(GLuint program, const GLchar *name) {
    Program *object = Lookup(g->programs, program);
    if (!object || !name) return GL_INVALID_INDEX;
    for (std::size_t i = 0; i < object->uniformBlocks.size(); ++i)
        if (object->uniformBlocks[i].name == name) return GLuint(i);
    return GL_INVALID_INDEX;
}
void GLAD_API_PTR GetActiveUniformBlockiv(GLuint program, GLuint index, GLenum pname, GLint *value) {
    Program *object = Lookup(g->programs, program);
    if (!object || index >= object->uniformBlocks.size() || !value) return;
    const BlockInfo &block = object->uniformBlocks[index];
    switch (pname) {
    case GL_UNIFORM_BLOCK_DATA_SIZE: *value = GLint(block.size); break;
    case GL_UNIFORM_BLOCK_BINDING: *value = GLint(block.glBinding); break;
    case GL_UNIFORM_BLOCK_NAME_LENGTH: *value = GLint(block.name.size() + 1); break;
    default: *value = 0; break;
    }
}
void GLAD_API_PTR UniformBlockBinding(GLuint program, GLuint index, GLuint binding) {
    Program *object = Lookup(g->programs, program);
    if (object && index < object->uniformBlocks.size()) object->uniformBlocks[index].glBinding = binding;
}
void GLAD_API_PTR ShaderStorageBlockBinding(GLuint program, GLuint index, GLuint binding) {
    Program *object = Lookup(g->programs, program);
    if (object && index < object->storageBlocks.size()) object->storageBlocks[index].glBinding = binding;
}
void GLAD_API_PTR GetUniformIndices(GLuint program, GLsizei count, const GLchar *const *names, GLuint *indices) {
    Program *object = Lookup(g->programs, program);
    for (GLsizei i = 0; i < count; ++i) {
        indices[i] = GL_INVALID_INDEX;
        if (!object || !names[i]) continue;
        std::string name = names[i];
        if (name.size() > 3 && name.compare(name.size() - 3, 3, "[0]") == 0) name.resize(name.size() - 3);
        for (std::size_t u = 0; u < object->uniforms.size(); ++u)
            if (object->uniforms[u].name == name) indices[i] = GLuint(u);
        for (std::size_t u = 0; u < object->blockUniforms.size(); ++u)
            if (object->blockUniforms[u].name == name) indices[i] = GLuint(object->uniforms.size() + u);
    }
}
void GLAD_API_PTR GetActiveUniformsiv(GLuint program, GLsizei count, const GLuint *indices, GLenum pname, GLint *values) {
    Program *object = Lookup(g->programs, program);
    for (GLsizei i = 0; i < count; ++i) {
        values[i] = 0;
        if (!object || indices[i] >= object->uniforms.size() + object->blockUniforms.size()) continue;
        const bool inBlock = indices[i] >= object->uniforms.size();
        const Uniform &uniform = inBlock ? object->blockUniforms[indices[i] - object->uniforms.size()]
                                         : object->uniforms[indices[i]];
        switch (pname) {
        case GL_UNIFORM_TYPE: values[i] = GLint(uniform.glType); break;
        case GL_UNIFORM_SIZE: values[i] = GLint(uniform.arraySize); break;
        // Default-block uniforms have no block offset in OpenGL.
        case GL_UNIFORM_OFFSET: values[i] = inBlock ? GLint(uniform.offset) : -1; break;
        case GL_UNIFORM_BLOCK_INDEX: values[i] = inBlock ? GLint(uniform.binding) : -1; break;
        case GL_UNIFORM_ARRAY_STRIDE: values[i] = inBlock ? GLint(uniform.arrayStride) : -1; break;
        case GL_UNIFORM_MATRIX_STRIDE: values[i] = inBlock ? GLint(uniform.matrixStride) : -1; break;
        case GL_UNIFORM_NAME_LENGTH: values[i] = GLint(uniform.name.size() + 1); break;
        default: break;
        }
    }
}
void GLAD_API_PTR GetActiveUniform(GLuint program, GLuint index, GLsizei capacity, GLsizei *length, GLint *size, GLenum *type, GLchar *name) {
    Program *object = Lookup(g->programs, program);
    if (!object || index >= object->uniforms.size()) return;
    const Uniform &uniform = object->uniforms[index];
    if (size) *size = GLint(uniform.arraySize);
    if (type) *type = uniform.glType;
    CopyLog(uniform.isArray ? uniform.name + "[0]" : uniform.name, capacity, length, name);
}
void GLAD_API_PTR GetProgramInterfaceiv(GLuint program, GLenum interface, GLenum pname, GLint *value) {
    Program *object = Lookup(g->programs, program);
    if (!object || !value) return;
    *value = 0;
    if (pname != GL_ACTIVE_RESOURCES) return;
    if (interface == GL_SHADER_STORAGE_BLOCK) *value = GLint(object->storageBlocks.size());
    else if (interface == GL_UNIFORM_BLOCK) *value = GLint(object->uniformBlocks.size());
    else if (interface == GL_UNIFORM) *value = GLint(object->uniforms.size());
}
GLuint GLAD_API_PTR GetProgramResourceIndex(GLuint program, GLenum interface, const GLchar *name) {
    Program *object = Lookup(g->programs, program);
    if (!object || !name) return GL_INVALID_INDEX;
    const auto *list = interface == GL_SHADER_STORAGE_BLOCK ? &object->storageBlocks
                       : interface == GL_UNIFORM_BLOCK      ? &object->uniformBlocks
                                                            : nullptr;
    if (!list) return GL_INVALID_INDEX;
    for (std::size_t i = 0; i < list->size(); ++i)
        if ((*list)[i].name == name) return GLuint(i);
    return GL_INVALID_INDEX;
}
void GLAD_API_PTR GetProgramResourceiv(GLuint program, GLenum interface, GLuint index, GLsizei propertyCount,
                                       const GLenum *properties, GLsizei capacity, GLsizei *length, GLint *values) {
    Program *object = Lookup(g->programs, program);
    if (length) *length = 0;
    if (!object) return;
    const auto *list = interface == GL_SHADER_STORAGE_BLOCK ? &object->storageBlocks
                       : interface == GL_UNIFORM_BLOCK      ? &object->uniformBlocks
                                                            : nullptr;
    if (!list || index >= list->size()) return;
    const BlockInfo &block = (*list)[index];
    GLsizei written = 0;
    for (GLsizei i = 0; i < propertyCount && written < capacity; ++i) {
        switch (properties[i]) {
        case GL_BUFFER_BINDING: values[written++] = GLint(block.glBinding); break;
        case GL_BUFFER_DATA_SIZE: values[written++] = GLint(block.size); break;
        case GL_NAME_LENGTH: values[written++] = GLint(block.name.size() + 1); break;
        default: values[written++] = 0; break;
        }
    }
    if (length) *length = written;
}
GLint GLAD_API_PTR GetFragDataLocation(GLuint, const GLchar *) { return 0; }
void GLAD_API_PTR TransformFeedbackVaryings(GLuint program, GLsizei count, const GLchar *const *names, GLenum mode) {
    Program *object = Lookup(g->programs, program);
    if (!object) return;
    object->feedbackVaryings.clear();
    if (mode != GL_INTERLEAVED_ATTRIBS) {
        Warn("only interleaved transform feedback is implemented");
        return;
    }
    for (GLsizei i = 0; i < count; ++i) object->feedbackVaryings.emplace_back(names[i]);
}
// Reads one uniform element back from the program's default block.
void GetUniform(GLuint name, GLint location, bool wantFloat, void *out) {
    Program *program = Lookup(g->programs, name);
    if (!program || location < 0 || std::size_t(location) >= program->locationTable.size() || !out) return;
    const auto [index, element] = program->locationTable[std::size_t(location)];
    const Uniform &uniform = program->uniforms[index];
    if (uniform.base == UniformBase::Sampler) {
        if (wantFloat) *static_cast<GLfloat *>(out) = GLfloat(uniform.units[element]);
        else *static_cast<GLint *>(out) = uniform.units[element];
        return;
    }
    const std::uint32_t columnStride = uniform.matrixStride ? uniform.matrixStride : 4u * uniform.rows;
    const std::uint32_t stride = uniform.arrayStride ? uniform.arrayStride : columnStride * uniform.columns;
    const std::size_t base = uniform.offset + std::size_t(element) * stride;
    int written = 0;
    for (int column = 0; column < uniform.columns; ++column)
        for (int row = 0; row < uniform.rows; ++row, ++written) {
            const std::size_t at = base + std::size_t(column) * columnStride + std::size_t(row) * 4;
            if (at + 4 > program->defaults.size()) return;
            std::uint32_t bits;
            std::memcpy(&bits, program->defaults.data() + at, 4);
            float f;
            std::memcpy(&f, &bits, 4);
            const bool isFloat = uniform.base == UniformBase::Float;
            if (wantFloat) static_cast<GLfloat *>(out)[written] = isFloat ? f : GLfloat(GLint(bits));
            else static_cast<GLint *>(out)[written] = isFloat ? GLint(f) : GLint(bits);
        }
}
void GLAD_API_PTR GetUniformfv(GLuint program, GLint location, GLfloat *values) { GetUniform(program, location, true, values); }
void GLAD_API_PTR GetUniformiv(GLuint program, GLint location, GLint *values) { GetUniform(program, location, false, values); }
void GLAD_API_PTR GetUniformuiv(GLuint program, GLint location, GLuint *values) { GetUniform(program, location, false, values); }

template <class F> void (*Erase(F function))(void) { return reinterpret_cast<void (*)(void)>(function); }
} // namespace

Program *CurrentProgram() { return Lookup(g->programs, g->gl.program); }

void DestroyProgramResources(Program &program) {
    for (auto &module : program.modules) {
        if (module) vkDestroyShaderModule(g->device, module, nullptr);
        module = VK_NULL_HANDLE;
    }
    if (program.computePipeline) vkDestroyPipeline(g->device, program.computePipeline, nullptr);
    if (program.layout) vkDestroyPipelineLayout(g->device, program.layout, nullptr);
    if (program.setLayout) vkDestroyDescriptorSetLayout(g->device, program.setLayout, nullptr);
    program.computePipeline = VK_NULL_HANDLE;
    program.layout = VK_NULL_HANDLE;
    program.setLayout = VK_NULL_HANDLE;
}

void ShutdownShaderCompiler() {
    // glslang's process state is retained: a later device in the same process
    // (tests create several hosts) reuses it, and FinalizeProcess is not
    // re-entrant with a subsequent InitializeProcess on every platform.
}

void InstallShaderEntries(std::unordered_map<std::string, void (*)(void)> &table) {
    table["glCreateShader"] = Erase(&CreateShader);
    table["glShaderSource"] = Erase(&ShaderSource);
    table["glCompileShader"] = Erase(&CompileShader);
    table["glGetShaderiv"] = Erase(&GetShaderiv);
    table["glGetShaderInfoLog"] = Erase(&GetShaderInfoLog);
    table["glDeleteShader"] = Erase(&DeleteShader);
    table["glIsShader"] = Erase(&IsShader);
    table["glCreateProgram"] = Erase(&CreateProgram);
    table["glAttachShader"] = Erase(&AttachShader);
    table["glDetachShader"] = Erase(&DetachShader);
    table["glLinkProgram"] = Erase(&LinkProgram);
    table["glValidateProgram"] = Erase(&ValidateProgram);
    table["glGetProgramiv"] = Erase(&GetProgramiv);
    table["glGetProgramInfoLog"] = Erase(&GetProgramInfoLog);
    table["glDeleteProgram"] = Erase(&DeleteProgram);
    table["glUseProgram"] = Erase(&UseProgram);
    table["glIsProgram"] = Erase(&IsProgram);
    table["glGetUniformLocation"] = Erase(&GetUniformLocation);
    table["glGetAttribLocation"] = Erase(&GetAttribLocation);
    table["glUniform1i"] = Erase(&Uniform1i);
    table["glUniform2i"] = Erase(&Uniform2i);
    table["glUniform3i"] = Erase(&Uniform3i);
    table["glUniform4i"] = Erase(&Uniform4i);
    table["glUniform1ui"] = Erase(&Uniform1ui);
    table["glUniform2ui"] = Erase(&Uniform2ui);
    table["glUniform3ui"] = Erase(&Uniform3ui);
    table["glUniform4ui"] = Erase(&Uniform4ui);
    table["glUniform1f"] = Erase(&Uniform1f);
    table["glUniform2f"] = Erase(&Uniform2f);
    table["glUniform3f"] = Erase(&Uniform3f);
    table["glUniform4f"] = Erase(&Uniform4f);
    table["glUniform1iv"] = Erase(&Uniform1iv);
    table["glUniform2iv"] = Erase(&Uniform2iv);
    table["glUniform3iv"] = Erase(&Uniform3iv);
    table["glUniform4iv"] = Erase(&Uniform4iv);
    table["glUniform1uiv"] = Erase(&Uniform1uiv);
    table["glUniform2uiv"] = Erase(&Uniform2uiv);
    table["glUniform3uiv"] = Erase(&Uniform3uiv);
    table["glUniform4uiv"] = Erase(&Uniform4uiv);
    table["glUniform1fv"] = Erase(&Uniform1fv);
    table["glUniform2fv"] = Erase(&Uniform2fv);
    table["glUniform3fv"] = Erase(&Uniform3fv);
    table["glUniform4fv"] = Erase(&Uniform4fv);
    table["glUniformMatrix2fv"] = Erase(&UniformMatrix2fv);
    table["glUniformMatrix3fv"] = Erase(&UniformMatrix3fv);
    table["glUniformMatrix4fv"] = Erase(&UniformMatrix4fv);
    table["glProgramUniform1i"] = Erase(&ProgramUniform1i);
    table["glProgramUniform1ui"] = Erase(&ProgramUniform1ui);
    table["glProgramUniform1f"] = Erase(&ProgramUniform1f);
    table["glProgramUniform2ui"] = Erase(&ProgramUniform2ui);
    table["glProgramUniform2f"] = Erase(&ProgramUniform2f);
    table["glProgramUniform3f"] = Erase(&ProgramUniform3f);
    table["glProgramUniform4f"] = Erase(&ProgramUniform4f);
    table["glProgramUniformMatrix4fv"] = Erase(&ProgramUniformMatrix4fv);
    table["glGetUniformBlockIndex"] = Erase(&GetUniformBlockIndex);
    table["glGetActiveUniformBlockiv"] = Erase(&GetActiveUniformBlockiv);
    table["glUniformBlockBinding"] = Erase(&UniformBlockBinding);
    table["glShaderStorageBlockBinding"] = Erase(&ShaderStorageBlockBinding);
    table["glGetUniformIndices"] = Erase(&GetUniformIndices);
    table["glGetActiveUniformsiv"] = Erase(&GetActiveUniformsiv);
    table["glGetActiveUniform"] = Erase(&GetActiveUniform);
    table["glGetProgramInterfaceiv"] = Erase(&GetProgramInterfaceiv);
    table["glGetProgramResourceIndex"] = Erase(&GetProgramResourceIndex);
    table["glGetProgramResourceiv"] = Erase(&GetProgramResourceiv);
    table["glGetFragDataLocation"] = Erase(&GetFragDataLocation);
    table["glTransformFeedbackVaryings"] = Erase(&TransformFeedbackVaryings);
    table["glGetUniformfv"] = Erase(&GetUniformfv);
    table["glGetUniformiv"] = Erase(&GetUniformiv);
    table["glGetUniformuiv"] = Erase(&GetUniformuiv);
}

} // namespace JudasVulkan::detail
