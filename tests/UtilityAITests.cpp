#include "UtilityAI.h"
#include "../third_party/nlohmann/json.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Json = nlohmann::json;
unsigned checks = 0, failures = 0;
void Check(bool condition, const std::string& name) {
    ++checks;
    if (!condition) { ++failures; std::cerr << "FAIL " << name << '\n'; }
}
void Near(double actual, double expected, const std::string& name) {
    Check(std::abs(actual - expected) < 1e-12, name);
}
template<class F> void Reject(F&& function, const std::string& name) {
    try { function(); Check(false, name); }
    catch (const std::invalid_argument&) { Check(true, name); }
}
Json Definition() {
    return {{"version", 1}, {"name", "Competing needs"}, {"interval", .2}, {"switchAdvantage", .1},
        {"actions", {{{"id", "eat"}, {"cooldown", 1.0}, {"minCommitment", .5},
            {"requiredFacts", {"food"}}, {"considerations", {{{"fact", "hunger"}}}}},
            {{"id", "rest"}, {"considerations", {{{"fact", "fatigue"}}}}}}}};
}
Json Candidates(double hunger, double fatigue) {
    return {{{"action", "eat"}, {"target", "table"}, {"facts", {{"food", 1}, {"hunger", hunger}}}},
            {{"action", "rest"}, {"target", "bed"}, {"facts", {{"fatigue", fatigue}}}}};
}
Json Tick(UtilityAgent& agent, double dt, const Json& candidates) {
    return Json::parse(agent.Tick(dt, candidates.dump()));
}
std::string Action(const Json& value) {
    return value.at("selected").is_null() ? "" : value.at("selected").at("action").get<std::string>();
}
void Decisions() {
    const auto definition = Definition();
    UtilityAgent agent(definition.dump());
    Check(agent.Due(0), "first decision is due without a startup delay");
    auto result = Tick(agent, 0, Candidates(.8, .2));
    Check(Action(result) == "eat" && result.at("changed") == true, "need and target select an actual action");
    Check(result.at("selected").at("target") == "table", "target identity retained");
    Near(result.at("candidates")[0].at("score"), .8, "linear score known answer");
    Check(!agent.Due(.1), "cadence avoids unnecessary candidate generation");
    result = Tick(agent, .1, Json::array());
    Check(result.at("evaluated") == false && Action(result) == "eat", "running action survives skipped evaluation");
    result = Tick(agent, .1, Candidates(.1, .9));
    Check(Action(result) == "eat" && result.at("reason") == "minimum commitment", "commitment prevents immediate thrashing");
    result = Tick(agent, .4, Candidates(.5, .55));
    Check(Action(result) == "eat" && result.at("reason") == "switch advantage not reached", "small score advantage keeps current action");
    result = Tick(agent, .2, Candidates(.1, .9));
    Check(Action(result) == "rest" && result.at("changed") == true, "higher utility interrupts after commitment");
    Check(result.at("previous").at("action") == "eat", "transition exposes previous action for cleanup");
    Near(result.at("cooldowns").at("eat"), 1, "exited action begins cooldown");
    result = Tick(agent, .2, Candidates(1, .1));
    Check(Action(result) == "rest" && result.at("candidates")[0].at("reason") == "cooldown", "cooldown excludes otherwise attractive action");
    result = Tick(agent, 1, Candidates(1, .1));
    Check(Action(result) == "eat", "cooldown expires in simulation time");
    agent.Finish("success");
    result = Json::parse(agent.Inspect());
    Check(result.at("selected").is_null() && result.at("status") == "success", "completion clears selection");
    Near(result.at("cooldowns").at("eat"), 1, "completion starts cooldown");
    Check(agent.Due(0), "finish schedules an immediate next decision");
    result = Tick(agent, 0, Candidates(1, .1));
    Check(Action(result) == "rest", "completed action cannot immediately restart during cooldown");

    auto delayed = definition;
    delayed["initialDelay"] = .15;
    UtilityAgent staggered(delayed.dump());
    Check(!staggered.Due(.1), "authored initial phase permits deterministic staggering");
    auto asleep = Tick(staggered, .1, Json::array());
    Check(asleep.at("evaluated") == false && !asleep.contains("candidates"), "skipped ticks avoid diagnostic-tree serialization");
    result = Tick(staggered, .05, Candidates(.8, .2));
    Check(result.at("evaluated") == true && Action(result) == "eat", "staggered deadline executes normally");

    auto uninterruptible = definition;
    uninterruptible["actions"][0]["interruptible"] = false;
    UtilityAgent committed(uninterruptible.dump());
    Tick(committed, 0, Candidates(.9, .1));
    result = Tick(committed, 1, Candidates(.1, .9));
    Check(Action(result) == "eat" && result.at("reason") == "current action is not interruptible", "explicit noninterruptible execution");
    auto missing = Candidates(.9, .1);
    missing[0]["facts"]["food"] = 0;
    result = Tick(committed, .2, missing);
    Check(Action(result) == "rest" && result.at("reason") == "current candidate missing or ineligible", "invalid target overrides commitment safely");
    result = Tick(committed, .2, Json::array());
    Check(result.at("selected").is_null() && result.at("status") == "idle", "no available candidates cancels existing action");

    UtilityAgent ties(definition.dump());
    auto tie = Candidates(.5, .5);
    std::reverse(tie.begin(), tie.end());
    result = Tick(ties, 0, tie);
    Check(Action(result) == "eat", "ties use stable action identity rather than caller ordering");
    auto target = tie.back(); target["target"] = "aardvark"; tie.push_back(target);
    UtilityAgent targetTies(definition.dump());
    result = Tick(targetTies, 0, tie);
    Check(result.at("selected").at("target") == "aardvark", "same-action ties use stable target identity");
    target["facts"]["hunger"] = 1;
    auto retarget = Candidates(.5, .1); retarget.push_back(target);
    result = Tick(ties, 1, retarget);
    Check(result.at("selected").at("target") == "aardvark", "same action can retarget after commitment");
    Near(result.at("cooldowns").at("eat"), 0, "retarget does not invalidate selected action with own cooldown");
    UtilityAgent restored(definition.dump()); restored.Restore(ties.Snapshot());
    Check(restored.Snapshot() == ties.Snapshot(), "retarget state is a valid restorable snapshot");
}

