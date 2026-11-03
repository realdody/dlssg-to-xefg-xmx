// The dlssg-to-xefg tab of OptiScaler's menu. Its settings are kept in [Plugin.<name>] of OptiScaler.ini when this dll
// is in OptiScaler's custom\<name>\dlssg, without OptiScaler the defaults are used
#pragma once

#include <atomic>
#include <cstdint>

namespace Plugin
{
	struct Options
	{
		// Only matters where XeFG would pick the XMX network itself
		std::atomic_bool xmxNetwork = true;
		// Lets XeFG composite the UI from the game's HUDless color instead of interpolating it with the scene
		std::atomic_bool hudlessUi = true;
		// Not saved
		std::atomic_bool debugLines = false;
	};

	inline Options options;

	enum class Xmx : int32_t
	{
		Unknown, // XeFG hasn't looked at a GPU yet
		Supported,
		Unsupported,
	};

	inline std::atomic<Xmx> xmx = Xmx::Unknown;

	// One real frame of the frame generator that ran last
	struct Status
	{
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t generatedFrames = 0; // between two real frames
		bool hudless = false;
		bool xmxNetwork = false;
		bool failed = false; // no XeFG context
	};

	void Report(const Status& status);
} // namespace Plugin
