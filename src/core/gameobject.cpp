#include <lunar/core/gameobject.hpp>
#include <lunar/core/component.hpp>
#include <lunar/core/scene.hpp>
#include <lunar/debug/log.hpp>
#include <functional>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <map>

namespace lunar
{
	template<typename T>
	T& GetRequiredComponent(Scene* scene, const Entity& entity)
	{
		T* component = scene->getComponent<T>(entity);
		DEBUG_ASSERT(component != nullptr, "GameObject is missing one of its core components");
		return *component;
	}

	GameObject::GameObject(Scene* scene, const Entity& entity) noexcept
		: scene(scene),
		entity(entity)
	{
	}

	bool GameObject::valid() const
	{
		return scene != nullptr && entity.valid();
	}

	Entity GameObject::getEntity() const
	{
		return entity;
	}

	Scene* GameObject::getScene() const
	{
		return scene;
	}

	std::string_view GameObject::getName() const
	{
		return GetRequiredComponent<Name>(scene, entity).value;
	}

	GameObject GameObject::getParent() const
	{
		return GameObject(scene, GetRequiredComponent<Hierarchy>(scene, entity).parent);
	}

	std::vector<GameObject> GameObject::getChildren() const
	{
		auto   children = std::vector<GameObject>();
		Entity child    = GetRequiredComponent<Hierarchy>(scene, entity).firstChild;

		while (child != nullptr)
		{
			children.emplace_back(scene, child);
			child = GetRequiredComponent<Hierarchy>(scene, child).nextSibling;
		}

		return children;
	}

	GameObject GameObject::createChildObject(const std::string_view& name)
	{
		return scene->createGameObject(name, *this);
	}

	void GameObject::setParent(GameObject parent)
	{
		DEBUG_ASSERT(parent == nullptr || parent->getScene() == scene, "Parent belongs to another scene");
		scene->setParent(entity, parent.getEntity());
	}

	void GameObject::destroy()
	{
		scene->destroyEntity(entity);
	}

	glm::mat4 GameObject::getWorldTransform() const
	{
		return scene->resolveWorldTransform(entity).matrix;
	}

	glm::vec3 GameObject::getWorldPos() const
	{
		return glm::vec3(scene->resolveWorldTransform(entity).matrix[3]);
	}

	glm::quat GameObject::getWorldRotation() const
	{
		return scene->resolveWorldTransform(entity).rotation;
	}

	glm::vec3 GameObject::getWorldScale() const
	{
		return scene->resolveWorldTransform(entity).scale;
	}

	glm::vec3 GameObject::getLocalPos() const
	{
		return getTransform().position;
	}

	glm::vec3 GameObject::getLocalRotation() const
	{
		return getTransform().rotation;
	}

	glm::vec3 GameObject::getLocalScale() const
	{
		return getTransform().scale;
	}

	void GameObject::setWorldPos(glm::vec3 pos)
	{
		const GameObject parent = getParent();
		if (parent != nullptr)
		{
			auto parent_mat         = parent->getWorldTransform();
			auto parent_inv         = glm::inverse(parent_mat);
			auto local_pos          = parent_inv * glm::vec4(pos, 1.f);
			getTransform().position = glm::vec3(local_pos);
		}
		else
			getTransform().position = pos;
	}

	void GameObject::setLocalPos(glm::vec3 pos)
	{
		getTransform().position = pos;
	}

	const Transform& GameObject::getTransform() const
	{
		return GetRequiredComponent<Transform>(scene, entity);
	}

	Transform& GameObject::getTransform()
	{
		return GetRequiredComponent<Transform>(scene, entity);
	}
}

namespace Core
{
	//GameObject::GameObject(const std::string_view& name, Scene* scene, GameObject* parent) 
	//	: name(name),
	//	nameHash(Lunar::imp::fnv1a_hash(this->name)),
	//	components(),
	//	transform(),
	//	scene(scene),
	//	parent(parent != nullptr ? parent->id : -1),
	//	Identifiable()
	//{
	//}

