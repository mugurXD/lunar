#include <trok/world/world_map.hpp>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace trok
{
	namespace
	{
		constexpr float    MIN_SPAN          = 1000.f;
		constexpr float    MAX_SPAN          = 80000.f;
		constexpr float    ZOOM_STEP         = 1.25f;
		constexpr uint32_t PLANS_PER_FRAME   = 1;
		constexpr float    ROAD_THICKNESS    = 2.f;
		constexpr float    VIEWER_RADIUS     = 5.f;
		constexpr float    SETTLEMENT_RADIUS = 4.f;
		constexpr float    HALF              = 0.5f;
		constexpr ImU32    VOID_COLOR        = IM_COL32(24, 26, 30, 255);
		constexpr ImU32    ROAD_COLOR        = IM_COL32(40, 40, 44, 255);
		constexpr ImU32    RIVER_COLOR       = IM_COL32(70, 120, 180, 255);
		constexpr float    RIVER_MIN_THICK   = 1.5f;
		constexpr float    RIVER_WIDTH_SCALE = 0.25f;
		constexpr ImU32    VIEWER_COLOR      = IM_COL32(255, 80, 60, 255);
		constexpr ImU32    SETTLEMENT_COLOR  = IM_COL32(245, 235, 180, 255);
		constexpr ImU32    OUTLINE_COLOR     = IM_COL32(15, 15, 18, 255);
		constexpr float    SEA_DEPTH_RANGE   = 45.f;

		const glm::vec3    SEA_SHALLOW       = { 0.24f, 0.47f, 0.62f };
		const glm::vec3    SEA_DEEP          = { 0.05f, 0.14f, 0.30f };

		ImVec2 Screen(const glm::vec2& origin, const glm::vec2& size, const glm::dvec2& center, float scale, const glm::dvec2& world)
		{
			const glm::dvec2 offset = (world - center) * static_cast<double>(scale);
			return { origin.x + size.x * HALF + static_cast<float>(offset.x), origin.y + size.y * HALF + static_cast<float>(offset.y) };
		}

		ImU32 ToColor(const glm::vec3& color)
		{
			return IM_COL32(static_cast<int>(color.r * 255.f), static_cast<int>(color.g * 255.f), static_cast<int>(color.b * 255.f), 255);
		}

		ImU32 BiomeColor(const Biome& biome)
		{
			return ToColor(glm::mix(biome.colors.lowColor, biome.colors.highColor, HALF));
		}

		ImU32 SeaColor(float below)
		{
			return ToColor(glm::mix(SEA_SHALLOW, SEA_DEEP, glm::clamp(below / SEA_DEPTH_RANGE, 0.f, 1.f)));
		}
	}

	WorldMapWindow::WorldMapWindow(std::shared_ptr<const BiomeLibrary>   biomes,
	                               std::shared_ptr<const RegionPlanner>  planner,
	                               std::shared_ptr<const ClimateSampler> climate,
	                               ElevationCurve                        elevation,
	                               lunar::World::WorldSettings           settings) noexcept
		: biomes(std::move(biomes)),
		planner(std::move(planner)),
		climate(std::move(climate)),
		elevation(std::move(elevation)),
		settings(settings)
	{
	}

	std::shared_ptr<const RegionPlan> WorldMapWindow::findPlan(lunar::World::RegionCoord coord)
	{
		const auto found = plans.find(coord);
		if (found != plans.end())
			return found->second;

		if (plannedHere >= PLANS_PER_FRAME)
			return nullptr;

		plannedHere++;
		return plans.emplace(coord, std::make_shared<const RegionPlan>(planner->plan(coord, settings))).first->second;
	}

	std::vector<RegionContext::Region> WorldMapWindow::gatherVisible(const View& view)
	{
		const double     half_span = static_cast<double>(view.size.x) * HALF / view.scale;
		const double     half_high = static_cast<double>(view.size.y) * HALF / view.scale;
		const glm::dvec2 minimum   = view.center - glm::dvec2(half_span, half_high);
		const glm::dvec2 maximum   = view.center + glm::dvec2(half_span, half_high);

		const lunar::World::RegionCoord lowest  = lunar::World::RegionAt(lunar::World::ChunkAt(minimum.x, minimum.y, settings), settings);
		const lunar::World::RegionCoord highest = lunar::World::RegionAt(lunar::World::ChunkAt(maximum.x, maximum.y, settings), settings);

		std::vector<RegionContext::Region> visible;
		for (int32_t z = lowest.z; z <= highest.z; z++)
		{
			for (int32_t x = lowest.x; x <= highest.x; x++)
			{
				const std::shared_ptr<const RegionPlan> plan = findPlan({ x, z });
				if (plan != nullptr)
					visible.push_back({ { x, z }, plan });
			}
		}

		return visible;
	}

	void WorldMapWindow::drawBiomes(ImDrawList& drawing, const View& view, const RegionContext& context)
	{
		const double cell      = BiomeCellSize(settings);
		const double half_span = static_cast<double>(view.size.x) * HALF / view.scale;
		const double half_high = static_cast<double>(view.size.y) * HALF / view.scale;
		const double first_x   = std::floor((view.center.x - half_span) / cell) * cell;
		const double first_z   = std::floor((view.center.y - half_high) / cell) * cell;

		for (double z = first_z; z < view.center.y + half_high; z += cell)
		{
			for (double x = first_x; x < view.center.x + half_span; x += cell)
			{
				const std::optional<BiomeIndex> biome = BiomeAt(context, x + cell * HALF, z + cell * HALF);
				if (!biome.has_value())
					continue;

				const float height = elevation.heightAt(climate->sampleContinentalness(x + cell * HALF, z + cell * HALF));
				const ImU32 color  = height < elevation.seaLevel ? SeaColor(elevation.seaLevel - height)
				                                                 : BiomeColor(biomes->get(*biome));

				drawing.AddRectFilled(Screen(view.origin, view.size, view.center, view.scale, { x, z }),
				                      Screen(view.origin, view.size, view.center, view.scale, { x + cell, z + cell }),
				                      color);
			}
		}
	}

	void WorldMapWindow::drawSettlements(ImDrawList& drawing, const View& view, const std::vector<RegionContext::Region>& visible)
	{
		for (const RegionContext::Region& region : visible)
		{
			for (const Settlement& settlement : region.plan->settlements)
			{
				const ImVec2 at = Screen(view.origin, view.size, view.center, view.scale, settlement.center);

				drawing.AddCircleFilled(at, SETTLEMENT_RADIUS, SETTLEMENT_COLOR);
				drawing.AddCircle(at, SETTLEMENT_RADIUS, OUTLINE_COLOR);
				drawing.AddText({ at.x + SETTLEMENT_RADIUS * 2.f, at.y - SETTLEMENT_RADIUS }, SETTLEMENT_COLOR, settlement.type.c_str());
			}
		}
	}

	void WorldMapWindow::drawRoads(ImDrawList& drawing, const View& view, const RoadNetwork& roads)
	{
		std::vector<ImVec2> points;

		for (size_t road = 0; road < roads.getRoadCount(); road++)
		{
			const std::span<const glm::vec3> centreline = roads.getRoad(road);
			if (centreline.size() < 2)
				continue;

			points.clear();
			points.reserve(centreline.size());
			for (const glm::vec3& point : centreline)
				points.push_back(Screen(view.origin, view.size, view.center, view.scale, { point.x, point.z }));

			drawing.AddPolyline(points.data(), static_cast<int>(points.size()), ROAD_COLOR, ImDrawFlags_None, ROAD_THICKNESS);
		}
	}

	void WorldMapWindow::drawRivers(ImDrawList& drawing, const View& view, const std::vector<RegionContext::Region>& visible)
	{
		std::vector<ImVec2> points;

		for (const RegionContext::Region& region : visible)
		{
			for (const River& river : region.plan->rivers)
			{
				if (river.points.size() < 2)
					continue;

				points.clear();
				points.reserve(river.points.size());
				for (const RiverPoint& point : river.points)
					points.push_back(Screen(view.origin, view.size, view.center, view.scale, point.position));

				const float thickness = std::max(RIVER_MIN_THICK, static_cast<float>(river.points.back().width) * RIVER_WIDTH_SCALE);
				drawing.AddPolyline(points.data(), static_cast<int>(points.size()), RIVER_COLOR, ImDrawFlags_None, thickness);
			}
		}
	}

	std::optional<glm::dvec2> WorldMapWindow::draw(const glm::vec3& viewer, const RoadNetwork* roads)
	{
		plannedHere = 0;

		if (!ImGui::Begin("World map"))
		{
			ImGui::End();
			return std::nullopt;
		}

		if (following)
			center = { viewer.x, viewer.z };

		ImGui::Text("%.0f km across", span / 1000.f);
		ImGui::SameLine();
		if (ImGui::Button(following ? "Following" : "Follow"))
			following = true;

		ImGui::SameLine();
		ImGui::Text("(%.0f, %.0f) - double click to travel", center.x, center.y);

		const ImVec2 canvas = ImGui::GetContentRegionAvail();
		if (canvas.x < 1.f || canvas.y < 1.f)
		{
			ImGui::End();
			return std::nullopt;
		}

		const ImVec2 top_left = ImGui::GetCursorScreenPos();
		const View   view     =
		{
			.origin = { top_left.x, top_left.y },
			.size   = { canvas.x, canvas.y },
			.center = center,
			.scale  = canvas.x / span
		};

		ImGui::InvisibleButton("canvas", canvas);

		if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
		{
			const ImVec2 delta = ImGui::GetIO().MouseDelta;

			center   -= glm::dvec2(delta.x, delta.y) / static_cast<double>(view.scale);
			following = false;
		}

		if (ImGui::IsItemHovered() && ImGui::GetIO().MouseWheel != 0.f)
			span = std::clamp(span / std::pow(ZOOM_STEP, ImGui::GetIO().MouseWheel), MIN_SPAN, MAX_SPAN);

		ImDrawList& drawing = *ImGui::GetWindowDrawList();
		drawing.PushClipRect(top_left, { top_left.x + canvas.x, top_left.y + canvas.y }, true);
		drawing.AddRectFilled(top_left, { top_left.x + canvas.x, top_left.y + canvas.y }, VOID_COLOR);

		const std::vector<RegionContext::Region> visible = gatherVisible(view);
		const RegionContext                      context(settings, visible);

		drawBiomes(drawing, view, context);
		drawRivers(drawing, view, visible);

		if (roads != nullptr)
			drawRoads(drawing, view, *roads);

		drawSettlements(drawing, view, visible);

		const ImVec2 at = Screen(view.origin, view.size, view.center, view.scale, { viewer.x, viewer.z });
		drawing.AddCircleFilled(at, VIEWER_RADIUS, VIEWER_COLOR);
		drawing.AddCircle(at, VIEWER_RADIUS, OUTLINE_COLOR);

		drawing.PopClipRect();

		std::optional<glm::dvec2> travel;
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			const ImVec2 cursor = ImGui::GetIO().MousePos;

			travel = view.center + glm::dvec2(cursor.x - (view.origin.x + view.size.x * HALF),
			                                  cursor.y - (view.origin.y + view.size.y * HALF)) / static_cast<double>(view.scale);
		}

		ImGui::End();
		return travel;
	}
}
