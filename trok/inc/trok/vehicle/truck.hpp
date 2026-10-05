#pragma once
#include <trok/vehicle/vehicle.hpp>

namespace trok
{
	class Truck : public Vehicle
	{
	public:
		using Vehicle::Vehicle;
		using Vehicle::drive;
	};
}
