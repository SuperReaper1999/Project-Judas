#pragma once
#include "quickjs.h"
#include <cstdint>
#include <memory>
#include <string>
class RuntimeWorld;

class ScriptAIReservations {
public:
    ScriptAIReservations();
    ~ScriptAIReservations();
    JSValue Dispatch(JSContext*,const std::string&,int,JSValueConst*,RuntimeWorld&,
                     std::uint64_t owner,std::uint64_t slot);
    void ReleaseClaims(std::uint64_t owner,std::uint64_t slot);
    void Retire(std::uint64_t owner,std::uint64_t slot);
private:
    struct Impl;
    std::unique_ptr<Impl> m;
};
