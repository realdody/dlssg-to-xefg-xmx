#pragma once

#include "NGX.h"
#include "XeFG.h"

#include <wrl/client.h>

#include <memory>

using Microsoft::WRL::ComPtr;

// One DLSSG feature instance, driving one XeFG context
class FrameGenerator
{
public:
	// 6x
	static constexpr uint32_t MaxFrames = 5;

	explicit FrameGenerator(ID3D12Device* device) : device(device) {}

	// Called once per generated frame, DLSSG.MultiFrameIndex 1 starts a new real frame
	NGXResult Evaluate(ID3D12GraphicsCommandList* cmdList, const NGXParameter& params);

	// Probes XeFG's own XMX network and the CUDA tensor-core route once, as soon as a device is available. Fills
	// Plugin::xmxSupport and Plugin::cudaSupport so the menu can disable paths that would fail before the first FG
	// context. Called from NGX's Init and, as a fallback, from CreateContext.
	static void ProbeSupport(ID3D12Device* device);

private:
	// What the context was created for, it's recreated when any of it changes
	struct Settings
	{
		uint32_t width = 0;
		uint32_t height = 0;
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
		uint32_t initFlags = 0;
		uint32_t uiMode = XEFGD_UI_MODE_AUTO;
		int32_t model = 0;

		bool operator==(const Settings&) const = default;
	};

	// Copies of a real frame's inputs, XeFG reads them after the game moved on
	struct Frame
	{
		ComPtr<ID3D12Resource> backbuffer;
		ComPtr<ID3D12Resource> hudless;
		ComPtr<ID3D12Resource> depth;
		ComPtr<ID3D12Resource> mvecs;
		xefgd_frame_t data{};
		bool valid = false;
	};

	struct Context
	{
		xefgd_handle_t handle = nullptr;
		ComPtr<ID3D12Resource> outputs[MaxFrames];
		xefgd_2d_t outputBases[MaxFrames]{};

		// Ping-ponged between the previous and the current real frame
		Frame frames[2];
		uint32_t current = 0;

		// The previous and the current real frame can be interpolated
		bool pairValid = false;

		~Context();
	};

	enum class InterpolateResult
	{
		Interpolated,
		Skipped, // no usable pair, the real frame is shown instead
		Failed,
	};

	bool CreateContext();
	void CaptureRealFrame(ID3D12GraphicsCommandList* cmdList, const NGXParameter& params);
	InterpolateResult Interpolate(ID3D12GraphicsCommandList* cmdList, D3D12_RESOURCE_DESC outputDesc,
		uint32_t frameIndex, uint32_t frameCount);

	ComPtr<ID3D12Device> device;
	Settings settings;
	std::shared_ptr<Context> context;
	bool createFailed = false; // retried when the settings change

	// The menu path last seen, and the route launches since the context or the path changed. A few generated frames
	// later they say whether the selected path really runs on tensor cores
	int32_t loggedPath = -1;
	uint64_t routeLaunches = 0;
	uint32_t routeFrames = 0;
	bool routeReported = false;
};
