#include "WorldMapBake.h"

#include "EngineHost.h"
#include "PlatformServices.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "Scene.h"
#include "SceneSerialization.h"
#include "ScreenshotWriter.h"
#include "WorldPresentation.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <glm/gtc/matrix_transform.hpp>

namespace fs = std::filesystem;

namespace {
double Number(const char* text) {
    std::size_t end = 0;
    const double value = std::stod(text, &end);
    if (text[end] || !std::isfinite(value)) throw std::runtime_error("expected a finite number");
    return value;
}

int Integer(const char* text, int min, int max) {
    const double value = Number(text);
    if (value < min || value > max || std::trunc(value) != value)
        throw std::runtime_error("integer argument is outside its supported range");
    return static_cast<int>(value);
}
}

int RunWorldMapBake(int argc, char** argv) {
    if (argc < 2 || std::string(argv[1]) != "--bake-map") return -1;
    try {
        if (argc != 14 && argc != 15)
            throw std::runtime_error("--bake-map project scene-or-- output.png minX minZ maxX maxZ bottomY topY width height orientation [layerMask]");
        const fs::path output = fs::absolute(argv[4]);
        if (output.extension() != ".png") throw std::runtime_error("map output must end in .png");
        const double minX = Number(argv[5]), minZ = Number(argv[6]);
        const double maxX = Number(argv[7]), maxZ = Number(argv[8]);
        const double bottom = Number(argv[9]), top = Number(argv[10]);
        const int width = Integer(argv[11], 1, 4096), height = Integer(argv[12], 1, 4096);
        const int orientation = Integer(argv[13], 0, 3);
        CategoryMask mask = kAllCategories;
        if (argc == 15) {
            std::size_t end = 0;
            if (argv[14][0] == '-') throw std::runtime_error("expected an unsigned layer mask");
            mask = std::stoull(argv[14], &end, 0);
            if (argv[14][end] || !mask) throw std::runtime_error("expected a nonzero layer mask");
        }
        if (maxX <= minX || maxZ <= minZ || top <= bottom)
            throw std::runtime_error("map X, Z and elevation bounds must be increasing");

        Project project;
        Scene scene;
        std::string error;
        if (!project.Load(argv[2], error)) throw std::runtime_error(error);
        const auto scenePath = std::string(argv[3]) == "-" ? project.StartupScenePath() : project.Resolve(argv[3]);
        if (!LoadSceneFromFile(scenePath, scene, error)) throw std::runtime_error(error);
        for (auto& object : scene.Objects()) object.renderCamera.reset();
        scene.Settings().mainCameraRenderMask = mask;

        const double dx = maxX - minX, dz = maxZ - minZ;
        const glm::vec3 eye(glm::dvec3(minX + dx / 2, top, minZ + dz / 2) - scene.Settings().worldOrigin);
        const float halfWidth = float((orientation % 2 ? dz : dx) / 2);
        const float halfHeight = float((orientation % 2 ? dx : dz) / 2);
        const float depth = float(top - bottom);
        if (!std::isfinite(eye.x) || !std::isfinite(eye.y) || !std::isfinite(eye.z) ||
            !std::isfinite(halfWidth) || !std::isfinite(halfHeight) || !std::isfinite(depth) ||
            halfWidth <= 0 || halfHeight <= 0 || depth <= 0 || eye.y - 1.f == eye.y)
            throw std::runtime_error("map bounds exceed the local renderer coordinate range");

        const std::array<glm::vec3, 4> up{{{0,0,-1},{1,0,0},{0,0,1},{-1,0,0}}};
        const auto view = glm::lookAt(eye, eye - glm::vec3(0,1,0), up[orientation]);
        const auto projection = glm::ortho(-halfWidth,halfWidth,-halfHeight,halfHeight,0.f,depth);
        EngineHost host;
        if (!host.Init("Judas world map bake", 128, 128, false, error)) throw std::runtime_error(error);
        host.OpenProjectAssets(project.RootDir(), project.AssetsDir());
        auto& resources = host.Resources();
        resources.SetBlockingMode(true);
        RuntimeWorld world;
        world.ConfigureProject(project.Settings());
        if (!world.Build(scene,&resources,error,&project.Settings().classification,&project.Settings().navigation))
            throw std::runtime_error(error);

        auto& renderer = host.GetRenderer();
        const auto target = renderer.CreateRenderTarget(width,height,error,true);
        if (!target.IsValid()) throw std::runtime_error(error);
        RenderWorldFrame(renderer,width,height,world,view,projection,eye,1.f,0.f,WorldRenderPurpose::Observer,&target);
        for (const auto& [id, record] : host.Assets().Records()) {
            if (resources.StateOf(id) == ResourceState::Failed)
                throw std::runtime_error(record.relativePath + ": " + resources.ErrorOf(id));
        }
        TextureData image;
        if (!renderer.ReadTextureForDiagnostics(renderer.RenderTargetTexture(target),image))
            throw std::runtime_error("could not read the map render target");
        std::vector<unsigned char> rgb;
        rgb.reserve(std::size_t(width)*height*3);
        for (int y = height-1; y >= 0; --y) {
            for (int x = 0; x < width; ++x) {
                const auto pixel = image.pixels.begin() + (std::size_t(y)*width+x)*TextureData::kChannels;
                rgb.insert(rgb.end(),pixel,pixel+3);
            }
        }
        fs::create_directories(output.parent_path());
        const auto staged = CreateImportStagingFile(output);
        try {
            if (!WriteRgbPng(staged,width,height,rgb)) throw std::runtime_error("could not encode map PNG");
            ReplaceStagedFile(staged,output);
        } catch (...) {
            std::error_code ec;
            fs::remove(staged,ec);
            throw;
        }
        renderer.DestroyRenderTarget(target);
        std::printf("%s | %dx%d | %ju bytes\n",output.string().c_str(),width,height,static_cast<uintmax_t>(fs::file_size(output)));
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr,"Map bake: %s\n",e.what());
        return 1;
    }
}
