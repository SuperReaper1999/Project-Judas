// Shared private M84 brush stencil. Included inside an anonymous namespace.
bool BrushPoint(const VolumeTerrainSource& t,glm::vec3 p,const VolumeBrush& b){return Finite(p)&&std::isfinite(b.radius)&&b.radius>=.05f&&b.radius<=1024&&std::isfinite(b.strength)&&b.strength>=0&&b.strength<=1&&unsigned(b.kind)<=2&&glm::all(glm::lessThanEqual(glm::abs(p),t.extent*.5f+glm::vec3(b.radius)));}
void Dab(VolumeTerrainSource& t,glm::vec3 p,const VolumeBrush& b){
 auto n=t.resolution;auto cell=t.extent/glm::vec3(n-glm::uvec3(1));
 // A two-diagonal halo covers all extracted edges and finite-difference
 // normals around the brush. No remote distance-field rewrite is required.
 float reach=b.radius+(b.kind==VolumeBrushKind::Smooth?0.f:2*glm::length(cell));
 glm::ivec3 low,high;for(unsigned k=0;k<3;++k){low[k]=std::max(1,int(std::floor((p[k]-reach+t.extent[k]*.5f)/cell[k])));high[k]=std::min(int(n[k])-2,int(std::ceil((p[k]+reach+t.extent[k]*.5f)/cell[k])));}
 auto old=b.kind==VolumeBrushKind::Smooth?t.density:std::vector<float>{};
 for(int z=low.z;z<=high.z;++z)for(int y=low.y;y<=high.y;++y)for(int x=low.x;x<=high.x;++x){auto i=t.Index(unsigned(x),unsigned(y),unsigned(z));float distance=glm::length(t.Point(unsigned(x),unsigned(y),unsigned(z))-p);if(distance>reach)continue;float sphere=distance-b.radius;auto& v=t.density[i];if(b.kind==VolumeBrushKind::Remove)v=glm::mix(v,std::max(v,-sphere),b.strength);else if(b.kind==VolumeBrushKind::Add)v=glm::mix(v,std::min(v,sphere),b.strength);else if(sphere<0){float average=(old[t.Index(x-1,y,z)]+old[t.Index(x+1,y,z)]+old[t.Index(x,y-1,z)]+old[t.Index(x,y+1,z)]+old[t.Index(x,y,z-1)]+old[t.Index(x,y,z+1)])/6;v=glm::mix(v,average,b.strength*std::clamp(-sphere/b.radius,0.f,1.f));}}
}
