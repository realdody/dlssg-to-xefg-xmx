#include "Plugin.h"
#include "Log.h"
#include "Util.h"
#include "XeFG.h"
#include "optiscaler_plugin.h"

#include <format>
#include <mutex>
#include <string>

namespace
{
	const OptiPluginApi* api = nullptr;
	OptiPlugin* self = nullptr;

	std::mutex mutex;
	Plugin::Status status;
	ULONGLONG reportedAt = 0;

	// No real frame for this long and frame generation is off
	constexpr ULONGLONG IdleAfterMs = 1000;

	void Green(const char* text) { api->TextColored(0.45f, 0.85f, 0.45f, 1.0f, text); }
	void Red(const char* text) { api->TextColored(1.0f, 0.45f, 0.4f, 1.0f, text); }

	void DrawStatus()
	{
		Plugin::Status current;
		bool active;
		{
			std::scoped_lock lock(mutex);
			current = status;
			active = reportedAt != 0 && GetTickCount64() - reportedAt < IdleAfterMs;
		}

		const auto state = XeFG::State();
		auto logTooltip = [] { api->ItemTooltip(ThisModulePath().replace_extension(L".log").string().c_str()); };

		api->Section("Status");

		if (state == XeFG::LoadState::Failed)
		{
			Red("No compatible libxess_fg.dll");
			logTooltip();
		}
		else if (active && current.failed)
		{
			Red("XeFG couldn't start");
			logTooltip();
		}
		else if (active)
		{
			Green(std::format("Generating {}x", current.generatedFrames + 1).c_str());
			api->Text(std::format("{}x{} \xC2\xB7 {}", current.width, current.height,
				current.hudless ? "HUDless UI" : "UI interpolated").c_str());
		}
		else
		{
			api->TextDisabled("Idle");
		}

		if (state != XeFG::LoadState::Loaded)
			return;

		const auto& library = XeFG::Library();
		auto line = std::format("XeFG {}.{}.{}", library.major, library.minor, library.patch);

		if (active && !current.failed)
			line += current.xmxNetwork ? " \xC2\xB7 XMX network" : " \xC2\xB7 default network";

		api->Text(line.c_str());
		api->ItemTooltip(std::format("{}\n{}", library.path.string(),
			library.knownBuild ? "Known build" : "Functions found by scanning").c_str());
	}

	void DrawSettings()
	{
		api->Section("Settings");

		const bool noXmx = Plugin::xmx == Plugin::Xmx::Unsupported;
		bool xmxNetwork = Plugin::options.xmxNetwork && !noXmx;

		api->Disabled(noXmx);

		if (api->Checkbox("XMX network", &xmxNetwork))
		{
			Plugin::options.xmxNetwork = xmxNetwork;
			api->SetBool(self, "XmxNetwork", xmxNetwork);
		}

		api->ItemTooltip(noXmx ? "Only for Intel GPUs with XMX"
							   : "XeFG's network for Intel GPUs with XMX.\nOff uses the one every other GPU gets.");
		api->Disabled(false);

		bool hudlessUi = Plugin::options.hudlessUi;

		if (api->Checkbox("HUDless UI", &hudlessUi))
		{
			Plugin::options.hudlessUi = hudlessUi;
			api->SetBool(self, "HudlessUi", hudlessUi);
		}

		api->ItemTooltip("Takes the UI from the game's HUDless image, so it isn't warped by interpolation.\n"
						 "Turn off if the game's HUDless image is broken.");

		bool debugLines = Plugin::options.debugLines;

		if (api->Checkbox("Debug lines", &debugLines))
			Plugin::options.debugLines = debugLines;

		api->ItemTooltip("XeFG draws debug lines into the frames it generated. Not saved.");
	}

	void DrawTab(void*)
	{
		DrawStatus();
		DrawSettings();
	}
} // namespace

void Plugin::Report(const Status& reported)
{
	std::scoped_lock lock(mutex);
	status = reported;
	reportedAt = GetTickCount64();
}

extern "C" __declspec(dllexport) bool OptiPluginInit(const OptiPluginApi* optiApi, OptiPlugin* plugin)
{
	api = optiApi;
	self = plugin;

	Log::Info("OptiScaler plugin API version {}", api->version);

	Plugin::options.xmxNetwork = api->GetBool(self, "XmxNetwork", Plugin::options.xmxNetwork);
	Plugin::options.hudlessUi = api->GetBool(self, "HudlessUi", Plugin::options.hudlessUi);

	return api->RegisterTab(self, "dlssg-to-xefg", "XeFG replacing DLSS Frame Generation", DrawTab, nullptr);
}
