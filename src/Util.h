#pragma once

#include <Windows.h>
#include <d3d12.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <mutex>
#include <vector>

// Keeps objects a recorded command list may still use alive for a while, Collect is called once per evaluation.
// ponytail: counts evaluations instead of waiting on a fence, enough for the usual 2-3 frames in flight at 6x
namespace DeferredDestroy
{
	constexpr uint64_t Delay = 32;

	struct Entry
	{
		uint64_t destroyAt;
		std::shared_ptr<void> object;
	};

	inline std::mutex mutex;
	// Leaked on purpose: destroying XeFG contexts from our static destructors at process exit would call into a
	// libxess_fg.dll that may already be torn down
	inline std::vector<Entry>& entries = *new std::vector<Entry>;
	inline uint64_t evaluations = 0;

	inline void Add(std::shared_ptr<void> object)
	{
		std::scoped_lock lock(mutex);
		entries.push_back({ evaluations + Delay, std::move(object) });
	}

	inline void Collect(bool all = false)
	{
		std::vector<Entry> due;
		{
			std::scoped_lock lock(mutex);
			evaluations++;
			auto split = std::partition(entries.begin(), entries.end(),
				[all](const Entry& e) { return !all && e.destroyAt > evaluations; });
			due.assign(std::make_move_iterator(split), std::make_move_iterator(entries.end()));
			entries.erase(split, entries.end());
		}
		// Destroyed here, outside the lock
	}
} // namespace DeferredDestroy

inline std::filesystem::path ModulePath(HMODULE module)
{
	wchar_t path[MAX_PATH]{};
	GetModuleFileNameW(module, path, MAX_PATH);
	return path;
}

// Full path of this dll
inline std::filesystem::path ThisModulePath()
{
	HMODULE module = nullptr;
	GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<LPCWSTR>(&ThisModulePath), &module);

	return ModulePath(module);
}

inline void Barrier(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* resource, D3D12_RESOURCE_STATES before,
	D3D12_RESOURCE_STATES after)
{
	if (before == after)
		return;

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = resource;
	barrier.Transition.StateBefore = before;
	barrier.Transition.StateAfter = after;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	cmdList->ResourceBarrier(1, &barrier);
}

// Both resources are back in their states afterwards
inline void Copy(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* destination, D3D12_RESOURCE_STATES destinationState,
	ID3D12Resource* source, D3D12_RESOURCE_STATES sourceState)
{
	Barrier(cmdList, destination, destinationState, D3D12_RESOURCE_STATE_COPY_DEST);
	Barrier(cmdList, source, sourceState, D3D12_RESOURCE_STATE_COPY_SOURCE);
	cmdList->CopyResource(destination, source);
	Barrier(cmdList, destination, D3D12_RESOURCE_STATE_COPY_DEST, destinationState);
	Barrier(cmdList, source, D3D12_RESOURCE_STATE_COPY_SOURCE, sourceState);
}

// Typed formats for views on typeless or depth resources. 8-bit sRGB maps to UNORM, typed UAVs can't be sRGB
inline DXGI_FORMAT TypedFormat(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		return DXGI_FORMAT_B8G8R8A8_UNORM;
	case DXGI_FORMAT_R10G10B10A2_TYPELESS:
		return DXGI_FORMAT_R10G10B10A2_UNORM;
	case DXGI_FORMAT_R16G16B16A16_TYPELESS:
		return DXGI_FORMAT_R16G16B16A16_FLOAT;
	case DXGI_FORMAT_R16G16_TYPELESS:
		return DXGI_FORMAT_R16G16_FLOAT;
	case DXGI_FORMAT_R32G32_TYPELESS:
		return DXGI_FORMAT_R32G32_FLOAT;
	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;
	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_D16_UNORM:
		return DXGI_FORMAT_R16_UNORM;
	default:
		return format;
	}
}
