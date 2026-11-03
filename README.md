# dlssg-to-xefg-xmx

Replaces DLSS Frame Generation with Intel XeSS Frame Generation (XeFG), up to 6x. Inspired by Nukem9's
[dlssg-to-fsr3](https://github.com/Nukem9/dlssg-to-fsr3).

`libxess_fg.dll` only exports XeFG's proxy swap chain. Internally that swap chain drives an unexported
`xefgD3D12CreateContext / Init / Execute` API, which this dll locates and calls directly with the DLSSG inputs. That's
what makes it work as an `nvngx_dlssg` replacement, and it lifts XeFG's 4x cap and its non-Intel 2x clamp without
patching any code.

DX12 only, XeFG has no Vulkan API.

## Usage

`dlssg_to_xefg_intel_is_better.dll` exports the same `NVSDK_NGX_D3D12_*` functions as `nvngx_dlssg.dll`, for a loader
that calls those directly. Put `libxess_fg.dll` (XeFG 1.3.1 or 1.3.4) next to it, unless the game loads a compatible one
itself.

## Installation

1. Download dlssg-to-xefg-#.zip from the [latest release](https://github.com/realdody/dlssg-to-xefg/releases/latest).
2. Make sure you're using OptiScaler v10 https://github.com/optiscaler/OptiScaler-nightly/releases/latest.
3. Extract the XeMFG XMX folder from the zip into OptiScaler\custom. The folder structure should be: gamefolderwithoptiscaler'sdll\OptiScaler\custom\XeMFG XMX
4. Launch the game and open OptiScaler with insert. 
5. The recommended FG input is DLSSG via NVNGX, as it lets you use XeMFG XMX as an output directly.
   If DLSSG via NVNGX is not available or doesn't work, you can use any other input by setting the FG Output to DLSSG, and the FG replacement to XeMFG XMX. Then press Save Settings in the bottom right to save

7. Make sure "Current DLSSG state" says "ON" with the fake frame count you selected. Then check \OptiScaler\custom\XeMFG XMX\dlssg\dlssg_to_xefg_intel_is_better.log
   It should say "Ported XMX MFG model installed, one optical flow field per generated batch" and "Creating XeFG context: 2560x1440, format: 28, flags: 1, ui mode: 4, model: 16"

Model 16 means it's running the XMX path.

## Which libxess_fg.dll is used

1. Every `libxess_fg.dll` already loaded in the process, the first compatible one wins
2. The `libxess_fg.dll` next to this dll
3. A `libxess_fg.dll` up to three folders above this dll, which covers OptiScaler's own copy in its root
   (`\OptiScaler\libxess_fg.dll`)

Compatible means a build with known function addresses (XeFG 1.3.1 from XeSS SDK 3.0.2, XeFG 1.3.4), or any XeFG
1.3.x where the functions can be found by scanning and the settings match one of the known layouts. The struct layouts
were only verified for 1.3.1 and 1.3.4.

`dlssg_to_xefg_intel_is_better.log` next to the dll says which one was picked.

## Build

Visual Studio 2026 (v145 toolset), open `dlssg-to-xefg.sln` and build Release x64. The output lands in `bin\Release`.

The ported XeSS MFG programs are not committed, and no MFG-Unlocker checkout or submodule is needed: the build
downloads its pinned v1.7.0 release dll, extracts the program set and the validated tensor-core fatbin into
`bin\Release\xefg_mfg_programs`, which has to ship next to the dll. The download and its contents are checked against
pinned SHA-256 values and cached under `obj\Release\packages`. The generated folder also carries MFG-Unlocker's MIT
LICENSE. For an offline build, pass `/p:MfgUnlockerDll=<path to MFGUnlocker.dll>`. No local CUDA toolkit is needed:
`xefg_cuda_layers.fatbin` is the pinned RCDATA 7300 resource (`2ffae52e…`) from that dll.

The tensor-core route is optional at runtime. The fatbin has SASS for RTX 20/30/40/50 and is loaded next to the program
set; no CUDA runtime is linked or shipped. On an RTX GPU the log shows `cuda_route active`, then `cuda_layer` lines,
with `cuda_dispatch_fallback` for any layer it had to leave on DXIL. The native layer hashes target XeFG 3.0.2; 1.3.4
may keep those layers on DXIL until its hashes are added. Set `XEFG_TENSOR_CORES=0` to force the DXIL path without
deleting the fatbin. `CudaRoot` is only for rebuilding the kernels from source with CUDA 13.2 during development; it
overwrites the extracted fatbin in the build output.

### Trying the tensor-core path

1. Build Release normally; the pinned fatbin is extracted automatically. Only set `CudaRoot` if you changed the kernels.
2. Keep `libxess_fg.dll` 1.3.1 (XeSS SDK 3.0.2) next to the dll; the native U-Net hashes in the route target that build.
3. Launch, then check the log for `cuda_route active`, `"compute_capability":"12.0"` on an RTX 50 card, and
   `cuda_layer` lines. `cuda_dispatch_fallback` means that layer stayed on DXIL by design or because its bindings
   were incomplete.
4. If the log says `Tensor-core route inactive: <reason>`, send that line and the surrounding `cuda_route` lines; the
   reason distinguishes a missing fatbin, a non-NVIDIA/unsupported adapter, a wrapped device, and NVAPI failure.

## Credits

The port in `src/MfgPort/` and `src/MfgKernels.cpp` is from [MFG-Unlocker](https://github.com/dashdogy/MFG-Unlocker),
MIT licensed, Copyright (c) 2026 Michael Robles. The CUDA route and kernels in `src/MfgPort/Cuda/` are also from
MFG-Unlocker. See [NOTICE.md](NOTICE.md).

## License

GPLv3, see [LICENSE](LICENSE).
