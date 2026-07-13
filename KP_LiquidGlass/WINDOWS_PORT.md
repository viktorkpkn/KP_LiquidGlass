# KP_LiquidGlass — Windows (CUDA / DirectX) port status

State as of 1.0.0-beta build 12 (2026-07-13): the Windows port is built,
installed, and CUDA-verified in After Effects ("works perfectly, smooth,
fast" on an RTX 4080). The DirectX path compiles and deploys but has not yet
been exercised end-to-end in AE (see "Outstanding" below). The macOS/Metal
kernel has since been brought to full parity with everything below — see
"Metal parity" — so both platforms now render the same feature set.

## Project layout

`KP_LiquidGlass/Win/` holds the Visual Studio project (adapted from the
SDK's `SDK_Invert_ProcAmp` sample, not from a blank template):

- `KP_LiquidGlass.sln` / `KP_LiquidGlass.vcxproj` — x64, custom build steps
  for the CUDA kernel file and for each HLSL kernel (ParseHLSL.py + dxc),
  the PiPL step, and a post-build copy of `DirectX_Assets/` next to the
  `.aex`. `/p:KPEnableCuda=0|1` switches the CUDA custom-build step and the
  `KP_ENABLE_CUDA` preprocessor define on/off; with it off, NVIDIA cards are
  served through DirectX like AMD/Intel.
- Required environment variables: `CUDA_SDK_BASE_PATH` (CUDA 13.0, see
  "CUDA version pin" below), `DXC_SDK_BASE_PATH`, `BOOST_BASE_PATH`,
  `AE_PLUGIN_BUILD_DIR` (where the build drops the `.aex`). A real (non-
  WindowsApps-stub) Python 3 must precede the WindowsApps alias on `PATH`
  for the HLSL custom build step.

## Kernels

`KP_LiquidGlass_Kernel.cu` implements **11** kernels in the SDK's cross-API
macro DSL (`GF_KERNEL_FUNCTION`): PrepareMatte, JFASeed, JFAStep,
JFAResolve, BlurHorizontal, BlurVertical, BuildBevelHeight,
BlurBackgroundHorizontal, BlurBackgroundVertical, ShadowOffset (added for
the inner/outer shadow and caustics fields), and the composite
LiquidGlassKernel. CUDA launcher functions (`KP*_CUDA`) live at the bottom
of the same file, statically compiled by nvcc. Each kernel also gets its own
ParseHLSL+dxc custom-build pair in the vcxproj, producing
`DirectX_Assets/<Name>.cso|.rs` deployed next to the `.aex`.

## Verification status

1. **CUDA — verified.** Built, installed, and user-confirmed correct and
   fast in AE (Mercury GPU Acceleration on CUDA).
2. **DirectX — implemented, not yet verified in AE.** The DX render
   sequence (`SmartRenderGPU_DirectX` in `KP_LiquidGlass.cpp`) compiles and
   the shaders build, but `PF_OutFlag2_SUPPORTS_DIRECTX_RENDERING` is not
   yet declared in `GlobalSetup`/the PiPL, so Mercury GPU on DirectX will
   not currently route to this effect's GPU path at all. Adding that flag
   and doing a Mercury-DirectX smoke test (same checks as CUDA: matte
   shape, rim displacement, light stroke, zoom A/B) is the next open item.
   Left deliberately unset for the 1.0.0-beta release rather than shipping
   an unverified path — see the root `RELEASE_NOTES.md`.
3. **CPU fallback** is still a passthrough on both platforms (unchanged by
   this port).

## Metal parity (2026-07-13, Mac-side)

Composite On Top, the baked inner shadow, and the outer shadow +
caustics-through-glass system — plus the two Windows-only fixes below —
have been ported into the embedded Metal source in `KP_LiquidGlass.h`, so
macOS now renders all of this for real rather than showing inert controls:

- **Sub-pixel JFA seeding** (animation-jitter fix) and the **26-tap edge
  blur** (fixes ghosting/"tripling" at large Edge Blur) were ported
  directly from this file's `KPJFASeedKernel`/`KPSampleDisplaced` into
  Metal's `JFASeedKernel`/`SampleThroughMatrixBlurred`. The "Known cosmetic
  divergence" note below (edge-blur sampling space) still applies — that
  part was left as-is, not a bug.
- The shadow/caustics/composite-on-top system was ported into a new Metal
  `ShadowOffsetKernel` (11th pipeline) and an extended `LiquidGlassKernel`,
  following the same convention this port established: new values are
  passed via a separate `LiquidGlassShadowParams` constant buffer and
  appended kernel arguments, not by growing the frozen
  `LiquidGlassKernelParams` struct. `KP_MetalPipeline_Count` is now 11 to
  match.
- **New since the initial Windows port**: a **Confine to Layer Bounds**
  toggle (`KP_ID_CONFINE_TO_BOUNDS`). Outer Shadow/Caustics can't honor
  blend modes outside the shape's silhouette without real underlying
  pixels (non-Composite-on-Top mode) — they fall back to painting a
  translucent offset copy of the shape, which is visible as an unwanted
  halo. This toggle suppresses that fallback entirely. It's been mirrored
  into `KPLiquidGlassKernel`/`KPLiquidGlassRender_CUDA` here (trailing
  `inConfineToBounds` arg) and into `SmartRenderGPU_CUDA`'s call site and
  `KPRenderParamsDX`, but **only Metal has actually run and been verified**
  — the CUDA/DirectX host wiring for this one specific field needs a look
  and a test pass on a real Windows machine before trusting it.

Net effect: `LiquidGlassKernelParams` is still byte-identical everywhere it
always was; everything added since is via the appended
`LiquidGlassShadowParams`/`LiquidGlassRenderParams` path on both platforms,
so there's no ABI drift to reconcile going forward.

## Known cosmetic divergence

- **Edge Blur sampling space**: the portable kernel's 26-tap displaced blur
  (`KPSampleDisplaced` in the `.cu`) offsets taps in sample space (post
  `compToSample` matrix); the Metal kernel's `SampleThroughMatrixBlurred`
  offsets pre-matrix. Visually identical under near-uniform transforms;
  revisit only if a strongly anisotropic transform makes the blur look
  different across platforms. Both are now 26-tap Poisson disc (Metal was
  upgraded from 5-tap in the parity pass above), so this is purely a
  sample-space-vs-comp-space difference now, not a tap-count one.

## CPU fallback reminder

The CPU render is still a passthrough on both platforms. On Windows this
matters more: any machine where DX12/CUDA init fails will silently show no
effect. A real CPU implementation is the highest-value robustness item
after DirectX verification.
