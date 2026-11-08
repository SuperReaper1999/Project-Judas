#include "EngineHost.h"
#include "terrain/TerrainAsset.h"
#include "terrain/TerrainMesher.h"
#include "terrain/TerrainMetadata.h"
#include "terrain/TerrainPreview.h"
#include <algorithm>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <stdexcept>

namespace {
unsigned checks = 0;
void Check(bool ok, const std::string &text) {
    ++checks;
    if (!ok)
        throw std::runtime_error(text);
}
TextureData Pixel(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255) {
    TextureData data;
    data.width = data.height = 1;
    data.pixels = {r, g, b, a};
    return data;
}
MeshData Quad(glm::vec3 n, glm::vec3 shading) {
    const auto u = glm::normalize(
        glm::cross(std::abs(n.y) < .8f ? glm::vec3(0, 1, 0) : glm::vec3(0, 0, 1), n));
    const auto v = glm::cross(n, u);
    MeshData mesh;
    for (auto p : {glm::vec2(-1, -1), glm::vec2(1, -1), glm::vec2(1, 1), glm::vec2(-1, 1)}) {
        MeshVertex vertex{};
        vertex.position = (u * p.x + v * p.y) * 4.f;
        vertex.normal = shading;
        vertex.uv1 = (p + 1.f) * .5f;
        mesh.vertices.push_back(vertex);
    }
    mesh.indices = {0, 1, 2, 0, 2, 3};
    return mesh;
}
} // namespace
int main(int argc, char **argv) {
    try {
        std::string error;
        auto source = TerrainV2::Create(17, 17, .5f);
        const auto defaults = TerrainV2::Serialize(source);
        Check(defaults.find("materialDetail") == std::string::npos,
              "default metadata remains byte compatible");
        source.materialDetail.tileMetres = {1, 2, 8, 16};
        source.materialDetail.roughness = {.2f, .4f, .6f, .8f};
        source.materialDetail.normalTexture = "test-normal";
        source.materialDetail.normalMetres = .25f;
        source.materialDetail.normalStrength = 1.3f;
        const auto authored = TerrainV2::Serialize(source);
        TerrainV2::Source parsed;
        Check(TerrainV2::Parse(authored, parsed, error) &&
                  parsed.materialDetail == source.materialDetail,
              "source roundtrip keeps all material controls");
        const auto metadata = TerrainV2::SerializeMetadata(source);
        Check(TerrainV2::ParseMetadata(metadata, parsed, error) &&
                  parsed.materialDetail == source.materialDetail,
              "native header and reference-save metadata retain detail");
        for (float value : {-1.f, 0.f, .124f, 1025.f, INFINITY, NAN}) {
            auto bad = source;
            bad.materialDetail.tileMetres.z = value;
            Check(!TerrainV2::Validate(bad, error), "reject invalid layer tile size");
            bad = source;
            bad.materialDetail.normalMetres = value;
            Check(!TerrainV2::Validate(bad, error), "reject invalid normal tile size");
        }
        for (float value : {-1.f, 1.01f, INFINITY, NAN}) {
            auto bad = source;
            bad.materialDetail.roughness.w = value;
            Check(!TerrainV2::Validate(bad, error), "reject invalid roughness");
        }
        for (float value : {-1.f, 2.01f, INFINITY, NAN}) {
            auto bad = source;
            bad.materialDetail.normalStrength = value;
            Check(!TerrainV2::Validate(bad, error), "reject invalid normal strength");
        }
        Check(TerrainV2::Parse(defaults, parsed, error),
              "restore valid destination for negative controls");
        for (const auto &badDetail :
             {std::string("null"), std::string("{\"tileMetres\":[1,2,3]}"),
              std::string("{\"roughness\":[1,1,1,1,1]}"), std::string("{\"normalMetres\":0}"),
              std::string("{\"normalStength\":1}")}) {
            auto bad = defaults;
            bad.insert(bad.find('{') + 1, "\"materialDetail\":" + badDetail + ",");
            Check(!TerrainV2::Parse(bad, parsed, error) && TerrainV2::Serialize(parsed) == defaults,
                  "malformed material metadata is rejected atomically");
        }
        EngineHost host;
        Check(host.Init("Terrain material projection controls", 192, 192, false, error), error);
        auto &r = host.GetRenderer();
        auto weights = r.CreateTexture(Pixel(255, 0, 0, 0));
        auto white = r.CreateTexture(Pixel(255, 255, 255));
        auto flatNormal = r.CreateTexture(Pixel(128, 128, 255));
        auto tiltedNormal = r.CreateTexture(Pixel(191, 128, 238));
        std::array<TextureHandle, 4> layers{white, white, white, white};
        const std::array<glm::vec3, 4> colours{glm::vec3(1), glm::vec3(1), glm::vec3(1),
                                               glm::vec3(1)};
        const auto rootRotation = glm::angleAxis(.7f, glm::normalize(glm::vec3(1, 2, 3)));
        unsigned maximumDifference = 0;
        for (unsigned placement = 0; placement < 2; ++placement) {
            const auto rotation = placement ? rootRotation : glm::quat(1, 0, 0, 0);
            const glm::vec3 position = placement ? glm::vec3(27, -11, 43) : glm::vec3(0);
            const auto root = glm::translate(glm::mat4(1), position) * glm::mat4_cast(rotation);
            for (auto n : {glm::vec3(1, 0, 0), glm::vec3(-1, 0, 0), glm::vec3(0, 1, 0),
                           glm::vec3(0, -1, 0), glm::vec3(0, 0, 1), glm::vec3(0, 0, -1)}) {
                // Independent analytic tangent: +Y on X walls, +X otherwise.
                const auto tangent = n.x != 0 ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
                const auto expected = glm::normalize(n + tangent * (126.f / 255));
                const auto actualMesh = r.CreateMesh(Quad(n, n));
                const auto oracleMesh = r.CreateMesh(Quad(n, expected));
                const auto view =
                    glm::lookAt(position + rotation * n * 8.f, position,
                                rotation * (n.x != 0 ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0)));
                auto draw = [&](bool terrain, MeshHandle mesh, TextureHandle normal,
                                float strength) {
                    r.BeginFrame(192, 192);
                    r.SetCamera(view, glm::ortho(-4.f, 4.f, -4.f, 4.f, 1.f, 16.f));
                    r.SetLighting(glm::normalize(rotation * (n + tangent * .4f)), {.8f, .8f, .8f},
                                  {.05f, .05f, .05f});
                    r.SetMaterialBindings({});
                    if (terrain) {
                        TerrainV2::MaterialDetail detail;
                        detail.normalStrength = strength;
                        r.BeginTerrainMaterial(weights, layers, colours, glm::inverse(root), detail,
                                               normal);
                    }
                    r.DrawMesh(mesh, position, rotation, {1, 1, 1}, white, {1, 1, 1}, 1);
                    if (terrain)
                        r.EndTerrainMaterial();
                    r.EndMeshBatch();
                    std::vector<unsigned char> image(192 * 192 * 4);
                    glReadPixels(0, 0, 192, 192, GL_RGBA, GL_UNSIGNED_BYTE, image.data());
                    r.EndFrame();
                    return image;
                };
                const auto original = draw(false, actualMesh, {}, 0);
                Check(draw(true, actualMesh, {}, 1) == original,
                      "absent detail preserves geometric shading on each face");
                Check(draw(true, actualMesh, flatNormal, 1) == original,
                      "neutral normal preserves exact shading on each face");
                Check(draw(true, actualMesh, tiltedNormal, 0) == original,
                      "zero strength is an exact opt-out");
                const auto actual = draw(true, actualMesh, tiltedNormal, 1);
                const auto expectedImage = draw(false, oracleMesh, {}, 0);
                for (int y = 16; y < 176; ++y)
                    for (int x = 16; x < 176; ++x)
                        for (unsigned c = 0; c < 3; ++c) {
                            const auto i = (size_t(y) * 192 + x) * 4 + c;
                            maximumDifference = std::max(
                                maximumDifference,
                                unsigned(std::abs(int(actual[i]) - int(expectedImage[i]))));
                        }
                Check(actual != original, "normal detail affects actual lit pixels");
                if (argc > 1 && placement == 0 && n.y == 1) {
                    std::filesystem::create_directories(argv[1]);
                    auto capture = [&](const char *name, const auto &pixels) {
                        TextureData image;
                        image.width = image.height = 192;
                        image.pixels = pixels;
                        std::ofstream file(std::filesystem::path(argv[1]) / name, std::ios::binary);
                        const auto tga = TerrainV2::EncodePreviewTga(image);
                        file.write(tga.data(), tga.size());
                    };
                    capture("normal-disabled.tga", original);
                    capture("normal-enabled.tga", actual);
                }
                r.DestroyMesh(actualMesh);
                r.DestroyMesh(oracleMesh);
            }
        }
        // Independent ordinary UV geometry verifies each layer's metre scale.
        TextureData pattern;
        pattern.width = pattern.height = 32;
        for (unsigned y = 0; y < 32; ++y)
            for (unsigned x = 0; x < 32; ++x) {
                pattern.pixels.push_back((x * 8) ^ ((y / 8) * 63));
                pattern.pixels.push_back(y * 8);
                pattern.pixels.push_back((x + y) * 4);
                pattern.pixels.push_back(255);
            }
        auto patternTexture = r.CreateTexture(pattern);
        for (unsigned layer = 0; layer < 4; ++layer) {
            auto image = Pixel(0, 0, 0, 0);
            image.pixels[layer] = 255;
            auto layerWeights = r.CreateTexture(image);
            TerrainV2::MaterialDetail detail;
            detail.tileMetres = {1, 2, 8, 16};
            auto geometry = Quad({0, 1, 0}, {0, 1, 0});
            auto raw = r.CreateMesh(geometry);
            for (auto &vertex : geometry.vertices)
                vertex.uv =
                    glm::vec2(vertex.position.x, vertex.position.z) / detail.tileMetres[layer];
            auto reference = r.CreateMesh(geometry);
            auto layerTextures = layers;
            layerTextures[layer] = patternTexture;
            auto tiled = [&](bool terrain) {
                r.BeginFrame(192, 192);
                r.SetCamera(glm::lookAt(glm::vec3(0, 8, 0), glm::vec3(0), glm::vec3(0, 0, -1)),
                            glm::ortho(-4.f, 4.f, -4.f, 4.f, 1.f, 16.f));
                r.SetLighting({0, 1, 0}, {1, 1, 1}, {});
                r.SetMaterialBindings({});
                if (terrain)
                    r.BeginTerrainMaterial(layerWeights, layerTextures, colours, glm::mat4(1),
                                           detail, {});
                r.DrawMesh(terrain ? raw : reference, {}, {1, 0, 0, 0}, {1, 1, 1}, patternTexture,
                           {1, 1, 1}, 1);
                if (terrain)
                    r.EndTerrainMaterial();
                r.EndMeshBatch();
                std::vector<unsigned char> pixels(192 * 192 * 4);
                glReadPixels(0, 0, 192, 192, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                r.EndFrame();
                return pixels;
            };
            const auto actual = tiled(true), referencePixels = tiled(false);
            unsigned difference = 0;
            for (size_t i = 0; i < actual.size(); ++i)
                difference = std::max(difference,
                                      unsigned(std::abs(int(actual[i]) - int(referencePixels[i]))));
            Check(difference <= 1,
                  "each layer's metre tiling matches independent ordinary UV geometry");
            r.DestroyMesh(raw);
            r.DestroyMesh(reference);
            r.DestroyTexture(layerWeights);
        }
        r.DestroyTexture(patternTexture);
        // PBR roughness must use the SAME blend as albedo, not a separate
        // nearest-layer choice. Compare with an ordinary independent material.
        auto mixWeights = r.CreateTexture(Pixel(64, 191, 0, 0));
        auto plane = r.CreateMesh(Quad({0, 1, 0}, {0, 1, 0}));
        MaterialDefinition material;
        material.model = MaterialModel::PBR;
        material.roughness = .8f;
        material.doubleSided = true;
        auto ownerMaterial = r.CreateMaterial(material);
        material.roughness *= (64.f * .2f + 191.f * .8f) / 255.f;
        auto referenceMaterial = r.CreateMaterial(material);
        auto pbr = [&](bool terrain, MaterialHandle handle) {
            r.BeginFrame(192, 192);
            r.SetCamera(glm::lookAt(glm::vec3(0, 7, 4), glm::vec3(0), glm::vec3(0, 1, 0)),
                        glm::perspective(glm::radians(60.f), 1.f, .1f, 30.f));
            r.SetLighting(glm::normalize(glm::vec3(0, 7, 4)), {.8f, .8f, .8f}, {.05f, .05f, .05f});
            MaterialBinding binding;
            binding.handle = handle;
            r.SetMaterialBindings({binding});
            if (terrain) {
                TerrainV2::MaterialDetail detail;
                detail.roughness = {.2f, .8f, 1, 1};
                r.BeginTerrainMaterial(mixWeights, layers, colours, glm::mat4(1), detail, {});
            }
            r.DrawMesh(plane, {}, {1, 0, 0, 0}, {1, 1, 1}, white, {.5f, .5f, .5f}, 1);
            if (terrain)
                r.EndTerrainMaterial();
            r.EndMeshBatch();
            std::vector<unsigned char> pixels(192 * 192 * 4);
            glReadPixels(0, 0, 192, 192, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            r.EndFrame();
            return pixels;
        };
        const auto roughnessPixels = pbr(true, ownerMaterial);
        const auto roughnessReference = pbr(false, referenceMaterial);
        unsigned roughDifference = 0;
        for (size_t i = 0; i < roughnessPixels.size(); ++i)
            roughDifference =
                std::max(roughDifference,
                         unsigned(std::abs(int(roughnessPixels[i]) - int(roughnessReference[i]))));
        Check(roughDifference <= 1, "layer roughness matches independently weighted PBR material");
        Check(roughnessPixels != pbr(false, ownerMaterial),
              "roughness changes actual PBR highlights");
        r.SetMaterialBindings({});
        r.DestroyMaterial(ownerMaterial);
        r.DestroyMaterial(referenceMaterial);
        r.DestroyMesh(plane);
        r.DestroyTexture(mixWeights);
        Check(maximumDifference <= 1,
              "normal lighting matches independent analytic normals within one RGBA8 quantum");
        Check(glGetError() == GL_NO_ERROR,
              "terrain detail and ordinary draws leave no graphics errors");
        for (auto handle : {weights, white, flatNormal, tiltedNormal})
            r.DestroyTexture(handle);
        std::cout << "Maximum analytic-normal pixel difference: " << maximumDifference << '\n';
        std::cout << "Terrain material checks passed: " << checks << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "After " << checks << " checks: " << e.what() << '\n';
        return 1;
    }
}
