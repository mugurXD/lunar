#include <trok/world/road_service.hpp>
#include <trok/world/dressers.hpp>
#include <trok/world/shapers.hpp>

#include <lunar/debug.hpp>

#include <algorithm>
#include <limits>
#include <utility>

namespace trok
{
	namespace
	{
		constexpr float HALF                 = 0.5f;
		constexpr float TOWN_MATCH_DISTANCE  = 1.f;
		constexpr float NODE_SNAP_SCALE      = 3.f;

		float PathLength(std::span<const glm::vec3> points)
		{
			float length = 0.f;
			for (size_t point = 1; point < points.size(); point++)
				length += glm::distance(points[point - 1], points[point]);

			return length;
		}

		HeightSampler BaseHeight(std::shared_ptr<const TerrainGenerator> generator, RegionContext context)
		{
			return [generator = std::move(generator), context = std::move(context)](double x, double z) {
				return generator->sampleBaseHeight(context, x, z);
			};
		}
	}

	RoadService::RoadService(lunar::JobSystem&                 jobs,
	                         std::shared_ptr<TerrainGenerator> generator,
	                         RoadClass                         road_class,
	                         RoadPlannerSettings               settings,
	                         size_t                            max_links_in_flight) noexcept
		: jobs(jobs),
		generator(generator),
		roadClass(std::move(road_class)),
		settings(std::move(settings)),
		layer(std::make_shared<RoadLayer>()),
		maxLinksInFlight(max_links_in_flight)
	{
		generator->addShaper(std::make_shared<const RoadShaper>(layer));
		generator->addDresser(std::make_shared<const RoadDresser>(layer));
	}

	RoadService::~RoadService() noexcept
	{
		for (const lunar::JobHandle& handle : handles)
			jobs.cancel(handle);
	}

	void RoadService::plan(std::span<const RoadLink> links, const RegionContext& context)
	{
		const HeightSampler sampler = BaseHeight(generator, context);

		pendingLinks += links.size();
		for (const RoadLink& link : links)
			planLink({ .road = link }, sampler);
	}

	void RoadService::request(std::span<const TownLink> links, const RegionContext& context)
	{
		for (const TownLink& link : links)
		{
			if (isKnown(link))
				continue;

			known.push_back(link);
			queue.push_back({ .road = link.road, .context = context });
		}
	}

	std::vector<std::vector<glm::vec3>> RoadService::update(const glm::vec2& focus)
	{
		startNearest(focus);

		if (!changed)
			return {};

		changed = false;
		layer->set(std::make_shared<const RoadNetwork>(roads, roadClass, roadIds));

		std::vector<std::vector<glm::vec3>> published(roads.begin() + static_cast<std::ptrdiff_t>(publishedRoads), roads.end());
		publishedRoads = roads.size();
		return published;
	}

	std::shared_ptr<const RoadNetwork> RoadService::getNetwork() const
	{
		return layer->get();
	}

	bool RoadService::isKnown(const TownLink& link) const
	{
		return std::ranges::any_of(known, [&](const TownLink& other) { return other.from == link.from && other.to == link.to; });
	}

