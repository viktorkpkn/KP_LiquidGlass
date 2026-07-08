# KP_LiquidGlass

Copyright (c) 2026 Viktor Kopeikin. Licensed under the PolyForm Noncommercial
License 1.0.0 — see [LICENSE](LICENSE) and [RELEASE_NOTES.md](RELEASE_NOTES.md).

An After Effects GPU effect (SmartFX) that renders Figma/Apple-style "liquid
glass" from any layer's alpha: an exact-distance bevel field drives
slope-based refraction with edge inversion, chromatic dispersion, roughness
frost, a bi-directional clear-coat light, tint, and zoom. Metal render path
(macOS, verified); CUDA/DirectX kernels and host code are written but not yet
compiled on Windows (see `KP_LiquidGlass/WINDOWS_PORT.md`). The CPU render is
currently a passthrough.

## Repository layout

```
KP_LiquidGlass/          the plugin source (this is the only folder you edit)
  KP_LiquidGlass.h       params, structs, embedded Metal kernels
  KP_LiquidGlass.cpp     host code: params, PreRender/AEGP transforms, GPU dispatch
  KP_LiquidGlass_Kernel.cu     cross-API kernels (CUDA/OpenCL/HLSL) + CUDA launchers
  KP_LiquidGlass_Kernel.cl     include shim (OpenCL string generation)
  KP_LiquidGlass_Kernel.chlsl  include shim (HLSL/DXIL generation)
  KP_LiquidGlassPiPL.r   plugin property list
  Mac/                   Xcode project
  install.sh             macOS: build + install to the user MediaCore folder
  WINDOWS_PORT.md        Windows/CUDA/DirectX porting status, steps, risks
```

## Building (macOS)

Requirements: Xcode, and the **Adobe After Effects SDK 25.6** (June 2025).
The SDK itself is NOT included here — it is Adobe-licensed and must not be
redistributed in this repo.

1. Download the AE SDK 25.6 from Adobe's developer site.
2. Place this `KP_LiquidGlass` folder at
   `<SDK>/Examples/Effect/KP_LiquidGlass` — the Xcode project references SDK
   headers (`Examples/Headers`, `Examples/Util`, `Examples/GPUUtils`) by
   relative path and will not build from anywhere else.
3. Run `./install.sh` (builds Debug arm64 and copies the plugin to
   `~/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/`).
4. Restart After Effects (plugins load only at startup). AE must NOT be
   running while the plugin file is replaced — a mid-session swap produces
   a half-loaded state with silently broken parameters.

Notes:
- CLI builds must be single-arch (`install.sh` passes `ARCHS=arm64`): the
  SDK template's per-arch custom build rules collide on shared output paths.
- AE does not load symlinked plugin bundles; always copy.

## Using the effect

- Simple: apply to a shape layer, set **Background Layer** to the layer to
  refract. The glass shape = the layer's own alpha.
- Full composite (no precomp): put the effect on an **adjustment layer**,
  check **Sample underlying layers**, and set **Glass Shape** to a shape
  layer (its video switch may be off). Important: the Glass Shape layer must
  NOT have the Adjustment Layer switch enabled — AE hands such layers to
  effects as a white solid, which flattens the glass (a warning is logged
  when diagnostics are enabled).
- Reference look: Refraction 100–250 %, Softness 1, Thickness 30–60,
  Spread 100 %.

## Development notes

- The Metal kernels (in `KP_LiquidGlass.h`) and the cross-API kernels
  (`KP_LiquidGlass_Kernel.cu`) are deliberate duplicates — mirror any kernel
  change in both (this matches Adobe's own SDK sample structure).
- Dev diagnostics (a Debug View parameter and `/tmp/KP_LiquidGlass.log`
  logging) are compiled out; re-enable via `KP_LIQUIDGLASS_ENABLE_LOG` in
  `KP_LiquidGlass.cpp`.
- Changing the parameter list breaks saved instances: delete + re-apply the
  effect in test projects afterwards (converted ⚠ instances silently drop
  edits).
- MFR (multi-frame rendering) is intentionally unsupported: PreRender
  resolves layer transforms through AEGP suites, which are not
  render-thread-safe. The ⚠ icon on the effect in AE is this notice.
