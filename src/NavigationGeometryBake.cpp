#include "NavigationAsset.h"
#include "NavigationBackend.h"
#include <Recast.h>
#include <DetourAlloc.h>
#include <memory>
#include <cmath>
#include <cstring>
bool BakeNavigationGeometry(const NavigationGeometry& geometry,const NavigationSurfaceSettings& settings,const NavigationProfile& profile,NavigationData& out,std::string& error){rcContext context(false);NavigationData data;data.settings=settings;data.profile=profile;data.fingerprint=geometry.fingerprint;NavigationCompressor compressor;
 rcConfig cfg{};cfg.cs=settings.cellSize;cfg.ch=settings.cellHeight;cfg.walkableSlopeAngle=profile.slope;cfg.walkableHeight=static_cast<int>(std::ceil(profile.height/cfg.ch));cfg.walkableClimb=static_cast<int>(std::floor(profile.climb/cfg.ch));cfg.walkableRadius=static_cast<int>(std::ceil(profile.radius/cfg.cs));cfg.maxSimplificationError=settings.simplification;cfg.minRegionArea=settings.minRegion*settings.minRegion;cfg.tileSize=settings.tileSize;cfg.borderSize=cfg.walkableRadius+3;cfg.width=cfg.height=cfg.tileSize+cfg.borderSize*2;
 glm::vec3 bmin=-settings.halfExtents,bmax=settings.halfExtents;int width,height;rcCalcGridSize(&bmin.x,&bmax.x,cfg.cs,&width,&height);int tx=(width+cfg.tileSize-1)/cfg.tileSize,tz=(height+cfg.tileSize-1)/cfg.tileSize;if(tx*tz>4096||cfg.walkableHeight<3){error="navigation bake exceeds 4096 tiles or insufficient vertical resolution";return false;}int count=geometry.triangles.size()/3;std::vector<unsigned char> areas(count);rcMarkWalkableTriangles(&context,cfg.walkableSlopeAngle,geometry.vertices.data(),geometry.vertices.size()/3,geometry.triangles.data(),count,areas.data());
 for(int z=0;z<tz;++z)for(int x=0;x<tx;++x){cfg.bmin[0]=bmin.x+(x*cfg.tileSize-cfg.borderSize)*cfg.cs;cfg.bmin[1]=bmin.y;cfg.bmin[2]=bmin.z+(z*cfg.tileSize-cfg.borderSize)*cfg.cs;cfg.bmax[0]=bmin.x+((x+1)*cfg.tileSize+cfg.borderSize)*cfg.cs;cfg.bmax[1]=bmax.y;cfg.bmax[2]=bmin.z+((z+1)*cfg.tileSize+cfg.borderSize)*cfg.cs;
  std::unique_ptr<rcHeightfield,decltype(&rcFreeHeightField)> hf(rcAllocHeightfield(),rcFreeHeightField);std::unique_ptr<rcCompactHeightfield,decltype(&rcFreeCompactHeightfield)> chf(rcAllocCompactHeightfield(),rcFreeCompactHeightfield);std::unique_ptr<rcHeightfieldLayerSet,decltype(&rcFreeHeightfieldLayerSet)> layers(rcAllocHeightfieldLayerSet(),rcFreeHeightfieldLayerSet);
  auto fail=[&](){error="Recast failed building tile "+std::to_string(x)+","+std::to_string(z);return false;};
  if(!hf||!chf||!layers||!rcCreateHeightfield(&context,*hf,cfg.width,cfg.height,cfg.bmin,cfg.bmax,cfg.cs,cfg.ch)||!rcRasterizeTriangles(&context,geometry.vertices.data(),geometry.vertices.size()/3,geometry.triangles.data(),areas.data(),count,*hf,cfg.walkableClimb))return fail();
  rcFilterLowHangingWalkableObstacles(&context,cfg.walkableClimb,*hf);rcFilterLedgeSpans(&context,cfg.walkableHeight,cfg.walkableClimb,*hf);rcFilterWalkableLowHeightSpans(&context,cfg.walkableHeight,*hf);
  if(!rcBuildCompactHeightfield(&context,cfg.walkableHeight,cfg.walkableClimb,*hf,*chf)||!rcErodeWalkableArea(&context,cfg.walkableRadius,*chf))return fail();
  for(auto& v:geometry.modifiers)rcMarkConvexPolyArea(&context,v.polygon.data(),v.polygon.size()/3,v.bottom,v.top,v.blocked?RC_NULL_AREA:static_cast<unsigned char>(v.area+1),*chf);
  if(!rcBuildRegionsMonotone(&context,*chf,cfg.borderSize,cfg.minRegionArea,0))return fail();
  for(int span=0;span<chf->spanCount;++span)if(chf->spans[span].reg==0)chf->areas[span]=RC_NULL_AREA;
  if(!rcBuildHeightfieldLayers(&context,*chf,cfg.borderSize,cfg.walkableHeight,*layers))return fail();
  for(int i=0;i<layers->nlayers;++i){auto& l=layers->layers[i];dtTileCacheLayerHeader h{};h.magic=DT_TILECACHE_MAGIC;h.version=DT_TILECACHE_VERSION;h.tx=x;h.ty=z;h.tlayer=i;std::memcpy(h.bmin,l.bmin,sizeof(h.bmin));std::memcpy(h.bmax,l.bmax,sizeof(h.bmax));h.width=l.width;h.height=l.height;h.minx=l.minx;h.maxx=l.maxx;h.miny=l.miny;h.maxy=l.maxy;h.hmin=l.hmin;h.hmax=l.hmax;unsigned char* bytes=nullptr;int size=0;if(dtStatusFailed(dtBuildTileCacheLayer(&compressor,&h,l.heights,l.areas,l.cons,&bytes,&size)))return fail();data.layers.emplace_back(bytes,bytes+size);dtFree(bytes);}
 }
 if(data.layers.empty()){error="no walkable navigation polygons for this profile/frame";return false;}out=std::move(data);return true;
}
