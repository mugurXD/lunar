#include <lunar/core/gameobject.hpp>
#include <lunar/core/component.hpp>
#include <lunar/core/scene.hpp>
#include <lunar/debug/log.hpp>
#include <functional>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <atomic>
#include <map>

namespace lunar
{
	std::atomic<size_t> GAMEOBJECT_COUNTER = 1;

	GameObject_T::GameObject_T(Scene* scene, const std::string_view& name, GameObject parent) noexcept
		: name(name),
		nameHash(lunar::imp::fnv1a_hash(name)),
		parent(parent),
		id(GAMEOBJECT_COUNTER++),
		scene(scene),
		transform()
	{

	}

	size_t GameObject_T::getId() const
	{
		return id;
	}

	Scene* GameObject_T::getScene()
	{
		return scene;
	}

	Component GameObject_T::getComponent(const std::type_info& ty)
	{
		for (auto& component : scene->components)
		{
			if (typeid(*component).hash_code() == ty.hash_code() && component->getGameObject()->getId() == id)
				return component;
		}

		return nullptr;
	}

	std::vector<Component> GameObject_T::getComponents()
	{
		auto list = std::vector<Component>();
		for (auto& component : scene->components)
			if (component->getGameObject() == this)
				list.emplace_back(component);
		return list;
	}

	std::string_view GameObject_T::getName() const
	{
		return name;
	}

	GameObject GameObject_T::getParent()
	{
		return parent;
	}

	Component_T* GameObject_T::addComponent(Component created)
	{
		auto& comp       = scene->components.emplace_back(created);
		comp->gameObject = make_handle(scene->objects, this);
		comp->scene      = scene;
		comp->start();
		return comp.get();
	}

	GameObject GameObject_T::createChildObject(const std::string_view& name)
	{
		return getScene()->createGameObject(name, this);
	}

	std::vector<GameObject> GameObject_T::getChildren()
	{
		auto  children = std::vector<GameObject>();
		auto& objects  = getScene()->objects;
		for (auto& object : objects)
			if (object.parent == this)
				children.push_back(make_handle(objects, &object));
		return children;
	}

	void GameObject_T::update()
	{

	}

	glm::mat4 GameObject_T::getWorldTransform() const
	{
		auto scale       = getWorldScale();
		auto rotation    = getWorldRotation();
		auto position    = getWorldPos();
		auto scale_mat   = glm::scale(glm::mat4(1.f), scale);
		auto translation = glm::translate(glm::mat4(1.f), position);
		auto rot_mat     = glm::mat4(rotation);
		return translation * rot_mat * scale_mat;
	}

	glm::vec3 GameObject_T::getWorldPos() const
	{
		if (parent == nullptr)
			return transform.position;
		else
			return glm::vec3(
				parent->getWorldTransform() * glm::vec4(transform.position, 1)
			);
	}

	glm::quat GameObject_T::getWorldRotation() const
	{
		if (parent == nullptr)
			return glm::quat(glm::radians(transform.rotation));
		else
			return glm::normalize(
				parent->getWorldRotation() * glm::quat(glm::radians(transform.rotation))
			);
	}

	glm::vec3 GameObject_T::getWorldScale() const
	{
		if (parent == nullptr)
			return transform.scale;
		else
			return transform.scale * parent->getWorldScale();
	}

	glm::vec3 GameObject_T::getLocalPos() const
	{
		return transform.position;
	}

	void GameObject_T::setWorldPos(glm::vec3 pos)
	{
		if (parent != nullptr)
		{
			auto parent_mat    = parent->getWorldTransform();
			auto parent_inv    = glm::inverse(parent_mat);
			auto local_pos     = parent_inv * glm::vec4(pos, 1.f);
			transform.position = glm::vec3(local_pos);
		}
		else
			transform.position = pos;
	}

	void GameObject_T::setLocalPos(glm::vec3 pos)
	{
		transform.position = pos;
	}

	const Transform& GameObject_T::getTransform() const
	{
		return transform;
	}

	Transform& GameObject_T::getTransform()
	{
		return transform;
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
