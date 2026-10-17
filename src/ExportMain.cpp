#include "ProjectExporter.h"
#include "BuildIdentity.h"
#include <cstdio>
int main(int argc, char** argv) {
    if (HandleBuildIdentityArguments(argc, argv, "export")) return 0;
    if (argc < 3 || argc > 4) {
        std::fprintf(stderr, "usage: judas_export project.judasproj package-directory [Release-runtime]\n");
        return 2;
    }
    Project project; ProjectExportOptions options; ProjectExportResult result; std::string error;
    options.destination = argv[2];
    if (argc == 4) options.runtimeExecutable = argv[3];
    if (!project.Load(argv[1], error) || !ExportProject(project, options, result, error)) {
        std::fprintf(stderr, "%s\n", error.c_str()); return 1;
    }
    std::printf("Exported %s: %zu assets, %zu scenes, %ju bytes, %.3f seconds\n",
        result.packageDirectory.c_str(), result.assetCount, result.sceneCount,
        result.bytes, result.seconds);
    return 0;
}
