#include <lunar/world/chunk_storage.hpp>
#include <lunar/debug.hpp>

#include <filesystem>
#include <format>
#include <fstream>
#include <string_view>
#include <system_error>

namespace lunar::World
{
	namespace
	{
		constexpr std::string_view REGION_FILE_FORMAT = "{}.{}.chunks";
		constexpr uint32_t         FILE_MAGIC         = 0x4B43484C;
		constexpr uint32_t         FILE_VERSION       = 1;

		struct FileHeader
		{
			uint32_t magic          = FILE_MAGIC;
			uint32_t version        = FILE_VERSION;
			uint32_t samplesPerSide = 0;
		};

		struct RecordHeader
		{
			int32_t  chunkX      = 0;
			int32_t  chunkZ      = 0;
			uint32_t sampleCount = 0;
		};

		template<typename T>
		bool ReadValue(std::istream& stream, T& value)
		{
			return static_cast<bool>(stream.read(reinterpret_cast<char*>(&value), sizeof(T)));
		}

		template<typename T>
		void WriteValue(std::ostream& stream, const T& value)
		{
			stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
		}
	}

	ChunkStorage::ChunkStorage(const Fs::Path& directory, const WorldSettings& settings) noexcept
		: directory(directory),
		settings(settings)
	{
		std::error_code create_status;
		std::filesystem::create_directories(directory, create_status);
		if (create_status)
			DEBUG_ERROR("Failed to create chunk directory '{}': {}", directory.string(), create_status.message());
	}

	std::optional<Heightmap> ChunkStorage::load(ChunkCoord coord) const
	{
		const std::lock_guard lock(mutex);

		const RegionCoord  region = RegionAt(coord, settings);
		const RegionIndex& index  = getIndex(region);
		const auto         found  = index.offsets.find(coord);
		if (found == index.offsets.end())
			return std::nullopt;

		Heightmap heightmap = { .samplesPerSide = HeightmapSamplesPerSide(settings), .heights = std::vector<float>(getSampleCount()) };

		std::ifstream file(getRegionPath(region), std::ios::binary);
		file.seekg(static_cast<std::streamoff>(found->second + sizeof(RecordHeader)));
		file.read(reinterpret_cast<char*>(heightmap.heights.data()), static_cast<std::streamsize>(heightmap.heights.size() * sizeof(float)));
		if (!file)
		{
			DEBUG_ERROR("Failed to read chunk ({}, {}) from storage", coord.x, coord.z);
			return std::nullopt;
		}

		return heightmap;
	}

	bool ChunkStorage::save(ChunkCoord coord, const Heightmap& heightmap) const
	{
		const std::lock_guard lock(mutex);

		const RegionCoord region = RegionAt(coord, settings);
		RegionIndex&      index  = getIndex(region);
		if (!index.usable || heightmap.heights.size() != getSampleCount())
			return false;

		if (index.offsets.contains(coord))
			return true;

		const Fs::Path path = getRegionPath(region);
		if (index.validEnd == 0 && !createRegionFile(path))
			return false;

		if (index.validEnd == 0)
			index.validEnd = sizeof(FileHeader);

		std::error_code resize_status;
		std::filesystem::resize_file(path, index.validEnd, resize_status);
		if (resize_status)
		{
			DEBUG_ERROR("Failed to prepare '{}' for writing: {}", path.string(), resize_status.message());
			return false;
		}

		std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
		file.seekp(static_cast<std::streamoff>(index.validEnd));
		WriteValue(file, RecordHeader { coord.x, coord.z, static_cast<uint32_t>(heightmap.heights.size()) });
		file.write(reinterpret_cast<const char*>(heightmap.heights.data()), static_cast<std::streamsize>(heightmap.heights.size() * sizeof(float)));
		file.flush();
		if (!file)
		{
			DEBUG_ERROR("Failed to store chunk ({}, {})", coord.x, coord.z);
			return false;
		}

		index.offsets.emplace(coord, index.validEnd);
		index.validEnd += getRecordSize();
		return true;
	}

	bool ChunkStorage::createRegionFile(const Fs::Path& path) const
	{
		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		WriteValue(file, FileHeader { .samplesPerSide = HeightmapSamplesPerSide(settings) });
		if (!file)
		{
			DEBUG_ERROR("Failed to create chunk file '{}'", path.string());
			return false;
		}

		return true;
	}

	ChunkStorage::RegionIndex& ChunkStorage::getIndex(RegionCoord region) const
	{
		const auto found = indices.find(region);
		if (found != indices.end())
			return found->second;

		return indices.emplace(region, scanRegion(region)).first->second;
	}

	ChunkStorage::RegionIndex ChunkStorage::scanRegion(RegionCoord region) const
	{
		RegionIndex    index;
		const Fs::Path path = getRegionPath(region);

		std::error_code size_status;
		const uint64_t  file_size = std::filesystem::file_size(path, size_status);
		if (size_status || file_size < sizeof(FileHeader))
			return index;

		std::ifstream file(path, std::ios::binary);
		FileHeader    header;
		if (!ReadValue(file, header) || header.magic != FILE_MAGIC || header.version != FILE_VERSION || header.samplesPerSide != HeightmapSamplesPerSide(settings))
		{
			DEBUG_ERROR("Chunk file for region ({}, {}) is incompatible and will not be used", region.x, region.z);
			index.usable = false;
			return index;
		}

		index.validEnd = sizeof(FileHeader);

		RecordHeader record;
		while (index.validEnd + getRecordSize() <= file_size && ReadValue(file, record) && record.sampleCount == getSampleCount())
		{
			index.offsets.emplace(ChunkCoord { record.chunkX, record.chunkZ }, index.validEnd);
			index.validEnd += getRecordSize();
			file.seekg(static_cast<std::streamoff>(index.validEnd));
		}

		return index;
	}

	Fs::Path ChunkStorage::getRegionPath(RegionCoord region) const
	{
		return directory / std::format(REGION_FILE_FORMAT, region.x, region.z);
	}

	size_t ChunkStorage::getSampleCount() const
	{
		const size_t samples_per_side = HeightmapSamplesPerSide(settings);
		return samples_per_side * samples_per_side;
	}

	uint64_t ChunkStorage::getRecordSize() const
	{
		return sizeof(RecordHeader) + getSampleCount() * sizeof(float);
	}
}
