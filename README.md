# KP_LiquidGlass
![KP_LiquidGlass preview](Sample/KP_LiquidGlass_PreviewA-animated.webp)

## Overview
This is an After Effects effect that renders Apple-style "Liquid
Glass" from any layer's alpha, including text layers, but primarily Shape Layers.
GPU accelerated, Metal on macOS, CUDA on PC. The CPU render is
currently a passthrough on both platforms.

## Using the effect

**Option 1.**
1. Apply to a shape layer.
2. Set **Background Layer** to the layer to
   refract. That's it.

**Option 2.** (better)
1. Apply to an adjustment layer.
2. Pick a shape layer in "Glass Shape"
3. Toggle **Sample underlying layers** on.
4. Change the sourced shape layer's opacity to 0% to be able to freely move it around.

## Sample project

`Sample/LiquidGlass_TestProject.aep` is an example AE project
demonstrating the difference between both hosting workflows — open it
after installing the plugin to see a configured starting point and read my notes.



---

## Repository layout

```
KP_LiquidGlass/          the plugin source (this is the only folder you edit)
  KP_LiquidGlass.h       params, structs, embedded Metal kernels
  KP_LiquidGlass.cpp     host code: params, PreRender/AEGP transforms, GPU dispatch
  KP_LiquidGlass_Kernel.cu     cross-API kernels (CUDA/OpenCL/HLSL) + CUDA launchers
  KP_LiquidGlass_Kernel.cl     include shim (OpenCL string generation)
  KP_LiquidGlass_Kernel.chlsl  include shim (HLSL/DXIL generation)
  KP_LiquidGlassPiPL.r   plugin property list
  Mac/                   Xcode project (macOS build)
  Win/                   Visual Studio project (Windows build)
  install.sh             macOS: build + install to the user MediaCore folder
  WINDOWS_PORT.md        Windows/CUDA/DirectX porting status, steps, risks
Sample/                  sample After Effects project + preview media
```

## Building (macOS)

Requirements: Xcode, and the **Adobe After Effects SDK 25.6** (June 2025).
The SDK itself is NOT included.

1. Download the AE SDK 25.6 from [Adobe](https://developer.adobe.com/after-effects/).
2. Place this `KP_LiquidGlass` folder at
   `<SDK>/Examples/Effect/KP_LiquidGlass` — the Xcode project references SDK
   headers (`Examples/Headers`, `Examples/Util`, `Examples/GPUUtils`) by
   relative path and will not build from anywhere else.
3. Run `./install.sh` (builds Debug arm64 and copies the plugin to
   `~/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/`), or build in Xcode and replace manually.
4. Restart After Effects, because plugins load only at startup.

Notes:
- CLI builds must be single-arch (`install.sh` passes `ARCHS=arm64`): the
  SDK template's per-arch custom build rules collide on shared output paths.
- AE does not load symlinked plugin bundles; always copy.

## Building (Windows)

Requirements: Visual Studio 2022, the Adobe After Effects SDK 25.6, CUDA
Toolkit (for the CUDA render path), DXC (for the DirectX shader path). See
`KP_LiquidGlass/WINDOWS_PORT.md` for exact toolchain versions, environment
variables, and current verification status — the CUDA path is built and
verified in AE; the DirectX-only path is unverified.

1. Place this `KP_LiquidGlass` folder at
   `<SDK>\Examples\Effect\KP_LiquidGlass`.
2. Open `KP_LiquidGlass/Win/KP_LiquidGlass.sln` in Visual Studio and build
   (`/p:KPEnableCuda=1` for the CUDA path, `0` to build DirectX-only).
3. Copy the built `.aex` and its `DirectX_Assets/` folder to
   `C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\`.
4. Restart After Effects.


## Development notes

- Dev diagnostics (a Debug View parameter and `/tmp/KP_LiquidGlass.log`
  logging) are compiled out; re-enable via `KP_LIQUIDGLASS_ENABLE_LOG` in
  `KP_LiquidGlass.cpp`.
- Changing the parameter list breaks saved instances: delete + re-apply the
  effect in test projects afterwards (converted ⚠ instances silently drop
  edits).
- MFR (multi-frame rendering) is intentionally unsupported: PreRender
  resolves layer transforms through AEGP suites, which are not
  render-thread-safe. The ⚠ icon on the effect in AE is this notice.


Copyright (c) 2026 Viktor Kopeikin. Licensed under the **MIT License** — see
[LICENSE](LICENSE) and [RELEASE_NOTES.md](RELEASE_NOTES.md).
