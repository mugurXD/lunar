#include <trok/world/drive_planner.hpp>
#include <trok/world/road_routes.hpp>

#include <lunar/utils/geometry.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <queue>
#include <utility>

namespace trok
{
	namespace
	{
		constexpr float    HALF           = 0.5f;
		constexpr float    INFINITE_COST  = std::numeric_limits<float>::infinity();
		constexpr float    SEGMENT_MARGIN = 8.f;
		constexpr uint32_t NO_PARENT      = std::numeric_limits<uint32_t>::max();

		const std::array<glm::ivec2, 8> STEPS =
		{
			glm::ivec2(1, 0), glm::ivec2(-1, 0), glm::ivec2(0, 1),  glm::ivec2(0, -1),
			glm::ivec2(1, 1), glm::ivec2(1, -1), glm::ivec2(-1, 1), glm::ivec2(-1, -1)
		};

		glm::vec2 Flat(const glm::vec3& point)
		{
			return { point.x, point.z };
		}

		uint32_t IndexOf(const glm::ivec2& cell, const glm::ivec2& size)
		{
			return static_cast<uint32_t>(cell.y * size.x + cell.x);
		}

		bool IsInside(const glm::ivec2& cell, const glm::ivec2& size)
		{
			return cell.x >= 0 && cell.y >= 0 && cell.x < size.x && cell.y < size.y;
		}
	}

	DrivePlanner::DrivePlanner(GroundSampler ground, DrivePlannerSettings settings) noexcept
		: ground(std::move(ground)),
		settings(settings)
	{
	}

	std::vector<DrivePoint> DrivePlanner::plan(const RoadNetwork& network, const glm::vec3& from, const glm::vec3& to)
	{
		if (!prepare(network, Flat(from), Flat(to)))
			return {};

		const RoadClass& road_class = network.getRoadClass();
		const uint32_t   start      = IndexOf(cellOf(Flat(from)), size);
		const uint32_t   goal       = IndexOf(cellOf(Flat(to)), size);
		const glm::vec2  target     = centreOf(goal);
		const auto       estimate   = [&](uint32_t index) { return glm::distance(centreOf(index), target); };

		costs.assign(cells.size(), INFINITE_COST);
		parents.assign(cells.size(), NO_PARENT);
		resolve(start);

		using Entry = std::pair<float, uint32_t>;
		std::priority_queue<Entry, std::vector<Entry>, std::greater<>> open;
		costs[start] = 0.f;
		open.push({ estimate(start), start });

		while (!open.empty())
		{
			const auto [priority, current] = open.top();
			open.pop();

			if (current == goal)
				return smooth(trace(goal, from, to));

			if (priority > costs[current] + estimate(current))
				continue;

			const glm::ivec2 cell = { static_cast<int32_t>(current) % size.x, static_cast<int32_t>(current) / size.x };
			for (const glm::ivec2& step : STEPS)
			{
				if (!IsInside(cell + step, size))
					continue;

				const uint32_t  next   = IndexOf(cell + step, size);
				const glm::vec2 move   = glm::normalize(glm::vec2(step));
				const float     length = glm::length(glm::vec2(step)) * settings.cellSize;
				const float     cost   = costs[current] + stepCost(cells[current], resolve(next), move, length, road_class);
				if (cost >= costs[next])
					continue;

				costs[next]   = cost;
				parents[next] = current;
				open.push({ cost + estimate(next), next });
			}
		}

		return {};
	}

	bool DrivePlanner::prepare(const RoadNetwork& network, const glm::vec2& from, const glm::vec2& to)
	{
		const glm::vec2  minimum = glm::min(from, to) - settings.windowMargin;
		const glm::vec2  maximum = glm::max(from, to) + settings.windowMargin;
		const glm::ivec2 window  = glm::ivec2(glm::ceil((maximum - minimum) / settings.cellSize));
		if (window.x > settings.maxWindowCells || window.y > settings.maxWindowCells)
			return false;

		origin = minimum;
		size   = window;
		cells.assign(static_cast<size_t>(size.x) * static_cast<size_t>(size.y), Cell {});

		stampRoads(network, minimum, maximum);
		stampJunctions(network, minimum, maximum);
		return true;
	}

	void DrivePlanner::stampRoads(const RoadNetwork& network, const glm::vec2& minimum, const glm::vec2& maximum)
	{
		const std::span<const glm::vec3> points     = network.getCentreline();
		const RoadClass&                 road_class = network.getRoadClass();
		const float                      reach      = road_class.halfWidth();
		const glm::vec2                  margin     = glm::vec2(reach + SEGMENT_MARGIN);

		for (const uint32_t segment : network.segmentsWithin(minimum - margin, maximum + margin))
		{
			const glm::vec2 from   = Flat(points[segment]);
			const glm::vec2 to     = Flat(points[segment + 1]);
			const glm::vec2 course = to - from;
			if (course == glm::vec2(0.f))
				continue;

			const glm::vec2  direction = glm::normalize(course);
			const glm::vec2  side      = { -direction.y, direction.x };
			const glm::ivec2 lowest    = cellOf(glm::min(from, to) - reach);
			const glm::ivec2 highest   = cellOf(glm::max(from, to) + reach);

			for (int32_t z = lowest.y; z <= highest.y; z++)
			{
				for (int32_t x = lowest.x; x <= highest.x; x++)
				{
					const uint32_t  index    = IndexOf({ x, z }, size);
					const glm::vec2 centre   = centreOf(index);
					float           amount   = 0.f;
					const float     distance = lunar::DistanceToSegment(centre, from, to, amount);
					if (distance > reach || distance >= cells[index].roadDistance)
						continue;

					cells[index] = Cell
					{
						.surface      = Surface::eRoad,
						.height       = glm::mix(points[segment].y, points[segment + 1].y, amount) + road_class.surfaceOffset,
						.offset       = glm::dot(centre - from, side),
						.direction    = direction,
						.roadDistance = distance
					};
				}
			}
		}
	}

