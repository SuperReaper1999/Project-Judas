#include "RuntimeWorld.h"
#include "Prefab.h"
#include <chrono>
#include <iostream>
int main(){using C=std::chrono::steady_clock;bool sane=true;for(unsigned n:{400,800,1600,3200,6400}){Scene input;for(unsigned i=0;i<n;++i)input.CreateObject("inert");for(int repeat=0;repeat<3;++repeat){std::string error;Scene resolved,flat;auto a=C::now();bool r=ResolvePrefabs(input,nullptr,resolved,error);auto b=C::now();bool f=FlattenHierarchy(resolved,flat,error);auto c=C::now();RuntimeWorld w;bool built=w.Build(input,nullptr,error);auto d=C::now();sane&=r&&f&&built&&flat.Objects().size()==n;auto ms=[](auto x,auto y){return std::chrono::duration<double,std::milli>(y-x).count();};std::cout<<"{\"objects\":"<<n<<",\"repeat\":"<<repeat<<",\"resolveMs\":"<<ms(a,b)<<",\"flattenMs\":"<<ms(b,c)<<",\"buildMs\":"<<ms(c,d)<<",\"sane\":"<<sane<<"}\n";}}return sane?0:1;}