	//GameObject::GameObject()
	//	: name(),
	//	nameHash(),
	//	components(),
	//	transform(),
	//	scene(nullptr),
	//	parent(),
	//	Identifiable(-1)
	//{
	//}

	//GameObject::~GameObject()
	//{
	//}

	//GameObject* GameObject::getParent()
	//{
	//	if (parent != -1)
	//		return &scene->getGameObject(parent);

	//	return nullptr;
	//}

	//std::span<std::shared_ptr<Component>> GameObject::getComponents()
	//{
	//	return components;
	//}

	//void GameObject::addComponent(std::shared_ptr<Component> constructed)
	//{
	//	constructed->_scene = scene;
	//	constructed->_gameObject = id;
	//	components.emplace_back(constructed);
	//}

	//Component* GameObject::getComponent(const std::type_info& ty)
	//{
	//	for (auto& component : components)
	//		if (typeid(*component).hash_code() == ty.hash_code())
	//			return component.get();

	//	return nullptr;
	//}

	//std::vector<GameObject*> GameObject::getChildren()
	//{
	//	auto children = std::vector<GameObject*> {};
	//	auto& gameObjects = getParentScene()->getGameObjects();
	//	for (auto& object : gameObjects)
	//	{
	//		if (object.getParentId() == id)
	//			children.push_back(&object);
	//	}
	//	return std::move(children);
	//}

	//Identifiable::NativeType GameObject::getParentId() const
	//{
	//	return parent;
	//}

	//TransformComponent& GameObject::getTransform()
	//{
	//	return transform;
	//}

	//const TransformComponent& GameObject::getTransform() const
	//{
	//	return transform;
	//}

 //   size_t GameObject::getNameHash() const
 //   {
 //       return nameHash;
 //   }

 //   const std::string &GameObject::getName() const
 //   {
 //       return name;
 //   }

	//Scene* GameObject::getParentScene()
	//{
	//	return scene;
	//}

	//void GameObject::update()
	//{
	//	for (auto& component : components)
	//	{
	//		//if (component->_getClassFlags() & ComponentClassFlagBits::eUpdateable)
	//		component->update();
	//	}
	//	//for (auto& component_ptr : components)
	//	//{
	//	//	Component* component = component_ptr.get();
	//	//	if (component->isUpdateable())
	//	//		component->update();
	//	//}
	//}

	//void GameObject::renderUpdate(Render::RenderContext& context)
	//{
	//	for (auto& component : components)
	//	{
	//		component->renderUpdate(context);
	//	}
	//}

	//void GameObject::fromJson(nlohmann::json& json)
	//{
	//	using json_obj = nlohmann::json;
	//	using comp_ptr = std::unique_ptr<Component>;

	//	static std::map<std::string, std::function<void(json_obj&, comp_ptr&)>> component_types = {};

 //       name = json["name"];
	//	nameHash = std::hash<std::string>{}(name);
	//	DEBUG_LOG("Loading GameObject \"{}\" from json object.", name);
	//	if (json.contains("components"))
	//	{
	//		auto& components_json = json["components"];
	//		for (auto& [key, comp_data] : components_json.items())
	//		{
	//			std::string type = comp_data["type"];
	//			if (!component_types.contains(type))
	//			{
	//				DEBUG_ERROR("Component type \"{}\" does not exist.", type);
	//				throw;
	//			}

	//			std::unique_ptr<Component> component_ptr;
	//			component_types.at(comp_data["type"])(comp_data, component_ptr);
	//			if(component_ptr != nullptr)
	//				components.push_back(std::move(component_ptr));
	//		}
	//	}
	//	
	//	if (json.contains("transform"))
	//	{
	//		auto& transform_json = json["transform"];
	//		auto& position = transform_json["position"];
	//		auto& rotation = transform_json["rotation"];
	//		auto& scale = transform_json["scale"];
	//		transform.position = glm::dvec3 { position["x"], position["y"], position["z"] };
	//		transform.rotation = glm::dvec3 { rotation["x"], rotation["y"], rotation["z"] };
	//		transform.scale = glm::dvec3 { scale["x"], scale["y"], scale["z"] };
	//	}
	//}
}
