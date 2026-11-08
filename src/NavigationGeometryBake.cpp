#include "NavigationAsset.h"
#include "NavigationBackend.h"
#include "navigation/NavigationRasterBins.h"
#include "navigation/NavigationTileReuse.h"
#include <Recast.h>
#include <DetourAlloc.h>
#include <memory>
#include <cmath>
#include <cstring>
#include <limits>
bool ValidateNavigationBakeSettings(const NavigationSurfaceSettings& settings,
                                    const NavigationProfile& profile,std::string& error){
    SceneObject candidate;candidate.navigationSurface=settings;
    ProjectNavigation profiles;profiles.profiles.at(0)=profile;
    if(!ValidateNavigationComponents(candidate,error)||!profiles.Validate(error))return false;
    // Validate in binary64 before Recast converts counts to signed integers or
    // stores heights in its pinned 13-bit spans. Otherwise oversized inputs can
    // overflow or silently flatten geometry rather than produce a useful error.
    const double cellsX=std::floor(2.*settings.halfExtents.x/settings.cellSize+.5);
    const double cellsZ=std::floor(2.*settings.halfExtents.z/settings.cellSize+.5);
    const double tilesX=std::ceil(cellsX/settings.tileSize),tilesZ=std::ceil(cellsZ/settings.tileSize);
    const double radius=std::ceil(double(profile.radius)/settings.cellSize);
    if(tilesX*tilesZ>4096||settings.tileSize+2*(radius+3)>std::numeric_limits<int>::max()/2.){
        error="navigation bake exceeds 4096 tiles or representable erosion bounds";return false;
    }
    if(2.*settings.halfExtents.y/settings.cellHeight>RC_SPAN_MAX_HEIGHT||
       double(profile.height)/settings.cellHeight>RC_SPAN_MAX_HEIGHT||
       std::ceil(double(profile.height)/settings.cellHeight)<3){
        error="navigation height bounds/profile exceed Recast's span range or vertical resolution; reduce bounds or adjust cell height";
        return false;
    }
    return true;
}
bool BakeNavigationGeometry(const NavigationGeometry& geometry,const NavigationSurfaceSettings& settings,const NavigationProfile& profile,NavigationData& out,std::string& error,bool allowEmpty,const std::function<bool()>& cancelled,const NavigationBakeUpdate* update){if(!ValidateNavigationBakeSettings(settings,profile,error))return false;rcContext context(false);NavigationData data;data.settings=settings;data.profile=profile;data.fingerprint=geometry.fingerprint;NavigationCompressor compressor;
 rcConfig cfg{};cfg.cs=settings.cellSize;cfg.ch=settings.cellHeight;cfg.walkableSlopeAngle=profile.slope;cfg.walkableHeight=static_cast<int>(std::ceil(profile.height/cfg.ch));cfg.walkableClimb=static_cast<int>(std::floor(profile.climb/cfg.ch));cfg.walkableRadius=static_cast<int>(std::ceil(profile.radius/cfg.cs));cfg.maxSimplificationError=settings.simplification;cfg.minRegionArea=settings.minRegion*settings.minRegion;cfg.tileSize=settings.tileSize;cfg.borderSize=cfg.walkableRadius+3;cfg.width=cfg.height=cfg.tileSize+cfg.borderSize*2;
 glm::vec3 bmin=-settings.halfExtents,bmax=settings.halfExtents;int width,height;rcCalcGridSize(&bmin.x,&bmax.x,cfg.cs,&width,&height);int tx=(width+cfg.tileSize-1)/cfg.tileSize,tz=(height+cfg.tileSize-1)/cfg.tileSize;if(tx*tz>4096||cfg.walkableHeight<3){error="navigation bake exceeds 4096 tiles or insufficient vertical resolution";return false;}int count=geometry.triangles.size()/3;std::vector<unsigned char> areas(count);rcMarkWalkableTriangles(&context,cfg.walkableSlopeAngle,geometry.vertices.data(),geometry.vertices.size()/3,geometry.triangles.data(),count,areas.data());
 NavigationRasterBins bins;
 if(!bins.Build(geometry,areas,bmin,cfg.cs,cfg.tileSize,cfg.borderSize,tx,tz,error,4*1024*1024,cancelled))return false;
 NavigationTileReuse reuse;
 if(!reuse.Prepare(update,settings,profile,tx,tz,cfg.borderSize,error))return false;
 for(int z=0;z<tz;++z)for(int x=0;x<tx;++x){if(cancelled&&cancelled()){error="cancelled";return false;}
  if(reuse.Append(x,z,data.layers)){++data.reusedTiles;continue;}
  ++data.rebuiltTiles;
  cfg.bmin[0]=bmin.x+(x*cfg.tileSize-cfg.borderSize)*cfg.cs;cfg.bmin[1]=bmin.y;cfg.bmin[2]=bmin.z+(z*cfg.tileSize-cfg.borderSize)*cfg.cs;cfg.bmax[0]=bmin.x+((x+1)*cfg.tileSize+cfg.borderSize)*cfg.cs;cfg.bmax[1]=bmax.y;cfg.bmax[2]=bmin.z+((z+1)*cfg.tileSize+cfg.borderSize)*cfg.cs;
  std::unique_ptr<rcHeightfield,decltype(&rcFreeHeightField)> hf(rcAllocHeightfield(),rcFreeHeightField);std::unique_ptr<rcCompactHeightfield,decltype(&rcFreeCompactHeightfield)> chf(rcAllocCompactHeightfield(),rcFreeCompactHeightfield);std::unique_ptr<rcHeightfieldLayerSet,decltype(&rcFreeHeightfieldLayerSet)> layers(rcAllocHeightfieldLayerSet(),rcFreeHeightfieldLayerSet);
  auto fail=[&](){error="Recast failed building tile "+std::to_string(x)+","+std::to_string(z);return false;};
  if(!hf||!chf||!layers||!rcCreateHeightfield(&context,*hf,cfg.width,cfg.height,cfg.bmin,cfg.bmax,cfg.cs,cfg.ch)||!(bins.FullScan()?rcRasterizeTriangles(&context,geometry.vertices.data(),geometry.vertices.size()/3,geometry.triangles.data(),areas.data(),count,*hf,cfg.walkableClimb):
     (bins.At(x,z).indices.empty()||rcRasterizeTriangles(&context,geometry.vertices.data(),geometry.vertices.size()/3,bins.At(x,z).indices.data(),bins.At(x,z).areas.data(),int(bins.At(x,z).areas.size()),*hf,cfg.walkableClimb))))return fail();
  rcFilterLowHangingWalkableObstacles(&context,cfg.walkableClimb,*hf);rcFilterLedgeSpans(&context,cfg.walkableHeight,cfg.walkableClimb,*hf);rcFilterWalkableLowHeightSpans(&context,cfg.walkableHeight,*hf);
  if(!rcBuildCompactHeightfield(&context,cfg.walkableHeight,cfg.walkableClimb,*hf,*chf)||!rcErodeWalkableArea(&context,cfg.walkableRadius,*chf))return fail();
  for(auto& v:geometry.modifiers)rcMarkConvexPolyArea(&context,v.polygon.data(),v.polygon.size()/3,v.bottom,v.top,v.blocked?RC_NULL_AREA:static_cast<unsigned char>(v.area+1),*chf);
  if(!rcBuildRegionsMonotone(&context,*chf,cfg.borderSize,cfg.minRegionArea,0))return fail();
  for(int span=0;span<chf->spanCount;++span)if(chf->spans[span].reg==0)chf->areas[span]=RC_NULL_AREA;
  if(!rcBuildHeightfieldLayers(&context,*chf,cfg.borderSize,cfg.walkableHeight,*layers))return fail();
  for(int i=0;i<layers->nlayers;++i){auto& l=layers->layers[i];dtTileCacheLayerHeader h{};h.magic=DT_TILECACHE_MAGIC;h.version=DT_TILECACHE_VERSION;h.tx=x;h.ty=z;h.tlayer=i;std::memcpy(h.bmin,l.bmin,sizeof(h.bmin));std::memcpy(h.bmax,l.bmax,sizeof(h.bmax));h.width=l.width;h.height=l.height;h.minx=l.minx;h.maxx=l.maxx;h.miny=l.miny;h.maxy=l.maxy;h.hmin=l.hmin;h.hmax=l.hmax;unsigned char* bytes=nullptr;int size=0;if(dtStatusFailed(dtBuildTileCacheLayer(&compressor,&h,l.heights,l.areas,l.cons,&bytes,&size)))return fail();data.layers.emplace_back(bytes,bytes+size);dtFree(bytes);}
 }
 if(data.layers.empty()&&!allowEmpty){error="no walkable navigation polygons for this profile/frame";return false;}out=std::move(data);return true;
}
