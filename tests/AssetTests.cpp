// Milestone 9: standalone, headless tests for the CPU-side asset loaders
// (src/ModelLoader.*, src/TextureLoader.*) — no OpenGL context, no window,
// same "physics primitives in isolation" spirit as
// tests/PhysicsPrimitiveTests.cpp and tests/CollisionTests.cpp. Loading a
// model or a texture never touches the GPU in this engine (see
// docs/ARCHITECTURE.md, "Milestone 9, Mesh representation") specifically so
// it can be tested this way.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <tuple>

#include <glm/glm.hpp>

#include "ModelLoader.h"
#include "TextureLoader.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* description) {
    if (condition) {
        std::printf("  OK   %s\n", description);
    } else {
        std::printf("  FAIL %s\n", description);
        ++g_failures;
    }
}

void TestBeaconModelLoads() {
    std::printf("Model: assets/models/beacon.obj loads with real geometry\n");
    MeshData mesh;
    std::string error;
    const bool ok = LoadObjMesh("assets/models/beacon.obj", mesh, error);
    Check(ok, "LoadObjMesh succeeds");
    if (!ok) {
        std::printf("  (error: %s)\n", error.c_str());
        return;
    }
    Check(!mesh.vertices.empty(), "vertex count is non-zero");
    Check(!mesh.indices.empty(), "index count is non-zero (imported models are indexed)");
    Check(mesh.indices.size() % 3 == 0, "index count is a whole number of triangles");

    bool everyVertexHasRealNormal = true;
    bool everyVertexHasRealUV = false;  // at least one non-zero UV is enough to prove UVs loaded
    for (const MeshVertex& vertex : mesh.vertices) {
        if (glm::length(vertex.normal) < 0.99f || glm::length(vertex.normal) > 1.01f) {
            everyVertexHasRealNormal = false;
        }
        if (vertex.uv.x != 0.0f || vertex.uv.y != 0.0f) {
            everyVertexHasRealUV = true;
        }
    }
    Check(everyVertexHasRealNormal, "every vertex has a unit-length normal");
    Check(everyVertexHasRealUV, "UV data is present (not all zero)");

    // beacon.obj is documented (see the file itself) as 5 faces / 6
    // triangles / 18 vertex-instances — pin the exact numbers so a future
    // accidental edit to the asset is caught here, not just visually.
    Check(mesh.vertices.size() == 18, "vertex count matches beacon.obj's documented 18");
    Check(mesh.indices.size() == 18, "index count matches beacon.obj's documented 18 (6 triangles)");
}

void TestMissingModelFailsCleanly() {
    std::printf("Model: a missing file fails intelligibly, not silently\n");
    MeshData mesh;
    std::string error;
    const bool ok = LoadObjMesh("assets/models/does_not_exist.obj", mesh, error);
    Check(!ok, "LoadObjMesh returns false for a missing file");
    Check(!error.empty(), "an error message is produced");
    Check(error.find("does_not_exist.obj") != std::string::npos,
          "the error message names the offending path");
    Check(mesh.vertices.empty(), "no partial/garbage geometry is left in outMesh");
}

void TestBeaconTextureLoads() {
    std::printf("Texture: assets/textures/beacon.png loads with sensible dimensions\n");
    TextureData texture;
    std::string error;
    const bool ok = LoadTextureFromFile("assets/textures/beacon.png", texture, error);
    Check(ok, "LoadTextureFromFile succeeds");
    if (!ok) {
        std::printf("  (error: %s)\n", error.c_str());
        return;
    }
    Check(texture.width == 256, "width matches the authored 256px texture");
    Check(texture.height == 256, "height matches the authored 256px texture");
    Check(texture.pixels.size() ==
              static_cast<size_t>(texture.width) * texture.height * TextureData::kChannels,
          "pixel buffer size matches width * height * 4 channels (always decoded to RGBA)");

    // The texture is authored as four distinct solid-ish quadrants (see
    // assets/textures/beacon.png's generator notes in
    // docs/ARCHITECTURE.md) — confirm the decoded pixel data actually
    // varies rather than being a solid color end to end (a real, if crude,
    // check that decoding produced real image content, not a blank buffer).
    const auto pixelAt = [&](int x, int y) {
        const size_t index =
            (static_cast<size_t>(y) * texture.width + x) * TextureData::kChannels;
        return std::make_tuple(texture.pixels[index], texture.pixels[index + 1],
                                texture.pixels[index + 2]);
    };
    const auto topLeft = pixelAt(64, 192);      // inside the top-left quadrant (post v-flip)
    const auto bottomRight = pixelAt(192, 64);  // inside the bottom-right quadrant
    Check(topLeft != bottomRight, "distinct quadrants decode to genuinely different colors");
}

void TestMissingTextureFailsCleanly() {
    std::printf("Texture: a missing file fails intelligibly, not silently\n");
    TextureData texture;
    std::string error;
    const bool ok = LoadTextureFromFile("assets/textures/does_not_exist.png", texture, error);
    Check(!ok, "LoadTextureFromFile returns false for a missing file");
    Check(!error.empty(), "an error message is produced");
    Check(error.find("does_not_exist.png") != std::string::npos,
          "the error message names the offending path");
    Check(texture.pixels.empty(), "no partial/garbage pixel data is left in outTexture");
}

}  // namespace

int main() {
    std::printf("=== Judas Asset Loading Tests ===\n\n");
    TestBeaconModelLoads();
    TestMissingModelFailsCleanly();
    TestBeaconTextureLoads();
    TestMissingTextureFailsCleanly();

    std::printf("\n");
    if (g_failures == 0) {
        std::printf("ALL TESTS PASSED\n");
        return 0;
    }
    std::printf("%d CHECK(S) FAILED\n", g_failures);
    return 1;
}