	void DrivePlanner::stampJunctions(const RoadNetwork& network, const glm::vec2& minimum, const glm::vec2& maximum)
	{
		const RoadClass& road_class = network.getRoadClass();
		const glm::vec2  margin     = glm::vec2(road_class.halfWidth() + SEGMENT_MARGIN);

		for (const uint32_t node : network.nodesWithin(minimum - margin, maximum + margin))
		{
			const RoadNode&  junction = network.getNodes()[node];
			const glm::vec2  centre   = Flat(junction.position);
			const glm::ivec2 lowest   = cellOf(centre - junction.radius);
			const glm::ivec2 highest  = cellOf(centre + junction.radius);

			for (int32_t z = lowest.y; z <= highest.y; z++)
			{
				for (int32_t x = lowest.x; x <= highest.x; x++)
				{
					const uint32_t index = IndexOf({ x, z }, size);
					if (glm::distance(centreOf(index), centre) <= junction.radius)
						cells[index] = Cell { .surface = Surface::eJunction, .height = junction.position.y + road_class.surfaceOffset };
				}
			}
		}
	}

	const DrivePlanner::Cell& DrivePlanner::resolve(uint32_t index)
	{
		Cell& cell = cells[index];
		if (cell.surface != Surface::eUnknown)
			return cell;

		const std::optional<float> height = ground(centreOf(index));
		cell.surface = height.has_value() && *height > settings.seaLevel ? Surface::eTerrain : Surface::eBlocked;
		cell.height  = height.value_or(0.f);
		return cell;
	}

	float DrivePlanner::stepCost(const Cell& from, const Cell& to, const glm::vec2& move, float length, const RoadClass& road_class) const
	{
		const float grade = std::abs(to.height - from.height) / length;
		if (to.surface == Surface::eBlocked || grade > settings.maxGrade)
			return INFINITE_COST;

		if (to.surface == Surface::eJunction)
			return length;

		if (to.surface == Surface::eTerrain)
			return length * (settings.offRoadCost + settings.gradeCost * grade);

		const float drift = (to.offset - LaneOffset(road_class, glm::dot(move, to.direction) >= 0.f)) / road_class.laneWidth;
		return length * (1.f + settings.laneCost * drift * drift);
	}

	std::vector<DrivePoint> DrivePlanner::trace(uint32_t goal, const glm::vec3& from, const glm::vec3& to) const
	{
		std::vector<DrivePoint> path;
		for (uint32_t index = goal; index != NO_PARENT; index = parents[index])
		{
			const glm::vec2 centre = centreOf(index);
			const Cell&     cell   = cells[index];
			path.push_back({ .position = { centre.x, cell.height, centre.y }, .onRoad = cell.surface == Surface::eRoad || cell.surface == Surface::eJunction });
		}

		std::ranges::reverse(path);
		path.front().position = { from.x, path.front().position.y, from.z };
		path.back().position  = to;
		return path;
	}

	std::vector<DrivePoint> DrivePlanner::smooth(const std::vector<DrivePoint>& path) const
	{
		std::vector<DrivePoint> resampled = { path.front() };
		float                   carried   = 0.f;
		for (size_t index = 1; index < path.size(); index++)
		{
			const glm::vec3& from   = path[index - 1].position;
			const glm::vec3& to     = path[index].position;
			const float      length = glm::distance(from, to);

			float along = settings.pathSpacing - carried;
			for (; along < length; along += settings.pathSpacing)
				resampled.push_back({ .position = glm::mix(from, to, along / length), .onRoad = path[index].onRoad });

			carried = length - (along - settings.pathSpacing);
		}

		if (resampled.size() > 1 && glm::distance(resampled.back().position, path.back().position) < settings.pathSpacing * HALF)
			resampled.back() = path.back();
		else
			resampled.push_back(path.back());

		for (uint32_t pass = 0; pass < settings.smoothingPasses; pass++)
			for (size_t index = 1; index + 1 < resampled.size(); index++)
				resampled[index].position = glm::mix(resampled[index].position, (resampled[index - 1].position + resampled[index + 1].position) * HALF, HALF);

		return resampled;
	}

	glm::ivec2 DrivePlanner::cellOf(const glm::vec2& position) const
	{
		return glm::clamp(glm::ivec2(glm::floor((position - origin) / settings.cellSize)), glm::ivec2(0), size - 1);
	}

	glm::vec2 DrivePlanner::centreOf(uint32_t index) const
	{
		const glm::ivec2 cell = { static_cast<int32_t>(index) % size.x, static_cast<int32_t>(index) / size.x };
		return origin + (glm::vec2(cell) + HALF) * settings.cellSize;
	}
}
