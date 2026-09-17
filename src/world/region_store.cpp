#include <lunar/world/region_store.hpp>
#include <lunar/debug.hpp>

namespace lunar::World
{
	void ReportRegionReady(RegionCoord coord, bool loaded_from_storage)
	{
		DEBUG_LOG("Region ({}, {}) {}.", coord.x, coord.z, loaded_from_storage ? "loaded from save" : "planned and saved");
	}
}
