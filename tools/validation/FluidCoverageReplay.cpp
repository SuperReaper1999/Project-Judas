// Offline diagnostic of an explicitly declared cell model, not fluid truth.
#include "FluidCoverageObservation.h"
#include "CollisionShapes.h"
#include "../../third_party/nlohmann/json.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {
using Json=nlohmann::json;
using namespace FluidCoverageObservation;
glm::dvec3 Vector(const Json& a) {
    if(!a.is_array()||a.size()!=3)throw std::invalid_argument("Expected three-vector");
    return {a.at(0).get<double>(),a.at(1).get<double>(),a.at(2).get<double>()};
}
glm::dquat Rotation(const Json& a) {
    if(!a.is_array()||a.size()!=4)throw std::invalid_argument("Expected wxyz quaternion");
    return {a.at(0).get<double>(),a.at(1).get<double>(),a.at(2).get<double>(),a.at(3).get<double>()};
}
bool SameOwner(const Json& a,const Json& b) {
    return a.at("valid").get<bool>()&&b.at("valid").get<bool>()&&
        a.at("id")==b.at("id")&&a.at("world")==b.at("world");
}
Json Report(const Result& r) {
    return {{"lower",r.lowerFraction},{"upper",r.upperFraction},
        {"unresolvedNumericalFractionUpper",r.unresolvedFractionUpper},
        {"nodes",r.nodes},{"cellTests",r.cellTests},{"budgetExhausted",r.budgetExhausted},
        {"depthExhausted",r.depthExhausted}};
}
}

int main(int argc,char** argv) try {
    if(argc!=4)throw std::invalid_argument("usage: coverage-replay <boundary.json> <particles.csv> <node-budget>");
    const std::string budgetText=argv[3];std::size_t consumed=0;
    const auto count=std::stoull(budgetText,&consumed);
    if(consumed!=budgetText.size()||count<1||count>100000)throw std::invalid_argument("Budget must be 1..100000");
    Json state;{std::ifstream input(argv[1]);if(!input)throw std::invalid_argument("Cannot open sidecar");input>>state;}
    if(state.at("schema")!=1||!state.at("markerParticleCountsMatch").get<bool>())
        throw std::invalid_argument("Unsupported/incomplete marker snapshot");
    const auto owner=state.at("observedBody");const auto pose=state.at("observedPose");
    const Box body{Vector(pose.at("position")),Vector(state.at("observedHalfExtents")),Rotation(pose.at("rotation_wxyz"))};
    bool actualBox=false;
    for(const auto& row:state.at("owners"))if(SameOwner(row.at("handle"),owner)) {
        if(!row.at("currentShapeAvailable").get<bool>())throw std::invalid_argument("Observed owner is unavailable");
        actualBox=row.at("enabled").get<bool>()&&!row.at("sensor").get<bool>()&&
            row.at("shapeType").get<int>()==static_cast<int>(ShapeType::Box)&&
            Vector(row.at("halfExtents"))==body.half&&Vector(row.at("pivotOffset"))==glm::dvec3(0)&&
            row.at("children").empty();
    }
    if(!actualBox)throw std::invalid_argument("Only the actual enabled box fixture is supported");
    const double density=state.at("couplingSettings").at("restDensity").get<double>();
    if(!std::isfinite(density)||density<=0)throw std::invalid_argument("Invalid density");
    const auto frame=Rotation(state.at("fixture_rotation_wxyz"));
    std::ifstream particles(argv[2]);if(!particles)throw std::invalid_argument("Cannot open particles");
    std::string line;std::getline(particles,line);
    if(line!="x,y,z,vx,vy,vz,ax,ay,az,mass")throw std::invalid_argument("Unexpected particle schema");
    std::vector<Box> matter;
    while(std::getline(particles,line)) {
        std::istringstream input(line);std::vector<double> values;std::string field;
        while(std::getline(input,field,',')) {
            std::size_t used=0;const double value=std::stod(field,&used);
            if(used!=field.size()||!std::isfinite(value))throw std::invalid_argument("Invalid particle value");
            values.push_back(value);
        }
        if(values.size()!=10||values[9]<=0)throw std::invalid_argument("Invalid particle record");
        matter.push_back({{values[0],values[1],values[2]},glm::dvec3(.5*std::cbrt(values[9]/density)),frame});
    }
    if(matter.size()!=state.at("particleCount").get<std::size_t>())throw std::invalid_argument("Particle count differs");
    auto retained=matter;std::size_t ignored=0,owned=0;
    for(const auto& marker:state.at("markers")) {
        if(!SameOwner(marker.at("owner"),owner)){++ignored;continue;}
        if(!marker.at("particleExists").get<bool>()||marker.at("particleIndex").get<std::size_t>()>=matter.size())
            throw std::invalid_argument("Owned marker lacks actual material witness");
        retained.push_back({Vector(marker.at("birthPosition")),glm::dvec3(marker.at("halfWidth").get<double>()),
                            Rotation(marker.at("birthRotation_wxyz"))});++owned;
    }
    const Options options{7,static_cast<std::size_t>(count)};
    Json report={{"schema",1},{"stage",state.at("stage")},{"materialRevision",state.at("materialRevision")},
        {"nodeBudget",count},{"particleCount",matter.size()},{"ownedCellCount",owned},{"foreignMarkersIgnored",ignored},
        {"particleCubes",Report(Measure(body,matter,options))},
        {"particleAndRecordedBirthCells",Report(Measure(body,retained,options))},
        {"model","Mass-equivalent particle cubes in the declared fixture frame; optionally union recorded owner birth cells clipped by the current body"},
        {"physicalImmersionEstablished",false},{"hiddenSurfaceUncertainty","Not constrained beyond trivial [0,1] by these numerical cell bounds"},
        {"ownershipHistoryAbsent",owned==0},{"heldFieldPublicationPoseCaptured",state.at("heldFieldPublicationPoseCaptured")},
        {"scope","Recorded birth cells are not proof of a current free surface; no native wetness query, pressure law or desired immersion is used"}};
    std::cout<<report.dump(2)<<'\n';return 0;
} catch(const std::exception& error){std::cerr<<"Coverage replay: "<<error.what()<<'\n';return 2;}