	std::optional<RoadService::PlannedLink> RoadService::routeOf(const RoadLink& link) const
	{
		std::vector<glm::vec2> towns;
		for (const TownLink& town_link : known)
			for (const glm::vec2& centre : { town_link.road.from, town_link.road.to })
				if (std::ranges::find(towns, centre) == towns.end())
					towns.push_back(centre);

		const RoadNetwork      network      = RoadNetwork(roads, roadClass);
		const RoadConnectivity connectivity = ConnectRoads(network, towns.size(), [&](const glm::vec3& point) -> std::optional<size_t> {
			const auto found = std::ranges::find_if(towns, [&](const glm::vec2& centre) { return glm::distance(centre, glm::vec2(point.x, point.z)) <= TOWN_MATCH_DISTANCE; });
			return found == towns.end() ? std::nullopt : std::optional<size_t>(static_cast<size_t>(found - towns.begin()));
		});

		const auto group_of = [&](const glm::vec2& centre) {
			return connectivity.townGroups[static_cast<size_t>(std::ranges::find(towns, centre) - towns.begin())];
		};

		const std::optional<size_t> from_group = group_of(link.from);
		const std::optional<size_t> to_group   = group_of(link.to);
		if (from_group.has_value() && from_group == to_group)
			return std::nullopt;

		if (!from_group.has_value() && !to_group.has_value())
			return PlannedLink { .road = link };

		const bool      leaves_from = !from_group.has_value() || to_group.has_value();
		const glm::vec2 origin      = leaves_from ? link.from : link.to;
		const size_t    target      = leaves_from ? *to_group : *from_group;

		const float              snap     = roadClass.halfWidth() * NODE_SNAP_SCALE;
		std::optional<JoinPoint> join;
		float                    shortest = std::numeric_limits<float>::max();
		for (size_t road = 0; road < roads.size(); road++)
		{
			if (connectivity.roadGroups[road] != target)
				continue;

			const std::vector<glm::vec3>& points    = roads[road];
			const float                   length    = PathLength(points);
			float                         travelled = 0.f;
			for (size_t point = 0; point < points.size(); point++)
			{
				travelled += point == 0 ? 0.f : glm::distance(points[point - 1], points[point]);

				const float distance = glm::distance(origin, glm::vec2(points[point].x, points[point].z));
				if (distance >= shortest)
					continue;

				shortest = distance;
				if (travelled < snap)
					join = JoinPoint { .position = points.front() };
				else if (length - travelled < snap)
					join = JoinPoint { .position = points.back() };
				else
					join = JoinPoint { .position = points[point], .splitRoad = static_cast<uint32_t>(road), .splitPoint = static_cast<uint32_t>(point) };
			}
		}

		std::optional<glm::vec2> crossing;
		if (join->splitRoad.has_value())
		{
			const std::vector<glm::vec3>& trunk = roads[*join->splitRoad];
			const glm::vec3&                 ahead = trunk[join->splitPoint + 1];
			const glm::vec3&                 back  = trunk[join->splitPoint - 1];
			crossing = glm::normalize(glm::vec2(ahead.x - back.x, ahead.z - back.z));
		}

		return PlannedLink
		{
			.road    = { origin, glm::vec2(join->position.x, join->position.z) },
			.arrival = RoadArrival { .height = join->position.y, .crossing = crossing },
			.join    = join
		};
	}

	void RoadService::startNearest(const glm::vec2& focus)
	{
		while (pendingLinks < maxLinksInFlight && !queue.empty())
		{
			const auto nearest = std::ranges::min_element(queue, {}, [&](const QueuedLink& queued) {
				return glm::distance(focus, (queued.road.from + queued.road.to) * HALF);
			});

			const std::optional<PlannedLink> route   = routeOf(nearest->road);
			const HeightSampler              sampler = BaseHeight(generator, nearest->context);
			queue.erase(nearest);

			if (!route.has_value())
				continue;

			pendingLinks++;
			planLink(*route, sampler);
		}
	}

	void RoadService::planLink(const PlannedLink& link, const HeightSampler& sampler)
	{
		handles.push_back(jobs.submit(
			[link, sampler, road_class = roadClass, settings = settings] {
				return SplitRoad(link.road.from, link.road.to, road_class, sampler, settings, link.arrival);
			},
			[this, sampler, join = link.join](std::optional<std::vector<RoadSegment>> segments) {
				if (segments.has_value())
					planSegments(std::move(*segments), sampler, join);
				else
					finishLink();
			}));
	}

	void RoadService::planSegments(std::vector<RoadSegment> segments, const HeightSampler& sampler, std::optional<JoinPoint> join)
	{
		const auto progress = std::make_shared<LinkProgress>(LinkProgress { .pieces = std::vector<std::vector<glm::vec3>>(segments.size()), .remaining = segments.size() });

		for (size_t index = 0; index < segments.size(); index++)
		{
			handles.push_back(jobs.submit(
				[segment = std::move(segments[index]), sampler, road_class = roadClass, settings = settings] {
					return PlanSegment(segment, road_class, sampler, settings);
				},
				[this, progress, index, join](std::optional<std::vector<glm::vec3>> piece) {
					if (piece.has_value())
						progress->pieces[index] = std::move(*piece);
					else
						progress->failed = true;

					if (--progress->remaining > 0)
						return;

					if (!progress->failed)
					{
						std::vector<glm::vec3> road = JoinSegments(progress->pieces, settings);
						if (join.has_value())
						{
							road = ArriveAt(std::move(road), join->position, roadClass);
							if (join->splitRoad.has_value())
							{
								std::vector<glm::vec3>& trunk = roads[*join->splitRoad];
								std::vector<glm::vec3>  rest(trunk.begin() + join->splitPoint, trunk.end());
								trunk.resize(join->splitPoint + 1);
								roadIds[*join->splitRoad] = nextRoadId++;
								roads.push_back(std::move(rest));
								roadIds.push_back(nextRoadId++);
							}
						}

						roads.push_back(std::move(road));
						roadIds.push_back(nextRoadId++);
						changed = true;
					}

					finishLink();
				}));
		}
	}

	void RoadService::finishLink()
	{
		if (--pendingLinks > 0)
			return;

		handles.clear();
		DEBUG_LOG("Road planning finished, the network has {} roads", roads.size());
	}
}
