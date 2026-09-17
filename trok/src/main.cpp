#include <trok/world/region_plan.hpp>
#include <trok/world/terrain_generator.hpp>

#include <lunar/core/engine.hpp>
#include <lunar/render/components.hpp>
#include <lunar/world/chunk_storage.hpp>
#include <lunar/world/region_store.hpp>
#include <lunar/world/terrain_generator.hpp>
#include <lunar/world/terrain_world.hpp>
#include <lunar/world/world_storage.hpp>
#include <lunar/debug.hpp>

#include <format>
#include <memory>
#include <optional>
#include <string_view>

using namespace lunar;
using namespace lunar::Render;

namespace
{
	constexpr float                   MOVE_SPEED           = 8.f;
	constexpr float                   SPRINT_MULTIPLIER    = 24.f;
	constexpr float                   MAX_CAMERA_PITCH     = 89.f;
	constexpr float                   CAMERA_START_HEIGHT  = 25.f;
	constexpr float                   FPS_REFRESH_SECONDS  = 3.f;
	constexpr int                     WINDOW_WIDTH         = 1280;
	constexpr int                     WINDOW_HEIGHT        = 720;
	constexpr int                     WINDOW_SAMPLES       = 4;
	constexpr int32_t                 VIEW_RADIUS          = 64;
	constexpr std::string_view        WINDOW_TITLE         = "trok";
	constexpr std::string_view        FPS_TITLE_FORMAT     = "trok | FPS: {}";
	constexpr std::string_view        WORLD_SAVE_DIRECTORY = "saves/trok";
	constexpr std::string_view        CHUNK_DIRECTORY      = "chunks";
	constexpr World::WorldInfo        NEW_WORLD_INFO       = { .seed = 1337, .generatorVersion = 1 };

	const glm::vec3 CAMERA_START_POSITION = { 0.f, 0.f, 30.f };
	const glm::vec3 CAMERA_START_ROTATION = { -90.f, -15.f, 0.f };
	const glm::vec3 SUN_DIRECTION         = { -0.4f, -1.f, -0.3f };

	class FlyCamera : public Component_T
	{
	public:
		void update(const FrameTime& frame_time) override
		{
			const Camera*   camera    = getGameObject()->getComponent<Camera>();
			const float     speed     = Input::GetAction("sprint") ? MOVE_SPEED * SPRINT_MULTIPLIER : MOVE_SPEED;
			const glm::vec2 axis      = Input::GetAxis();
			const glm::vec2 rotation  = Input::GetRotation();
			Transform&      transform = getTransform();

			transform.position   += camera->right * axis.x * frame_time.deltaTime * speed;
			transform.position   += camera->front * axis.y * frame_time.deltaTime * speed;
			transform.rotation   += glm::vec3 { rotation.x, rotation.y, 0.f };
			transform.rotation.y  = glm::clamp(transform.rotation.y, -MAX_CAMERA_PITCH, MAX_CAMERA_PITCH);
		}
	};
}

int main()
{
	auto engine = EngineBuilder()
		.applicationName(WINDOW_TITLE)
		.renderBackend(Backend::eVulkan)
		.window(
			WindowBuilder()
				.title(WINDOW_TITLE)
				.size(WINDOW_WIDTH, WINDOW_HEIGHT)
				.samples(WINDOW_SAMPLES)
		)
		.build();

	Scene&    scene  = engine.getActiveScene();
	Window_T& window = engine.getWindow();

	std::optional<World::WorldStorage> opened_storage = World::WorldStorage::openOrCreate(Fs::fromBase(WORLD_SAVE_DIRECTORY), NEW_WORLD_INFO);
	if (!opened_storage.has_value())
	{
		DEBUG_ERROR("Could not open or create the world in '{}'", WORLD_SAVE_DIRECTORY);
		return 1;
	}

	const auto                 world_storage     = std::make_shared<const World::WorldStorage>(std::move(*opened_storage));
	const World::WorldSettings world_settings    = { .viewRadius = VIEW_RADIUS };
	const auto                 chunk_storage     = std::make_shared<const World::ChunkStorage>(world_storage->getDirectory() / CHUNK_DIRECTORY, world_settings);
	const auto                 terrain_generator = std::make_shared<const trok::TerrainGenerator>(trok::TerrainSettings { .seed = world_storage->getInfo().seed });
	const auto                 region_planner    = std::make_shared<const trok::RegionPlanner>(world_storage->getInfo().generatorVersion);

	World::RegionStore<trok::RegionPlan>       regions(engine.getJobSystem(), world_storage, region_planner, world_settings);
	World::RegionChunkSource<trok::RegionPlan> chunk_source(regions, terrain_generator, chunk_storage, world_settings);
	World::TerrainWorld                        terrain(scene, engine.getJobSystem(), engine.getRenderer().getMeshes(), chunk_source, world_settings);

	const trok::RegionContext spawn_context(world_settings, {});

	GameObject player = scene.createGameObject("Player");
	player->getTransform().position   = CAMERA_START_POSITION;
	player->getTransform().position.y = terrain_generator->sampleHeight(spawn_context, CAMERA_START_POSITION.x, CAMERA_START_POSITION.z) + CAMERA_START_HEIGHT;
	player->getTransform().rotation   = CAMERA_START_ROTATION;
	player->addComponent<Camera>();
	player->addComponent<FlyCamera>();
	scene.setMainCamera(player);

	GameObject sun = scene.createGameObject("Sun");
	sun->addComponent<DirectionalLight>(SUN_DIRECTION);

	window.registerAction("toggle_menu", { { "keyboard.esc" } });
	window.registerAction("sprint",      { { "keyboard.shift" } });

	engine.addSystem(SystemPhase::eUpdate, [&regions, &terrain, &player](Scene&, const FrameTime&) {
		regions.update(player->getTransform().position);
		terrain.update(player->getTransform().position);
	});

	float seconds_since_title_update = 0.f;
	engine.addSystem(SystemPhase::eUpdate, [&window, &seconds_since_title_update](Scene&, const FrameTime& frame_time) {
		if (window.getActionDown("toggle_menu"))
			window.toggleCursorLocked();

		seconds_since_title_update += frame_time.deltaTime;
		if (seconds_since_title_update < FPS_REFRESH_SECONDS)
			return;

		seconds_since_title_update = 0.f;
		window.setTitle(std::format(FPS_TITLE_FORMAT, static_cast<int>(1.f / frame_time.deltaTime)));
	});

	engine.runGameLoop();

	return 0;
}
