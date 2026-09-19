#include <lunar/core/engine.hpp>
#include <lunar/render/components.hpp>
#include <lunar/file/image_file.hpp>
#include <lunar/debug.hpp>

#include <string>
#include <vector>

using namespace lunar;
using namespace lunar::Render;

namespace
{
	constexpr float     MOVE_SPEED            = 8.f;
	constexpr float     SPRINT_MULTIPLIER     = 24.f;
	constexpr float     MAX_CAMERA_PITCH      = 89.f;
	constexpr float     CUBE_SPIN_SPEED       = 45.f;
	constexpr float     MOON_SPIN_SPEED       = 120.f;
	constexpr int       WINDOW_WIDTH          = 1280;
	constexpr int       WINDOW_HEIGHT         = 720;
	constexpr int       WINDOW_SAMPLES        = 4;
	constexpr int       CHECKER_SIZE          = 16;
	constexpr int       CHECKER_CELL_SIZE     = 2;
	constexpr uint32_t  CHECKER_LIGHT_COLOR   = 0xFFE0E0E0;
	constexpr uint32_t  CHECKER_DARK_COLOR    = 0xFF303030;
	constexpr size_t    SHADER_MATERIAL_COUNT = 20;
	constexpr int       SKY_WIDTH             = 256;
	constexpr int       SKY_HEIGHT            = 128;
	constexpr int       SKY_CHANNELS          = 3;
	constexpr float     SKY_HORIZON_HEIGHT    = 0.5f;

	constexpr float     FPS_REFRESH_SECONDS   = 3.f;

	const glm::vec3 CAMERA_START_POSITION = { 0.f, 1.f, 8.f };
	constexpr float     CAMERA_START_YAW      = -90.f;
	constexpr float     CAMERA_START_PITCH    = -8.f;
	const glm::vec3     SPIN_AXIS             = { 0.f, 1.f, 0.f };
	const glm::vec3 MOON_OFFSET           = { 3.f, 0.f, 0.f };
	const glm::vec3 MOON_SCALE            = { 0.4f, 0.4f, 0.4f };
	const glm::vec3 FLOOR_POSITION        = { 0.f, -2.f, 0.f };
	const glm::vec3 FLOOR_SCALE           = { 20.f, 0.1f, 20.f };
	const glm::vec3 SUN_DIRECTION         = { -0.4f, -1.f, -0.3f };
	const glm::vec3 SKY_GROUND_COLOR      = { 0.20f, 0.17f, 0.14f };
	const glm::vec3 SKY_HORIZON_COLOR     = { 1.50f, 1.40f, 1.25f };
	const glm::vec3 SKY_ZENITH_COLOR      = { 0.25f, 0.45f, 0.95f };
}

class SimpleMovement : public Component_T
{
public:
	SimpleMovement(float yaw, float pitch) : yaw(yaw), pitch(pitch) {}

	void update(const FrameTime& frame_time) override
	{
		const Camera*   camera    = getGameObject()->getComponent<Camera>();
		const float     speed     = Input::GetAction("sprint") ? MOVE_SPEED * SPRINT_MULTIPLIER : MOVE_SPEED;
		const glm::vec2 axis      = Input::GetAxis();
		const glm::vec2 rotation  = Input::GetRotation();
		Transform&      transform = getTransform();

		yaw   += rotation.x;
		pitch  = glm::clamp(pitch + rotation.y, -MAX_CAMERA_PITCH, MAX_CAMERA_PITCH);

		transform.position += camera->right * axis.x * frame_time.deltaTime * speed;
		transform.position += camera->front * axis.y * frame_time.deltaTime * speed;
		transform.rotation  = CameraRotation(yaw, pitch);
	}

private:
	float yaw   = 0.f;
	float pitch = 0.f;
};

class Spinner : public Component_T
{
public:
	Spinner(float degrees_per_second) : degreesPerSecond(degrees_per_second) {}

