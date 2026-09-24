#include <trok/world/biome.hpp>
#include <trok/world/biome_tuning.hpp>
#include <trok/world/road.hpp>
#include <trok/world/road_planner.hpp>
#include <trok/world/climate.hpp>
#include <trok/world/elevation.hpp>
#include <trok/world/region_plan.hpp>
#include <trok/world/dressers.hpp>
#include <trok/world/terrain_generator.hpp>
#include <trok/world/world_map.hpp>
#include <trok/vehicle/chase_camera.hpp>
#include <trok/vehicle/truck.hpp>
#include <trok/vehicle/truck_tuning.hpp>

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

#include <imgui.h>

#include <format>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

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
	constexpr std::string_view        TUNED_TRUCK_FILE     = "trucks/box_truck.tuned.json";
	constexpr std::string_view        TUNED_BIOME_FILE     = "biomes.tuned.json";
	constexpr std::string_view        ROAD_FILE_NAME       = "roads.json";
	constexpr float                   ROAD_PLANNING_MARGIN = 200.f;
	constexpr int32_t                 TOWN_SURVEY_RADIUS   = 1;
	constexpr int32_t                 TOWN_LINK_REACH      = 2;
	constexpr int32_t                 COLLIDER_RADIUS      = 1;
	constexpr float                   TRUCK_SPAWN_HEIGHT   = 1.5f;
	constexpr float                   GROUND_RAY_HEIGHT    = 2000.f;
	constexpr uint16_t                GROUND_RAY_MASK      = Physics::TERRAIN_CATEGORY | Physics::ROAD_CATEGORY;
	constexpr float                   REVERSE_THRESHOLD    = 1.f;
	constexpr float                   AUTO_HOLD_SPEED      = 0.5f;
	constexpr float                   FOG_START_FRACTION   = 0.5f;
	constexpr float                   FOG_END_FRACTION     = 0.95f;
	constexpr float                   NEAR_PLANE_DEPTH     = 1.f;
	constexpr float                   PICK_STEP            = 4.f;
	constexpr float                   PICK_MAX_DISTANCE    = 2000.f;
	constexpr float                   ENDPOINT_MARKER_SIZE = 1.5f;
	constexpr float                   NODE_MARKER_SIZE     = 0.35f;
	constexpr std::string_view        MARKER_NAME          = "RoadMarker";
	constexpr World::WorldInfo        NEW_WORLD_INFO       = { .seed = 1337, .generatorVersion = 1 };

	const glm::vec3 CAMERA_START_POSITION  = { 0.f, 0.f, 30.f };
	const glm::vec3 SUN_DIRECTION          = { -0.4f, -1.f, -0.3f };
	const glm::vec3 SKY_COLOR              = { 0.6f, 0.745f, 0.76f };
	const glm::vec3 ENDPOINT_MARKER_COLOR  = { 0.9f, 0.1f, 0.1f };
	const glm::vec3 NODE_MARKER_COLOR      = { 0.1f, 0.9f, 0.2f };

	const Physics::VehicleInput PARKED_INPUT = { .brake = 1.f };

	MeshHandle MakeMarkerMesh(MeshRegistry& meshes, const glm::vec3& color)
	{
		MeshData data = CreateCubeMeshData();
		for (Vertex& vertex : data.vertices)
			vertex.color = glm::vec4(color, 1.f);

		return meshes.create(data);
	}

	GameObject PlaceMarker(Scene& scene, MeshHandle mesh, const glm::vec3& position, float size)
	{
		GameObject marker = scene.createGameObject(MARKER_NAME);

		marker->getTransform().position = position;
		marker->getTransform().scale    = glm::vec3(size);
		marker->addComponent<MeshRenderer>(mesh);
		return marker;
	}

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

	struct ScreenRay
	{
		glm::vec3 origin    = {};
		glm::vec3 direction = {};
	};

	ScreenRay CursorRay(const Camera& camera, const glm::vec2& cursor_uv, int width, int height)
	{
		const glm::vec2 ndc          = { cursor_uv.x * 2.f - 1.f, 1.f - cursor_uv.y * 2.f };
		const glm::mat4 view_inverse = glm::inverse(camera.getViewMatrix());
		const glm::mat4 unproject    = glm::inverse(camera.getProjectionMatrix(width, height));
		const glm::vec4 on_near      = unproject * glm::vec4(ndc, NEAR_PLANE_DEPTH, 1.f);
		const glm::vec3 origin       = glm::vec3(view_inverse[3]);

		return { origin, glm::normalize(glm::vec3(view_inverse * glm::vec4(glm::vec3(on_near) / on_near.w, 1.f)) - origin) };
	}

	std::optional<glm::vec3> PickTerrain(const glm::vec3&                      from,
	                                     const glm::vec3&                      direction,
	                                     World::RegionStore<trok::RegionPlan>& regions,
	                                     const trok::TerrainGenerator&         generator,
	                                     const World::WorldSettings&           settings)
	{
		glm::vec3 previous = from;
		float     above    = 0.f;

		for (float travelled = 0.f; travelled <= PICK_MAX_DISTANCE; travelled += PICK_STEP)
		{
			const glm::vec3                          position = from + direction * travelled;
			const std::optional<trok::RegionContext> context  = regions.gatherContext(World::ChunkAt(position, settings));
			if (!context.has_value())
				return std::nullopt;

			const float height = position.y - generator.sampleHeight(*context, position.x, position.z);
			if (height <= 0.f && travelled > 0.f)
				return glm::mix(previous, position, above / (above - height));

			above    = height;
			previous = position;
		}

		return std::nullopt;
	}

	struct TownLink
	{
		glm::vec2 from = {};
		glm::vec2 to   = {};
	};

	struct TownSurvey
	{
		std::vector<TownLink>                    links;
		std::vector<trok::RegionContext::Region> regions;
	};

	TownSurvey SurveyTowns(const trok::RegionPlanner& planner, const World::WorldSettings& settings, World::RegionCoord centre, int32_t radius)
	{
		const int32_t                         side = radius * 2 + 1;
		std::vector<std::optional<glm::vec2>> towns(static_cast<size_t>(side) * side);
		TownSurvey                            survey;

		const auto town_at = [&](int32_t x, int32_t z) -> std::optional<glm::vec2>& {
			return towns[static_cast<size_t>(z + radius) * side + (x + radius)];
		};

		for (int32_t z = -radius; z <= radius; z++)
		{
			for (int32_t x = -radius; x <= radius; x++)
			{
				const World::RegionCoord coord = { centre.x + x, centre.z + z };
				const auto               plan  = std::make_shared<const trok::RegionPlan>(planner.plan(coord, settings));

				survey.regions.push_back({ coord, plan });
				if (!plan->settlements.empty())
					town_at(x, z) = glm::vec2(plan->settlements.front().center);
			}
		}

		const glm::ivec2 steps[] = { { 1, 0 }, { 0, 1 } };
		for (int32_t z = -radius; z <= radius; z++)
		{
			for (int32_t x = -radius; x <= radius; x++)
			{
				if (!town_at(x, z).has_value())
					continue;

				for (const glm::ivec2& step : steps)
				{
					for (int32_t reach = 1; reach <= TOWN_LINK_REACH; reach++)
					{
						const int32_t other_x = x + step.x * reach;
						const int32_t other_z = z + step.y * reach;
						if (other_x > radius || other_z > radius)
							break;

						if (!town_at(other_x, other_z).has_value())
							continue;

						survey.links.push_back({ *town_at(x, z), *town_at(other_x, other_z) });
						break;
					}
				}
			}
		}

		return survey;
	}

	std::optional<trok::RegionContext> GatherRegions(World::RegionStore<trok::RegionPlan>& regions,
	                                                 const glm::vec2&                      start,
	                                                 const glm::vec2&                      end,
	                                                 const World::WorldSettings&           settings)
	{
		const glm::vec2          minimum = glm::min(start, end) - glm::vec2(ROAD_PLANNING_MARGIN);
		const glm::vec2          maximum = glm::max(start, end) + glm::vec2(ROAD_PLANNING_MARGIN);
		const World::RegionCoord lowest  = World::RegionAt(World::ChunkAt(minimum.x, minimum.y, settings), settings);
		const World::RegionCoord highest = World::RegionAt(World::ChunkAt(maximum.x, maximum.y, settings), settings);

		std::vector<trok::RegionContext::Region> gathered;
		for (int32_t z = lowest.z; z <= highest.z; z++)
		{
			for (int32_t x = lowest.x; x <= highest.x; x++)
			{
				std::shared_ptr<const trok::RegionPlan> plan = regions.find({ x, z });
				if (plan == nullptr)
					return std::nullopt;

				gathered.push_back({ { x, z }, std::move(plan) });
			}
		}

		return trok::RegionContext(settings, std::move(gathered));
	}

	std::optional<float> GroundHeight(Scene& scene, float x, float z)
	{
		const glm::vec3                          top = { x, GROUND_RAY_HEIGHT, z };
		const std::optional<Physics::RaycastHit> hit = Physics::CastRay(*scene.getPhysicsWorld(), top, { x, -GROUND_RAY_HEIGHT, z }, GROUND_RAY_MASK);
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

	const auto                          road_class       = LoadGameData<trok::RoadClass>(ROAD_FILE_NAME);
	std::optional<trok::BiomeLibrary>   loaded_biomes    = LoadGameData<trok::BiomeLibrary>(BIOME_FILE_NAME);
	std::optional<trok::ElevationCurve> loaded_elevation = LoadGameData<trok::ElevationCurve>(ELEVATION_FILE_NAME);
	const auto                          truck_definition = LoadGameData<trok::TruckDefinition>(TRUCK_FILE_NAME);
	if (!loaded_biomes.has_value() || !loaded_elevation.has_value() || !truck_definition.has_value() || !road_class.has_value())
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
	const auto                 biomes            = std::make_shared<trok::BiomeLibrary>(std::move(*loaded_biomes));
	const auto                 climate           = std::make_shared<const trok::ClimateSampler>(world_storage->getInfo().seed);
	const trok::ElevationCurve elevation         = std::move(*loaded_elevation);
	const auto                 road_layer        = std::make_shared<trok::RoadLayer>();
	const auto                 terrain_generator = std::make_shared<trok::TerrainGenerator>(biomes, climate, elevation, world_storage->getInfo().seed, road_layer);
	const auto                 region_planner    = std::make_shared<const trok::RegionPlanner>(world_storage->getInfo().generatorVersion, world_storage->getInfo().seed, biomes, climate, elevation);

	terrain_generator->addDresser(std::make_shared<const trok::SeaDresser>(elevation.seaLevel));
	terrain_generator->addDresser(std::make_shared<const trok::RiverWaterDresser>());
	terrain_generator->addDresser(std::make_shared<const trok::RoadDresser>(road_layer));

	World::RegionStore<trok::RegionPlan>       regions(engine.getJobSystem(), world_storage, region_planner, world_settings);
	World::RegionChunkSource<trok::RegionPlan> chunk_source(regions, terrain_generator, chunk_storage, world_settings);
	World::TerrainWorld                        terrain(scene, engine.getJobSystem(), engine.getRenderer().getMeshes(), chunk_source, world_settings);
	World::TerrainColliders                    colliders(scene, terrain, world_settings, COLLIDER_RADIUS);
	trok::TruckTuningWindow                    tuning(*truck_definition, Fs::fromData(TUNED_TRUCK_FILE));
	trok::BiomeTuningWindow                    biome_tuning(biomes, Fs::fromData(TUNED_BIOME_FILE));
	trok::WorldMapWindow                       world_map(biomes, region_planner, climate, elevation, world_settings);
	std::optional<trok::Truck>                 truck;

	GameObject player = scene.createGameObject("Player");
	player->getTransform().position = CAMERA_START_POSITION;
	player->getTransform().rotation = CameraRotation(CAMERA_START_YAW, CAMERA_START_PITCH);
	player->addComponent<Camera>();
	player->addComponent<FlyCamera>(CAMERA_START_YAW, CAMERA_START_PITCH);
	scene.setMainCamera(player);

	GameObject        chase_camera = scene.createGameObject("Chase Camera");
	trok::ChaseCamera chase(trok::ChaseCameraSettings {
		.maxDistance = 500.f
		
		});
	chase_camera->addComponent<Camera>();

	GameObject sun = scene.createGameObject("Sun");
	sun->addComponent<DirectionalLight>(SUN_DIRECTION);

	const float view_distance = static_cast<float>(VIEW_RADIUS) * world_settings.getChunkSize();
	GameObject  sky           = scene.createGameObject("Sky");
	sky->addComponent<DistanceFog>(SKY_COLOR, view_distance * FOG_START_FRACTION, view_distance * FOG_END_FRACTION);

	window.registerAction("toggle_menu",    { { "keyboard.esc" } });
	window.registerAction("sprint",         { { "keyboard.shift" } });
	window.registerAction("brake",          { { "keyboard.space" } });
	window.registerAction("toggle_camera",  { { "keyboard.f" } });
	window.registerAction("road_point",     { { "keyboard.f5" }, { "mouse.right_button" } });
	window.registerAction("road_build",     { { "keyboard.f6" }, { "mouse.middle_button" } });
	window.registerAction("toggle_terrain", { { "keyboard.f7" } });
	window.registerAction("recover",        { { "keyboard.i" } });

	const auto is_driving = [&scene, &chase_camera] { return scene.getMainCamera() == chase_camera->getComponent<Camera>(); };

	engine.addSystem(SystemPhase::eFixedUpdate, [&](Scene&, const FrameTime& frame_time) {
		if (truck.has_value())
			truck->drive(is_driving() ? ReadDrivingInput(truck->getVehicle().getForwardSpeed()) : PARKED_INPUT, frame_time.deltaTime);
	});

	const MeshHandle endpoint_marker_mesh = MakeMarkerMesh(engine.getRenderer().getMeshes(), ENDPOINT_MARKER_COLOR);
	const MeshHandle node_marker_mesh     = MakeMarkerMesh(engine.getRenderer().getMeshes(), NODE_MARKER_COLOR);

	std::shared_ptr<const trok::RoadNetwork> road_network;
	std::vector<std::vector<glm::vec3>>      planned_roads;
	bool                                     roads_ready     = false;
	bool                                     roads_requested = false;
	std::vector<GameObject>  road_markers;
	std::optional<glm::vec2> road_start;
	std::optional<glm::vec2> road_end;
	bool                     terrain_visible = true;
	bool                     roads_planned   = false;

	const auto regenerate_chunks = [&] {
		engine.getJobSystem().waitIdle();
		engine.getJobSystem().processCompleted();
		terrain_generator->refresh();
		terrain.reload();
		colliders.clear();
	};

	const auto connect_towns = [&] {
		roads_requested = true;

		engine.getJobSystem().submit(
			[planner = region_planner, generator = terrain_generator, settings = world_settings, shape = *road_class,
			 sea_level = elevation.seaLevel, centre = World::RegionAt(CAMERA_START_POSITION, world_settings)] {
				const TownSurvey          survey = SurveyTowns(*planner, settings, centre, TOWN_SURVEY_RADIUS);
				const trok::RegionContext context(settings, survey.regions);

				std::vector<std::vector<glm::vec3>> centrelines;
				for (const TownLink& link : survey.links)
				{
					const std::optional<std::vector<glm::vec3>> road = trok::PlanRoad(link.from, link.to, shape, [&](double x, double z) {
						return generator->sampleHeight(context, x, z);
					}, { .seaLevel = sea_level });

					if (road.has_value())
						centrelines.push_back(*road);
				}

				DEBUG_LOG("Connected {} of {} town pairs", centrelines.size(), survey.links.size());
				return centrelines;
			},
			[&](std::vector<std::vector<glm::vec3>> centrelines) {
				planned_roads = std::move(centrelines);
				roads_ready   = true;
			});
	};

	const auto place_road_point = [&](const glm::vec3& point) {
		if (road_start.has_value() && road_end.has_value())
		{
			for (GameObject& marker : road_markers)
				marker.destroy();

			road_markers.clear();
			road_start.reset();
			road_end.reset();
		}

		const bool is_start = !road_start.has_value();

		(is_start ? road_start : road_end) = glm::vec2(point.x, point.z);
		road_markers.push_back(PlaceMarker(scene, endpoint_marker_mesh, point, ENDPOINT_MARKER_SIZE));
		DEBUG_LOG("Road {} set to ({:.0f}, {:.0f})", is_start ? "start" : "end", point.x, point.z);
	};

	const auto build_road = [&] {
		const std::optional<trok::RegionContext> context = GatherRegions(regions, *road_start, *road_end, world_settings);
		if (!context.has_value())
		{
			DEBUG_ERROR("The regions between the road endpoints are not loaded yet");
			return;
		}

		road_layer->set(nullptr);
		const std::optional<std::vector<glm::vec3>> centreline = trok::PlanRoad(*road_start, *road_end, *road_class, [&](double x, double z) {
			return terrain_generator->sampleHeight(*context, x, z);
		}, { .seaLevel = elevation.seaLevel });

		if (!centreline.has_value())
			return;

		DEBUG_LOG("Planned a road of {} points between ({:.0f}, {:.0f}) and ({:.0f}, {:.0f})", centreline->size(), road_start->x, road_start->y, road_end->x, road_end->y);
		for (const glm::vec3& point : *centreline)
			road_markers.push_back(PlaceMarker(scene, node_marker_mesh, point, NODE_MARKER_SIZE));

		road_network = std::make_shared<const trok::RoadNetwork>(*centreline, *road_class);
		road_layer->set(road_network);
		roads_planned = true;
		regenerate_chunks();
	};

	bool                      placed_above_terrain = false;
	std::optional<glm::dvec2> travelling;
	engine.addSystem(SystemPhase::eUpdate, [&](Scene&, const FrameTime&) {
		const glm::vec3 viewer = is_driving() ? chase_camera->getTransform().position : player->getTransform().position;
		chunk_source.setStorageEnabled(!engine.isDebugMode() && !roads_planned);
		regions.update(viewer);
		terrain.update(viewer);

		if (engine.isDebugMode() && biome_tuning.draw())
			regenerate_chunks();

		if (engine.isDebugMode())
		{
			const std::optional<glm::dvec2> target = world_map.draw(viewer, road_network.get());
			if (target.has_value())
			{
				player->getTransform().position = { static_cast<float>(target->x), 0.f, static_cast<float>(target->y) };
				placed_above_terrain            = false;
				travelling                      = target;

				DEBUG_LOG("Travelling to ({:.0f}, {:.0f})", target->x, target->y);
			}
		}

		if (window.getActionDown("toggle_terrain"))
		{
			terrain_visible = !terrain_visible;
			DEBUG_LOG("Terrain rendering {}", terrain_visible ? "enabled" : "disabled");
		}

		if (!terrain_visible)
			scene.forEach<World::TerrainChunk, MeshRenderer>([](Entity, const World::TerrainChunk&, MeshRenderer& renderer) {
				renderer.visible = false;
			});

		if (!roads_requested)
			connect_towns();

		if (roads_ready)
		{
			roads_ready = false;

			if (!planned_roads.empty())
			{
				engine.getJobSystem().waitIdle();
				engine.getJobSystem().processCompleted();

				road_network = std::make_shared<const trok::RoadNetwork>(std::move(planned_roads), *road_class);
				road_layer->set(road_network);
				roads_planned = true;
				regenerate_chunks();
			}
		}

		const bool ui_has_mouse = ImGui::GetIO().WantCaptureMouse;

		if (window.getActionDown("road_point") && !ui_has_mouse)
		{
			const Camera* camera = scene.getMainCamera();
			if (camera == nullptr)
				DEBUG_ERROR("There is no active camera to aim with");
			else if (const ScreenRay ray = CursorRay(*camera, window.getCursorUv(), window.getRenderWidth(), window.getRenderHeight());
			         const std::optional<glm::vec3> point = PickTerrain(ray.origin, ray.direction, regions, *terrain_generator, world_settings))
				place_road_point(*point);
			else
				DEBUG_ERROR("There is no terrain under the cursor");
		}

		if (window.getActionDown("road_build") && !ui_has_mouse && road_start.has_value() && road_end.has_value())
			build_road();

		if (!placed_above_terrain)
			placed_above_terrain = PlaceAboveTerrain(player->getTransform(), regions, *terrain_generator, world_settings);
		else if (travelling.has_value())
		{
			const float ground = player->getTransform().position.y - CAMERA_START_HEIGHT;

			if (truck.has_value())
				truck->recover({ static_cast<float>(travelling->x), ground + TRUCK_SPAWN_HEIGHT, static_cast<float>(travelling->y) });

			travelling.reset();
		}
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

		if (engine.isDebugMode())
			tuning.draw(*truck);

		if (window.getActionDown("toggle_camera"))
		{
			player->getTransform().position = chase_camera->getTransform().position;
			scene.setMainCamera(is_driving() ? player : chase_camera);
		}

		if (window.getActionDown("recover"))
		{
			const glm::vec3            stuck  = truck->getTransform().position;
			const std::optional<float> ground = GroundHeight(scene, stuck.x, stuck.z);

			if (ground.has_value())
				truck->recover({ stuck.x, *ground + TRUCK_SPAWN_HEIGHT, stuck.z });
			else
				DEBUG_ERROR("There is no ground under the truck to recover onto");
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
