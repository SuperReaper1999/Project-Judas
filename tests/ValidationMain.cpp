// Opt-in focused tests share an ordinary entrypoint with exact build provenance.
// Their existing main is renamed only in these explicitly registered targets.
#ifdef main
#undef main
#endif
#include "BuildIdentity.h"

#ifdef JUDAS_VALIDATION_NO_ARGUMENTS
int JudasValidationMain();
#else
int JudasValidationMain(int argc, char** argv);
#endif

int main(int argc, char** argv) {
    if (HandleBuildIdentityArguments(argc, argv, "validation")) return 0;
#ifdef JUDAS_VALIDATION_NO_ARGUMENTS
    (void)argc;
    (void)argv;
    return JudasValidationMain();
#else
    return JudasValidationMain(argc, argv);
#endif
}
