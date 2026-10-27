#include "AssetDatabase.h"
#include "InputSystem.h"
#include "Project.h"
#include "ProjectStarter.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "ScriptSystem.h"
#include "../third_party/nlohmann/json.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
using Json = nlohmann::json;

namespace {
std::string Read(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("cannot read " + path.string());
    return {std::istreambuf_iterator<char>(file), {}};
}
void Write(const fs::path& path, const std::string& bytes) {
    std::ofstream file(path, std::ios::binary);
    file << bytes;
    if (!file) throw std::runtime_error("cannot write " + path.string());
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: judas_first_project_tests NEW-owned-output\n";
        return 2;
    }
    unsigned checks = 0;
    const auto check = [&](bool value, const std::string& label) {
        ++checks;
        if (!value) throw std::runtime_error(label);
        std::cout << "PASS " << label << '\n';
    };
    try {
        const auto output = fs::absolute(argv[1]);
        check(!fs::exists(output), "fresh owned output required");
        fs::create_directories(output);
        std::string error;
        Project project;
        check(Project::CreateNew((output / "My First Game").string(), "My First Game", project, error),
              "ordinary new project creates project/assets/scenes/save directories: " + error);
        check(!project.Settings().legacyGameplay && !project.Settings().saveIdentity.empty(),
              "modern project has explicit identity and no implicit gameplay controller");
        const auto source = Read(fs::path(__FILE__).parent_path().parent_path() / "docs/judasjs/examples/first-project.js");
        Scene starter;
        check(PopulateVisibleProjectStarter(project,starter,error),"visible template creates ordinary tracked game.js and scene owner: "+error);
        check(starter.Objects().size()==1&&!starter.Objects()[0].body&&starter.Objects()[0].scripts.size()==1,"visible starter has a render-only script owner, no hidden controller");
        check(Read(fs::path(project.AssetsDir())/"game.js")==source,"template uses the documented public-API script exactly");
        check(!PopulateVisibleProjectStarter(project,starter,error),"template refuses overwriting an existing game script");
        const fs::path script = fs::path(project.AssetsDir()) / "first-project.js";
        Write(script, source);
        AssetDatabase assets;
        assets.Scan(project.RootDir(), project.AssetsDir());
        AssetRecord record;
        check(assets.Track(script.string(), record, error), "actual documented script receives ordinary stable asset identity");
        check(ScriptSystem::CheckProjectSources(assets).empty(), "first project compiles and links without executing game code");
        Scene scene;
        auto& object = scene.CreateObject("My rotating box");
        const auto id = object.id;
        object.transform.position = {0, 2, 0};
        object.render = SceneRenderComponent{};
        object.scripts.push_back({1, record.id, true, "{}"});
        project.Settings().startupScene = "Scenes/main.judas";
        check(SaveSceneToFile(scene, project.StartupScenePath(), error) && project.Save(error),
              "authored script attachment and startup scene save through production serializers");
        const auto authored = Read(project.StartupScenePath());
        Project reopened;
        Scene loaded;
        check(reopened.Load(project.ProjectFile(), error) && LoadSceneFromFile(reopened.StartupScenePath(), loaded, error),
              "saved first project reopens without private fixtures");
        ResourceManager resources(nullptr, &assets);
        RuntimeWorld world;
        world.legacyGameplay = reopened.Settings().legacyGameplay;
        check(world.Build(loaded, &resources, error, &reopened.Settings().classification, &reopened.Settings().navigation),
              "normal runtime world builds headlessly: " + error);
        InputSystem input;
        for (unsigned frame = 0; frame < 30; ++frame) {
            world.UpdateScripts(&input, 1.f / 60);
            world.PresentationScripts(&input, 1.f / 60, .5f);
        }
        auto* scripts = world.Scripts();
        check(scripts && scripts->Diagnostics().empty(), "documented lifecycle runs without VM faults");
        const auto records = scripts->Capture();
        check(records.size() == 1, "one independent script instance owns its state");
        const auto state = Json::parse(records.front().json);
        const auto pose = world.RuntimeDefinition(id)->transform;
        check(state["frames"] == 30 && state["presentations"] == 30 &&
                  std::abs(state["elapsed"].get<double>() - .5) < 1e-5 &&
                  std::abs(pose.rotation.y - std::sin(.25f)) < 1e-5,
              "actual entity rotation follows ordinary project script rather than cached status");
        check(world.view && world.view->pose.position == glm::vec3(0, 2, 6) && world.view->fov == 65,
              "presentation phase publishes the explicit project camera without a player controller");
        Write(script, "export default class { update( { deliberately broken");
        check(!ScriptSystem::CheckProjectSources(assets).empty(), "invalid source independently fails project preflight");
        check(!scripts->Reload(true, error) && error.find("first-project.js") != std::string::npos,
              "failed deliberate reload returns actionable script location");
        check(scripts->Capture().front().json == records.front().json &&
                  world.RuntimeDefinition(id)->transform.rotation == pose.rotation,
              "rejected reload preserves actual live instance and visual endpoint");
        auto replacement = source;
        const auto position = replacement.find("default: 1.0");
        check(position != std::string::npos, "the documented authored speed value is present");
        replacement.replace(position, std::string("default: 1.0").size(), "default: 2.0");
        Write(script, replacement);
        check(scripts->Reload(true, error), "fixed project source deliberately reloads without a native build");
        world.UpdateScripts(&input, 1.f / 60);
        const auto continued = Json::parse(scripts->Capture().front().json);
        check(continued["frames"] == 31 && std::abs(continued["elapsed"].get<double>() - (.5 + 2. / 60)) < 1e-5,
              "state survives reload and edited gameplay value changes subsequent execution");
        check(Read(project.StartupScenePath()) == authored,
              "runtime changes and reload never overwrite the authored scene");
        world.EndScripts();
        check(!world.view, "ordinary destroy callback releases project camera on Stop");
        std::ofstream(output / "results.json") << Json{{"checks", checks}, {"failures", 0},
            {"scope", "headless project/serialization/actual VM/failed and successful reload; no visual/editor/export acceptance"}}.dump(2) << '\n';
        std::cout << "SUMMARY " << checks << " checks\n";
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << "FAIL " << failure.what() << '\n';
        return 1;
    }
}
