# KP_LiquidGlass — Windows (CUDA / DirectX) port handoff

State as of 2026-07-08: all cross-platform code is written and the macOS
build still compiles and renders identically (Metal untouched). Nothing on
this list has been compiled on Windows yet — that is this session's job.

## What is already done

- `KP_LiquidGlass_Kernel.cu` — all 10 kernels in the SDK's cross-API macro
  DSL (`GF_KERNEL_FUNCTION`), written component-wise so CUDA (no vector
  operators), OpenCL-C and HLSL all accept them. CUDA launcher functions are
  at the bottom (`KP*_CUDA`), statically compiled by nvcc.
- `KP_LiquidGlass_Kernel.cl` / `.chlsl` — thin `#include` shims (same
  pattern as SDK_Invert_ProcAmp).
- Host code: `GPUDeviceSetup` / `GPUDeviceSetdown` / `SmartRenderGPU` branch
  on `what_gpu` for CUDA / DIRECTX / METAL. The CUDA and DirectX render
  sequences live in `SmartRenderGPU_CUDA` / `SmartRenderGPU_DirectX`
  (KP_LiquidGlass.cpp), modeled on the SDK sample's host code.
- Scratch buffers for CUDA/DX come from `CreateGPUWorld` used as linear
  device buffers (a BGRA128 world holds any of the scalar/float2 sets).
- `KP_ENABLE_CUDA=0` (preprocessor) builds without the CUDA toolkit; NVIDIA
  cards are then served via DirectX like AMD/Intel.

## Setting up the Windows project

There is no vcxproj yet — adapt the sample's rather than starting blank:

1. Copy `Examples/Effect/SDK_Invert_ProcAmp/Win/` to
   `Examples/Effect/KP_LiquidGlass/Win/`, rename the .sln/.vcxproj, and
   fix all `SDK_Invert_ProcAmp` file references to `KP_LiquidGlass`
   (sources: `KP_LiquidGlass.cpp`, `..\..\..\Util\DirectXUtils.cpp`,
   `..\..\..\Util\Smart_Utils.cpp`; PiPL: `KP_LiquidGlassPiPL.r`;
   kernels: the three `KP_LiquidGlass_Kernel.*` files).
2. Required environment/property variables (same as the sample):
   `CUDA_SDK_BASE_PATH`, `DXC_SDK_BASE_PATH`, `BOOST_BASE_PATH`.
3. The `.chlsl` custom-build step must extract and compile **each of our 10
   kernels**. Replace the sample's two ParseHLSL+dxc line pairs with ten,
   one pair per name:

   KPPrepareMatteKernel, KPJFASeedKernel, KPJFAStepKernel,
   KPJFAResolveKernel, KPBlurHorizontalKernel, KPBlurVerticalKernel,
   KPBuildBevelHeightKernel, KPBlurBackgroundHorizontalKernel,
   KPBlurBackgroundVerticalKernel, KPLiquidGlassKernel

   Pattern per kernel (adjust `<Name>`):
   ```
   python "..\..\..\GPUUtils\ParseHLSL.py" -i "$(ProjectDir)%(Filename).i" -o "$(IntDir)." -e <Name>
   "$(DXC_SDK_BASE_PATH)\bin\x64\dxc.exe" "$(IntDir)<Name>.hlsl" -E main -Fo "$(IntDir)DirectX_Assets\<Name>.cso" -Frs "$(IntDir)DirectX_Assets\<Name>.rs" -T cs_6_5 -enable-16bit-types -ignore-line-directives
   ```
   Also extend the `<Outputs>` lists accordingly, and make sure the
   `DirectX_Assets` folder (all 10 .cso + .rs) is deployed next to the
   built `.aex` — `GetShaderPath` resolves `DirectX_Assets/<name>.cso|.rs`
   relative to the module.
4. The `.cl` custom step (OpenCL string) can be kept as-is; the generated
   header name would be `KP_LiquidGlass_Kernel.cl.h` with
   `kKP_LiquidGlass_Kernel_OpenCLString`. The host code has **no OpenCL
   branch** (deliberately: AE-Windows uses DirectX/CUDA), so this step can
   also simply be removed from the project.
5. No CUDA toolkit? Remove the `.cu` custom step, don't link its .obj, and
   define `KP_ENABLE_CUDA=0` in the C++ preprocessor definitions.

## Verification order on Windows

1. Build with CUDA disabled first (`KP_ENABLE_CUDA=0`) — exercises only the
   DirectX path, which covers AMD and NVIDIA.
2. In AE: Project Settings > Video Rendering must be on Mercury GPU
   (DirectX). Apply the effect to a shape layer with a Background Layer —
   same smoke tests as the Mac build: matte shape, rim displacement, light
   stroke, zoom A/B.
3. Then enable CUDA and re-test with the renderer on CUDA.

## Known risks (untestable from macOS — check these first if it misbehaves)

- **GF DSL argument counts**: the composite kernel has ~37 arguments; the
  sample only exercises 9. If the BOOST_PP-based macros choke, split the
  argument list or move values into fewer packed args.
- **GF_PTR parameters on device functions under HLSL**: KPBackgroundTap /
  KPSampleBilinear / KPSampleDisplaced take buffer pointers; HLSL SM6
  accepts StructuredBuffer function params, but if ParseHLSL's generation
  disagrees, inline those helpers into the composite kernel.
- **Constant-buffer packing**: the `KP*ParamsDX` structs are tightly packed
  and field order matches kernel argument order exactly (this mirrors how
  the sample's param structs work). If DX renders garbage, dump the
  generated `<Name>.hlsl` in `$(IntDir)` and compare its cbuffer layout
  against the struct.
- **float2 buffers (JFA)**: seeds are float2 arrays; verify the generated
  HLSL uses a StructuredBuffer<float2> and that ParseHLSL handles it.
- **Edge blur offsets**: the portable kernel applies the 5-tap edge blur in
  sample space, Metal applies it pre-matrix; visually identical for
  near-uniform transforms. Cosmetic-only divergence.
- **DX pass ordering**: each `DXShaderExecution::Execute` syncs (matching
  the sample), so the 15+ dispatches are correct but not pipelined — slow
  first, optimize later.
- After the Metal kernels change in KP_LiquidGlass.h, mirror the change in
  KP_LiquidGlass_Kernel.cu — they are duplicate implementations by design
  (same as Adobe's sample).

## CPU fallback reminder

The CPU render is still a passthrough. On Windows this matters more: any
machine where DX12 init fails will silently show no effect. A real CPU
implementation is the highest-value robustness item after the port works.
