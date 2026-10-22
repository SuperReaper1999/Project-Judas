#include "physics/OverlapPairIndex.h"
#include <cstdio>
#include <random>
#include <string>

using JudasPhysicsDetail::OverlapPairIndex;

int main(int argc, char** argv) {
    const bool negative = argc == 2 && std::string(argv[1]) == "--omit-move";
    unsigned checks = 0, failures = 0;
    const auto check = [&](bool value, const char* label) {
        ++checks;
        if (!value) { ++failures; std::printf("FAIL %s\n",label); }
    };
    DynamicAabbTree tree;
    OverlapPairIndex index;
    struct Body { int proxy = -1; unsigned generation = 0; bool source = false; Aabb box; };
    std::vector<Body> bodies(80);
    std::mt19937 random(0x173ab);
    const auto box = [&]() {
        glm::vec3 p;
        for (int k=0;k<3;++k) p[k] = float(int(random()%40)-20)*.25f;
        return Aabb{p-glm::vec3(.6f), p+glm::vec3(.6f)};
    };
    const auto verify = [&]() {
        check(index.Synchronize(tree), "bounded index available");
        std::vector<std::pair<unsigned,unsigned>> actual, expected;
        index.Pairs([&](unsigned a,unsigned b){actual.emplace_back(a,b);});
        // Independent quadratic oracle. No tree/index traversal is reused.
        for(unsigned a=0;a<bodies.size();++a)
            for(unsigned b=a+1;b<bodies.size();++b)
                if(bodies[a].proxy>=0&&bodies[b].proxy>=0&&
                   (bodies[a].source||bodies[b].source)&&bodies[a].box.Overlaps(bodies[b].box))
                    expected.emplace_back(a,b);
        check(actual==expected,"all overlap pairs equal brute force in canonical order");
        for(unsigned a=0;a<bodies.size();++a) if(bodies[a].proxy>=0&&bodies[a].source) {
            std::vector<unsigned> wanted;
            for(unsigned b=0;b<bodies.size();++b)
                if(a!=b&&bodies[b].proxy>=0&&bodies[a].box.Overlaps(bodies[b].box))wanted.push_back(b);
            const auto* found=index.Neighbours(a);
            check(found&&*found==wanted,"endpoint adjacency includes active/inactive neighbours");
        }
    };
    for(unsigned i=0;i<bodies.size();++i){auto& b=bodies[i];b.box=box();b.source=(i%3)!=0;
        b.proxy=tree.CreateProxy(b.box,i);index.SetBody(i,b.proxy,b.generation,b.source);}
    verify();
    // Move an inactive endpoint into a source's stationary enclosure. Forgetting
    // this notification must fail; testing only moving sources would miss it.
    bodies[0].box=bodies[1].box;
    tree.MoveProxy(bodies[0].proxy,bodies[0].box);
    if(!negative)index.Moved(0);
    verify();
    if(negative)return failures?1:0;
    for(unsigned turn=0;turn<3000;++turn){
        const unsigned slot=random()%bodies.size();auto& b=bodies[slot];
        switch(random()%5){
        case 0:
            if(b.proxy>=0){index.Remove(slot);tree.DestroyProxy(b.proxy);b.proxy=-1;}
            break;
        case 1:
            if(b.proxy<0){++b.generation;b.box=box();b.proxy=tree.CreateProxy(b.box,slot);
                index.SetBody(slot,b.proxy,b.generation,b.source);}
            break;
        case 2:
            b.source=!b.source;index.SetBody(slot,b.proxy,b.generation,b.source);break;
        default:
            if(b.proxy>=0){b.box=box();tree.MoveProxy(b.proxy,b.box);index.Moved(slot);}
        }
        if(turn%7==0)verify();
        if(turn%127==0){tree.ShiftOrigin(glm::dvec3(8,-4,2));
            for(auto& body:bodies)if(body.proxy>=0){body.box.min-=glm::vec3(8,-4,2);body.box.max-=glm::vec3(8,-4,2);}
            index.Invalidate();verify();}
    }
    OverlapPairIndex tiny(1);
    tiny.SetBody(0,bodies[0].proxy,0,true);
    check(!tiny.Synchronize(tree),"budget exhaustion explicitly selects native fallback");
    check(tiny.StorageBytes()==0,"disabled index releases retained payload");
    std::printf("overlap index: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
