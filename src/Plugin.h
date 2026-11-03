// The dlssg-to-xefg tab of OptiScaler's menu. Its settings are kept in [Plugin.<name>] of OptiScaler.ini when this dll
// is in OptiScaler's custom\<name>\dlssg, without OptiScaler the defaults are used
#pragma once

#include <atomic>
#include <cstdint>

namespace Plugin
{
	// The selected implementation of the MFG model
	enum class Path : int32_t
	{
		XmxPort,
		Cuda,
		Fallback, // DP4a model 18
	};

	struct Options
	{
		// The ported XMX model on DXIL, the same model on CUDA tensor cores, or XeFG's DP4a model
		std::atomic<Path> path = Path::Cuda;
		// Lets XeFG composite the UI from the game's HUDless color instead of interpolating it with the scene
		std::atomic_bool hudlessUi = true;
		// Not saved
		std::atomic_bool debugLines = false;
	};

	inline Options options;

	enum class CudaSupport : int32_t
	{
		Unknown, // XeFG hasn't looked at a GPU yet
		Supported,
		Unsupported,
	};

	inline std::atomic<CudaSupport> cudaSupport = CudaSupport::Unknown;

	// Which implementation of the MFG model is running
	enum class Network : int32_t
	{
		Unknown,
		XmxPort,
		Cuda,
		Fallback, // DP4a model 18, the ported model isn't available
	};

	// One real frame of the frame generator that ran last
	struct Status
	{
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t generatedFrames = 0; // between two real frames
		bool hudless = false;
		Network network = Network::Unknown;
		bool failed = false; // no XeFG context
		bool fp16 = false;   // 16-bit float (scRGB) back buffer, XeFG only takes 8-bit and HDR10
	};

	void Report(const Status& status);
} // namespace Plugin
