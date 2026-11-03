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

	const char* NetworkName(Plugin::Network network)
	{
		switch (network)
		{
		case Plugin::Network::XmxPort:
			return "XMX Port";
		case Plugin::Network::Cuda:
			return "CUDA";
		case Plugin::Network::Fallback:
			return "Fallback (DP4a)";
		default:
			return "Unknown";
		}
	}

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
		else if (active && current.failed && current.fp16)
		{
			Red("16-bit HDR isn't supported");
			api->Text("XeFG only supports SDR and HDR10, switch the game to one of them");
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
			line += std::format(" \xC2\xB7 {}", NetworkName(current.network));

		api->Text(line.c_str());
		api->ItemTooltip(std::format("{}\n{}", library.path.string(),
			library.knownBuild ? "Known build" : "Functions found by scanning").c_str());
	}

	void DrawSettings()
	{
		api->Section("Settings");

		static constexpr const char* PathNames[] = { "XMX Port", "CUDA", "Fallback (DP4a)" };
		const auto support = Plugin::cudaSupport.load();
		const bool cudaDisabled = support == Plugin::CudaSupport::Unsupported;

		int32_t path = (int32_t)Plugin::options.path.load();

		// A saved CUDA choice on an unsupported GPU runs the XMX port, show what is actually running
		if (path == (int32_t)Plugin::Path::Cuda && cudaDisabled)
			path = (int32_t)Plugin::Path::XmxPort;

		const bool disabled[] = { false, cudaDisabled, false };

		if (api->Combo("Network path", &path, PathNames, 3, disabled))
		{
			Plugin::options.path = (Plugin::Path)path;
			api->SetInt(self, "NetworkPath", path);
		}

		api->ItemTooltip(cudaDisabled
			? "CUDA needs an NVIDIA RTX GPU with tensor cores.\n"
			  "XMX Port runs the ported model on DXIL, Fallback uses XeFG's DP4a model."
			: "XMX Port: ported XMX model on DXIL\n"
			  "CUDA: ported XMX model on NVIDIA tensor cores\n"
			  "Fallback (DP4a): XeFG's default model");

		bool hudlessUi = Plugin::options.hudlessUi;

		if (api->Checkbox("HUDless UI", &hudlessUi))
		{
			Plugin::options.hudlessUi = hudlessUi;
			api->SetBool(self, "HudlessUi", hudlessUi);
		}

		api->ItemTooltip("Uses the game's HUDless image to draw the UI into generated frames, instead of\n"
						 "interpolating the UI with the scene. It doesn't highlight the HUD.\n"
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

	const int32_t path = api->GetInt(self, "NetworkPath", (int32_t)Plugin::options.path.load());
	if (path >= (int32_t)Plugin::Path::XmxPort && path <= (int32_t)Plugin::Path::Fallback)
		Plugin::options.path = (Plugin::Path)path;

	Plugin::options.hudlessUi = api->GetBool(self, "HudlessUi", Plugin::options.hudlessUi);

	return api->RegisterTab(self, "XeMFG XMX", "XeFG replacing DLSS Frame Generation", DrawTab, nullptr);
}
