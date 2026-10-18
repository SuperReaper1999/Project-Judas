#include "LiquidGeometry.h"
#include <iostream>
#include <iomanip>
static double volume(const LiquidGeometry& g){double sum=0;for(const auto&t:g.cells)sum+=std::abs(glm::dot(t[1]-t[0],glm::cross(t[2]-t[0],t[3]-t[0])))/6;return sum;}
int main(){
 LiquidGeometry g;g.cells.push_back({glm::dvec3(-1,-1,0),glm::dvec3(1,1e-16,0),glm::dvec3(0,2e-11,1),glm::dvec3(0,-1,1)});auto t=g.cells[0];std::array<double,4> q;for(unsigned i=0;i<4;++i)q[i]=t[i].y;auto actual=LiquidClip(t,q,0);auto under=LiquidCutGeometry(g,{{0,1,0,0}}),over=LiquidCutGeometry(g,{{0,-1,0,0}});
 std::cout<<std::setprecision(24)<<"crossing_tet full="<<volume(g)<<" clipped="<<actual.volume<<" excess="<<actual.volume-volume(g)<<" complementary_partition_error="<<volume(under)+volume(over)-volume(g)<<"\n";
 LiquidGeometry out;out.cells.push_back({glm::dvec3(0,1e-13,0),glm::dvec3(1000,2e-13,0),glm::dvec3(0,2e-13,1000),glm::dvec3(0,9e-13,0)});auto impossible=LiquidCutGeometry(out,{{0,1,0,0}});std::cout<<"strictly_outside_tet full="<<volume(out)<<" clipped_count="<<impossible.cells.size()<<" clipped_volume="<<volume(impossible)<<" min_height=1e-13 max_height=9e-13 plane_height=0\n";
}
