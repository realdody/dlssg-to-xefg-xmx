#pragma once
// Ported from MFG-Unlocker source/native/overlay_native.h, MIT licensed, see NOTICE.md.
//
// The public COM unwrapping contracts of NVIDIA, ReShade and Special K. MFG-Unlocker also pins every
// vtable involved; that check is omitted here because the CUDA route validates the resolved device's
// vtable against D3D12Core.dll (RuntimeDevice) before accepting it. A wrapper that cannot be unwrapped
// still falls back to the fence's device in cuda_route.cpp.
#include <Windows.h>
#include <wrl/client.h>

#include <array>

namespace single_overlay::native
{
template<class T> bool Unwrap(IUnknown* input, Microsoft::WRL::ComPtr<T>& output) noexcept
{
    using Microsoft::WRL::ComPtr;
    constexpr GUID baseGuid = {0xadec44e2,0x61f0,0x45c3,{0xad,0x9f,0x1b,0x37,0x37,0x92,0x84,0xff}};
    // ReShade 6.8.0 source/com_utils.hpp: IID_UnwrappedObject.
    constexpr GUID reshadeGuid = {0x7f2c9a11,0x3b4e,0x4d6a,{0x81,0x2f,0x5e,0x9c,0xd3,0x7a,0x1b,0x42}};
    // Special K 26.9.25: IID_IUnwrappedDXGISwapChain.
    constexpr GUID specialKGuid = {0xe8a33b4a,0x1405,0x424c,{0xae,0x88,0x0d,0x3e,0x9d,0x46,0xc9,0x14}};
    output.Reset();
    ComPtr<IUnknown> current = input;
    std::array<ComPtr<IUnknown>, 4> visited;
    for (size_t i = 0; current && i < visited.size(); ++i)
    {
        if (FAILED(current.As(&visited[i]))) return false;
        for (size_t j = 0; j < i; ++j) if (visited[j].Get() == visited[i].Get()) return false;
        ComPtr<IUnknown> base;
        HRESULT hr = current->QueryInterface(baseGuid, reinterpret_cast<void**>(base.GetAddressOf()));
        for (const GUID& next : {reshadeGuid, specialKGuid})
        {
            if (hr != E_NOINTERFACE) break;
            if (base) return false;
            hr = current->QueryInterface(next, reinterpret_cast<void**>(base.GetAddressOf()));
        }
        if (hr == E_NOINTERFACE && base) return false;
        if (hr == E_NOINTERFACE) return SUCCEEDED(current.As(&output)) && output;
        if (hr != S_OK || !base) return false;
        current = base;
    }
    return false;
}
} // namespace single_overlay::native
