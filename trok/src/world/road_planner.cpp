#include <trok/world/road_planner.hpp>

#include <lunar/debug.hpp>
#include <lunar/world/grid.hpp>

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <queue>
#include <unordered_map>

namespace trok
{
	namespace
	{
		constexpr int    TURN_OPTIONS[]  = { -1, 0, 1 };
		constexpr float  ACROSS[]        = { -1.f, 0.f, 1.f };
		constexpr size_t CENTRE          = 1;
		constexpr size_t HEIGHT_OPTIONS  = 5;
		constexpr float  GOAL_TOLERANCE  = 2.f;
		constexpr float  STRAIGHT_COSINE = 0.9998f;
		constexpr float  HALF            = 0.5f;

		using CrossSection = std::array<float, std::size(ACROSS)>;

		struct Node
		{
			glm::vec2 position  = {};
			float     height    = 0.f;
			int       heading   = 0;
			float     cost      = 0.f;
			uint32_t  parent    = 0;
			bool      goal      = false;
			float     clearance = 0.f;
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
			int32_t level   = 0;

			bool operator==(const VisitedKey&) const = default;
		};

		struct VisitedHash
		{
			size_t operator()(const VisitedKey& key) const
			{
				return lunar::World::HashGridCoord(key.x, key.z) ^ (lunar::World::HashGridCoord(key.heading, key.level) << 1);
			}
		};

		struct StepGround
		{
			CrossSection middle = {};
			CrossSection end    = {};
		};

		struct Earthwork
		{
			float cut  = 0.f;
			float fill = 0.f;
			bool  wet  = false;
		};

		float OverLimit(float value, float limit)
		{
			const float excess = std::max(value - limit, 0.f) / limit;
			return excess * excess;
		}

		float SlopeCost(float rise, float run, const RoadClass& road_class)
		{
			return road_class.climbCost * std::abs(rise) + run * road_class.limitPenalty * OverLimit(std::abs(rise) / run, road_class.maxGrade);
		}

		float CurveStep(float radius, int headings)
		{
			const float half_turn = glm::pi<float>() / static_cast<float>(headings);
			return 2.f * radius * std::sin(half_turn) / (std::cos(half_turn) * std::cos(half_turn));
		}

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

		VisitedKey KeyOf(const glm::vec2& position, int heading, float clearance, float step, float height_step)
		{
			return
			{
				.x       = static_cast<int32_t>(std::round(position.x / step)),
				.z       = static_cast<int32_t>(std::round(position.y / step)),
				.heading = heading,
				.level   = static_cast<int32_t>(std::round(clearance / height_step))
			};
		}

		StepGround SampleStep(const glm::vec2& from, const glm::vec2& to, float half_width, const HeightSampler& sample_height)
		{
			const glm::vec2 direction = glm::normalize(to - from);
			const glm::vec2 side      = glm::vec2(-direction.y, direction.x) * half_width;
			const glm::vec2 middle    = (from + to) * HALF;

			StepGround ground;
			for (size_t index = 0; index < std::size(ACROSS); index++)
			{
				const glm::vec2 middle_point = middle + side * ACROSS[index];
				const glm::vec2 end_point    = to + side * ACROSS[index];

				ground.middle[index] = sample_height(middle_point.x, middle_point.y);
				ground.end[index]    = sample_height(end_point.x, end_point.y);
			}

			return ground;
		}

		void Measure(Earthwork& work, const CrossSection& ground, float height, float sea_level)
		{
			for (const float point : ground)
			{
				work.cut  = std::max(work.cut, point - height);
				work.fill = std::max(work.fill, height - point);
				work.wet  = work.wet || point < sea_level;
			}
		}

		std::optional<float> StepCost(const StepGround&          ground,
		                              float                      from_height,
		                              float                      to_height,
		                              int                        turn,
		                              float                      step,
		                              const RoadClass&           road_class,
		                              const RoadPlannerSettings& settings)
		{
			if (to_height <= settings.seaLevel)
				return std::nullopt;

			const float middle_height = (from_height + to_height) * HALF;
			Earthwork   work;
			Measure(work, ground.middle, middle_height, settings.seaLevel);
			Measure(work, ground.end, to_height, settings.seaLevel);

			const float clearance = std::min(middle_height - ground.middle[CENTRE], to_height - ground.end[CENTRE]);
			const bool  bridge    = clearance > road_class.bridgeHeight;
			if (work.wet && !bridge)
				return std::nullopt;

			const float structure = bridge ? road_class.bridgeCost : road_class.cutCost * work.cut + road_class.fillCost * work.fill;
			const float too_deep  = road_class.limitPenalty * OverLimit(work.cut, road_class.maxCutDepth);

			return SlopeCost(to_height - from_height, step, road_class)
			     + step * (1.f + structure + too_deep + road_class.turnPenalty * static_cast<float>(std::abs(turn)));
		}

		float Estimate(const glm::vec2& position, float height, const glm::vec2& end, float end_height, const RoadClass& road_class)
		{
			const float run      = glm::distance(position, end);
			const float climb    = std::abs(end_height - height);
			const float climbing = road_class.climbCost * climb;
			if (climb <= road_class.maxGrade * run)
				return run + climbing;

			const float excess = (climb / run - road_class.maxGrade) / road_class.maxGrade;
			return climbing + std::min(climb / road_class.maxGrade, run * (1.f + road_class.limitPenalty * excess * excess));
		}

		std::array<float, HEIGHT_OPTIONS> HeightOptions(float height, float ground, float climb)
		{
			return { height - climb, height, height + climb, std::clamp(ground, height - climb, height + climb), ground };
		}