	void update(const FrameTime& frame_time) override
	{
		getTransform().rotation = glm::angleAxis(glm::radians(degreesPerSecond * frame_time.deltaTime), SPIN_AXIS) * getTransform().rotation;
	}

private:
	float degreesPerSecond = 0.f;
};
//
//GpuMesh CreateCheckerCube(RenderContext_T& context)
//{
//	struct ShaderMaterial
//	{
//		glm::vec2 atlasBegin = { 0.f, 0.f };
//		glm::vec2 atlasEnd   = { 1.f, 1.f };
//		float     metallic   = 0.1f;
//		float     roughness  = 0.5f;
//		float     ao         = 1.0f;
//		float     padding    = 0.f;
//	};
//	static_assert(sizeof(ShaderMaterial) == 32, "ShaderMaterial must match the std430 layout used by pbr.frag");
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
//	for (const Face& face : faces)
//	{
//		const auto first_index = static_cast<uint32_t>(vertices.size());
//		for (const glm::vec2 corner : { glm::vec2{ 0, 0 }, glm::vec2{ 1, 0 }, glm::vec2{ 1, 1 }, glm::vec2{ 0, 1 } })
//		{
//			const glm::vec3 position = face.normal + face.u * (corner.x * 2.f - 1.f) + face.v * (corner.y * 2.f - 1.f);
//			vertices.push_back(Vertex{ position, corner.x, face.normal, corner.y, glm::vec4{ 1.f } });
//		}
//		indices.insert(indices.end(), { first_index, first_index + 1, first_index + 2, first_index + 2, first_index + 3, first_index });
//	}
//
//	auto material_data = std::vector<uint8_t>(sizeof(ShaderMaterial) * SHADER_MATERIAL_COUNT + sizeof(int) * (indices.size() / 3), 0);
//	*reinterpret_cast<ShaderMaterial*>(material_data.data()) = ShaderMaterial{};
//
//	auto checker_pixels = std::vector<uint32_t>();
//	for (int y = 0; y < CHECKER_SIZE; y++)
//		for (int x = 0; x < CHECKER_SIZE; x++)
//			checker_pixels.push_back(((x / CHECKER_CELL_SIZE) % 2) ^ ((y / CHECKER_CELL_SIZE) % 2) ? CHECKER_LIGHT_COLOR : CHECKER_DARK_COLOR);
//
//	const GpuTexture atlas = GpuTextureBuilder()
//		.fromByteBuffer(TextureFormat::eRGBA, TextureDataFormat::eUnsignedByte, CHECKER_SIZE, CHECKER_SIZE, checker_pixels.data())
//		.destFormat(TextureFormat::eRGBA)
//		.type(TextureType::e2D)
//		.wrapping(TextureWrapping::eRepeat)
//		.minFiltering(TextureFiltering::eNearest)
//		.magFiltering(TextureFiltering::eNearest)
//		.build(&context);
//
//	return context.createMesh
//	(
//		context.createBuffer(GpuBufferType::eVertex,        GpuBufferUsageFlagBits::eStatic, vertices.size() * sizeof(Vertex),  vertices.data()),
//		context.createBuffer(GpuBufferType::eIndex,         GpuBufferUsageFlagBits::eStatic, indices.size() * sizeof(uint32_t), indices.data()),
//		MeshTopology::eTriangles,
//		context.createBuffer(GpuBufferType::eShaderStorage, GpuBufferUsageFlagBits::eStatic, material_data.size(),              material_data.data()),
//		atlas
//	);
//}
//
//GpuCubemap CreateSkybox(RenderContext_T& context)
//{
//	const Fs::Path hdr_path = Fs::fromData("skybox/sky.hdr");
//	if (Fs::fileExists(hdr_path))
//	{
//		const Fs::ImageFile image = Fs::ImageFile(hdr_path);
//		return context.createCubemap(image.width, image.height, image.bytes, true);
//	}
//
//	DEBUG_WARN("'{}' not found, using a generated sky gradient.", hdr_path.generic_string());
//
//	auto pixels = std::vector<float>();
//	pixels.reserve(SKY_WIDTH * SKY_HEIGHT * SKY_CHANNELS);
//	for (int y = 0; y < SKY_HEIGHT; y++)
//	{
//		const float     height = static_cast<float>(y) / (SKY_HEIGHT - 1);
//		const glm::vec3 color  = (height < SKY_HORIZON_HEIGHT)
//			? glm::mix(SKY_GROUND_COLOR, SKY_HORIZON_COLOR, height / SKY_HORIZON_HEIGHT)
//			: glm::mix(SKY_HORIZON_COLOR, SKY_ZENITH_COLOR, (height - SKY_HORIZON_HEIGHT) / (1.f - SKY_HORIZON_HEIGHT));
//
//		for (int x = 0; x < SKY_WIDTH; x++)
//			pixels.insert(pixels.end(), { color.r, color.g, color.b });
//	}
//
//	return context.createCubemap(SKY_WIDTH, SKY_HEIGHT, pixels.data(), true);
//}

int main()
{
	auto engine = EngineBuilder()
		.applicationName("Example app")
		.renderBackend(lunar::Render::Backend::eVulkan)
		.window(
			WindowBuilder()
				.title("lunar - example")
				.size(WINDOW_WIDTH, WINDOW_HEIGHT)
				.samples(WINDOW_SAMPLES)
		)
		.build();

	Scene&           scene     = engine.getActiveScene();
	Window_T&        window    = engine.getWindow();
	const MeshHandle cube_mesh = engine.getRenderer().getCubeMesh();

	GameObject player = scene.createGameObject("Player");
	player->getTransform().position = CAMERA_START_POSITION;
	player->getTransform().rotation = CameraRotation(CAMERA_START_YAW, CAMERA_START_PITCH);
	player->addComponent<Camera>();
	player->addComponent<SimpleMovement>(CAMERA_START_YAW, CAMERA_START_PITCH);
	scene.setMainCamera(player);

	GameObject cube = scene.createGameObject("Cube");
	cube->addComponent<MeshRenderer>(cube_mesh);
	cube->addComponent<Spinner>(CUBE_SPIN_SPEED);

	GameObject moon = cube->createChildObject("Moon");
	moon->getTransform().position = MOON_OFFSET;
	moon->getTransform().scale    = MOON_SCALE;
	moon->addComponent<MeshRenderer>(cube_mesh);
	moon->addComponent<Spinner>(MOON_SPIN_SPEED);

	GameObject ground = scene.createGameObject("Floor");
	ground->getTransform().position = FLOOR_POSITION;
	ground->getTransform().scale    = FLOOR_SCALE;
	ground->addComponent<MeshRenderer>(cube_mesh);

	GameObject sun = scene.createGameObject("Sun");
	sun->addComponent<DirectionalLight>(SUN_DIRECTION);

	//scene.setEnvironment(CreateSkybox(context));

	window.registerAction("toggle_menu", { { "keyboard.esc" } });
	window.registerAction("sprint",      { { "keyboard.shift" } });

	float seconds = 0;
	engine.addSystem(SystemPhase::eUpdate, [&window, &seconds](Scene&, const FrameTime& ft) {
		if (window.getActionDown("toggle_menu"))
			window.toggleCursorLocked();

		seconds += ft.deltaTime;
		if (seconds > FPS_REFRESH_SECONDS)
		{
			seconds = 0.f;
			window.setTitle(std::string("lunar - example | FPS: ") + std::to_string(static_cast<int>(1.f / ft.deltaTime)));
		}
	});

	engine.runGameLoop();

	return 0;
}
