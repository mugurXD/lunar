#include <trok/world/road_planner.hpp>

#include <lunar/debug.hpp>
#include <lunar/world/grid.hpp>

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_map>

namespace trok
{
	namespace
	{
		constexpr int   TURN_OPTIONS[]       = { -1, 0, 1 };
		constexpr float GOAL_TOLERANCE       = 1.5f;
		constexpr float GRADIENT_EPSILON     = 1.f;
		constexpr float GRADE_WARNING_FACTOR = 1.05f;
		constexpr float MIN_SEGMENT_FRACTION = 0.25f;

		struct Node
		{
			glm::vec2 position = {};
			int       heading  = 0;
			float     cost     = 0.f;
			uint32_t  parent   = 0;
		};

		struct Queued
		{
			float    estimate = 0.f;
			uint32_t node     = 0;

			bool operator>(const Queued& other) const { return estimate > other.estimate; }
		};

		struct VisitedKey
		{
			int32_t x       = 0;
			int32_t z       = 0;
			int     heading = 0;

			bool operator==(const VisitedKey&) const = default;
		};

		struct VisitedHash
		{
			size_t operator()(const VisitedKey& key) const
			{
				return lunar::World::HashGridCoord(key.x, key.z) ^ static_cast<size_t>(key.heading);
			}
		};

		glm::vec2 HeadingDirection(int heading, int headings)
		{
			const float angle = glm::two_pi<float>() * static_cast<float>(heading) / static_cast<float>(headings);
			return { std::cos(angle), std::sin(angle) };
		}

		int NearestHeading(const glm::vec2& direction, int headings)
		{
			const float angle   = std::atan2(direction.y, direction.x);
			const float rounded = std::round(angle / glm::two_pi<float>() * static_cast<float>(headings));
			return (static_cast<int>(rounded) % headings + headings) % headings;
		}

		VisitedKey KeyOf(const glm::vec2& position, int heading, float step)
		{
			return
			{
				.x       = static_cast<int32_t>(std::round(position.x / step)),
				.z       = static_cast<int32_t>(std::round(position.y / step)),
				.heading = heading
			};
		}

		float WaterCost(float height, const RoadPlannerSettings& settings)
		{
			return std::max(settings.seaLevel - height, 0.f) * settings.waterPenalty;
		}

		float GradeCost(const glm::vec2& from, float from_height, const glm::vec2& to, float to_height, const RoadClass& road_class)
		{
			const float run = glm::distance(from, to);
			if (run <= 0.f)
				return 0.f;

			const float excess = std::max(std::abs(to_height - from_height) / run - road_class.maxGrade, 0.f) / road_class.maxGrade;
			return road_class.slopePenalty * excess * excess;
		}

		float TurnCost(const glm::vec2& previous, const glm::vec2& point, const glm::vec2& next, const RoadClass& road_class)
		{
			const glm::vec2 incoming = point - previous;
			const glm::vec2 outgoing = next - point;
			const float     spread   = glm::length(incoming) * glm::length(outgoing) * glm::distance(previous, next);
			if (spread <= 0.f)
				return 0.f;

			const float curvature = 2.f * std::abs(incoming.x * outgoing.y - incoming.y * outgoing.x) / spread;
			const float excess    = std::max(curvature * road_class.minCurveRadius - 1.f, 0.f);
			return road_class.turnPenalty * excess * excess;
		}

