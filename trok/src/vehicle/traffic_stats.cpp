#include <trok/vehicle/traffic_stats.hpp>

#include <imgui.h>

#include <format>
#include <iterator>
#include <numeric>
#include <span>
#include <string>
#include <utility>

namespace trok
{
	namespace
	{
		constexpr char   WINDOW_TITLE[] = "Traffic";
		constexpr double PERCENT        = 100.0;

		constexpr const char* SPAWN_LABELS[]   = { "Spawned", "No road spot", "Out of range", "In view", "Terrain unloaded", "Crowded", "No trip", "No course", "No gap in lane" };
		constexpr const char* DESPAWN_LABELS[] = { "Lost", "Stalled", "Frozen", "Too far", "Surplus" };

		static_assert(std::size(SPAWN_LABELS)   == std::to_underlying(SpawnOutcome::eCount));
		static_assert(std::size(DESPAWN_LABELS) == std::to_underlying(DespawnReason::eCount));

		void Text(const std::string& text)
		{
			ImGui::TextUnformatted(text.c_str());
		}

		double Percent(double part, double whole)
		{
			return whole > 0.0 ? part / whole * PERCENT : 0.0;
		}

		double Rate(uint64_t count, float elapsed)
		{
			return elapsed > 0.f ? static_cast<double>(count) / elapsed : 0.0;
		}

		void DrawPopulation(const TrafficSnapshot& snapshot)
		{
			if (!ImGui::CollapsingHeader("Population", ImGuiTreeNodeFlags_DefaultOpen))
				return;

			Text(std::format("Vehicles: {} ({} frozen)", snapshot.vehicles, snapshot.frozen));
			Text(std::format("Near focus: {} of {:.1f} targeted ({:.0f}%)", snapshot.nearFoci, snapshot.targetNearFoci, Percent(static_cast<double>(snapshot.nearFoci), snapshot.targetNearFoci)));
			Text(std::format("Due to despawn but in view: {}", snapshot.awaitingDespawn));
		}

		void DrawDriving(const TrafficSnapshot& snapshot)
		{
			if (!ImGui::CollapsingHeader("Driving", ImGuiTreeNodeFlags_DefaultOpen))
				return;

			Text(std::format("Stalled: {} (longest {:.1f} s)", snapshot.stalled, snapshot.longestStall));
			Text(std::format("Without a course: {} (longest {:.1f} s)", snapshot.lost, snapshot.longestLost));
			Text(std::format("Mean speed: {:.1f} of {:.1f} m/s desired ({:.0f}%)", snapshot.meanSpeed, snapshot.meanDesiredSpeed, Percent(snapshot.meanSpeed, snapshot.meanDesiredSpeed)));
			Text(std::format("Off course: {:.2f} m mean, {:.2f} m worst", snapshot.meanDeviation, snapshot.largestDeviation));
			Text(std::format("Closest gap: {:.1f} m ({} under minimum)", snapshot.closestGap, snapshot.tailgating));
		}

		void DrawNavigation(const TrafficSnapshot& snapshot, const TrafficCounters& counters)
		{
			if (!ImGui::CollapsingHeader("Navigation", ImGuiTreeNodeFlags_DefaultOpen))
				return;

			Text(std::format("Trips continued: {}, stranded: {}", counters.tripsContinued, counters.tripsStranded));
			Text(std::format("Course plans: {}, failed: {} ({:.1f}%)", counters.plans, counters.failedPlans, Percent(static_cast<double>(counters.failedPlans), static_cast<double>(counters.plans))));
			Text(std::format("Awaiting replan: {} (longest {:.1f} s overdue)", snapshot.awaitingReplan, snapshot.longestReplanWait));
		}

		void DrawTally(const char* title, std::span<const uint64_t> counts, std::span<const char* const> labels, float elapsed)
		{
			if (!ImGui::CollapsingHeader(title, ImGuiTreeNodeFlags_DefaultOpen))
				return;

			const uint64_t total = std::accumulate(counts.begin(), counts.end(), uint64_t { 0 });
			for (size_t index = 0; index < counts.size(); index++)
				Text(std::format("{}: {} ({:.1f}%, {:.2f}/s)", labels[index], counts[index], Percent(static_cast<double>(counts[index]), static_cast<double>(total)), Rate(counts[index], elapsed)));
		}
	}

	void DrawTrafficStats(Traffic& traffic, lunar::Scene& scene)
	{
		if (ImGui::Begin(WINDOW_TITLE, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			const TrafficSnapshot  snapshot = traffic.inspect(scene);
			const TrafficCounters& counters = traffic.getCounters();

			DrawPopulation(snapshot);
			DrawDriving(snapshot);
			DrawNavigation(snapshot, counters);
			DrawTally("Spawn attempts", counters.spawns, SPAWN_LABELS, counters.elapsed);
			DrawTally("Despawns", counters.despawns, DESPAWN_LABELS, counters.elapsed);

			Text(std::format("Counting for {:.0f} s", counters.elapsed));
			if (ImGui::Button("Reset"))
				traffic.resetCounters();
		}

		ImGui::End();
	}
}
