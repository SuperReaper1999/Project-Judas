#include "render/ShaderLibrary.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace {
unsigned checks = 0, failures = 0;
void Check(bool condition, const char *message) {
    ++checks;
    failures += !condition;
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", message);
}
std::uint64_t Fingerprint(std::string_view source) {
    std::uint64_t value = 14695981039346656037ull;
    for (const unsigned char byte : source) {
        value ^= byte;
        value *= 1099511628211ull;
    }
    return value;
}
} // namespace

int main() {
    using namespace JudasRender::Shaders;
    // Five shaders retain the original Renderer.cpp extraction identities.
    // The fragment identity deliberately advances for the draw-uniform Legacy
    // lighting guard; the original was 0xbcfc7c527b10a7ba. Independent native
    // pixels and matched workloads qualify that functional change separately.
    // UI fragment advances from 0x32630b836494ebd9 for the shared HDR display
    // boundary; UIPreviewColorTests independently verifies colour and no double resolve.
    // Mesh vertex advances from 0xa489fa0ee7ef5e6f for the explicit physical-water
    // mesh flag. BasicWaterPreviewTests compares the 2-m physical surface with an
    // independent prescribed plane and a wrong-height negative control.
    // Terrain V2 deliberately advances mesh vertex/fragment and shadow vertex
    // from 4b73bb5e4885e341 / 61e1ebd9f7bdef94 / 0ef81ea9763f020d.
    // The shared lossless height/seam deformation is tested against actual GPU
    // colour-pass depth and an independent CPU-height shadow reference; ordinary
    // geometry/state restoration and existing runtime render controls remain.
    // Terrain V2 complementary LOD coverage advances only mesh/shadow fragments
    // from e02de1398961fd82 / c7555650c92125eb. Native modern/portable tests compare
    // every covered colour/shadow depth sample with both endpoint geometries;
    // unchanged terrain and ordinary-draw state restoration retain their gates.
    // Terrain material detail advances only mesh fragment a07894508176164f.
    // TerrainV2MaterialTests compares all six signed tangent projections with
    // independent analytic normals, metre tiling with ordinary UV geometry and
    // blended roughness with an ordinary PBR material on modern/portable GL.
    const std::array<Builtin, 8> builtins = {
        Builtin::MeshVertex, Builtin::MeshFragment, Builtin::ShadowVertex, Builtin::ShadowFragment,
        Builtin::UIVertex,   Builtin::UIFragment,   Builtin::DebugVertex,  Builtin::DebugFragment};
    const std::array<std::uint64_t, 8> approved = {
        0xb1f08758c8089b60ull, 0x02a51d3314b70acdull, 0xa707161edcfa99daull, 0x925b7b5dc8ab7d48ull,
        0x83393b059e8364efull, 0x28a3c2a8126fe9e3ull, 0x0183210d9e1a752cull, 0xcabdf66de2492589ull};
    for (std::size_t i = 0; i < builtins.size(); ++i) {
        const auto *source = Source(builtins[i]);
        if (source && Fingerprint(source) != approved[i])
            std::fprintf(stderr, "Shader identity %s: expected %016llx, actual %016llx\n",
                         Identity(builtins[i]), static_cast<unsigned long long>(approved[i]),
                         static_cast<unsigned long long>(Fingerprint(source)));
        Check(source && Fingerprint(source) == approved[i],
              "canonical built-in matches its reviewed portable shader identity");
        std::string prepared, error;
        Check(PrepareSource(source, Profile::Desktop330, prepared, error) && prepared == source &&
                  error.empty(),
              "desktop 330 preparation preserves all canonical source bytes");
        Check(
            PrepareSource(source, Profile::Desktop450, prepared, error) &&
                prepared == std::string("#version 450 core") + std::string(source).substr(17),
            "desktop 450 profile changes only the declared version before view-block preparation");
        Check(
            PrepareSource(source, Profile::ES300, prepared, error) &&
                prepared.find("#version 300 es\nprecision highp float;") == 0 &&
                prepared.find("samplerBuffer uBones") == std::string::npos &&
                std::strcmp(Identity(builtins[i]), "unknown built-in shader") != 0,
            "ES source has explicit precision, portable palette representation and named identity");
    }

    const std::string palette =
        "#version 330 core\nuniform samplerBuffer uBones;\n"
        "mat4 bone(uint joint) { int b=int(joint)*4;return mat4(texelFetch(uBones,b),"
        "texelFetch(uBones,b+1),texelFetch(uBones,b+2),texelFetch(uBones,b+3)); }\n";
    const std::string expectedES =
        "#version 300 es\nprecision highp float;\nprecision highp int;\n"
        "precision highp sampler2D;\nprecision highp sampler3D;\n"
        "precision highp sampler2DShadow;\nprecision highp sampler2DArray;\n"
        "uniform sampler2D uBones;\n"
        "mat4 bone(uint joint) { int b=int(joint)*4;return mat4(texelFetch(uBones,ivec2(0,b/4),0),"
        "texelFetch(uBones,ivec2(1,b/4),0),texelFetch(uBones,ivec2(2,b/4),0),"
        "texelFetch(uBones,ivec2(3,b/4),0)); }\n";
    std::string prepared = "previous published source", error;
    Check(PrepareSource(palette, Profile::ES300, prepared, error) && prepared == expectedES,
          "known-answer ES palette conversion exactly preserves the original four-texel mapping");
    const auto first = prepared;
    Check(PrepareSource(palette, Profile::ES300, prepared, error) && prepared == first,
          "repeated preparation is deterministic and does not depend on another profile's state");
    Check(PrepareSource(expectedES, Profile::ES300, prepared, error) && prepared == expectedES,
          "already native ES source is not adapted a second time");
    const std::string native450 = "#version 450 core\nlayout(local_size_x=32)in;void main(){}\n";
    Check(PrepareSource(native450, Profile::Desktop450, prepared, error) && prepared == native450,
          "native modern source retains its declared newer-language constructs");
    const auto previous = prepared;
    for (const auto source : {std::string_view{}, std::string_view("void main(){}"),
                              std::string_view(native450), std::string_view(expectedES)}) {
        Check(!PrepareSource(source, Profile::Desktop330, prepared, error) &&
                  prepared == previous && !error.empty(),
              "invalid or newer/native input is rejected without changing the published portable "
              "source");
    }
    std::string embeddedNul = "#version 330 core\n";
    embeddedNul.push_back('\0');
    embeddedNul += "void main(){}";
    Check(!PrepareSource(embeddedNul, Profile::Desktop450, prepared, error) && prepared == previous,
          "embedded NUL cannot silently truncate the driver input");
    Check(!PrepareSource(palette, static_cast<Profile>(255), prepared, error) &&
              prepared == previous,
          "unknown profile is rejected transactionally");
    Check(!Source(static_cast<Builtin>(255)) &&
              std::strcmp(Identity(static_cast<Builtin>(255)), "unknown built-in shader") == 0,
          "unknown built-in cannot alias an unrelated shader");
    Check(std::strcmp(ProfileName(Profile::Desktop330), ProfileName(Profile::Desktop450)) != 0 &&
              std::strcmp(ProfileName(Profile::Desktop450), ProfileName(Profile::ES300)) != 0,
          "diagnostic identities distinguish portable desktop, modern desktop and ES profiles");

    GLuint shader = 77, program = 88;
    Check(!CompileShader(GL_VERTEX_SHADER, nullptr, shader) && shader == 77,
          "invalid compile request preserves the caller's previous shader handle");
    Check(!CompileShader(GL_VERTEX_SHADER, Source(Builtin::MeshVertex), shader) && shader == 77,
          "compile without the owning context fails without publishing a partial shader");
    Check(!LinkProgram(1, 2, program) && program == 88,
          "link without the owning context preserves the previous program handle");
    std::printf("Shader library: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
