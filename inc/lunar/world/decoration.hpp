#pragma once
#include <lunar/api.hpp>
#include <lunar/file/filesystem.hpp>
#include <lunar/render/components.hpp>
#include <lunar/render/frustum.hpp>
#include <lunar/render/mesh_registry.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <nlohmann/json_fwd.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lunar::World
{
	using DecorationKind = uint16_t;
	using DecorationId   = uint64_t;

	struct LUNAR_API DecorationInstance
	{
		DecorationId   id       = 0;
		DecorationKind kind     = 0;
		glm::vec3      position = {};
		glm::quat      rotation = glm::quat(1.f, 0.f, 0.f, 0.f);
		float          scale    = 1.f;

		bool operator==(const DecorationInstance&) const = default;
	};

	struct LUNAR_API ChunkDecorations
	{
		std::vector<DecorationInstance> generated = {};
	};

	struct LUNAR_API DecorationLod
	{
		std::string model      = {};
		float       distance   = 0.f;
		float       fadeLength = 0.f;

		bool operator==(const DecorationLod&) const = default;
	};

	struct LUNAR_API DecorationType
	{
		std::string                name      = {};
		std::vector<DecorationLod> lods      = {};
		bool                       breakable = false;
		float                      mass      = 0.f;

		bool operator==(const DecorationType&) const = default;
	};

	struct LUNAR_API DecorationCatalog
	{
		std::vector<DecorationType> types = {};

		std::optional<DecorationKind> find(std::string_view name) const;

		bool operator==(const DecorationCatalog&) const = default;

		static nlohmann::json                   Serialize(const DecorationCatalog& catalog);
		static std::optional<DecorationCatalog> Deserialize(const nlohmann::json& json);
	};

	class LUNAR_API DecorationLibrary
	{
	public:
		struct LUNAR_API Kind
		{
			DecorationType                             type      = {};
			std::array<InstanceLod, MAX_INSTANCE_LODS> lods      = {};
			std::vector<std::vector<glm::vec3>>        colliders = {};
			Render::Bounds                             bounds    = {};
		};

		DecorationLibrary(Render::MeshRegistry& meshes, const DecorationCatalog& catalog, const Fs::Path& directory) noexcept;
		~DecorationLibrary() noexcept;

		DecorationLibrary(const DecorationLibrary&)            = delete;
		DecorationLibrary& operator=(const DecorationLibrary&) = delete;

		const Kind*                   find(DecorationKind kind)    const;
		std::optional<DecorationKind> indexOf(std::string_view name) const;
		size_t                        size()                       const;

	private:
		Render::MeshRegistry& meshes;
		std::vector<Kind>     kinds;
	};
}
