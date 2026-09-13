#include <lunar/api.hpp>
#include <lunar/core/engine.hpp>

int main()
{
	using namespace lunar;
	auto engine = EngineBuilder()
		.applicationName("Example app")
		.window(
			lunar::Render::WindowBuilder()
				.title("Hello, window!")
				.size(1280, 720)
				.samples(4)
		)
		.build();

	
	lunar::SceneLoader()
		.destination(engine.getActiveScene())
		.useCoreSerializers()
		.loadJsonFile(Fs::fromData("main_scene.json"));

	engine.runGameLoop();

	return 0;
}
//#include <lunar/render.hpp>
//#include <lunar/debug.hpp>
//#include <lunar/file/image_file.hpp>
//
//#include <lunar/core/component.hpp>
//#include <lunar/core/gameobject.hpp>
//#include <lunar/core/scene.hpp>
//#include <lunar/core/time.hpp>
//
//#include <imgui.h>
//#include <imgui_impl_glfw.h>
//#include <imgui_impl_opengl3.h>
//
//#include <string>
//#include <vector>
//
///*
//	Everything (default shaders, scene file, models) is loaded relative to the
//	working directory, so run this example with `resources/` as the working directory.
//*/
//
//using namespace lunar;
//using namespace lunar::Render;
//
//class Test2Comp : public Component_T
//{
//public:
//	Test2Comp(std::string str) : name(str) {}
//
//	void start() override
//	{
//		DEBUG_LOG("Test2Comp started (name: {})", name);
//	}
//
//	static Test2Comp Deserialize(const nlohmann::json& json)
//	{
//		return Test2Comp(json["name"]);
//	}
//
//	static nlohmann::json Serialize(const Test2Comp& comp)
//	{
//		auto json_object = nlohmann::json();
//		json_object["name"] = comp.name;
//		return json_object;
//	}
//
//	std::string name;
//};
//
//class TestComp : public Component_T
//{
//public:
//	TestComp(float x, float y) : x(x), y(y) {}
//
//	void start() override
//	{
//		DEBUG_LOG("TestComp started on '{}' (x: {}, y: {})", getGameObject()->getName(), x, y);
//	}
//
//	static TestComp Deserialize(const nlohmann::json& json)
//	{
//		return TestComp(json["x"], json["y"]);
//	}
//
//	static nlohmann::json Serialize(const TestComp& object)
//	{
//		auto json_object = nlohmann::json();
//		json_object["x"] = object.x;
//		json_object["y"] = object.y;
//		return json_object;
//	}
//
//	float x, y;
//};
//
//class SimpleMovement : public Component_T
//{
//	void update() override
//	{
//		auto& transform  = getTransform();
//		auto* camera     = getGameObject()->getComponent<Camera>();
//		float delta_time = static_cast<float>(Time::DeltaTime());
//		float speed      = Input::GetAction("sprint") ? 8.f * 4.f : 8.f;
//		auto  axis       = Input::GetAxis();
//		auto  rotation   = Input::GetRotation();
//
//		transform.position += camera->right * axis.x * delta_time * speed;
//		transform.position += camera->front * axis.y * delta_time * speed;
//		transform.rotation += glm::vec3{ rotation.x, rotation.y, 0.f };
//
//		// Only the pitch is clamped, so the camera can still turn all the way around.
//		transform.rotation.y = glm::clamp(transform.rotation.y, -89.f, 89.f);
//	}
//};
//
///*
//	Fallback assets, used when the original model/skybox files are not present.
//*/
//
//GpuMesh CreateCheckerCube(RenderContext_T* context)
//{
//	/*
//		The default PBR shader reads its materials from a storage buffer (binding 2):
//		20 materials followed by one material index per triangle. This mirrors the
//		layout written by GpuMeshBuilder when loading GLTF files.
//	*/
//	struct Material
//	{
//		glm::vec2 atlasBegin = { 0.f, 0.f };
//		glm::vec2 atlasEnd   = { 1.f, 1.f };
//		float     metallic   = 0.1f;
//		float     roughness  = 0.5f;
//		float     ao         = 1.0f;
//		float     padding    = 0.f;
//	};
//	static_assert(sizeof(Material) == 32, "Material must match the std430 layout used by pbr.frag");
//
//	struct Face { glm::vec3 normal, u, v; };
//	const Face faces[] =
//	{
//		{ {  1, 0, 0 }, { 0, 0,-1 }, { 0, 1, 0 } },
//		{ { -1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 } },
//		{ {  0, 1, 0 }, { 1, 0, 0 }, { 0, 0,-1 } },
//		{ {  0,-1, 0 }, { 1, 0, 0 }, { 0, 0, 1 } },
//		{ {  0, 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 } },
//		{ {  0, 0,-1 }, {-1, 0, 0 }, { 0, 1, 0 } },
//	};
//
//	auto vertices = std::vector<Vertex>();
//	auto indices  = std::vector<uint32_t>();
//	for (const auto& face : faces)
//	{
//		auto first = static_cast<uint32_t>(vertices.size());
//		for (glm::vec2 corner : { glm::vec2{ 0, 0 }, glm::vec2{ 1, 0 }, glm::vec2{ 1, 1 }, glm::vec2{ 0, 1 } })
//		{
//			auto position = face.normal + face.u * (corner.x * 2.f - 1.f) + face.v * (corner.y * 2.f - 1.f);
//			vertices.push_back(Vertex{ position, corner.x, face.normal, corner.y, glm::vec4{ 1.f } });
//		}
//		indices.insert(indices.end(), { first, first + 1, first + 2, first + 2, first + 3, first });
//	}
//
//	auto material_data = std::vector<uint8_t>(sizeof(Material) * 20 + sizeof(int) * (indices.size() / 3), 0);
//	*reinterpret_cast<Material*>(material_data.data()) = Material{};
//
//	std::vector<uint32_t> checker = {};
//	for (size_t y = 0; y < 16; y++)
//		for (size_t x = 0; x < 16; x++)
//			checker.push_back(((x / 2) % 2) ^ ((y / 2) % 2) ? 0xFFE0E0E0 : 0xFF303030);
//
//	GpuTexture atlas = GpuTextureBuilder()
//		.fromByteBuffer(TextureFormat::eRGBA, TextureDataFormat::eUnsignedByte, 16, 16, checker.data())
//		.destFormat(TextureFormat::eRGBA)
//		.type(TextureType::e2D)
//		.wrapping(TextureWrapping::eRepeat)
//		.minFiltering(TextureFiltering::eNearest)
//		.magFiltering(TextureFiltering::eNearest)
//		.build(context);
//
//	return context->createMesh
//	(
//		context->createBuffer(GpuBufferType::eVertex, GpuBufferUsageFlagBits::eStatic, vertices.size() * sizeof(Vertex), vertices.data()),
//		context->createBuffer(GpuBufferType::eIndex, GpuBufferUsageFlagBits::eStatic, indices.size() * sizeof(uint32_t), indices.data()),
//		MeshTopology::eTriangles,
//		context->createBuffer(GpuBufferType::eShaderStorage, GpuBufferUsageFlagBits::eStatic, material_data.size(), material_data.data()),
//		atlas
//	);
//}
//
//GpuCubemap CreateSkybox(RenderContext_T* context)
//{
//	auto hdr_path = Fs::fromData("skybox/sky.hdr");
//	if (Fs::fileExists(hdr_path))
//	{
//		auto image = Fs::ImageFile(hdr_path);
//		return context->createCubemap(image.width, image.height, image.bytes, true);
//	}
//
//	DEBUG_WARN("'{}' not found, using a generated sky gradient.", hdr_path.generic_string());
//
//	// Equirectangular image: the first row points straight down, the last one straight up.
//	constexpr int width  = 256;
//	constexpr int height = 128;
//
//	const glm::vec3 ground  = { 0.20f, 0.17f, 0.14f };
//	const glm::vec3 horizon = { 1.50f, 1.40f, 1.25f };
//	const glm::vec3 zenith  = { 0.25f, 0.45f, 0.95f };
//
//	auto pixels = std::vector<float>();
//	pixels.reserve(width * height * 3);
//	for (int y = 0; y < height; y++)
//	{
//		float     t     = static_cast<float>(y) / (height - 1);
//		glm::vec3 color = (t < 0.5f)
//			? glm::mix(ground, horizon, t / 0.5f)
//			: glm::mix(horizon, zenith, (t - 0.5f) / 0.5f);
//
//		for (int x = 0; x < width; x++)
//			pixels.insert(pixels.end(), { color.r, color.g, color.b });
//	}
//
//	return context->createCubemap(width, height, pixels.data(), true);
//}
//
///*
//	Debug UI (the engine's own debug panels are currently disabled).
//*/
//
//void DrawGameObjectNode(GameObject_T& object)
//{
//	auto name = std::string(object.getName());
//
//	ImGui::PushID(static_cast<int>(object.getId()));
//	if (ImGui::TreeNode(name.c_str()))
//	{
//		auto& transform = object.getTransform();
//		ImGui::DragFloat3("Position", &transform.position.x, 0.1f);
//		ImGui::DragFloat3("Rotation", &transform.rotation.x, 1.0f);
//		ImGui::DragFloat3("Scale",    &transform.scale.x,    0.1f);
//
//		for (auto& component : object.getComponents())
//			ImGui::BulletText("%s", component->getClassName());
//
//		for (auto& child : object.getChildren())
//			DrawGameObjectNode(child.get());
//
//		ImGui::TreePop();
//	}
//	ImGui::PopID();
//}
//
//void DrawDebugPanel(Scene& scene, Window_T& window)
//{
//	auto title = std::format("Scene: {}", scene.getName());
//
//	ImGui::Begin(title.c_str());
//	ImGui::Text("FPS: %d", Time::GetGlobalContext()->fps.load());
//	ImGui::Text("Cursor: %s (Esc to toggle)", window.isCursorLocked() ? "locked" : "free");
//	ImGui::SeparatorText("Game objects");
//
//	for (auto& object : scene.getGameObjects())
//		if (object.getParent() == nullptr)
//			DrawGameObjectNode(object);
//
//	ImGui::End();
//}
//
//int main()
//{
//	RenderContext context = std::make_shared<RenderContext_T>();
//	Window        window  = WindowBuilder()
//		.size(1280, 720)
//		.samples(4)
//		.build(context, "lunar - app example");
//
//	auto scene = Scene();
//
//	scene.addEventListener<Events::SceneObjectCreated>([](auto& e) {
//		DEBUG_LOG("Event triggered | created {}", e.gameObject->getName());
//	});
//
//	SceneLoader()
//		.destination(scene)
//		.useRenderContext(context)
//		.useCoreSerializers()
//		.useClassSerializer<TestComp>("testComp")
//		.useClassSerializer<Test2Comp>("test2Comp")
//		.loadJsonFile(Fs::fromData("main_scene.json"));
//
//	GameObject player = scene.getGameObject("Skibidi Toilet");
//	GameObject target = scene.getGameObject("Test Object");
//	DEBUG_ASSERT(player != nullptr && target != nullptr, "main_scene.json is missing the expected game objects");
//
//	Camera* camera = player->addComponent<Camera>();
//	player->addComponent<SimpleMovement>();
//	player->getTransform().rotation.x = -90.f; // face -Z, towards "Test Object"
//	scene.setMainCamera(camera);
//
//	scene.createGameObject("Bruh Moment", target.pointer());
//
//	auto  house_path = Fs::fromData("models/house.gltf");
//	auto* renderer   = target->addComponent<MeshRenderer>();
//	renderer->program = context->getProgram(GpuDefaultPrograms::eBasicPbrShader);
//	renderer->mesh    = Fs::fileExists(house_path)
//		? GpuMeshBuilder().useRenderContext(context.get()).fromMeshFile(house_path).build()
//		: CreateCheckerCube(context.get());
//
//	GpuCubemap skybox = CreateSkybox(context.get());
//
//	window->registerAction("toggle_menu", { { "keyboard.esc" } });
//	window->registerAction("sprint",      { { "keyboard.shift" } });
//	Input::SetGlobalHandler(&window.get());
//
//	Time::Update();
//	DEBUG_LOG("Game started in {} seconds.", Time::GetGlobalContext()->currentTime.load());
//
//	while (window->isActive())
//	{
//		Window_T::pollEvents();
//		Time::Update();
//
//		if (window->getActionDown("toggle_menu"))
//			window->toggleCursorLocked();
//
//		scene.update();
//
//		context->begin(&window.get());
//		context->clear(1.f, 1.f, 1.f, 1.f);
//		context->useCamera(camera);
//		context->draw(skybox);
//		context->draw(scene);
//
//		ImGui::SetCurrentContext(window->imguiGetHandle());
//		ImGui_ImplOpenGL3_NewFrame();
//		ImGui_ImplGlfw_NewFrame();
//		ImGui::NewFrame();
//		DrawDebugPanel(scene, window.get());
//		ImGui::Render();
//		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
//
//		context->end();
//		window->update();
//	}
//
//	return 0;
//}