void CurvesAndEligibility() {
    const Json definition = {{"actions", {{{"id", "choose"}, {"weight", 2}, {"mode", "mean"},
        {"considerations", {{{"fact", "distance"}, {"min", 10}, {"max", 20}, {"curve", "inverse"}, {"weight", 3}},
                            {{"fact", "need"}, {"curve", "power"}, {"exponent", 2}}}}}}}};
    UtilityAgent agent(definition.dump());
    auto candidate = Json::array({{{"action", "choose"}, {"facts", {{"distance", 15}, {"need", .5}}}}});
    auto result = Tick(agent, 0, candidate);
    Near(result.at("candidates")[0].at("score"), .875, "weighted mean/inverse/power known answer");
    Near(result.at("candidates")[0].at("considerations")[0].at("response"), .5, "inspector exposes individual response");
    candidate[0]["facts"].erase("need");
    result = Tick(agent, .2, candidate);
    Check(result.at("selected").is_null() && result.at("candidates")[0].at("reason") == "missing fact 'need'", "missing facts are actionable ineligible diagnostics");
    candidate[0]["facts"]["need"] = 1;
    candidate[0]["eligible"] = false;
    candidate[0]["reason"] = "path blocked";
    result = Tick(agent, .2, candidate);
    Check(result.at("selected").is_null() && result.at("candidates")[0].at("reason") == "path blocked", "developer eligibility and reason are respected");
    const Json product = {{"actions", {{{"id", "both"}, {"considerations", {{{"fact", "a"}}, {{"fact", "b"}, {"weight", 2}}}}}}}};
    UtilityAgent multiplied(product.dump());
    result = Tick(multiplied, 0, Json::array({{{"action", "both"}, {"facts", {{"a", .5}, {"b", .5}}}}}));
    Near(result.at("candidates")[0].at("score"), .125, "product weights are response exponents");
    auto extremes = product;
    extremes["actions"][0]["considerations"] = Json::array({{{"fact", "a"}, {"min", -1e308}, {"max", 1e308}}});
    UtilityAgent finite(extremes.dump());
    result = Tick(finite, 0, Json::array({{{"action", "both"}, {"facts", {{"a", 0}}}}}));
    Near(result.at("candidates")[0].at("score"), .5, "extreme finite normalized range does not overflow");
}

