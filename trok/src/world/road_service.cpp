#include <trok/world/road_service.hpp>
#include <trok/world/dressers.hpp>
#include <trok/world/shapers.hpp>

#include <lunar/debug.hpp>

#include <utility>

namespace trok
{
	namespace
	{
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
	                         RoadPlannerSettings               settings) noexcept
		: jobs(jobs),
		generator(generator),
		roadClass(std::move(road_class)),
		settings(std::move(settings)),
		layer(std::make_shared<RoadLayer>())
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
			planLink(link, sampler);
	}

	bool RoadService::update()
	{
		if (!changed || pendingLinks > 0)
			return false;

		changed = false;
		layer->set(std::make_shared<const RoadNetwork>(roads, roadClass));
		return true;
	}

	std::shared_ptr<const RoadNetwork> RoadService::getNetwork() const
	{
		return layer->get();
	}

	void RoadService::planLink(const RoadLink& link, const HeightSampler& sampler)
	{
		handles.push_back(jobs.submit(
			[link, sampler, road_class = roadClass, settings = settings] {
				return SplitRoad(link.from, link.to, road_class, sampler, settings);
			},
			[this, sampler](std::optional<std::vector<RoadSegment>> segments) {
				if (segments.has_value())
					planSegments(std::move(*segments), sampler);
				else
					finishLink();
			}));
	}

	void RoadService::planSegments(std::vector<RoadSegment> segments, const HeightSampler& sampler)
	{
		const auto progress = std::make_shared<LinkProgress>(LinkProgress { .pieces = std::vector<std::vector<glm::vec3>>(segments.size()), .remaining = segments.size() });

		for (size_t index = 0; index < segments.size(); index++)
		{
			handles.push_back(jobs.submit(
				[segment = std::move(segments[index]), sampler, road_class = roadClass, settings = settings] {
					return PlanSegment(segment, road_class, sampler, settings);
				},
				[this, progress, index](std::optional<std::vector<glm::vec3>> piece) {
					if (piece.has_value())
						progress->pieces[index] = std::move(*piece);
					else
						progress->failed = true;

					if (--progress->remaining > 0)
						return;

					if (!progress->failed)
					{
						roads.push_back(JoinSegments(progress->pieces, settings));
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
