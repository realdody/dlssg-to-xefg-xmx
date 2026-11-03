# Third-party notices

## MFG-Unlocker

`src/MfgPort/` and the model port in `src/MfgKernels.cpp` are ported from
[MFG-Unlocker](https://github.com/dashdogy/MFG-Unlocker), specifically `source/native/xess/runtime_layout.h`,
`runtime_layout.generated.h`, `runtime_payload.*`, `port/model_port.h`, `port/shader_port.h`, the tensor-core
route and kernels in `xess/cuda/`, and the NVAPI ownership helpers in `adapter_discovery.h` and `overlay_native.h`.

MIT License, Copyright (c) 2026 Michael Robles. The build writes a copy next to the generated program set. The full text
is reproduced here:

```
MIT License

Copyright (c) 2026 Michael Robles

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```

## Intel XeSS Frame Generation program data

This project does not ship Intel's XeSS SDK binaries, model weights or tensors, and it does not commit the ported
program set either. The build downloads MFG-Unlocker's pinned v1.7.0 release dll, extracts `xefg_mfg_programs\programs.tsv`
and the DXIL programs it lists, and verifies them against MFG-Unlocker's pinned manifest SHA-256. The dll reads that set
from next to itself at runtime. The release dll itself is not redistributed by this project.

That program data is derived from programs in Intel's XeSS SDK and is not covered by MFG-Unlocker's MIT license; it
remains subject to Intel's license terms for the XeSS SDK. MFG-Unlocker's own description of it is in
`source/native/xess/XEFG-PROGRAMS-NOTICE.txt`.

## NVIDIA and the tensor-core kernels

`src/MfgPort/Cuda/` contains MFG-Unlocker's XeFG tensor-core route and kernels, covered by the MIT text above. The
tensor-core fatbin is extracted at build time from MFG-Unlocker's pinned release DLL (RCDATA 7300, SHA-256
`2ffae52e…`), where it was built with CUDA Toolkit 13.2. The optional `CudaRoot` source build compiles
`xefg_cuda_layers.cu` with CUDA 13.2 instead. The dll does not link or ship the CUDA runtime. The route calls NVIDIA's
D3D12 NvAPI extensions in the installed driver at runtime. The NVIDIA driver and CUDA Toolkit are subject to NVIDIA's
terms; this project does not redistribute the CUDA Toolkit.

Intel and XeSS are trademarks of Intel Corporation or its subsidiaries.
