#include <trok/world/biome.hpp>
#include <trok/world/climate.hpp>
#include <trok/world/elevation.hpp>
#include <trok/world/region_plan.hpp>
#include <trok/world/terrain_generator.hpp>
#include <trok/vehicle/chase_camera.hpp>
#include <trok/vehicle/truck.hpp>

#include <lunar/core/engine.hpp>
#include <lunar/render/components.hpp>
#include <lunar/world/chunk_storage.hpp>
#include <lunar/physics/raycast.hpp>
#include <lunar/physics/rigid_body.hpp>
#include <lunar/world/region_store.hpp>
#include <lunar/world/terrain_colliders.hpp>
#include <lunar/world/terrain_generator.hpp>
#include <lunar/world/terrain_world.hpp>
#include <lunar/world/world_storage.hpp>
#include <lunar/file/json_file.hpp>
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
	constexpr float                   CAMERA_START_YAW     = -90.f;
	constexpr float                   CAMERA_START_PITCH   = -15.f;
	constexpr float                   FPS_REFRESH_SECONDS  = 3.f;
	constexpr int                     WINDOW_WIDTH         = 1280;
	constexpr int                     WINDOW_HEIGHT        = 720;
	constexpr int                     WINDOW_SAMPLES       = 4;
	constexpr int32_t                 VIEW_RADIUS          = 32;
	constexpr std::string_view        WINDOW_TITLE         = "trok";
	constexpr std::string_view        FPS_TITLE_FORMAT     = "trok | FPS: {}";
	constexpr std::string_view        WORLD_SAVE_DIRECTORY = "saves/trok";
	constexpr std::string_view        CHUNK_DIRECTORY      = "chunks";
	constexpr std::string_view        BIOME_FILE_NAME      = "biomes.json";
	constexpr std::string_view        ELEVATION_FILE_NAME  = "elevation.json";
	constexpr std::string_view        TRUCK_FILE_NAME      = "trucks/box_truck.json";
	constexpr int32_t                 COLLIDER_RADIUS      = 1;
	constexpr float                   TRUCK_SPAWN_HEIGHT   = 1.5f;
	constexpr float                   GROUND_RAY_HEIGHT    = 2000.f;
	constexpr float                   REVERSE_THRESHOLD    = 1.f;
	constexpr float                   AUTO_HOLD_SPEED      = 0.5f;
	constexpr float                   FOG_START_FRACTION   = 0.5f;
	constexpr float                   FOG_END_FRACTION     = 0.95f;
	constexpr World::WorldInfo        NEW_WORLD_INFO       = { .seed = 1337, .generatorVersion = 1 };

	const glm::vec3 CAMERA_START_POSITION = { 0.f, 0.f, 30.f };
	const glm::vec3 SUN_DIRECTION         = { -0.4f, -1.f, -0.3f };
	const glm::vec3 SKY_COLOR             = { 0.6f, 0.745f, 0.76f };

	const Physics::VehicleInput PARKED_INPUT = { .brake = 1.f };

	template<IsJsonSerializable T>
	std::optional<T> LoadGameData(std::string_view file_name)
	{
		std::optional<T> data = Fs::LoadJson<T>(Fs::fromData(file_name));
		if (!data.has_value())
			DEBUG_ERROR("Could not load '{}'", file_name);

		return data;
	}

	bool PlaceAboveTerrain(Transform&                            transform,
	                       World::RegionStore<trok::RegionPlan>& regions,
	                       const trok::TerrainGenerator&         generator,
	                       const World::WorldSettings&           settings)
	{
		const std::optional<trok::RegionContext> context = regions.gatherContext(World::ChunkAt(transform.position, settings));
		if (!context.has_value())
			return false;

		transform.position.y = generator.sampleHeight(*context, transform.position.x, transform.position.z) + CAMERA_START_HEIGHT;
		return true;
	}

	std::optional<float> GroundHeight(Scene& scene, float x, float z)
	{
		const glm::vec3                          top = { x, GROUND_RAY_HEIGHT, z };
		const std::optional<Physics::RaycastHit> hit = Physics::CastRay(*scene.getPhysicsWorld(), top, { x, -GROUND_RAY_HEIGHT, z }, Physics::TERRAIN_CATEGORY);
		return hit.has_value() ? std::optional<float>(hit->point.y) : std::nullopt;
	}

	Physics::VehicleInput ReadDrivingInput(float forward_speed)
	{
		const glm::vec2 axis     = Input::GetAxis();
		const bool      opposing = axis.y * forward_speed < 0.f && std::abs(forward_speed) > REVERSE_THRESHOLD;
		const bool      holding  = axis.y == 0.f && std::abs(forward_speed) < AUTO_HOLD_SPEED;
		const bool      braking  = Input::GetAction("brake") || holding;

		return Physics::VehicleInput
		{
			.throttle = opposing ? 0.f : axis.y,
			.brake    = braking ? 1.f : (opposing ? std::abs(axis.y) : 0.f),
			.steering = axis.x
		};
	}

	class FlyCamera : public Component_T
	{
	public:
		FlyCamera(float yaw, float pitch) noexcept
			: yaw(yaw),
			pitch(pitch)
		{
		}

		void update(const FrameTime& frame_time) override
		{
			const Camera* camera = getGameObject()->getComponent<Camera>();
			if (getGameObject()->getScene()->getMainCamera() != camera)
				return;

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

	std::optional<trok::BiomeLibrary>   loaded_biomes    = LoadGameData<trok::BiomeLibrary>(BIOME_FILE_NAME);
	std::optional<trok::ElevationCurve> loaded_elevation = LoadGameData<trok::ElevationCurve>(ELEVATION_FILE_NAME);
	const auto                          truck_definition = LoadGameData<trok::TruckDefinition>(TRUCK_FILE_NAME);
	if (!loaded_biomes.has_value() || !loaded_elevation.has_value() || !truck_definition.has_value())
		return 1;

	std::optional<World::WorldStorage> opened_storage = World::WorldStorage::openOrCreate(Fs::fromBase(WORLD_SAVE_DIRECTORY), NEW_WORLD_INFO);
	if (!opened_storage.has_value())
	{
		DEBUG_ERROR("Could not open or create the world in '{}'", WORLD_SAVE_DIRECTORY);
		return 1;
	}

	const auto                 world_storage     = std::make_shared<const World::WorldStorage>(std::move(*opened_storage));
	const World::WorldSettings world_settings    = { .sampleReachChunks = trok::BIOME_SAMPLE_REACH_CHUNKS, .viewRadius = VIEW_RADIUS };
	const auto                 chunk_storage     = std::make_shared<const World::ChunkStorage>(world_storage->getDirectory() / CHUNK_DIRECTORY, world_settings);
	const auto                 biomes            = std::make_shared<const trok::BiomeLibrary>(std::move(*loaded_biomes));
	const auto                 climate           = std::make_shared<const trok::ClimateSampler>(world_storage->getInfo().seed);
	const auto                 terrain_generator = std::make_shared<const trok::TerrainGenerator>(biomes, climate, std::move(*loaded_elevation), world_storage->getInfo().seed);
	const auto                 region_planner    = std::make_shared<const trok::RegionPlanner>(world_storage->getInfo().generatorVersion, world_storage->getInfo().seed, biomes, climate);

	World::RegionStore<trok::RegionPlan>       regions(engine.getJobSystem(), world_storage, region_planner, world_settings);
	World::RegionChunkSource<trok::RegionPlan> chunk_source(regions, terrain_generator, chunk_storage, world_settings);
	World::TerrainWorld                        terrain(scene, engine.getJobSystem(), engine.getRenderer().getMeshes(), chunk_source, world_settings);
	World::TerrainColliders                    colliders(scene, terrain, world_settings, COLLIDER_RADIUS);
	std::optional<trok::Truck>                 truck;

	GameObject player = scene.createGameObject("Player");
	player->getTransform().position = CAMERA_START_POSITION;
	player->getTransform().rotation = CameraRotation(CAMERA_START_YAW, CAMERA_START_PITCH);
	player->addComponent<Camera>();
	player->addComponent<FlyCamera>(CAMERA_START_YAW, CAMERA_START_PITCH);
	scene.setMainCamera(player);

	GameObject        chase_camera = scene.createGameObject("Chase Camera");
	trok::ChaseCamera chase(trok::ChaseCameraSettings {});
	chase_camera->addComponent<Camera>();

	GameObject sun = scene.createGameObject("Sun");
	sun->addComponent<DirectionalLight>(SUN_DIRECTION);

	const float view_distance = static_cast<float>(VIEW_RADIUS) * world_settings.getChunkSize();
	GameObject  sky           = scene.createGameObject("Sky");
	sky->addComponent<DistanceFog>(SKY_COLOR, view_distance * FOG_START_FRACTION, view_distance * FOG_END_FRACTION);

	window.registerAction("toggle_menu", { { "keyboard.esc" } });
	window.registerAction("sprint",        { { "keyboard.shift" } });
	window.registerAction("brake",         { { "keyboard.space" } });
	window.registerAction("toggle_camera", { { "keyboard.f" } });

	const auto is_driving = [&scene, &chase_camera] { return scene.getMainCamera() == chase_camera->getComponent<Camera>(); };

	engine.addSystem(SystemPhase::eFixedUpdate, [&](Scene&, const FrameTime& frame_time) {
		if (truck.has_value())
			truck->drive(is_driving() ? ReadDrivingInput(truck->getVehicle().getForwardSpeed()) : PARKED_INPUT, frame_time.deltaTime);
	});

	bool placed_above_terrain = false;
	engine.addSystem(SystemPhase::eUpdate, [&](Scene&, const FrameTime&) {
		const glm::vec3 viewer = is_driving() ? chase_camera->getTransform().position : player->getTransform().position;
		regions.update(viewer);
		terrain.update(viewer);

		if (!placed_above_terrain)
			placed_above_terrain = PlaceAboveTerrain(player->getTransform(), regions, *terrain_generator, world_settings);
	});

	engine.addSystem(SystemPhase::eUpdate, [&](Scene&, const FrameTime& frame_time) {
		if (!truck.has_value())
		{
			colliders.update(std::span(&CAMERA_START_POSITION, 1));

			const std::optional<float> ground = GroundHeight(scene, CAMERA_START_POSITION.x, CAMERA_START_POSITION.z);
			if (!ground.has_value())
				return;

			truck.emplace(scene, engine.getRenderer().getMeshes(), *truck_definition, glm::vec3(CAMERA_START_POSITION.x, *ground + TRUCK_SPAWN_HEIGHT, CAMERA_START_POSITION.z), glm::quat(1.f, 0.f, 0.f, 0.f));
			chase.snap(chase_camera->getTransform(), truck->getTransform());
			scene.setMainCamera(chase_camera);
		}

		colliders.update(std::span(&truck->getTransform().position, 1));
		truck->setSimulated(colliders.isReady(truck->getTransform().position));
		truck->updateWheels();

		if (window.getActionDown("toggle_camera"))
		{
			player->getTransform().position = chase_camera->getTransform().position;
			scene.setMainCamera(is_driving() ? player : chase_camera);
		}

		if (!is_driving())
			return;

		chase.orbit(Input::GetRotation(), Input::GetScroll());
		chase.update(chase_camera->getTransform(), truck->getTransform(), truck->getVehicle().getForwardSpeed(), frame_time.deltaTime);
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
