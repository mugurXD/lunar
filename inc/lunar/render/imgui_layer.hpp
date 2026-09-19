#pragma once
#include <lunar/core/common.hpp>
#include <lunar/render/command_list.hpp>
#include <lunar/render/render_device.hpp>
#include <lunar/render/window.hpp>

namespace lunar::Render
{
	class LUNAR_API ImGuiLayer
	{
	public:
		ImGuiLayer(RenderDevice& device, Swapchain& swapchain, Window_T& window) noexcept;
		~ImGuiLayer()                                                            noexcept;

		ImGuiLayer(const ImGuiLayer&)            = delete;
		ImGuiLayer& operator=(const ImGuiLayer&) = delete;

		void beginFrame();
		void endFrame();
		void record(CommandList& commands);

	private:
		RenderDevice& device;
		bool          initialized = false;
	};
}
