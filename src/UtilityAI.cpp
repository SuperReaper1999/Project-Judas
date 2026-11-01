#include "UtilityAI.h"
#include "../third_party/nlohmann/json.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using Json = nlohmann::json;
constexpr std::size_t kMaximumCandidates = 256;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::invalid_argument("Utility AI: " + message);
}

Json Parse(const std::string& text, std::size_t maximum = 524288) {
    Require(text.size() <= maximum, "JSON input exceeds its byte budget");
    try {
        std::vector<std::set<std::string>> objectKeys;
        return Json::parse(text, [&](int depth, Json::parse_event_t event, Json& value) {
            Require(depth <= 16, "JSON nesting exceeds 16 levels");
            if (event == Json::parse_event_t::object_start) objectKeys.emplace_back();
            if (event == Json::parse_event_t::key)
                Require(objectKeys.back().insert(value.get<std::string>()).second,
                        "duplicate JSON object key");
            if (event == Json::parse_event_t::object_end) objectKeys.pop_back();
            return true;
        });
    } catch (const std::invalid_argument&) { throw; }
    catch (const Json::exception& error) {
        throw std::invalid_argument(std::string("Utility AI: invalid JSON: ") + error.what());
    }
}

void Keys(const Json& object, std::initializer_list<const char*> allowed) {
    Require(object.is_object(), "object required");
    for (const auto& item : object.items()) {
        bool known = false;
        for (const auto* key : allowed) known |= item.key() == key;
        Require(known, "unknown property '" + item.key() + "'");
    }
}

double Number(const Json& value, const std::string& label, double minimum,
              double maximum = std::numeric_limits<double>::max()) {
    Require(value.is_number(), label + " requires a number");
    const double result = value.get<double>();
    Require(std::isfinite(result) && result >= minimum && result <= maximum,
            label + " is outside its finite range");
    return result;
}

double OptionNumber(const Json& object, const char* key, double fallback, double minimum,
              double maximum = std::numeric_limits<double>::max()) {
    return object.contains(key) ? Number(object.at(key), key, minimum, maximum) : fallback;
}

std::string Text(const Json& value, const std::string& label, bool empty = false,
                 std::size_t maximum = 128) {
    Require(value.is_string(), label + " requires text");
    auto result = value.get<std::string>();
    Require((empty || !result.empty()) && result.size() <= maximum &&
            result.find('\0') == std::string::npos, label + " is empty or too long");
    return result;
}

bool Boolean(const Json& object, const char* key, bool fallback) {
    if (!object.contains(key)) return fallback;
    Require(object.at(key).is_boolean(), std::string(key) + " requires a Boolean");
    return object.at(key).get<bool>();
}

void Delta(double dt) {
    Require(std::isfinite(dt) && dt >= 0 && dt <= 60, "dt must be finite seconds in [0,60]");
}

Json Normalize(const Json& input) {
    Keys(input, {"version", "name", "interval", "switchAdvantage", "initialDelay", "actions"});
    if (input.contains("version"))
        Require(input.at("version").is_number_integer() && input.at("version") == 1,
                "unsupported definition version");
    Json result = {{"version", 1}, {"name", input.contains("name") ? Text(input.at("name"), "name") : "Utility AI"},
                   {"interval", OptionNumber(input, "interval", .2, .001, 60)},
                   {"switchAdvantage", OptionNumber(input, "switchAdvantage", .05, 0, 1000000)},
                   {"actions", Json::array()}};
    result["initialDelay"] = OptionNumber(input, "initialDelay", 0, 0, result.at("interval"));
    Require(input.contains("actions") && input.at("actions").is_array() &&
            !input.at("actions").empty() && input.at("actions").size() <= 64,
            "actions requires 1..64 definitions");
    std::set<std::string> identifiers;
    for (const auto& action : input.at("actions")) {
        Keys(action, {"id", "weight", "mode", "cooldown", "minCommitment", "interruptible", "requiredFacts", "considerations"});
        Require(action.contains("id"), "action id required");
        const auto id = Text(action.at("id"), "action id");
        Require(identifiers.insert(id).second, "duplicate action id '" + id + "'");
        const auto mode = action.contains("mode") ? Text(action.at("mode"), "mode") : "product";
        Require(mode == "product" || mode == "mean", "mode must be product or mean");
        Json normalized = {{"id", id}, {"weight", OptionNumber(action, "weight", 1, std::numeric_limits<double>::min(), 1000000)},
            {"mode", mode}, {"cooldown", OptionNumber(action, "cooldown", 0, 0, 1000000000)},
            {"minCommitment", OptionNumber(action, "minCommitment", 0, 0, 1000000000)},
            {"interruptible", Boolean(action, "interruptible", true)},
            {"requiredFacts", Json::array()}, {"considerations", Json::array()}};
        if (action.contains("requiredFacts")) {
            const auto& required = action.at("requiredFacts");
            Require(required.is_array() && required.size() <= 32, "requiredFacts requires up to 32 fact names");
            std::set<std::string> names;
            for (const auto& item : required) {
                const auto name = Text(item, "required fact");
                Require(names.insert(name).second, "duplicate required fact");
                normalized["requiredFacts"].push_back(name);
            }
        }
        if (action.contains("considerations")) {
            const auto& considerations = action.at("considerations");
            Require(considerations.is_array() && considerations.size() <= 32, "considerations requires up to 32 entries");
            for (const auto& consideration : considerations) {
                Keys(consideration, {"fact", "weight", "min", "max", "curve", "exponent"});
                Require(consideration.contains("fact"), "consideration fact required");
                const double minimum = OptionNumber(consideration, "min", 0, -std::numeric_limits<double>::max());
                const double maximum = OptionNumber(consideration, "max", 1, -std::numeric_limits<double>::max());
                Require(minimum < maximum, "consideration min must be below max");
                const auto curve = consideration.contains("curve") ? Text(consideration.at("curve"), "curve") : "linear";
                Require(curve == "linear" || curve == "inverse" || curve == "power", "unknown response curve");
                normalized["considerations"].push_back({{"fact", Text(consideration.at("fact"), "fact")},
                    {"weight", OptionNumber(consideration, "weight", 1, std::numeric_limits<double>::min(), 1000000)},
                    {"min", minimum}, {"max", maximum}, {"curve", curve},
                    {"exponent", OptionNumber(consideration, "exponent", 2, std::numeric_limits<double>::min(), 128)}});
            }
        }
        result["actions"].push_back(std::move(normalized));
    }
    // The authored order is presentation only; ties and state identity are stable.
    std::sort(result["actions"].begin(), result["actions"].end(),
              [](const Json& a, const Json& b) { return a.at("id") < b.at("id"); });
    return result;
}