		std::vector<glm::vec2> Refine(std::vector<glm::vec2>     path,
		                              const HeightSampler&       sample_height,
		                              const RoadClass&           road_class,
		                              const RoadPlannerSettings& settings)
		{
			for (int pass = 0; pass < settings.refinementPasses && path.size() > 2; pass++)
			{
				std::vector<glm::vec2> refined = path;

				for (size_t index = 1; index + 1 < path.size(); index++)
				{
					const glm::vec2& previous        = path[index - 1];
					const glm::vec2& next            = path[index + 1];
					const float      previous_height = sample_height(previous.x, previous.y);
					const float      next_height     = sample_height(next.x, next.y);

					const auto cost = [&](const glm::vec2& candidate) {
						const float height = sample_height(candidate.x, candidate.y);

						return GradeCost(previous, previous_height, candidate, height, road_class)
						     + GradeCost(candidate, height, next, next_height, road_class)
						     + TurnCost(previous, candidate, next, road_class)
						     + WaterCost(height, settings);
					};

					const glm::vec2 along_x  = { GRADIENT_EPSILON, 0.f };
					const glm::vec2 along_z  = { 0.f, GRADIENT_EPSILON };
					const glm::vec2 gradient =
					{
						cost(path[index] + along_x) - cost(path[index] - along_x),
						cost(path[index] + along_z) - cost(path[index] - along_z)
					};

					const glm::vec2 smoothing    = (previous + next - path[index] * 2.f) * settings.smoothingWeight;
					const glm::vec2 descent      = gradient * (settings.terrainWeight / (2.f * GRADIENT_EPSILON));
					const glm::vec2 displacement = smoothing - descent;
					const float     travel       = glm::length(displacement);

					refined[index] = path[index] + (travel > settings.maxRefinementStep ? displacement * (settings.maxRefinementStep / travel) : displacement);
				}

				path = std::move(refined);
			}

			return path;
		}

		std::vector<glm::vec2> Resample(const std::vector<glm::vec2>& path, float spacing)
		{
			std::vector<glm::vec2> sampled          = { path.front() };
			float                  distance_to_next = spacing;

			for (size_t index = 0; index + 1 < path.size(); index++)
			{
				const glm::vec2 from   = path[index];
				const glm::vec2 to     = path[index + 1];
				const float     length = glm::distance(from, to);
				if (length <= 0.f)
					continue;

				float travelled = 0.f;
				while (distance_to_next <= length - travelled)
				{
					travelled += distance_to_next;
					sampled.push_back(glm::mix(from, to, travelled / length));
					distance_to_next = spacing;
				}

				distance_to_next -= length - travelled;
			}

			if (glm::distance(sampled.back(), path.back()) < spacing * MIN_SEGMENT_FRACTION)
				sampled.back() = path.back();
			else
				sampled.push_back(path.back());

			return sampled;
		}

		std::vector<float> FitProfile(const std::vector<glm::vec2>& path, const HeightSampler& sample_height, const RoadClass& road_class, const RoadPlannerSettings& settings)
		{
			std::vector<float> ground;
			ground.reserve(path.size());
			for (const glm::vec2& point : path)
				ground.push_back(sample_height(point.x, point.y));

			std::vector<float> heights = ground;
			const float        reach   = road_class.maxFillHeight;

			for (int pass = 0; pass < settings.profilePasses; pass++)
			{
				for (size_t index = 1; index + 1 < heights.size(); index++)
				{
					const float smoothed = (heights[index - 1] + heights[index] * 2.f + heights[index + 1]) * 0.25f;
					heights[index]       = glm::mix(smoothed, ground[index], settings.groundWeight);
				}

				for (size_t index = 1; index < heights.size(); index++)
				{
					const float limit = road_class.maxGrade * glm::distance(path[index], path[index - 1]);
					const float graded = std::clamp(heights[index], heights[index - 1] - limit, heights[index - 1] + limit);
					heights[index]     = std::clamp(graded, ground[index] - reach, ground[index] + reach);
				}

				for (size_t index = heights.size() - 1; index > 0; index--)
				{
					const float limit  = road_class.maxGrade * glm::distance(path[index], path[index - 1]);
					const float graded = std::clamp(heights[index - 1], heights[index] - limit, heights[index] + limit);
					heights[index - 1] = std::clamp(graded, ground[index - 1] - reach, ground[index - 1] + reach);
				}
			}

			return heights;
		}

