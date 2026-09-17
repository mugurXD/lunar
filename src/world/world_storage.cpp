#include <lunar/world/world_storage.hpp>
#include <lunar/debug.hpp>

#include <filesystem>
#include <format>
#include <string_view>
#include <system_error>

namespace lunar::World
{
	namespace
	{
		constexpr std::string_view WORLD_FILE_NAME       = "world.json";
		constexpr std::string_view REGION_DIRECTORY      = "regions";
		constexpr std::string_view REGION_FILE_FORMAT    = "{}.{}.json";
		constexpr uint32_t         WORLD_FORMAT_VERSION  = 1;
		constexpr uint32_t         REGION_FORMAT_VERSION = 1;
		constexpr int              READABLE_INDENTATION  = 4;

		struct RegionEnvelope
		{
			RegionCoord    coord = {};
			nlohmann::json plan  = nullptr;

			static nlohmann::json Serialize(const RegionEnvelope& envelope)
			{
				return nlohmann::json
				{
					{ "formatVersion", REGION_FORMAT_VERSION },
					{ "coord",         { envelope.coord.x, envelope.coord.z } },
					{ "plan",          envelope.plan }
				};
			}

			static std::optional<RegionEnvelope> Deserialize(const nlohmann::json& json)
			{
				if (json.at("formatVersion").get<uint32_t>() != REGION_FORMAT_VERSION)
					return std::nullopt;

				const nlohmann::json& coord = json.at("coord");

				return RegionEnvelope
				{
					.coord = { coord.at(0).get<int32_t>(), coord.at(1).get<int32_t>() },
					.plan  = json.at("plan")
				};
			}
		};

		std::optional<nlohmann::json> LoadJson(const Fs::Path& path)
		{
			const Fs::JsonFile file(path);
			if (file.content.is_null())
				return std::nullopt;

			return file.content;
		}

		bool SaveJson(const Fs::Path& path, nlohmann::json content, int indentation)
		{
			Fs::JsonFile file;
			file.content     = std::move(content);
			file.indentation = indentation;
			return file.toFile(path);
		}
	}

	nlohmann::json WorldInfo::Serialize(const WorldInfo& info)
	{
		return nlohmann::json
		{
			{ "formatVersion",    WORLD_FORMAT_VERSION },
			{ "seed",             info.seed },
			{ "generatorVersion", info.generatorVersion }
		};
	}

	std::optional<WorldInfo> WorldInfo::Deserialize(const nlohmann::json& json)
	{
		if (json.at("formatVersion").get<uint32_t>() != WORLD_FORMAT_VERSION)
			return std::nullopt;

		return WorldInfo
		{
			.seed             = json.at("seed").get<int32_t>(),
			.generatorVersion = json.at("generatorVersion").get<uint32_t>()
		};
	}

	std::optional<WorldStorage> WorldStorage::create(const Fs::Path& directory, const WorldInfo& info)
	{
		const Fs::Path world_path = directory / WORLD_FILE_NAME;
		if (std::filesystem::exists(world_path))
		{
			DEBUG_ERROR("A world already exists in '{}'", directory.string());
			return std::nullopt;
		}

		std::error_code create_status;
		std::filesystem::create_directories(directory / REGION_DIRECTORY, create_status);
		if (create_status)
		{
			DEBUG_ERROR("Failed to create world directory '{}': {}", directory.string(), create_status.message());
			return std::nullopt;
		}

		if (!SaveJson(world_path, WorldInfo::Serialize(info), READABLE_INDENTATION))
			return std::nullopt;

		return WorldStorage(directory, info);
	}

	std::optional<WorldStorage> WorldStorage::open(const Fs::Path& directory)
	{
		const std::optional<nlohmann::json> world_json = LoadJson(directory / WORLD_FILE_NAME);
		if (!world_json.has_value())
			return std::nullopt;

		const std::optional<WorldInfo> info = Fs::DeserializeJson<WorldInfo>(*world_json);
		if (!info.has_value())
			return std::nullopt;

		return WorldStorage(directory, *info);
	}

	std::optional<WorldStorage> WorldStorage::openOrCreate(const Fs::Path& directory, const WorldInfo& new_world_info)
	{
		return std::filesystem::exists(directory / WORLD_FILE_NAME) ? open(directory) : create(directory, new_world_info);
	}

	const WorldInfo& WorldStorage::getInfo() const
	{
		return info;
	}

	const Fs::Path& WorldStorage::getDirectory() const
	{
		return directory;
	}

	std::optional<nlohmann::json> WorldStorage::loadRegionJson(RegionCoord coord) const
	{
		const std::optional<nlohmann::json> json = LoadJson(getRegionPath(coord));
		if (!json.has_value())
			return std::nullopt;

		const std::optional<RegionEnvelope> envelope = Fs::DeserializeJson<RegionEnvelope>(*json);
		if (!envelope.has_value())
			return std::nullopt;

		if (envelope->coord != coord)
		{
			DEBUG_ERROR("Region file for ({}, {}) contains region ({}, {})", coord.x, coord.z, envelope->coord.x, envelope->coord.z);
			return std::nullopt;
		}

		return envelope->plan;
	}

	bool WorldStorage::saveRegionJson(RegionCoord coord, const nlohmann::json& plan) const
	{
		return SaveJson(getRegionPath(coord), RegionEnvelope::Serialize({ coord, plan }), Fs::COMPACT_JSON);
	}

	WorldStorage::WorldStorage(const Fs::Path& directory, const WorldInfo& info) noexcept
		: directory(directory),
		info(info)
	{
	}

	Fs::Path WorldStorage::getRegionPath(RegionCoord coord) const
	{
		return directory / REGION_DIRECTORY / std::format(REGION_FILE_FORMAT, coord.x, coord.z);
	}
}