void ValidationAndPersistence() {
    const auto definition = Definition();
    UtilityAgent agent(definition.dump());
    Tick(agent, 0, Candidates(.8, .2));
    const auto state = agent.Snapshot();
    for (const auto dt : {-1., 61., std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        Reject([&]{agent.Tick(dt, "[]");}, "invalid time rejected");
        Reject([&]{agent.Due(dt);}, "invalid Due time rejected");
        Check(agent.Snapshot() == state, "invalid time leaves authoritative state unchanged");
    }
    for (auto malformed : {"not JSON", "[", "null", "{}", "[{\"action\":\"missing\"}]", "[{\"action\":\"eat\",\"facts\":{\"hunger\":null}}]", "[{\"action\":\"eat\",\"facts\":{\"hunger\":1e400}}]", "[{\"action\":\"eat\",\"eligible\":1}]", "[{\"action\":\"eat\",\"target\":22}]"}) {
        Reject([&]{agent.Tick(.2, malformed);}, "malformed observation rejected");
        Check(agent.Snapshot() == state, "failed Tick publication is transactional");
    }
    auto duplicate = Candidates(.8, .2); duplicate.push_back(duplicate[0]);
    Reject([&]{agent.Tick(.2, duplicate.dump());}, "duplicate action/target rejected");
    auto excessive = Json::array();for(int i=0;i<257;++i)excessive.push_back({{"action","eat"},{"target",std::to_string(i)}});
    Reject([&]{agent.Tick(.2, excessive.dump());}, "candidate budget enforced");
    auto facts = Candidates(.8,.2);for(int i=0;i<129;++i)facts[0]["facts"][std::to_string(i)]=1;
    Reject([&]{agent.Tick(.2, facts.dump());}, "fact budget enforced");
    Reject([&]{agent.Finish("finished-ish");}, "unknown outcome rejected");
    Reject([&]{UtilityAgent bad(R"({"actions":[],"actions":[{"id":"eat"}]})");}, "duplicate JSON keys rejected");
    for (const auto& property : {"interval", "switchAdvantage"}) {
        auto invalid=definition;invalid[property]=-1;
        Reject([&]{UtilityAgent bad(invalid.dump());}, "negative definition configuration rejected");
    }
    auto invalid = definition;invalid["mystery"]=1;
    Reject([&]{UtilityAgent bad(invalid.dump());}, "unknown definition property rejected");
    invalid=definition;invalid["actions"].push_back(invalid["actions"][0]);
    Reject([&]{UtilityAgent bad(invalid.dump());}, "duplicate action identity rejected");
    invalid=definition;invalid["actions"][0]["considerations"][0]["max"]=0;
    Reject([&]{UtilityAgent bad(invalid.dump());}, "zero response range rejected");
    invalid=definition;invalid["actions"][0]["mode"]="magic";
    Reject([&]{UtilityAgent bad(invalid.dump());}, "unknown score composition rejected");
    invalid=definition;invalid["actions"][0]["considerations"][0]["curve"]="magic";
    Reject([&]{UtilityAgent bad(invalid.dump());}, "unknown curve rejected");
    const std::string nested=std::string(1000,'[')+"0"+std::string(1000,']');
    Reject([&]{UtilityAgent bad(nested);}, "deep JSON rejected before native stack overflow");
    Reject([&]{UtilityAgent bad(std::string(131073,' '));}, "definition byte budget enforced");
    auto snapshot=Json::parse(state);snapshot["version"]=2;
    Reject([&]{agent.Restore(snapshot.dump());}, "snapshot version mismatch rejected");
    Check(agent.Snapshot()==state, "rejected restore keeps previous state");
    snapshot=Json::parse(state);snapshot["definition"]["name"]="other";
    Reject([&]{agent.Restore(snapshot.dump());}, "full definition identity checked");
    snapshot=Json::parse(state);snapshot["selected"]["action"]="other";
    Reject([&]{agent.Restore(snapshot.dump());}, "unknown restored action rejected");
    snapshot=Json::parse(state);snapshot["cooldowns"]["eat"]=.1;
    Reject([&]{agent.Restore(snapshot.dump());}, "active restored action cannot be in cooldown");
    snapshot=Json::parse(state);snapshot["activeAge"]=100;
    Reject([&]{agent.Restore(snapshot.dump());}, "impossible restored age rejected");
    snapshot=Json::parse(state);snapshot.erase("cooldowns");
    Reject([&]{agent.Restore(snapshot.dump());}, "missing state is not silently invented");
    UtilityAgent continuation(definition.dump());continuation.Restore(state);
    for(int i=0;i<80;++i) {
        const auto candidates=Candidates((i%13)/13.,(i%7)/7.);
        Tick(agent,.05,candidates);Tick(continuation,.05,candidates);
        Check(agent.Snapshot()==continuation.Snapshot(), "cold reconstruction preserves actual future decisions and timers");
    }
}

void Benchmark() {
    const auto definition=Definition().dump();
    auto observed=Candidates(.8,.2);
    const auto base=observed;
    for(int i=1;i<3;++i)for(auto candidate:base) {
        candidate["target"]=candidate.at("target").get<std::string>()+std::to_string(i);
        observed.push_back(candidate);
    }
    const auto candidates=observed.dump();
    std::cout<<"schedule,agents,candidates_each,ticks,evaluations,median_ms,p95_ms,worst_ms\n";
    for(const bool staggered:{false,true})for(const unsigned count:{0u,100u,1000u}) {
        std::vector<std::unique_ptr<UtilityAgent>> agents;
        for(unsigned i=0;i<count;++i) {
            auto phase=Json::parse(definition);
            if(staggered)phase["initialDelay"]=.2*double(i)/count;
            agents.push_back(std::make_unique<UtilityAgent>(phase.dump()));
        }
        std::vector<double> times;
        unsigned evaluations=0;
        for(unsigned step=0;step<120;++step) {
            const auto start=std::chrono::steady_clock::now();
            for(auto& agent:agents) {
                const bool due=agent->Due(1./60);
                evaluations+=due;
                agent->Tick(1./60,due?candidates:"[]");
            }
            times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
        }
        std::sort(times.begin(),times.end());
        std::cout<<(staggered?"staggered":"same-phase")<<','<<count<<",6,120,"<<evaluations<<','<<(times[59]+times[60])*.5<<','<<times[113]<<','<<times.back()<<'\n';
    }
}
} // namespace
int main(int argc,char** argv) {
    try {
        if(argc>1&&std::string(argv[1])=="--benchmark"){Benchmark();return 0;}
        Decisions();CurvesAndEligibility();ValidationAndPersistence();
    } catch(const std::exception& error) {++failures;std::cerr<<"UNEXPECTED "<<error.what()<<'\n';}
    std::cout<<checks<<" checks, "<<failures<<" failures\n";
    return failures?1:0;
}
