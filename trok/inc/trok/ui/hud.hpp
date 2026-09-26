#pragma once
#include <trok/gameplay/deliveries.hpp>
#include <lunar/file/filesystem.hpp>
#include <lunar/ui/ui_layer.hpp>

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

#include <cstdint>
#include <optional>

namespace trok
{
	class Hud
	{
	public:
		Hud(lunar::UI::UiLayer& ui, const Fs::Path& document) noexcept;
		~Hud() noexcept;

		Hud(const Hud&)            = delete;
		Hud& operator=(const Hud&) = delete;

		void update(const Deliveries& deliveries, std::optional<int64_t> payout, float delta_time);
		void flash();

	private:
		struct State
		{
			int64_t     money       = 0;
			bool        hasDelivery = false;
			bool        pickingUp   = false;
			Rml::String stage       = {};
			int64_t     reward      = 0;
			Rml::String elapsed     = {};
			Rml::String parTime     = {};
			bool        late        = false;
			int64_t     payout      = 0;
			bool        showPayout  = false;
			bool        flash       = false;
		};

		Rml::Context*        context     = nullptr;
		Rml::DataModelHandle model       = {};
		State                state       = {};
		float                payoutTimer = 0.f;
		float                flashTimer  = 0.f;
	};
}