bool Same(const Json& a, const Json& b) {
    return !a.is_null() && !b.is_null() && a.at("action") == b.at("action") && a.at("target") == b.at("target");
}

Json Identity(const Json& candidate) {
    return {{"action", candidate.at("action")}, {"target", candidate.at("target")}};
}

// Scale first so extreme finite inputs do not overflow x-min or max-min.
double NormalizeInput(double x, double minimum, double maximum) {
    if (x <= minimum) return 0;
    if (x >= maximum) return 1;
    const double scale = std::max({std::abs(x), std::abs(minimum), std::abs(maximum)});
    return (x / scale - minimum / scale) / (maximum / scale - minimum / scale);
}
} // namespace

struct UtilityAgent::Impl {
    Json definition;
    std::map<std::string, Json> actions;
    std::map<std::string, double> cooldowns;
    Json selected = nullptr;
    Json diagnostics = Json::array();
    Json previous = nullptr;
    double elapsed = 0, until = 0, age = 0;
    bool changed = false, evaluated = false;
    std::string reason = "not evaluated", status = "idle";

    explicit Impl(Json value) : definition(std::move(value)) {
        until = definition.at("initialDelay");
        for (const auto& action : definition.at("actions")) {
            const auto id = action.at("id").get<std::string>();
            actions.emplace(id, action);
            cooldowns.emplace(id, 0);
        }
    }

    Json Inspect(bool detailed = true) const {
        double commitment = 0;
        if (!selected.is_null()) commitment = std::max(0., actions.at(selected.at("action").get<std::string>()).at("minCommitment").get<double>() - age);
        Json result = {{"name", definition.at("name")}, {"elapsed", elapsed}, {"untilEvaluation", until},
            {"commitmentRemaining", commitment}, {"selected", selected}, {"previous", previous},
            {"reason", reason}, {"status", status}, {"evaluated", evaluated}, {"changed", changed},
            {"cooldowns", cooldowns}};
        if (detailed) result["candidates"] = diagnostics;
        return result;
    }

    void Release() {
        if (!selected.is_null()) {
            const auto id = selected.at("action").get<std::string>();
            cooldowns.at(id) = actions.at(id).at("cooldown").get<double>();
        }
        selected = nullptr;
        age = 0;
    }

