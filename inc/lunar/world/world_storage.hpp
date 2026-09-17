#pragma once
#include <lunar/api.hpp>
#include <lunar/file/filesystem.hpp>
#include <lunar/file/json_file.hpp>
#include <lunar/world/grid.hpp>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <optional>

namespace lunar::World
{
	struct LUNAR_API WorldInfo
	{
		int32_t  seed             = 0;
		uint32_t generatorVersion = 0;

		bool operator==(const WorldInfo&) const = default;

		static nlohmann::json           Serialize(const WorldInfo& info);
		static std::optional<WorldInfo> Deserialize(const nlohmann::json& json);
	};

	class LUNAR_API WorldStorage
	{
	public:
		static std::optional<WorldStorage> create(const Fs::Path& directory, const WorldInfo& info);
		static std::optional<WorldStorage> open(const Fs::Path& directory);
		static std::optional<WorldStorage> openOrCreate(const Fs::Path& directory, const WorldInfo& new_world_info);

		const WorldInfo& getInfo()      const;
		const Fs::Path&  getDirectory() const;

		template<IsJsonSerializable Plan>
		std::optional<Plan> loadRegion(RegionCoord coord) const
		{
			const std::optional<nlohmann::json> plan = loadRegionJson(coord);
			if (!plan.has_value())
				return std::nullopt;

			return Fs::DeserializeJson<Plan>(*plan);
		}

		template<IsJsonSerializable Plan>
		bool saveRegion(RegionCoord coord, const Plan& plan) const
		{
			return saveRegionJson(coord, Plan::Serialize(plan));
		}

		std::optional<nlohmann::json> loadRegionJson(RegionCoord coord)                             const;
		bool                          saveRegionJson(RegionCoord coord, const nlohmann::json& plan) const;

	private:
		WorldStorage(const Fs::Path& directory, const WorldInfo& info) noexcept;

		Fs::Path getRegionPath(RegionCoord coord) const;

		Fs::Path  directory = {};
		WorldInfo info      = {};
	};
}
