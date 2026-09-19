#pragma once
#include <trok/vehicle/truck.hpp>

#include <lunar/file/filesystem.hpp>

#include <string>

namespace trok
{
	class TruckTuningWindow
	{
	public:
		TruckTuningWindow(TruckDefinition definition, Fs::Path save_path) noexcept;

		void draw(Truck& truck);

	private:
		void drawReadouts(const Truck& truck)                    const;
		void drawSettings(lunar::Physics::VehicleSettings& settings) const;
		void save(const lunar::Physics::VehicleSettings& settings);

		TruckDefinition definition;
		Fs::Path        savePath;
		std::string     status = {};
	};
}
