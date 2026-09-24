#pragma once
#include <lunar/api.hpp>
#include <lunar/world/chunk_storage.hpp>
#include <lunar/world/region.hpp>
#include <lunar/world/region_store.hpp>
#include <lunar/world/terrain.hpp>
#include <lunar/world/world_settings.hpp>

#include <glm/glm.hpp>

#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace lunar::World
{
	template<typename Plan>
	class TerrainDresser
	{
	public:
		TerrainDresser()          noexcept = default;
		virtual ~TerrainDresser() noexcept = default;

		TerrainDresser(const TerrainDresser&)            = delete;
		TerrainDresser& operator=(const TerrainDresser&) = delete;

		virtual void dress(const RegionContext<Plan>& context,
		                   ChunkCoord                 coord,
		                   const WorldSettings&       settings,
		                   const HeightSampler&       ground,
		                   std::vector<DressedMesh>&  output) const = 0;
	};

	template<typename Plan>
	class TerrainGenerator
	{
	public:
		TerrainGenerator()          noexcept = default;
		virtual ~TerrainGenerator() noexcept = default;

		TerrainGenerator(const TerrainGenerator&)            = delete;
		TerrainGenerator& operator=(const TerrainGenerator&) = delete;

		virtual float     sampleHeight(const RegionContext<Plan>& context, double x, double z)                                         const = 0;
		virtual glm::vec3 sampleColor(const RegionContext<Plan>& context, double x, double z, float height, const glm::vec3& normal) const = 0;

		void addDresser(std::shared_ptr<const TerrainDresser<Plan>> dresser)
		{
			dressers.push_back(std::move(dresser));
		}

		std::span<const std::shared_ptr<const TerrainDresser<Plan>>> getDressers() const
		{
			return dressers;
		}

	private:
		std::vector<std::shared_ptr<const TerrainDresser<Plan>>> dressers;
	};

	template<typename Plan>
	ChunkData LoadOrGenerateChunk(const TerrainGenerator<Plan>& generator,
	                              const ChunkStorage&           storage,
	                              const RegionContext<Plan>&    context,
	                              ChunkCoord                    coord,
	                              const WorldSettings&          settings,
	                              bool                          use_storage = true)
	{
		std::optional<Heightmap> heightmap = use_storage ? storage.load(coord) : std::nullopt;
		if (!heightmap.has_value())
		{
			heightmap = SampleHeightmap([&](double x, double z) { return generator.sampleHeight(context, x, z); }, coord, settings);

			if (use_storage)
				storage.save(coord, *heightmap);
		}

		Render::MeshData mesh = BuildChunkMesh(*heightmap, [&](double x, double z, float height, const glm::vec3& normal) {
			return generator.sampleColor(context, x, z, height, normal);
		}, coord, settings);

		const HeightSampler      ground = [&](double x, double z) { return generator.sampleHeight(context, x, z); };
		std::vector<DressedMesh> dressing;
		for (const std::shared_ptr<const TerrainDresser<Plan>>& dresser : generator.getDressers())
			dresser->dress(context, coord, settings, ground, dressing);

		return ChunkData { std::move(*heightmap), std::move(mesh), std::move(dressing) };
	}

	template<IsJsonSerializable Plan>
	class RegionChunkSource final : public ChunkSource
	{
	public:
		RegionChunkSource(RegionStore<Plan>&                            regions,
		                  std::shared_ptr<const TerrainGenerator<Plan>> generator,
		                  std::shared_ptr<const ChunkStorage>           storage,
		                  const WorldSettings&                          settings) noexcept
			: regions(regions),
			generator(std::move(generator)),
			storage(std::move(storage)),
			settings(settings)
		{
		}

		std::optional<ChunkWork> prepareChunk(ChunkCoord coord) override
		{
			std::optional<RegionContext<Plan>> context = regions.gatherContext(coord);
			if (!context.has_value())
				return std::nullopt;

			return ChunkWork([generator = generator, storage = storage, context = std::move(*context), settings = settings, coord, use_storage = useStorage] {
				return LoadOrGenerateChunk(*generator, *storage, context, coord, settings, use_storage);
			});
		}

		void setStorageEnabled(bool enabled)
		{
			useStorage = enabled;
		}

	private:
		bool                                          useStorage = true;
		RegionStore<Plan>&                            regions;
		std::shared_ptr<const TerrainGenerator<Plan>> generator;
		std::shared_ptr<const ChunkStorage>           storage;
		WorldSettings                                 settings;
	};
}
