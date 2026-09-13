#include <lunar/core/scene.hpp>
#include <lunar/core/component.hpp>
#include <lunar/render/components.hpp>
#include <lunar/render/context.hpp>
#include <lunar/render/mesh.hpp>
#include <lunar/render/common.hpp>
#include <lunar/file/json_file.hpp>

namespace lunar
{
	using namespace std;

	inline void LoadVec3f
	(
		const nlohmann::json& json,
		const std::string_view& name,
		glm::vec3& out
	)
	{
		if (json.contains(name))
		{
			auto& data = json[name];
			if (data.is_array())
			{
				out.x = data.size() > 0 ? data[0].get<float>() : out.x;
				out.y = data.size() > 1 ? data[1].get<float>() : out.y;
				out.z = data.size() > 2 ? data[2].get<float>() : out.z;;
			}
			else
			{
				out.x = data.value("x", out.x);
				out.y = data.value("y", out.y);
				out.z = data.value("z", out.z);
			}
		}
	}

	SceneLoader& SceneLoader::useRenderContext(Render::RenderContext context)
	{
		this->renderContext = context;
		return *this;
	}

	SceneLoader& SceneLoader::useCoreSerializers()
	{
		useCustomClassSerializer("core.render.camera",        [](const nlohmann::json& json) -> Component { 
			auto camera = make_shared<Camera>();
			camera->fov = json.value<float>("fov", camera->fov);
			
			LoadVec3f(json, "front", camera->front);
			LoadVec3f(json, "right", camera->right);
			LoadVec3f(json, "up",    camera->up);

			return camera;
		});

		useCustomClassSerializer("core.render.mesh_renderer", [&](const nlohmann::json& json) -> Component {
			auto mesh_renderer     = make_shared<MeshRenderer>();
			mesh_renderer->program = renderContext->getProgram(Render::GpuDefaultPrograms::eBasicPbrShader);

			if (json.contains("meshPath"))
			{
				auto path = Fs::fromData(json["meshPath"]);
				auto mesh_builder = Render::GpuMeshBuilder();
				auto mesh = mesh_builder
					.useRenderContext(renderContext.get())
					.fromMeshFile(path)
					.build();

				mesh_renderer->mesh = mesh;
			}

			return mesh_renderer;
		});

		

		return *this;
	}

	SceneLoader& SceneLoader::destination(Scene& scene)
	{
		this->result = &scene;
		return *this;
	}

	SceneLoader& SceneLoader::useCustomClassSerializer(const std::string& name, const ComponentJsonParser& parser)
	{
		visitors[name] = parser;
		return *this;
	}

	void SceneLoader::parseTransform
	(
		GameObject            object,
		const nlohmann::json& json
	)
	{
		auto& transform = object->getTransform();
		if (json.contains("transform"))
		{
			auto& data = json["transform"];
			LoadVec3f(data, "position", transform.position);
			LoadVec3f(data, "rotation", transform.rotation);
			LoadVec3f(data, "scale",    transform.scale);
		}
	}

	inline void LoadComponent
	(
		GameObject               object,
		SceneLoader::VisitorDict visitors,
		const nlohmann::json&    json
	)
	{
		std::string type = json["type"];
		if (not visitors.contains(type))
		{
			DEBUG_WARN("Found component of type '{}' but no visitor to parse it. Skipping...", type);
			return;
		}

		auto parser    = visitors.at(type);
		auto component = parser(json);
		if (component == nullptr)
		{
			DEBUG_WARN("Parsed component of type '{}', yet result was nullptr. Skipping...", type);
			return;
		}

		object->addComponent(component);
	}

	void SceneLoader::parseComponents
	(
		GameObject            object,
		const nlohmann::json& json
	)
	{
		if (json.contains("components"))
		{
			auto& data = json["components"];
			for(auto& [ key, component ] : data.items())
				LoadComponent(object, visitors, component);
		}
	}

	void SceneLoader::parseGameObject
	(
		const nlohmann::json& json,
		GameObject            parent
	)
	{
		const std::string name = json.contains("name")
			? json["name"]
			: "GameObject";

		GameObject object = result->createGameObject(name, parent);
		parseTransform(object, json);
		parseComponents(object, json);

		if (json.contains("children"))
		{
			auto& children = json["children"];
			for (auto& [key, child] : children.items())
				parseGameObject(child, object);
		}
	}

	SceneLoader& SceneLoader::loadJsonFile(const Fs::Path& path)
	{
		auto  json_file = Fs::JsonFile(path);
		auto& json      = json_file.content;
		
		result->setName(json.value<std::string>("name", "Unnamed Scene"));
		
		if (json.contains("gameObjects"))
		{
			auto& data = json["gameObjects"];
			for(auto& [ key, object ] : data.items())
				parseGameObject(object, nullptr);
		}

		return *this;
	}
}