		std::vector<glm::vec3> BuildCentreline(const std::vector<glm::vec2>&  path,
		                                       const HeightSampler&           sample_height,
		                                       const RoadClass&               road_class,
		                                       const RoadPlannerSettings&     settings)
		{
			const std::vector<glm::vec2> shaped  = Resample(Refine(path, sample_height, road_class, settings), settings.pointSpacing);
			const std::vector<float>     profile = FitProfile(shaped, sample_height, road_class, settings);

			std::vector<glm::vec3> centreline;
			float                  steepest = 0.f;

			centreline.reserve(shaped.size());
			for (size_t index = 0; index < shaped.size(); index++)
			{
				if (index > 0)
					steepest = std::max(steepest, std::abs(profile[index] - profile[index - 1]) / glm::distance(shaped[index], shaped[index - 1]));

				centreline.emplace_back(shaped[index].x, profile[index], shaped[index].y);
			}

			if (steepest > road_class.maxGrade * GRADE_WARNING_FACTOR)
				DEBUG_LOG("This route follows the ground at up to {:.0f}%, steeper than the {:.0f}% the road class prefers", steepest * 100.f, road_class.maxGrade * 100.f);

			return centreline;
		}
	}

	std::optional<std::vector<glm::vec3>> PlanRoad(const glm::vec2&           start,
	                                               const glm::vec2&           end,
	                                               const RoadClass&           road_class,
	                                               const HeightSampler&       sample_height,
	                                               const RoadPlannerSettings& settings)
	{
		const glm::vec2 minimum = glm::min(start, end) - glm::vec2(settings.searchMargin);
		const glm::vec2 maximum = glm::max(start, end) + glm::vec2(settings.searchMargin);

		std::vector<Node>                                                      nodes = { { start, NearestHeading(end - start, settings.headings) } };
		std::unordered_map<VisitedKey, float, VisitedHash>                     visited;
		std::priority_queue<Queued, std::vector<Queued>, std::greater<Queued>> open;

		visited[KeyOf(start, nodes.front().heading, settings.step)] = 0.f;
		open.push({ glm::distance(start, end), 0 });

		while (!open.empty() && nodes.size() < settings.maxNodes)
		{
			const uint32_t current_index = open.top().node;
			const Node     current       = nodes[current_index];
			open.pop();

			const auto recorded = visited.find(KeyOf(current.position, current.heading, settings.step));
			if (recorded != visited.end() && current.cost > recorded->second)
				continue;

			if (glm::distance(current.position, end) <= settings.step * GOAL_TOLERANCE)
			{
				std::vector<glm::vec2> path = { end };
				for (uint32_t index = current_index; ; index = nodes[index].parent)
				{
					path.push_back(nodes[index].position);
					if (index == 0)
						break;
				}

				std::ranges::reverse(path);
				return BuildCentreline(path, sample_height, road_class, settings);
			}

			const float current_height = sample_height(current.position.x, current.position.y);

			for (const int turn : TURN_OPTIONS)
			{
				const int       heading  = (current.heading + turn + settings.headings) % settings.headings;
				const glm::vec2 position = current.position + HeadingDirection(heading, settings.headings) * settings.step;
				if (position.x < minimum.x || position.x > maximum.x || position.y < minimum.y || position.y > maximum.y)
					continue;

				const float height = sample_height(position.x, position.y);
				const float slope  = std::abs(height - current_height) / settings.step;
				const float excess = std::max(slope - road_class.maxGrade, 0.f) / road_class.maxGrade;
				const float cost   = current.cost + settings.step * (1.f + WaterCost(height, settings)
				                                                        + road_class.slopePenalty * excess * excess
				                                                        + road_class.turnPenalty * std::abs(static_cast<float>(turn)));

				const VisitedKey key   = KeyOf(position, heading, settings.step);
				const auto       found = visited.find(key);
				if (found != visited.end() && found->second <= cost)
					continue;

				visited[key] = cost;
				nodes.push_back({ position, heading, cost, current_index });
				open.push({ cost + glm::distance(position, end), static_cast<uint32_t>(nodes.size() - 1) });
			}
		}

		DEBUG_ERROR("Could not plan a road between ({:.0f}, {:.0f}) and ({:.0f}, {:.0f})", start.x, start.y, end.x, end.y);
		return std::nullopt;
	}
}
