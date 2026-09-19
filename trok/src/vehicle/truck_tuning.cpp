#include <trok/vehicle/truck_tuning.hpp>

#include <lunar/file/json_file.hpp>

#include <imgui.h>

#include <cmath>
#include <format>
#include <span>
#include <utility>

namespace trok
{
	namespace
	{
		using lunar::Physics::VehicleSettings;

		constexpr char  WINDOW_TITLE[]        = "Truck tuning";
		constexpr char  SLIDER_FORMAT[]       = "%.3f";
		constexpr int   READABLE_INDENTATION  = 4;
		constexpr float KMH_PER_METRE_SECOND  = 3.6f;
		constexpr float FULL_TURN_DEGREES     = 360.f;

		const glm::vec3 LOCAL_FORWARD = { 0.f, 0.f, -1.f };

		struct FloatSetting
		{
			const char*              label   = nullptr;
			float VehicleSettings::* value   = nullptr;
			float                    minimum = 0.f;
			float                    maximum = 0.f;
		};

		constexpr FloatSetting SUSPENSION_SETTINGS[] =
		{
			{ "Rest length", &VehicleSettings::restLength, 0.1f,  1.5f },
			{ "Stiffness",   &VehicleSettings::stiffness,  1e4f,  4e5f },
			{ "Damping",     &VehicleSettings::damping,    0.f,   6e4f }
		};

		constexpr FloatSetting ENGINE_SETTINGS[] =
		{
			{ "Drive force",        &VehicleSettings::driveForce,        0.f, 1e5f },
			{ "Power",              &VehicleSettings::enginePower,       0.f, 1e6f },
			{ "Max speed",          &VehicleSettings::maxSpeed,          0.f, 60.f },
			{ "Brake force",        &VehicleSettings::brakeForce,        0.f, 3e5f },
			{ "Rolling resistance", &VehicleSettings::rollingResistance, 0.f, 1e3f },
			{ "Drag",               &VehicleSettings::dragCoefficient,   0.f, 20.f }
		};

		constexpr FloatSetting STEERING_SETTINGS[] =
		{
			{ "Max angle", &VehicleSettings::maxSteerAngle, 5.f,  60.f },
			{ "Speed",     &VehicleSettings::steerSpeed,    10.f, 360.f }
		};

		constexpr FloatSetting TYRE_SETTINGS[] =
		{
			{ "Cornering stiffness", &VehicleSettings::corneringStiffness, 0.f,  2e5f },
			{ "Friction",            &VehicleSettings::tyreFriction,       0.2f, 2.f },
			{ "Roll influence",      &VehicleSettings::rollInfluence,      0.f,  1.f }
		};

		void Text(const std::string& text)
		{
			ImGui::TextUnformatted(text.c_str());
		}

		void DrawSliders(const char* title, std::span<const FloatSetting> group, VehicleSettings& settings)
		{
			if (!ImGui::CollapsingHeader(title, ImGuiTreeNodeFlags_DefaultOpen))
				return;

			for (const FloatSetting& setting : group)
				ImGui::SliderFloat(setting.label, &(settings.*setting.value), setting.minimum, setting.maximum, SLIDER_FORMAT);
		}

		float Heading(const glm::quat& rotation)
		{
			const glm::vec3 forward = rotation * LOCAL_FORWARD;
			const float     degrees = glm::degrees(std::atan2(forward.x, -forward.z));
			return degrees < 0.f ? degrees + FULL_TURN_DEGREES : degrees;
		}
	}

	TruckTuningWindow::TruckTuningWindow(TruckDefinition definition, Fs::Path save_path) noexcept
		: definition(std::move(definition)),
		savePath(std::move(save_path))
	{
	}

	void TruckTuningWindow::draw(Truck& truck)
	{
		if (ImGui::Begin(WINDOW_TITLE, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			drawReadouts(truck);
			drawSettings(truck.editVehicle().editSettings());

			if (ImGui::Button("Save"))
				save(truck.getVehicle().getSettings());

			if (!status.empty())
				Text(status);
		}

		ImGui::End();
	}

	void TruckTuningWindow::drawReadouts(const Truck& truck) const
	{
		if (!ImGui::CollapsingHeader("Readouts", ImGuiTreeNodeFlags_DefaultOpen))
			return;

		const lunar::Physics::RaycastVehicle& vehicle  = truck.getVehicle();
		const lunar::Transform&               transform = truck.getTransform();
		const float                           speed    = vehicle.getForwardSpeed();

		Text(std::format("Speed: {:.1f} km/h ({:.1f} m/s)", speed * KMH_PER_METRE_SECOND, speed));
		Text(std::format("Position: ({:.1f}, {:.1f}, {:.1f})", transform.position.x, transform.position.y, transform.position.z));
		Text(std::format("Heading: {:.0f} degrees", Heading(transform.rotation)));
		Text(truck.isSimulated() ? "Driving" : "Frozen: waiting for terrain");

		for (size_t wheel = 0; wheel < vehicle.getWheels().size(); wheel++)
		{
			const lunar::Physics::WheelState& state = vehicle.getWheels()[wheel];
			Text(std::format("Wheel {}: {}, compression {:.3f} m", wheel, state.grounded ? "grounded" : "airborne", state.compression));
		}
	}

	void TruckTuningWindow::drawSettings(VehicleSettings& settings) const
	{
		DrawSliders("Suspension", SUSPENSION_SETTINGS, settings);
		DrawSliders("Engine",     ENGINE_SETTINGS,     settings);
		DrawSliders("Steering",   STEERING_SETTINGS,   settings);
		DrawSliders("Tyres",      TYRE_SETTINGS,       settings);
	}

	void TruckTuningWindow::save(const VehicleSettings& settings)
	{
		definition.vehicle = settings;

		Fs::JsonFile file;
		file.content     = TruckDefinition::Serialize(definition);
		file.indentation = READABLE_INDENTATION;

		status = file.toFile(savePath) ? std::format("Saved to {}", savePath.string()) : std::format("Could not save to {}", savePath.string());
	}
}
