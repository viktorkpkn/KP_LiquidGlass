# KP_LiquidGlass 1.1.0-beta — macOS + Windows

Liquid glass for After Effects. One effect turns any layer's alpha into
refractive glass in the style of Apple's Liquid Glass and Figma's glass
shader: an exact distance-field bevel drives slope-based refraction with
true edge inversion, chromatic dispersion, frosted roughness, a
bi-directional clear-coat highlight, tint, interior zoom, and inner/outer
shadows with caustics through the glass — no precomps, no displacement-map
rigs.

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
- Inner shadow (baked drop shadow) plus outer shadow and caustics through
  the refracted glass, each with independent color/intensity/softness and
  6 blend modes; optional Composite on Top for real compositing against the
  underlying image in the adjustment-layer workflow, or Confine to Layer
  Bounds to keep the effect inside the shape when it isn't
- Background sampling that tracks layer transforms live — drag either layer
  and the glass updates correctly, with cross-frame caching intact
- Out-of-background edges fade to transparent by default; optional
  "extend edge pixels" mode
- 26-tap edge blur (no ghosting at large Edge Blur values) and sub-pixel
  jump-flooding seeding (no jitter under animation)

## Requirements — read before installing

- **macOS on Apple Silicon** (M1 or later), GPU acceleration ON (Project
  Settings → Video Rendering and Effects → *Mercury GPU Acceleration
  (Metal)*), **or Windows with an NVIDIA/CUDA-capable GPU** — see the
  platform limitations below.
- **After Effects 2025 or newer** (developed and tested on AE 2026).
- GPU acceleration is not optional on either platform — see the first
  limitation below.

## Installation

**macOS:** copy `KP_LiquidGlass.plugin` to
`~/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/`.
Because this beta is not notarized by Apple, macOS quarantines downloaded
copies and After Effects may refuse to load the plugin. After copying it,
clear the flag once:

```
xattr -cr ~/Library/Application\ Support/Adobe/Common/Plug-ins/7.0/MediaCore/KP_LiquidGlass.plugin
```

**Windows:** copy the `.aex` and its `DirectX_Assets/` folder to
`C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\`.

Restart After Effects on either platform (plugins load only at startup).
The effect appears under **Effects → KP Effects → KP_LiquidGlass**.

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

A sample project (`Sample/LiquidGlass_TestProject.aep`) is included with a
configured starting point for both workflows.

## Known limitations (1.1.0-beta)

1. **GPU-only rendering — this is the important one.** When After Effects
   renders on the CPU (Mercury GPU Acceleration switched off, unsupported
   GPU, or some GPU-init failures), the effect passes the image through
   **unchanged, with no error message**. Your glass will simply be missing
   from previews and final renders. Always confirm GPU acceleration is
   active before rendering, and spot-check final output.
2. **Multi-Frame Rendering (MFR) is not supported by design** — the effect
   resolves layer transforms through APIs that are not render-thread-safe.
   AE shows a small warning icon on the effect for this reason; it is
   informational. Comps using this effect render single-threaded, so heavy
   comps may render slower than MFR-enabled ones.
3. **Apple Silicon only on macOS.** No Intel-Mac binary yet.
4. **Windows requires an NVIDIA/CUDA-capable GPU this beta.** The CUDA
   render path is built and verified in AE. The DirectX-only path (AMD,
   Intel, or non-CUDA NVIDIA GPUs) is compiled but has not been verified
   rendering in real AE — do not expect Mercury GPU acceleration to engage
   without a CUDA-capable GPU.
5. **Outer Shadow and Caustics blend modes only apply correctly against
   real pixels in the adjustment-layer + Composite on Top workflow.** On a
   plain shape layer the effect has no access to what's underneath outside
   its own silhouette, so it falls back to a translucent painted
   approximation that ignores the selected blend mode and can show as a
   soft offset copy of the shape. Enable **Confine to Layer Bounds** to
   suppress that fallback and keep the effect inside the shape instead.
6. **"Extend edge pixels" applies only when an explicit Background Layer is
   set.** When sampling the underlying composite (adjustment-layer
   workflow), edges always fade — extending a composite's internal
   boundaries smears content and is deliberately disabled.
7. **Tested breadth is narrow**: a handful of machines, AE 2026, comps up
   to 2.5K. Larger formats, deep color pipelines, and other AE versions
   have had limited coverage — reports welcome.
8. Very large Thickness/Softness/Shadow-Softness values on high-resolution
   comps cost render time (the gaussian passes scale with radius).

**Compatibility promise:** parameter identity is frozen and tracked by
stable IDs independent of on-screen position — new parameters get fresh
IDs and are only ever appended to that ID space, so projects saved with any
1.x-beta release will keep loading correctly in future updates even as the
visual layout and grouping evolve. (Projects saved with pre-release development builds
are not compatible; re-apply the effect.)

## License

Copyright (c) 2026 Viktor Kopeikin. Licensed under the **MIT License** (see
LICENSE).

## Feedback

This is a beta. If the glass misbehaves, include: OS and AE versions, GPU
model, comp resolution, whether GPU acceleration was active, and a
screenshot of the effect settings.
