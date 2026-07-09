# KP_LiquidGlass 1.0.0-beta — macOS

Liquid glass for After Effects. One effect turns any layer's alpha into
refractive glass in the style of Apple's Liquid Glass and Figma's glass
shader: an exact distance-field bevel drives slope-based refraction with
true edge inversion, chromatic dispersion, frosted roughness, a
bi-directional clear-coat highlight, tint, and interior zoom — no precomps,
no displacement-map rigs.

## Features

- Shape-accurate bevel from an exact (jump-flooding) distance field — even
  insets on any shape, including thin capsules and compound paths
- Physically-modeled displacement (magnitude and direction from the lens
  surface slope): edge inversion like the real thing, no seams at any
  thickness
- Chromatic dispersion concentrated in the refraction fold
- Clear-coat: thin hard specular stroke at the rim (width in px,
  resolution-independent control) over a soft directional base light,
  screened on top; angle, color, and intensity controls
- Frosted-glass roughness, tint, interior zoom, edge-only blur
- Background sampling that tracks layer transforms live — drag either layer
  and the glass updates correctly, with cross-frame caching intact
- Out-of-background edges fade to transparent by default; optional
  "extend edge pixels" mode

## Requirements — read before installing

- **macOS on Apple Silicon** (M1 or later). Intel Macs are not supported in
  this build.
- **After Effects 2025 or newer** (developed and tested on AE 2026).
- **GPU acceleration must be ON**: Project Settings → Video Rendering and
  Effects → *Mercury GPU Acceleration (Metal)*. This is not optional — see
  the first limitation below.

## Installation

Copy `KP_LiquidGlass.plugin` to:

```
~/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/
```

Because this beta is not notarized by Apple, macOS quarantines downloaded
copies and After Effects may refuse to load the plugin. After copying it,
clear the flag once:

```
xattr -cr ~/Library/Application\ Support/Adobe/Common/Plug-ins/7.0/MediaCore/KP_LiquidGlass.plugin
```

Restart After Effects (plugins load only at startup). The effect appears
under **Effects → KP Effects → KP_LiquidGlass**.

## Quick start

**Simple (one layer):** apply the effect to a shape layer and set
**Background Layer** to the layer you want refracted. The glass shape is the
layer's own alpha.

**Full composite (no precomping):** put the effect on an **adjustment
layer**, check **Sample underlying layers**, and set **Glass Shape** to a
shape layer that defines the glass (its video/eye switch may be off).

⚠ The Glass Shape layer must **not** have its own Adjustment Layer switch
enabled — After Effects hands adjustment-flagged layers to effects as a
blank white solid, and the glass will silently flatten.

A good starting look: Refraction 150–250 %, Softness 1, Thickness 30–60,
Spread 100 %.

## Known limitations (1.0.0-beta)

1. **GPU-only rendering — this is the important one.** When After Effects
   renders on the CPU (Mercury GPU Acceleration switched off, unsupported
   GPU, or some GPU-init failures), the effect passes the image through
   **unchanged, with no error message**. Your glass will simply be missing
   from previews and final renders. Always confirm Mercury GPU Acceleration
   (Metal) is active before rendering, and spot-check final output.
2. **Multi-Frame Rendering (MFR) is not supported by design** — the effect
   resolves layer transforms through APIs that are not render-thread-safe.
   AE shows a small warning icon on the effect for this reason; it is
   informational. Comps using this effect render single-threaded, so heavy
   comps may render slower than MFR-enabled ones.
3. **Apple Silicon only.** No Intel-Mac binary yet.
4. **No Windows version yet.** CUDA/DirectX kernels and host code exist in
   the source and a port is planned, but nothing has been compiled or
   tested on Windows. Do not expect project compatibility across platforms
   until that ships.
5. **"Extend edge pixels" applies only when an explicit Background Layer is
   set.** When sampling the underlying composite (adjustment-layer
   workflow), edges always fade — extending a composite's internal
   boundaries smears content and is deliberately disabled.
6. **Tested breadth is narrow**: one machine (Apple Silicon), AE 2026,
   comps up to 2.5K. Larger formats, deep color pipelines, and other AE
   versions have had no coverage yet — reports welcome.
7. Very large Thickness/Softness values on high-resolution comps cost
   render time (the gaussian passes scale with radius).

**Compatibility promise:** from this release onward the parameter layout is
frozen — future versions will only append new parameters, so projects saved
with 1.0.0-beta will load correctly in updates. (Projects saved with
pre-release development builds are not compatible; re-apply the effect.)

## License

Copyright (c) 2026 Viktor Kopeikin. Licensed under the **PolyForm Noncommercial
License 1.0.0** (see LICENSE): you may use, copy, modify, and share this
software **for noncommercial purposes**, with attribution. Any commercial
use requires a separate license from the author — get in touch.

## Feedback

This is a beta. If the glass misbehaves, include: macOS and AE versions,
GPU model, comp resolution, whether Mercury GPU (Metal) was active, and a
screenshot of the effect settings.
