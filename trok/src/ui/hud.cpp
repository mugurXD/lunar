#include <trok/ui/hud.hpp>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/ElementDocument.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <string_view>

namespace trok
{
	namespace
	{
		constexpr const char*      MODEL_NAME             = "hud";
		constexpr float            PAYOUT_DISPLAY_SECONDS = 2.5f;
		constexpr float            FLASH_SECONDS          = 0.08f;
		constexpr int              SECONDS_PER_MINUTE     = 60;
		constexpr float            PERCENT                = 100.f;
		constexpr float            DAMAGED_CONDITION      = 0.5f;
		constexpr std::string_view PICKUP_TEXT            = "Pick up the cargo";
		constexpr std::string_view DROPOFF_TEXT           = "Deliver the cargo";

		Rml::String FormatTime(double seconds)
		{
			const int whole_seconds = static_cast<int>(seconds);
			return std::format("{}:{:02}", whole_seconds / SECONDS_PER_MINUTE, whole_seconds % SECONDS_PER_MINUTE);
		}
	}

	Hud::Hud(lunar::UI::UiLayer& ui, const Fs::Path& document) noexcept
		: context(ui.getContext())
	{
		if (context == nullptr)
			return;

		Rml::DataModelConstructor constructor = context->CreateDataModel(MODEL_NAME);
		if (!constructor)
			return;

		constructor.Bind("money",        &state.money);
		constructor.Bind("hasDelivery",  &state.hasDelivery);
		constructor.Bind("pickingUp",    &state.pickingUp);
		constructor.Bind("stage",        &state.stage);
		constructor.Bind("reward",       &state.reward);
		constructor.Bind("elapsed",      &state.elapsed);
		constructor.Bind("parTime",      &state.parTime);
		constructor.Bind("late",         &state.late);
		constructor.Bind("payout",       &state.payout);
		constructor.Bind("showPayout",   &state.showPayout);
		constructor.Bind("flash",        &state.flash);
		constructor.Bind("condition",    &state.condition);
		constructor.Bind("conditionBar", &state.conditionBar);
		constructor.Bind("damaged",      &state.damaged);
		model = constructor.GetModelHandle();

		if (Rml::ElementDocument* loaded = ui.loadDocument(document))
			loaded->Show();
	}

	Hud::~Hud() noexcept
	{
		if (context != nullptr)
			context->RemoveDataModel(MODEL_NAME);
	}

	void Hud::flash()
	{
		flashTimer = FLASH_SECONDS;
	}

	void Hud::update(const Deliveries& deliveries, std::optional<int64_t> payout, float delta_time)
	{
		if (payout.has_value())
		{
			state.payout = *payout;
			payoutTimer  = PAYOUT_DISPLAY_SECONDS;
		}

		payoutTimer = std::max(payoutTimer - delta_time, 0.f);
		flashTimer  = std::max(flashTimer - delta_time, 0.f);

		const std::optional<Delivery>& delivery = deliveries.getDelivery();

		state.money       = deliveries.getMoney();
		state.showPayout  = payoutTimer > 0.f;
		state.flash       = flashTimer > 0.f;
		state.hasDelivery = delivery.has_value();

		if (delivery.has_value())
		{
			state.pickingUp = delivery->stage == DeliveryStage::ePickup;
			state.stage     = state.pickingUp ? PICKUP_TEXT : DROPOFF_TEXT;
			state.reward    = delivery->reward;
			state.elapsed   = FormatTime(delivery->elapsed);
			state.parTime   = FormatTime(delivery->parTime);
			state.late      = delivery->elapsed > delivery->parTime;

			state.condition    = static_cast<int>(std::lround(delivery->condition * PERCENT));
			state.conditionBar = std::format("{}%", state.condition);
			state.damaged      = delivery->condition < DAMAGED_CONDITION;
		}

		if (model)
			model.DirtyAllVariables();
	}
}
