#pragma once

#include <string>

// Alpha distributions retain advanced-fluid authoring, but cannot execute or
// export it until the contact-stability repair is qualified. Experimental/local
// builds keep the implementation available for the repair and its regressions.
// There is no project setting or environment-variable bypass in Alpha builds.
const char* AdvancedFluidDynamicsRestriction();
bool ValidateAdvancedFluidDynamicsRequest(bool requested, std::string& error);
