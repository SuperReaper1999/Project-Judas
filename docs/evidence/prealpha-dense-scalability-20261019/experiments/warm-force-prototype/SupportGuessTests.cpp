#include "RetainedSupportGuesses.h"
#include <iostream>
#include <stdexcept>

using namespace JudasPhysicsDetail;
namespace {
int checks=0;
void Check(bool okay,const char* message) {
    ++checks;
    if (!okay) throw std::runtime_error(message);
}
ContactKey Key(unsigned a=1,unsigned b=2,unsigned generationA=7,unsigned generationB=9) {
    return {a,b,generationA,generationB,0,0};
}
CachedContact Cached(ContactKey key,glm::dvec3 anchor,float load=3) {
    return {key,anchor,{0,1,0},load,{.25f,0,0},0,11,12};
}
}
int main() {
    std::vector<CachedContact> input{
        Cached(Key(),{0,0,0}),Cached(Key(),{.03,0,0},4),
        Cached(Key(3,4),{0,0,0},5)};
    RetainedSupportGuesses guesses(input);
    for (auto& row:input) row.normalImpulse=999;
    auto match=[&](ContactKey key,glm::dvec3 anchor,glm::vec3 normal=glm::vec3(0,1,0),
                   std::uint64_t ra=11,std::uint64_t rb=12) {
        return guesses.Match(key,anchor,normal,ra,rb);
    };
    auto first=match(Key(),{0,0,0});
    Check(first.first==3 && first.second==glm::vec3(.25f,0,0),"snapshot retains only original ordinary force guess");
    Check(match(Key(),{0,0,0}).first==4,"one-to-one nearest unused anchor match");
    Check(match(Key(),{0,0,0}).first==0,"no duplicated old force row");
    guesses.BeginMatchPass();
    Check(match(Key(),{0,0,0}).first==3,"next event still refers to same ordinary force solve");
    guesses.BeginMatchPass();
    Check(match(Key(1,2,8,9),{0,0,0}).first==0,"reused body generation rejected");
    Check(match(Key(),{0,0,0},{0,-1,0}).first==0,"opposed normal rejected");
    Check(match(Key(),{.2,0,0}).first==0,"distant feature rejected");
    Check(match(Key(),{0,0,0},{0,1,0},13,12).first==0,"changed geometry observation revision rejected");
    auto primitive=Key(); primitive.partA=1;
    Check(match(primitive,{0,0,0}).first==0,"different collision child rejected");
    Check(match(Key(5,6),{0,0,0}).first==0,"new actual impact contact starts with zero force guess");
    std::vector<unsigned char> included(7,0); included[2]=1;
    guesses.InvalidateBodies(included);
    guesses.BeginMatchPass();
    Check(match(Key(),{0,0,0}).first==0,"elastic or coupled departure permanently invalidates that step guess");
    Check(match(Key(3,4),{0,0,0}).first==5,"unaffected assembly retains its force guess");

    for (auto normal : {glm::vec3(1,0,0),glm::vec3(0,0,-1),glm::normalize(glm::vec3(1,2,3))}) {
        auto rotated=Cached(Key(),{0,0,0}); rotated.normal=normal;
        RetainedSupportGuesses oriented({rotated});
        Check(oriented.Match(Key(),{0,0,0},normal,11,12).first==3,"arbitrary normal uses contact frame, not world up");
    }
    std::cout<<"Support-force guess matching "<<checks<<" checks PASS\n";
}
