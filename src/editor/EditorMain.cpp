#include "EditorApplication.h"
#include "BuildIdentity.h"

int main(int argc, char** argv) {
    if (HandleBuildIdentityArguments(argc, argv, "editor")) return 0;
    EditorApplication editor;
    return editor.Run(argc, argv);
}
