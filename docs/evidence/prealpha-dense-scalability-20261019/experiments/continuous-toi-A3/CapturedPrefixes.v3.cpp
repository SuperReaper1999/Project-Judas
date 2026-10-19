#include "ContinuousBoxCCD.v3.h"
#include "third_party/nlohmann/json.hpp"
#include <fstream>
#include <iostream>
using Json=nlohmann::json;
float F(const Json&j){const auto bits=j.get<uint32_t>();float x;std::memcpy(&x,&bits,4);return x;}
double D(const Json&j){const auto bits=j.get<uint64_t>();double x;std::memcpy(&x,&bits,8);return x;}
glm::vec3 V(const Json&j){return {F(j[0]),F(j[1]),F(j[2])};}
RigidMotionSegment Segment(const Json&body,double start,double finish) {
    const Json *chosen=nullptr;
    for(const auto&s:body.at("motion"))if(D(s.at("begin"))<=start&&D(s.at("end"))>=finish)chosen=&s;
    if(!chosen)throw std::runtime_error("captured prefix crosses motion segments");
    RigidMotionSegment s;s.begin=D(chosen->at("begin"));s.end=D(chosen->at("end"));
    s.inverseMass=F(chosen->at("inverse_mass"));s.position=V(chosen->at("position"));
    s.linearVelocity=V(chosen->at("linear_velocity"));s.driftAngularVelocity=V(chosen->at("drift_angular_velocity"));
    const auto&q=chosen->at("orientation_wxyz");s.orientation={F(q[0]),F(q[1]),F(q[2]),F(q[3])};
    s.prescribed=chosen->at("prescribed");return s;
}
int main(int argc,char**argv) {
    if(argc!=4)return 2;
    std::ifstream input(argv[1]);std::string line;std::vector<Json> traces;
    while(std::getline(input,line))traces.push_back(Json::parse(line));
    Json reference;std::ifstream(argv[2])>>reference;Json output=Json::array();unsigned failures=0;
    for(const auto &proof:reference.at("results")) {
        const unsigned record=proof.at("record");const auto&t=traces.at(record-1);
        const double from=D(t.at("start")),to=D(t.at("result_time"));
        auto a=Segment(t.at("body_a"),from,to),b=Segment(t.at("body_b"),from,to);
        const continuous_box_ccd::MotionView av{&a,nullptr,V(t.at("body_a").at("shape").at("half_extents"))};
        const continuous_box_ccd::MotionView bv{&b,nullptr,V(t.at("body_b").at("shape").at("half_extents"))};
        const auto point=continuous_box_ccd::Point(av,bv,to);
        const auto prefix=continuous_box_ccd::SeparatedPrefix(av,bv,from,to);
        const bool expected=proof.at("classification")=="certified-separated-for-entire-closed-query-interval";
        const bool success=point.supported&&prefix.supported&&point.strictlySeparated==expected&&prefix.separated==expected;
        failures+=!success;
        output.push_back({{"record",record},{"expected_prefix_separated",expected},{"point_separated",point.strictlySeparated},
            {"point_gap_lower",point.gapLower},{"prefix_separated",prefix.separated},{"supported",prefix.supported},
            {"leaves",prefix.leaves},{"nodes",prefix.nodes},{"depth",prefix.maximumDepth},{"matches_exact_reference",success}});
    }
    Json result={{"schema","judas.continuous-box-captured-prefix.v1"},{"failures",failures},{"cases",output}};
    std::ofstream(argv[3])<<result.dump(2)<<'\n';std::cout<<result.dump(2)<<'\n';return failures?1:0;
}
