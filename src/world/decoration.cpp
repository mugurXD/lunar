#include <lunar/world/decoration.hpp>
#include <lunar/file/json_file.hpp>
#include <lunar/render/model.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <format>
#include <functional>

namespace lunar::World
{
	namespace
	{
		constexpr uint32_t CATALOG_FORMAT_VERSION = 1;

		nlohmann::json SerializeType(const DecorationType& type)
		{
			nlohmann::json lods = nlohmann::json::array();
			for (const DecorationLod& lod : type.lods)
				lods.push_back({ { "model", lod.model }, { "distance", lod.distance }, { "fade", lod.fadeLength } });

			return nlohmann::json
			{
				{ "name",      type.name },
				{ "lods",      std::move(lods) },
				{ "breakable", type.breakable },
				{ "mass",      type.mass }
			};
		}

		DecorationType DeserializeType(const nlohmann::json& json)
		{
			DecorationType type =
			{
				.name      = json.at("name").get<std::string>(),
				.breakable = json.value("breakable", false),
				.mass      = json.value("mass", 0.f)
			};

			for (const nlohmann::json& lod : json.at("lods"))
				type.lods.push_back({ lod.at("model").get<std::string>(), lod.at("distance").get<float>(), lod.value("fade", 0.f) });

			return type;
		}

		bool FadesWithinItsRange(const DecorationLod& lod)
		{
			return lod.fadeLength >= 0.f && lod.fadeLength <= lod.distance;
		}

		bool FadeOverlapsPrevious(const DecorationLod& previous, const DecorationLod& next)
		{
			return next.distance - next.fadeLength < previous.distance;
		}

		bool IsValid(const DecorationType& type)
		{
			return !type.name.empty()
			    && !type.lods.empty()
			    && type.lods.size() <= MAX_INSTANCE_LODS
			    && type.lods.front().distance > 0.f
			    && std::ranges::adjacent_find(type.lods, std::ranges::greater_equal {}, &DecorationLod::distance) == type.lods.end()
			    && std::ranges::all_of(type.lods, FadesWithinItsRange)
			    && std::ranges::adjacent_find(type.lods, FadeOverlapsPrevious) == type.lods.end()
			    && (!type.breakable || type.mass > 0.f);
		}

		bool HasUniqueNames(const std::vector<DecorationType>& types)
		{
			std::vector<std::string_view> names;
			for (const DecorationType& type : types)
				names.push_back(type.name);

			std::ranges::sort(names);
			return std::ranges::adjacent_find(names) == names.end();
		}
	}

	std::optional<DecorationKind> DecorationCatalog::find(std::string_view name) const
	{
		const auto found = std::ranges::find(types, name, &DecorationType::name);
		if (found == types.end())
			return std::nullopt;

		return static_cast<DecorationKind>(found - types.begin());
	}

	nlohmann::json DecorationCatalog::Serialize(const DecorationCatalog& catalog)
	{
		nlohmann::json types = nlohmann::json::array();
		for (const DecorationType& type : catalog.types)
			types.push_back(SerializeType(type));

		return nlohmann::json
		{
			{ "formatVersion", CATALOG_FORMAT_VERSION },
			{ "decorations",   std::move(types) }
		};
	}

	std::optional<DecorationCatalog> DecorationCatalog::Deserialize(const nlohmann::json& json)
	{
		if (json.at("formatVersion").get<uint32_t>() != CATALOG_FORMAT_VERSION)
			return std::nullopt;

		DecorationCatalog catalog;
		for (const nlohmann::json& entry : json.at("decorations"))
			catalog.types.push_back(DeserializeType(entry));

		if (!std::ranges::all_of(catalog.types, IsValid) || !HasUniqueNames(catalog.types))
		{
			Fs::ReportMalformedJson(std::format("decorations need unique names, 1 to {} levels of detail with increasing positive distances whose fades don't overlap the previous level, and a positive mass when breakable", MAX_INSTANCE_LODS));
			return std::nullopt;
		}

		return catalog;
	}

	DecorationLibrary::DecorationLibrary(Render::MeshRegistry& meshes, const DecorationCatalog& catalog, const Fs::Path& directory) noexcept
		: meshes(meshes)
	{
		for (const DecorationType& type : catalog.types)
		{
			Kind kind  = { .type = type };
			bool found = false;

			for (size_t level = 0; level < type.lods.size(); level++)
			{
				const std::optional<Render::ModelData> model = Render::LoadModel(directory / type.lods[level].model);
				if (!model.has_value() || model->mesh.vertices.empty())
					continue;

				if (!found)
				{
					kind.colliders = model->colliders;
					kind.bounds    = Render::Bounds::FromVertices(model->mesh.vertices);
					found          = true;
				}

				kind.lods[level] = { meshes.create(model->mesh), type.lods[level].distance, type.lods[level].fadeLength };
			}

			kinds.push_back(std::move(kind));
		}
	}

	DecorationLibrary::~DecorationLibrary() noexcept
	{
		for (const Kind& kind : kinds)
			for (const InstanceLod& lod : kind.lods)
				meshes.destroy(lod.mesh);
	}

	const DecorationLibrary::Kind* DecorationLibrary::find(DecorationKind kind) const
	{
		return kind < kinds.size() ? &kinds[kind] : nullptr;
	}

	std::optional<DecorationKind> DecorationLibrary::indexOf(std::string_view name) const
	{
		const auto found = std::ranges::find(kinds, name, [](const Kind& kind) -> const std::string& { return kind.type.name; });
		if (found == kinds.end())
			return std::nullopt;

		return static_cast<DecorationKind>(found - kinds.begin());
	}

	size_t DecorationLibrary::size() const
	{
		return kinds.size();
	}
}
