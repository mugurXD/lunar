#pragma once
#include <lunar/api.hpp>
#include <lunar/core/jobs.hpp>
#include <lunar/file/json_file.hpp>
#include <lunar/world/grid.hpp>
#include <lunar/world/region.hpp>
#include <lunar/world/world_settings.hpp>
#include <lunar/world/world_storage.hpp>

#include <glm/glm.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace lunar::World
{
	LUNAR_API void ReportRegionReady(RegionCoord coord, bool loaded_from_storage);

	template<IsJsonSerializable Plan>
	class RegionStore
	{
	public:
		RegionStore(JobSystem&                                 jobs,
		            std::shared_ptr<const WorldStorage>        storage,
		            std::shared_ptr<const RegionPlanner<Plan>> planner,
		            const WorldSettings&                       settings) noexcept
			: jobs(jobs),
			storage(std::move(storage)),
			planner(std::move(planner)),
			settings(settings)
		{
		}

		~RegionStore() noexcept
		{
			for (const auto& [coord, entry] : regions)
				if (entry.plan == nullptr)
					jobs.cancel(entry.job);
		}

		RegionStore(const RegionStore&)            = delete;
		RegionStore& operator=(const RegionStore&) = delete;

		void update(const glm::vec3& focus_position)
		{
			const RegionCoord center        = RegionAt(focus_position, settings);
			const int32_t     unload_radius = settings.regionLoadRadius + settings.unloadMargin;

			std::erase_if(regions, [&](const auto& entry) {
				if (IsWithin(entry.first, center, unload_radius))
					return false;

				cancelIfPending(entry.second);
				return true;
			});

			for (int32_t offset_z = -settings.regionLoadRadius; offset_z <= settings.regionLoadRadius; offset_z++)
				for (int32_t offset_x = -settings.regionLoadRadius; offset_x <= settings.regionLoadRadius; offset_x++)
					request({ center.x + offset_x, center.z + offset_z });
		}

		void request(RegionCoord coord)
		{
			if (regions.contains(coord))
				return;

			const JobHandle job = jobs.submit(
				[storage = storage, planner = planner, settings = settings, coord] { return loadOrPlan(*storage, *planner, coord, settings); },
				[this, coord](RegionResult result) { onRegionReady(coord, std::move(result)); }
			);

			regions.emplace(coord, RegionEntry { .job = job });
			pendingCount++;
		}

		std::shared_ptr<const Plan> find(RegionCoord coord) const
		{
			const auto found = regions.find(coord);
			return found != regions.end() ? found->second.plan : nullptr;
		}

		std::optional<RegionContext<Plan>> gatherContext(ChunkCoord chunk)
		{
			const std::vector<RegionCoord>                    needed = RegionsNeededForChunk(chunk, settings);
			std::vector<typename RegionContext<Plan>::Region> gathered;
			gathered.reserve(needed.size());

			for (const RegionCoord coord : needed)
			{
				std::shared_ptr<const Plan> plan = find(coord);
				if (plan == nullptr)
					request(coord);
				else
					gathered.push_back({ coord, std::move(plan) });
			}

			if (gathered.size() != needed.size())
				return std::nullopt;

			return RegionContext<Plan>(settings, std::move(gathered));
		}

		size_t getLoadedRegionCount() const
		{
			return regions.size() - pendingCount;
		}

		size_t getPendingRegionCount() const
		{
			return pendingCount;
		}

	private:
		struct RegionEntry
		{
			JobHandle                   job  = {};
			std::shared_ptr<const Plan> plan = nullptr;
		};

		struct RegionResult
		{
			std::shared_ptr<const Plan> plan   = nullptr;
			bool                        loaded = false;
		};

		static RegionResult loadOrPlan(const WorldStorage& storage, const RegionPlanner<Plan>& planner, RegionCoord coord, const WorldSettings& settings)
		{
			std::optional<Plan> loaded = storage.loadRegion<Plan>(coord);
			if (loaded.has_value())
				return { std::make_shared<const Plan>(std::move(*loaded)), true };

			Plan plan = planner.plan(coord, settings);
			storage.saveRegion(coord, plan);

			return { std::make_shared<const Plan>(std::move(plan)), false };
		}

		void onRegionReady(RegionCoord coord, RegionResult result)
		{
			const auto found = regions.find(coord);
			if (found == regions.end())
				return;

			ReportRegionReady(coord, result.loaded);
			found->second.plan = std::move(result.plan);
			pendingCount--;
		}

		void cancelIfPending(const RegionEntry& entry)
		{
			if (entry.plan != nullptr)
				return;

			jobs.cancel(entry.job);
			pendingCount--;
		}

		JobSystem&                                                  jobs;
		std::shared_ptr<const WorldStorage>                         storage;
		std::shared_ptr<const RegionPlanner<Plan>>                  planner;
		WorldSettings                                               settings;
		std::unordered_map<RegionCoord, RegionEntry, GridCoordHash> regions;
		size_t                                                      pendingCount = 0;
	};
}