    Json Candidates(const Json& input) const {
        Require(input.is_array() && input.size() <= kMaximumCandidates, "candidates requires at most 256 entries");
        Json output = Json::array();
        std::set<std::pair<std::string, std::string>> identities;
        for (const auto& candidate : input) {
            Keys(candidate, {"action", "target", "facts", "eligible", "reason"});
            Require(candidate.contains("action"), "candidate action required");
            const auto id = Text(candidate.at("action"), "candidate action");
            Require(actions.count(id) != 0, "unknown candidate action '" + id + "'");
            const auto target = candidate.contains("target") ? Text(candidate.at("target"), "target", true, 256) : "";
            Require(identities.emplace(id, target).second, "duplicate action/target candidate");
            const Json empty = Json::object();
            const auto& facts = candidate.contains("facts") ? candidate.at("facts") : empty;
            Require(facts.is_object() && facts.size() <= 128, "facts requires at most 128 numeric observations");
            for (const auto& fact : facts.items()) {
                Text(Json(fact.key()), "fact name");
                Number(fact.value(), "fact '" + fact.key() + "'", -std::numeric_limits<double>::max());
            }
            bool eligible = Boolean(candidate, "eligible", true);
            std::string why = candidate.contains("reason") ? Text(candidate.at("reason"), "reason", true, 512) : "";
            const auto& action = actions.at(id);
            if (!eligible && why.empty()) why = "developer eligibility rejected";
            if (cooldowns.at(id) > 0) { eligible = false; why = "cooldown"; }
            for (const auto& required : action.at("requiredFacts")) {
                const auto name = required.get<std::string>();
                if (!facts.contains(name) || facts.at(name).get<double>() <= 0) {
                    eligible = false;
                    why = "required fact '" + name + "' unavailable";
                    break;
                }
            }
            double aggregate = action.at("mode") == "product" ? 1 : 0;
            double totalWeight = 0;
            Json contributions = Json::array();
            for (const auto& consideration : action.at("considerations")) {
                const auto fact = consideration.at("fact").get<std::string>();
                if (!facts.contains(fact)) {
                    eligible = false;
                    why = "missing fact '" + fact + "'";
                    contributions.push_back({{"fact", fact}, {"reason", "missing fact"}});
                    continue;
                }
                const double inputValue = facts.at(fact).get<double>();
                const double normalized = NormalizeInput(inputValue, consideration.at("min"), consideration.at("max"));
                const auto curve = consideration.at("curve").get<std::string>();
                const double response = curve == "inverse" ? 1 - normalized :
                    curve == "power" ? std::pow(normalized, consideration.at("exponent").get<double>()) : normalized;
                const double weight = consideration.at("weight");
                const double contribution = action.at("mode") == "product" ? std::pow(response, weight) : response * weight;
                if (action.at("mode") == "product") aggregate *= contribution;
                else aggregate += contribution;
                totalWeight += weight;
                contributions.push_back({{"fact", fact}, {"input", inputValue}, {"normalized", normalized},
                    {"response", response}, {"weight", weight}, {"contribution", contribution}});
            }
            if (action.at("mode") == "mean") aggregate = totalWeight > 0 ? aggregate / totalWeight : 1;
            const double score = eligible ? aggregate * action.at("weight").get<double>() : 0;
            Require(std::isfinite(score), "non-finite score");
            output.push_back({{"action", id}, {"target", target}, {"eligible", eligible}, {"score", score},
                {"reason", why.empty() ? "eligible" : why}, {"considerations", std::move(contributions)}});
        }
        std::sort(output.begin(), output.end(), [](const Json& a, const Json& b) {
            return std::make_pair(a.at("action").get<std::string>(), a.at("target").get<std::string>()) <
                   std::make_pair(b.at("action").get<std::string>(), b.at("target").get<std::string>());
        });
        return output;
    }

    void AdvanceTime(double dt) {
        Require(std::isfinite(elapsed + dt) && std::isfinite(age + dt), "elapsed time overflow");
        elapsed += dt;
        Require(std::isfinite(elapsed), "elapsed time overflow");
        if (!selected.is_null()) age += dt;
        for (auto& pair : cooldowns) pair.second = std::max(0., pair.second - dt);
        previous = selected;
        changed = false;
        evaluated = false;
        until -= dt;
    }

    void Tick(double dt, const Json& input) {
        AdvanceTime(dt);
        if (until > 0) return;
        const double interval = definition.at("interval");
        until = interval - std::fmod(-until, interval);
        evaluated = true;
        diagnostics = Candidates(input);
        const Json* best = nullptr;
        const Json* current = nullptr;
        for (const auto& candidate : diagnostics) {
            if (!candidate.at("eligible").get<bool>()) continue;
            if (!best || candidate.at("score").get<double>() > best->at("score").get<double>()) best = &candidate;
            if (Same(candidate, selected)) current = &candidate;
        }
        if (current && best && !Same(*best, selected)) {
            const auto& action = actions.at(selected.at("action").get<std::string>());
            if (!action.at("interruptible").get<bool>()) { best = current; reason = "current action is not interruptible"; }
            else if (age < action.at("minCommitment").get<double>()) { best = current; reason = "minimum commitment"; }
            else if (best->at("score").get<double>() <= current->at("score").get<double>() + definition.at("switchAdvantage").get<double>()) {
                best = current; reason = "switch advantage not reached";
            } else reason = "higher utility";
        } else if (!current && !selected.is_null()) reason = "current candidate missing or ineligible";
        else reason = best ? "highest eligible utility" : "no eligible candidates";
        const Json next = best ? Identity(*best) : Json(nullptr);
        changed = (selected.is_null() != next.is_null()) || (!selected.is_null() && !Same(selected, next));
        if (changed) {
            Release();
            selected = next;
            // Retargeting the same action does not put its newly selected
            // target on an action-wide cooldown. Completion still does.
            if (!selected.is_null()) cooldowns.at(selected.at("action").get<std::string>()) = 0;
        }
        status = selected.is_null() ? "idle" : "running";
    }
};

