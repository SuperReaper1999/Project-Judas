#pragma once
#include "Deformable.h"
// A bounded physical sheet fitted to a selected, single-surface mesh region.
// Projection axis is the panel normal (0 X, 1 Y, 2 Z). Detailed source polygons
// remain presentation only. Curved/closed shells must be authored as regions.
struct PanelCookSettings {unsigned columns=6,rows=6,normalAxis=1;double padding=.001;};
bool CookDeformablePanel(const MeshData&,const PanelCookSettings&,DeformableAsset&,std::string&);
void MapDeformablePanel(const DeformableInstance&,double,MeshData&);
void PreparePanelRenderGroups(DeformableAsset&);
// Select an imported authored part, compacting only its referenced geometry and
// materials. Empty selects the entire mesh. This does not infer semantic panels.
bool SelectDeformablePanelRegion(const MeshData&,const std::string& part,MeshData&,std::string&);
