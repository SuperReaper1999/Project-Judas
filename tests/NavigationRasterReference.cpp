// Frozen pre-indexing operator, compiled only into the qualification target.
// It remains an independent whole-triangle scan with its original float math.
#include "NavigationAsset.h"
#define BakeNavigationGeometry ReferenceBakeNavigationGeometry
#define ValidateNavigationBakeSettings ReferenceValidateNavigationBakeSettings
#include "fixtures/terrain_v2/navigation_raster_reference.inc"
#undef ValidateNavigationBakeSettings
#undef BakeNavigationGeometry