UtilityAgent::UtilityAgent(const std::string& definitionJson)
    : m(std::make_unique<Impl>(Normalize(Parse(definitionJson, 131072)))) {}
UtilityAgent::~UtilityAgent() = default;
std::string UtilityAgent::NormalizeDefinition(const std::string& definitionJson) {
    return Normalize(Parse(definitionJson, 131072)).dump();
}
bool UtilityAgent::Due(double dt) const {
    Delta(dt);
    return m->until <= dt;
}
std::string UtilityAgent::Tick(double dt, const std::string& candidatesJson) {
    Delta(dt);
    // Parse even a skipped call to reject malformed input, but don't score it.
    const auto candidates = Parse(candidatesJson);
    Require(candidates.is_array() && candidates.size() <= kMaximumCandidates, "candidates requires at most 256 entries");
    if (!Due(dt)) {
        // No scorer/definition/diagnostic-tree copies on ordinary action ticks.
        m->AdvanceTime(dt);
        return m->Inspect(false).dump();
    }
    Impl next = *m;
    next.Tick(dt, candidates);
    const auto result = next.Inspect().dump();
    *m = std::move(next);
    return result;
}
void UtilityAgent::Finish(const std::string& status) {
    Require(status == "success" || status == "failure" || status == "cancelled", "unknown action outcome");
    m->previous = m->selected;
    m->changed = !m->selected.is_null();
    m->Release();
    m->until = 0;
    m->reason = "action " + status;
    m->status = status;
}
std::string UtilityAgent::Inspect() const { return m->Inspect().dump(); }
std::string UtilityAgent::Snapshot() const {
    return Json{{"version", 1}, {"definition", m->definition}, {"elapsed", m->elapsed},
        {"untilEvaluation", m->until}, {"activeAge", m->age}, {"selected", m->selected},
        {"status", m->status}, {"cooldowns", m->cooldowns}}.dump();
}
void UtilityAgent::Restore(const std::string& snapshotJson) {
    const auto value = Parse(snapshotJson);
    Keys(value, {"version", "definition", "elapsed", "untilEvaluation", "activeAge", "selected", "status", "cooldowns"});
    for (const char* key : {"version", "definition", "elapsed", "untilEvaluation", "activeAge", "selected", "status", "cooldowns"})
        Require(value.contains(key), std::string("snapshot missing ") + key);
    Require(value.at("version").is_number_integer() && value.at("version") == 1, "unsupported snapshot version");
    Require(value.at("definition") == m->definition, "snapshot definition is incompatible");
    Impl next(m->definition);
    next.elapsed = Number(value.at("elapsed"), "elapsed", 0);
    next.until = Number(value.at("untilEvaluation"), "untilEvaluation", 0, m->definition.at("interval"));
    next.age = Number(value.at("activeAge"), "activeAge", 0, next.elapsed);
    next.selected = value.at("selected");
    if (!next.selected.is_null()) {
        Keys(next.selected, {"action", "target"});
        Require(next.selected.contains("action") && next.selected.contains("target"), "selected action/target required");
        const auto id = Text(next.selected.at("action"), "selected action");
        Text(next.selected.at("target"), "selected target", true, 256);
        Require(next.actions.count(id) != 0, "unknown selected action");
    } else Require(next.age == 0, "idle snapshot cannot have active age");
    next.status = Text(value.at("status"), "status");
    Require(next.selected.is_null() ? (next.status == "idle" || next.status == "success" || next.status == "failure" || next.status == "cancelled") : next.status == "running",
            "snapshot action/status mismatch");
    Require(value.at("cooldowns").is_object() && value.at("cooldowns").size() == next.actions.size(), "snapshot cooldown identities mismatch");
    for (auto& pair : next.cooldowns) {
        Require(value.at("cooldowns").contains(pair.first), "snapshot cooldown missing action");
        pair.second = Number(value.at("cooldowns").at(pair.first), "cooldown", 0, next.actions.at(pair.first).at("cooldown"));
    }
    if (!next.selected.is_null()) Require(next.cooldowns.at(next.selected.at("action").get<std::string>()) == 0, "active action cannot be cooling down");
    next.reason = "restored; observations refresh at next evaluation";
    *m = std::move(next);
}