		glm::vec3 QuadraticBezier(const glm::vec3& from, const glm::vec3& control, const glm::vec3& to, float amount)
		{
			return glm::mix(glm::mix(from, control, amount), glm::mix(control, to, amount), amount);
		}

		bool CanArriveAt(const Node& node, const glm::vec2& end, float remaining, float step, int headings)
		{
			const glm::vec2 heading = HeadingDirection(node.heading, headings);
			const float     cosine  = std::cos(glm::pi<float>() / static_cast<float>(headings));

			return remaining >= step * HALF && remaining <= step * GOAL_TOLERANCE && glm::dot(heading, (end - node.position) / remaining) >= cosine;
		}

		bool IsStraight(const glm::vec3& previous, const glm::vec3& point, const glm::vec3& next)
		{
			const glm::vec2 incoming = glm::normalize(glm::vec2(point.x - previous.x, point.z - previous.z));
			const glm::vec2 outgoing = glm::normalize(glm::vec2(next.x - point.x, next.z - point.z));
			return glm::dot(incoming, outgoing) >= STRAIGHT_COSINE;
		}

		std::vector<glm::vec3> Trace(const std::vector<Node>& nodes, uint32_t last)
		{
			std::vector<glm::vec3> path;
			for (uint32_t index = last; ; index = nodes[index].parent)
			{
				path.emplace_back(nodes[index].position.x, nodes[index].height, nodes[index].position.y);
				if (index == 0)
					break;
			}

			std::ranges::reverse(path);
			return path;
		}

		std::vector<glm::vec3> Curve(const std::vector<glm::vec3>& path, float spacing)
		{
			std::vector<glm::vec3> centreline = { path.front() };

			for (size_t index = 1; index + 1 < path.size(); index++)
			{
				const glm::vec3& corner = path[index];
				if (IsStraight(path[index - 1], corner, path[index + 1]))
				{
					centreline.push_back(corner);
					continue;
				}

				const glm::vec3 entry   = (path[index - 1] + corner) * HALF;
				const glm::vec3 exit    = (corner + path[index + 1]) * HALF;
				const int       samples = std::max(1, static_cast<int>(std::ceil((glm::distance(entry, corner) + glm::distance(corner, exit)) / spacing)));

				for (int sample = centreline.back() == entry ? 1 : 0; sample <= samples; sample++)
					centreline.push_back(QuadraticBezier(entry, corner, exit, static_cast<float>(sample) / static_cast<float>(samples)));
			}

			centreline.push_back(path.back());
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
		const float     step    = CurveStep(road_class.minCurveRadius, settings.headings);
		const float     climb   = road_class.maxGrade * step;
		const float     arrival = sample_height(end.x, end.y);

		std::vector<Node>                                                      nodes = { { start, sample_height(start.x, start.y), NearestHeading(end - start, settings.headings) } };
		std::unordered_map<VisitedKey, float, VisitedHash>                     visited;
		std::priority_queue<Queued, std::vector<Queued>, std::greater<Queued>> open;

		visited[KeyOf(start, nodes.front().heading, 0.f, step, settings.heightStep)] = 0.f;
		open.push({ Estimate(start, nodes.front().height, end, arrival, road_class), 0 });

		while (!open.empty() && nodes.size() < settings.maxNodes)
		{
			const uint32_t current_index = open.top().node;
			const Node     current       = nodes[current_index];
			open.pop();

			if (current.goal)
				return Curve(Trace(nodes, current_index), settings.pointSpacing);

			const auto recorded = visited.find(KeyOf(current.position, current.heading, current.clearance, step, settings.heightStep));
			if (recorded != visited.end() && current.cost > recorded->second)
				continue;

			const float remaining = glm::distance(current.position, end);
			if (CanArriveAt(current, end, remaining, step, settings.headings))
			{
				const StepGround           ground    = SampleStep(current.position, end, road_class.halfWidth(), sample_height);
				const std::optional<float> last_step = StepCost(ground, current.height, arrival, 0, remaining, road_class, settings);

				if (last_step.has_value())
				{
					nodes.push_back({ end, arrival, current.heading, current.cost + *last_step, current_index, true });
					open.push({ nodes.back().cost, static_cast<uint32_t>(nodes.size() - 1) });
				}
			}

			for (const int turn : TURN_OPTIONS)
			{
				const int       heading  = (current.heading + turn + settings.headings) % settings.headings;
				const glm::vec2 position = current.position + HeadingDirection(heading, settings.headings) * step;
				if (position.x < minimum.x || position.x > maximum.x || position.y < minimum.y || position.y > maximum.y)
					continue;

				const StepGround ground = SampleStep(current.position, position, road_class.halfWidth(), sample_height);

				for (const float height : HeightOptions(current.height, ground.end[CENTRE], climb))
				{
					const std::optional<float> step_cost = StepCost(ground, current.height, height, turn, step, road_class, settings);
					if (!step_cost.has_value())
						continue;

					const float      cost      = current.cost + *step_cost;
					const float      clearance = height - ground.end[CENTRE];
					const VisitedKey key       = KeyOf(position, heading, clearance, step, settings.heightStep);
					const auto       found     = visited.find(key);
					if (found != visited.end() && found->second <= cost)
						continue;

					visited[key] = cost;
					nodes.push_back({ position, height, heading, cost, current_index, false, clearance });
					open.push({ cost + settings.heuristicWeight * Estimate(position, height, end, arrival, road_class), static_cast<uint32_t>(nodes.size() - 1) });
				}
			}
		}

		DEBUG_ERROR("Could not plan a road between ({:.0f}, {:.0f}) and ({:.0f}, {:.0f})", start.x, start.y, end.x, end.y);
		return std::nullopt;
	}
}
