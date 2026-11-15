#pragma once
// Shared diagnostic runner around the ordinary Application loop. Desktop CLI
// and opt-in Android development captures use identical timing/input policies.
int RunWholeEngineBenchmark(int argc,char** argv);
