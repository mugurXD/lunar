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
		constexpr ImU32    VIEWER_COLOR      = IM_COL32(255, 80, 60, 255);
		constexpr ImU32    SETTLEMENT_COLOR  = IM_COL32(245, 235, 180, 255);
		constexpr ImU32    OUTLINE_COLOR     = IM_COL32(15, 15, 18, 255);

		ImVec2 Screen(const glm::vec2& origin, const glm::vec2& size, const glm::dvec2& center, float scale, const glm::dvec2& world)
		{
			const glm::dvec2 offset = (world - center) * static_cast<double>(scale);
			return { origin.x + size.x * HALF + static_cast<float>(offset.x), origin.y + size.y * HALF + static_cast<float>(offset.y) };
		}

		ImU32 BiomeColor(const Biome& biome)
		{
			const glm::vec3 color = glm::mix(biome.colors.lowColor, biome.colors.highColor, HALF);
			return IM_COL32(static_cast<int>(color.r * 255.f), static_cast<int>(color.g * 255.f), static_cast<int>(color.b * 255.f), 255);
		}
	}

	WorldMapWindow::WorldMapWindow(std::shared_ptr<const BiomeLibrary>  biomes,
	                               std::shared_ptr<const RegionPlanner> planner,
	                               lunar::World::WorldSettings          settings) noexcept
		: biomes(std::move(biomes)),
		planner(std::move(planner)),
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

				drawing.AddRectFilled(Screen(view.origin, view.size, view.center, view.scale, { x, z }),
				                      Screen(view.origin, view.size, view.center, view.scale, { x + cell, z + cell }),
				                      BiomeColor(biomes->get(*biome)));
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
		const std::span<const glm::vec3> centreline = roads.getCentreline();
		if (centreline.size() < 2)
			return;

		std::vector<ImVec2> points;
		points.reserve(centreline.size());
		for (const glm::vec3& point : centreline)
			points.push_back(Screen(view.origin, view.size, view.center, view.scale, { point.x, point.z }));

		drawing.AddPolyline(points.data(), static_cast<int>(points.size()), ROAD_COLOR, ImDrawFlags_None, ROAD_THICKNESS);
	}

	void WorldMapWindow::draw(const glm::vec3& viewer, const RoadNetwork* roads)
	{
		plannedHere = 0;

		if (!ImGui::Begin("World map"))
		{
			ImGui::End();
			return;
		}

		if (following)
			center = { viewer.x, viewer.z };

		ImGui::Text("%.0f km across", span / 1000.f);
		ImGui::SameLine();
		if (ImGui::Button(following ? "Following" : "Follow"))
			following = true;

		ImGui::SameLine();
		ImGui::Text("(%.0f, %.0f)", center.x, center.y);

		const ImVec2 canvas = ImGui::GetContentRegionAvail();
		if (canvas.x < 1.f || canvas.y < 1.f)
		{
			ImGui::End();
			return;
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

		if (roads != nullptr)
			drawRoads(drawing, view, *roads);

		drawSettlements(drawing, view, visible);

		const ImVec2 at = Screen(view.origin, view.size, view.center, view.scale, { viewer.x, viewer.z });
		drawing.AddCircleFilled(at, VIEWER_RADIUS, VIEWER_COLOR);
		drawing.AddCircle(at, VIEWER_RADIUS, OUTLINE_COLOR);

		drawing.PopClipRect();
		ImGui::End();
	}
}
