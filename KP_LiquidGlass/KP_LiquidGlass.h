/*
** KP_LiquidGlass — Copyright (c) 2026 Viktor Kopeikin.
** Licensed under the PolyForm Noncommercial License 1.0.0.
** <https://polyformproject.org/licenses/noncommercial/1.0.0>
*/

#pragma once
#ifndef KP_LiquidGlass_H
#define KP_LiquidGlass_H

#include "AEConfig.h"
#include "entry.h"
#include "AEFX_SuiteHelper.h"
#include "PrSDKAESupport.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_EffectCBSuites.h"
#include "AE_EffectGPUSuites.h"
#include "AE_Macros.h"
#include "AEGP_SuiteHandler.h"
#include "String_Utils.h"
#include "Smart_Utils.h"

#ifndef AE_OS_WIN
    #define HAS_METAL 1
    #define HAS_CUDA 0
    #define HAS_HLSL 0
    #include <Metal/Metal.h>
#else
    #define HAS_METAL 0
    // Set KP_ENABLE_CUDA=0 in the project to build without the CUDA toolkit
    // (the effect then serves NVIDIA cards through DirectX like AMD/Intel).
    #ifndef KP_ENABLE_CUDA
        #define KP_ENABLE_CUDA 1
    #endif
    #define HAS_CUDA KP_ENABLE_CUDA
    #define HAS_HLSL 1
    #if HAS_CUDA
        #include <cuda_runtime.h>
    #endif
#endif

#define DESCRIPTION "Liquid glass refraction from any layer's alpha: distance-field bevel, edge inversion, dispersion, clear-coat light. Requires Mercury GPU Acceleration (Metal)."
#define AUTHOR_NOTE "Copyright (c) 2026 Viktor Kopeikin. PolyForm Noncommercial License 1.0.0."
#define NAME "KP_LiquidGlass"
#define MAJOR_VERSION 1
#define MINOR_VERSION 0
#define BUG_VERSION 0
#define STAGE_VERSION PF_Stage_BETA
#define BUILD_VERSION 12

// Parameter ORDER (indices) below reflects the 1.0.0b10 regrouped UI, in
// render-stacking order with collapsible Lighting and Shadows groups.
// Saved projects reconnect through the STABLE IDs (KP_ID_*), which never
// change; only the visual order and grouping did.
enum {
    KP_LIQUIDGLASS_INPUT = 0,
    KP_LIQUIDGLASS_ADJUSTMENT_MODE,
    KP_LIQUIDGLASS_BACKGROUND_LAYER,
    KP_LIQUIDGLASS_EXTEND_BACKGROUND,
    KP_LIQUIDGLASS_SHAPE_LAYER,
    KP_LIQUIDGLASS_COMPOSITE_ON_TOP,
    KP_LIQUIDGLASS_REFRACTION,
    KP_LIQUIDGLASS_SOFTNESS,
    KP_LIQUIDGLASS_THICKNESS,
    KP_LIQUIDGLASS_SPREAD,
    KP_LIQUIDGLASS_ZOOM,
    KP_LIQUIDGLASS_EDGE_BLUR,
    KP_LIQUIDGLASS_ROUGHNESS,
    KP_LIQUIDGLASS_DISPERSION,
    KP_LIQUIDGLASS_TINT_COLOR,
    KP_LIQUIDGLASS_TINT_OPACITY,
    KP_LIQUIDGLASS_GROUP_LIGHTING_START,
    KP_LIQUIDGLASS_LIGHT_ANGLE,
    KP_LIQUIDGLASS_LIGHT_COLOR,
    KP_LIQUIDGLASS_LIGHT_INTENSITY,
    KP_LIQUIDGLASS_STROKE_WIDTH,
    KP_LIQUIDGLASS_GROUP_LIGHTING_END,
    KP_LIQUIDGLASS_GROUP_SHADOWS_START,
    KP_LIQUIDGLASS_SHADOW_COLOR,
    KP_LIQUIDGLASS_SHADOW_OPACITY,
    KP_LIQUIDGLASS_SHADOW_DIRECTION,
    KP_LIQUIDGLASS_SHADOW_DISTANCE,
    KP_LIQUIDGLASS_SHADOW_SOFTNESS,
    KP_LIQUIDGLASS_SHADOW_MODE,
    KP_LIQUIDGLASS_CONFINE_TO_BOUNDS,
    KP_LIQUIDGLASS_OUTER_ANGLE,
    KP_LIQUIDGLASS_OUTER_DISTANCE,
    KP_LIQUIDGLASS_OUTER_SHADOW_COLOR,
    KP_LIQUIDGLASS_OUTER_SHADOW_INTENSITY,
    KP_LIQUIDGLASS_OUTER_SHADOW_SPREAD,
    KP_LIQUIDGLASS_OUTER_SHADOW_MODE,
    KP_LIQUIDGLASS_CAUSTICS_COLOR,
    KP_LIQUIDGLASS_CAUSTICS_INTENSITY,
    KP_LIQUIDGLASS_CAUSTICS_SPREAD,
    KP_LIQUIDGLASS_CAUSTICS_MODE,
    KP_LIQUIDGLASS_GROUP_SHADOWS_END,
    KP_LIQUIDGLASS_NUM_PARAMS
};

// Stable parameter IDs (uu.id): the original pre-regroup enum values for
// old params, fresh values for params added since. NEVER renumber these.
enum {
    KP_ID_ADJUSTMENT_MODE = 1,
    KP_ID_BACKGROUND_LAYER = 2,
    KP_ID_EXTEND_BACKGROUND = 3,
    KP_ID_SHAPE_LAYER = 4,
    KP_ID_REFRACTION = 5,
    KP_ID_SOFTNESS = 6,
    KP_ID_THICKNESS = 7,
    KP_ID_SPREAD = 8,
    KP_ID_ZOOM = 9,
    KP_ID_EDGE_BLUR = 10,
    KP_ID_STROKE_WIDTH = 11,
    KP_ID_LIGHT_ANGLE = 12,
    KP_ID_LIGHT_COLOR = 13,
    KP_ID_LIGHT_INTENSITY = 14,
    KP_ID_TINT_COLOR = 15,
    KP_ID_TINT_OPACITY = 16,
    KP_ID_ROUGHNESS = 17,
    KP_ID_DISPERSION = 18,
    KP_ID_COMPOSITE_ON_TOP = 19,
    KP_ID_SHADOW_COLOR = 20,
    KP_ID_SHADOW_OPACITY = 21,
    KP_ID_SHADOW_DIRECTION = 22,
    KP_ID_SHADOW_DISTANCE = 23,
    KP_ID_SHADOW_SOFTNESS = 24,
    KP_ID_SHADOW_MODE = 25,
    KP_ID_GROUP_LIGHTING_START = 26,
    KP_ID_GROUP_LIGHTING_END = 27,
    KP_ID_GROUP_SHADOWS_START = 28,
    KP_ID_GROUP_SHADOWS_END = 29,
    KP_ID_OUTER_ANGLE = 30,
    KP_ID_OUTER_DISTANCE = 31,
    KP_ID_OUTER_SHADOW_COLOR = 32,
    KP_ID_OUTER_SHADOW_INTENSITY = 33,
    KP_ID_OUTER_SHADOW_SPREAD = 34,
    KP_ID_OUTER_SHADOW_MODE = 35,
    KP_ID_CAUSTICS_COLOR = 36,
    KP_ID_CAUSTICS_INTENSITY = 37,
    KP_ID_CAUSTICS_SPREAD = 38,
    KP_ID_CAUSTICS_MODE = 39,
    KP_ID_CONFINE_TO_BOUNDS = 40
};

// Outer shadow & caustics defaults follow the user's reference setup.
#define OUTER_ANGLE_DFLT 180
#define OUTER_DISTANCE_MIN_VALUE 0
#define OUTER_DISTANCE_MAX_VALUE 2000
#define OUTER_DISTANCE_MIN_SLIDER 0
#define OUTER_DISTANCE_MAX_SLIDER 500
#define OUTER_DISTANCE_DFLT 40
#define OUTER_SHADOW_INTENSITY_DFLT 19
#define OUTER_SHADOW_SPREAD_DFLT 179
#define CAUSTICS_INTENSITY_DFLT 100
#define CAUSTICS_SPREAD_DFLT 89
#define OUTER_SPREAD_MIN_VALUE 0
#define OUTER_SPREAD_MAX_VALUE 2000
#define OUTER_SPREAD_MIN_SLIDER 0
#define OUTER_SPREAD_MAX_SLIDER 500
#define OUTER_INTENSITY_MIN_VALUE 0
#define OUTER_INTENSITY_MAX_VALUE 100
#define OUTER_INTENSITY_MIN_SLIDER 0
#define OUTER_INTENSITY_MAX_SLIDER 100

// Inner shadow defaults follow the user's reference Drop Shadow settings.
#define SHADOW_OPACITY_MIN_VALUE 0
#define SHADOW_OPACITY_MAX_VALUE 100
#define SHADOW_OPACITY_MIN_SLIDER 0
#define SHADOW_OPACITY_MAX_SLIDER 100
#define SHADOW_OPACITY_DFLT 20
#define SHADOW_DIRECTION_DFLT 0
#define SHADOW_DISTANCE_MIN_VALUE 0
#define SHADOW_DISTANCE_MAX_VALUE 2000
#define SHADOW_DISTANCE_MIN_SLIDER 0
#define SHADOW_DISTANCE_MAX_SLIDER 500
#define SHADOW_DISTANCE_DFLT 89
#define SHADOW_SOFTNESS_MIN_VALUE 0
#define SHADOW_SOFTNESS_MAX_VALUE 2000
#define SHADOW_SOFTNESS_MIN_SLIDER 0
#define SHADOW_SOFTNESS_MAX_SLIDER 500
#define SHADOW_SOFTNESS_DFLT 152

enum {
    KP_ShadowMode_Normal = 1,   // AE popups are 1-based
    KP_ShadowMode_Multiply,
    KP_ShadowMode_Screen,
    KP_ShadowMode_Overlay,
    KP_ShadowMode_SoftLight,
    KP_ShadowMode_HardLight,
    KP_ShadowMode_Count = KP_ShadowMode_HardLight
};

// Refraction is a percentage of Thickness: displacement at the rim =
// refraction% * bevel width.
#define REFRACTION_MIN_VALUE 0
#define REFRACTION_MAX_VALUE 400
#define REFRACTION_MIN_SLIDER 0
#define REFRACTION_MAX_SLIDER 200
#define REFRACTION_DFLT 250

#define SOFTNESS_MIN_VALUE 1
#define SOFTNESS_MAX_VALUE 500
#define SOFTNESS_MIN_SLIDER 1
#define SOFTNESS_MAX_SLIDER 100
#define SOFTNESS_DFLT 1

#define THICKNESS_MIN_VALUE 0
#define THICKNESS_MAX_VALUE 500
#define THICKNESS_MIN_SLIDER 0
#define THICKNESS_MAX_SLIDER 200
#define THICKNESS_DFLT 30

#define SPREAD_MIN_VALUE 0
#define SPREAD_MAX_VALUE 100
#define SPREAD_MIN_SLIDER 0
#define SPREAD_MAX_SLIDER 100
#define SPREAD_DFLT 100

#define ZOOM_MIN_VALUE 100
#define ZOOM_MAX_VALUE 300
#define ZOOM_MIN_SLIDER 100
#define ZOOM_MAX_SLIDER 200
#define ZOOM_DFLT 105

#define EDGE_BLUR_MIN_VALUE 0
#define EDGE_BLUR_MAX_VALUE 100
#define EDGE_BLUR_MIN_SLIDER 0
#define EDGE_BLUR_MAX_SLIDER 50
#define EDGE_BLUR_DFLT 10

#define STROKE_WIDTH_MIN_VALUE 0
#define STROKE_WIDTH_MAX_VALUE 200
#define STROKE_WIDTH_MIN_SLIDER 0
#define STROKE_WIDTH_MAX_SLIDER 200
#define STROKE_WIDTH_DFLT 4

#define LIGHT_ANGLE_DFLT (-45)
#define LIGHT_INTENSITY_MIN_VALUE 0
#define LIGHT_INTENSITY_MAX_VALUE 200
#define LIGHT_INTENSITY_MIN_SLIDER 0
#define LIGHT_INTENSITY_MAX_SLIDER 100
#define LIGHT_INTENSITY_DFLT 70

#define TINT_OPACITY_MIN_VALUE 0
#define TINT_OPACITY_MAX_VALUE 100
#define TINT_OPACITY_MIN_SLIDER 0
#define TINT_OPACITY_MAX_SLIDER 100
#define TINT_OPACITY_DFLT 0

#define ROUGHNESS_MIN_VALUE 0
#define ROUGHNESS_MAX_VALUE 100
#define ROUGHNESS_MIN_SLIDER 0
#define ROUGHNESS_MAX_SLIDER 50
#define ROUGHNESS_DFLT 2

#define DISPERSION_MIN_VALUE 0
#define DISPERSION_MAX_VALUE 100
#define DISPERSION_MIN_SLIDER 0
#define DISPERSION_MAX_SLIDER 100
#define DISPERSION_DFLT 100

// CPU-side state carried from PreRender to SmartRender.
// Pixel-unit values (softness, thickness, roughness, edgeBlur) are already
// scaled for the current downsample factor; refraction is a percentage of
// thickness and needs no scaling. The compToSample matrix maps this layer's
// render coordinates to the sample source's render coordinates;
// sampleLeft/Top is the checked-out sample world's origin.
struct LiquidGlassRenderParams
{
    A_long adjustmentMode;
    A_long hasBackground;
    A_long extendBackground;
    A_long hasShapeLayer;
    float strokeWidth;
    float refraction;
    float softness;
    float thickness;
    float spread;
    float zoom;
    float roughness;
    float dispersion;
    float edgeBlur;
    float lightDirX;
    float lightDirY;
    float lightRed;
    float lightGreen;
    float lightBlue;
    float lightIntensity;
    float tintRed;
    float tintGreen;
    float tintBlue;
    float tintOpacity;
    float zoomCenterX;
    float zoomCenterY;
    A_long maskLeft;
    A_long maskTop;
    A_long inputLeft;
    A_long inputTop;
    A_long sampleLeft;
    A_long sampleTop;
    A_long outputLeft;
    A_long outputTop;
    float compToSample00;
    float compToSample01;
    float compToSample02;
    float compToSample10;
    float compToSample11;
    float compToSample12;
    // Maps this layer's render coordinates into the Glass Shape layer's
    // world (identity when the matte comes from the effect input).
    float compToMask00;
    float compToMask01;
    float compToMask02;
    float compToMask10;
    float compToMask11;
    float compToMask12;
    // 1.0.0b8+ additions (not part of the Metal-mirrored kernel params
    // struct; the GPU sequences read these directly).
    A_long compositeOnTop;
    A_long shadowMode;
    float shadowRed;
    float shadowGreen;
    float shadowBlue;
    float shadowOpacity;
    float shadowOffsetX;
    float shadowOffsetY;
    float shadowSoftness;
    // 1.0.0b10: outer shadow & caustics (shared angle/distance offset).
    A_long outerShadowMode;
    A_long causticsMode;
    float outerOffsetX;
    float outerOffsetY;
    float outerShadowRed;
    float outerShadowGreen;
    float outerShadowBlue;
    float outerShadowIntensity;
    float outerShadowSpread;
    float causticsRed;
    float causticsGreen;
    float causticsBlue;
    float causticsIntensity;
    float causticsSpread;
    // Outer shadow/caustics can't honor blend modes outside the shape's own
    // silhouette when the effect has no access to real underlying pixels
    // (non-compositeOnTop) -- they fall back to a flat translucent paint of
    // the offset+blurred matte, which reads as a visible duplicate-shape
    // halo. This lets that halo be suppressed entirely instead.
    A_long confineToBounds;
};

// Mirrored in the Metal source below; all members must stay 4-byte scalars so
// the C++ and MSL layouts match. `refraction` here is the precomputed rim
// displacement in render pixels (percent * thickness).
struct LiquidGlassKernelParams
{
    int maskPitch;
    int samplePitch;
    int dstPitch;
    unsigned int width;
    unsigned int height;
    unsigned int maskWidth;
    unsigned int maskHeight;
    unsigned int sampleWidth;
    unsigned int sampleHeight;
    float refraction;
    float thickness;
    float spread;
    float zoom;
    float zoomCenterX;
    float zoomCenterY;
    float edgeBlur;
    float lightDirX;
    float lightDirY;
    float lightRed;
    float lightGreen;
    float lightBlue;
    float lightIntensity;
    float tintRed;
    float tintGreen;
    float tintBlue;
    float tintOpacity;
    float dispersion;
    int extendBackground;
    int maskLeft;
    int maskTop;
    int sampleLeft;
    int sampleTop;
    int outputLeft;
    int outputTop;
    float compToSample00;
    float compToSample01;
    float compToSample02;
    float compToSample10;
    float compToSample11;
    float compToSample12;
    float compToMask00;
    float compToMask01;
    float compToMask02;
    float compToMask10;
    float compToMask11;
    float compToMask12;
    float strokeWidth;
};

// Per-dispatch data for the separable blur passes (mirrored in MSL).
struct LiquidGlassPassParams
{
    int radius;
    float sigma;
    int step;
    int srcPitch;
    int dstPitch;
};

// 1.0.0b8-b10 additions (composite-on-top, inner/outer shadow, caustics),
// appended as their own constant buffer rather than growing the frozen
// LiquidGlassKernelParams above -- same technique CUDA/DX use to keep
// LiquidGlassKernelParams byte-identical across every platform (mirrored in
// MSL).
struct LiquidGlassShadowParams
{
    int compositeOnTop;
    int underPitch;
    float shadowRed;
    float shadowGreen;
    float shadowBlue;
    float shadowOpacity;
    int shadowMode;
    float outerShadowRed;
    float outerShadowGreen;
    float outerShadowBlue;
    float outerShadowIntensity;
    int outerShadowMode;
    float causticsRed;
    float causticsGreen;
    float causticsBlue;
    float causticsIntensity;
    int causticsMode;
    int confineToBounds;
};

#if HAS_CUDA
// Launchers implemented in KP_LiquidGlass_Kernel.cu (compiled by nvcc).
// Scalar pack order for KPLiquidGlassRender_CUDA is documented in that file.
extern void KPPrepareMatte_CUDA(float const *inMask, float *outMatte,
    int inMaskPitch, unsigned int inMaskWidth, unsigned int inMaskHeight,
    unsigned int inWidth, unsigned int inHeight,
    int inOutputLeft, int inOutputTop, int inMaskLeft, int inMaskTop,
    float const *inC2M);
extern void KPJFASeed_CUDA(float const *inMatte, float *outSeeds,
    unsigned int inWidth, unsigned int inHeight);
extern void KPJFAStep_CUDA(float const *inSrc, float *outDst,
    unsigned int inWidth, unsigned int inHeight, int inStep);
extern void KPJFAResolve_CUDA(float const *inSeeds, float const *inMatte, float *outField,
    unsigned int inWidth, unsigned int inHeight, float inThickness);
extern void KPBlurHorizontal_CUDA(float const *inSrc, float *outDst,
    unsigned int inWidth, unsigned int inHeight, int inRadius, float inSigma);
extern void KPBlurVertical_CUDA(float const *inSrc, float *outDst,
    unsigned int inWidth, unsigned int inHeight, int inRadius, float inSigma);
extern void KPBuildBevelHeight_CUDA(float const *inField, float *outHeight,
    unsigned int inWidth, unsigned int inHeight, float inSpread);
extern void KPBlurBackgroundHorizontal_CUDA(float const *inSrc, float *outDst,
    unsigned int inSampleWidth, unsigned int inSampleHeight,
    int inRadius, float inSigma, int inSrcPitch, int inDstPitch);
extern void KPBlurBackgroundVertical_CUDA(float const *inSrc, float *outDst,
    unsigned int inSampleWidth, unsigned int inSampleHeight,
    int inRadius, float inSigma, int inSrcPitch, int inDstPitch);
extern void KPShadowOffset_CUDA(float const *inMatte, float *outShadow,
    unsigned int inWidth, unsigned int inHeight,
    float inOffsetX, float inOffsetY);
extern void KPLiquidGlassRender_CUDA(
    float const *inMatte, float const *inBevel, float const *inHeightMap,
    float const *inSample, float const *inShadow, float const *inUnder,
    float const *inOuterShadow, float const *inCaustics, float *outDst,
    int inSamplePitch, int inDstPitch,
    unsigned int inWidth, unsigned int inHeight,
    unsigned int inSampleWidth, unsigned int inSampleHeight,
    float const *inScalars, int inExtendEdges,
    int inSampleLeft, int inSampleTop, int inOutputLeft, int inOutputTop,
    float const *inC2S,
    int inShadowMode, int inCompositeOnTop, int inUnderPitch,
    int inOuterShadowMode, int inCausticsMode, int inConfineToBounds);
#endif

#if HAS_HLSL
// Constant-buffer layouts for the DirectX shaders. Field order MUST match
// the value-argument order of the corresponding GF_KERNEL_FUNCTION in
// KP_LiquidGlass_Kernel.cu exactly.
struct KPPrepareMatteParamsDX
{
    int maskPitch;
    unsigned int maskWidth;
    unsigned int maskHeight;
    unsigned int width;
    unsigned int height;
    int outputLeft;
    int outputTop;
    int maskLeft;
    int maskTop;
    float c2m[6];
};

struct KPJFASeedParamsDX
{
    unsigned int width;
    unsigned int height;
};

struct KPJFAStepParamsDX
{
    unsigned int width;
    unsigned int height;
    int step;
};

struct KPJFAResolveParamsDX
{
    unsigned int width;
    unsigned int height;
    float thickness;
};

struct KPBlurParamsDX
{
    unsigned int width;
    unsigned int height;
    int radius;
    float sigma;
};

struct KPBuildBevelParamsDX
{
    unsigned int width;
    unsigned int height;
    float spread;
};

struct KPBlurBackgroundParamsDX
{
    unsigned int sampleWidth;
    unsigned int sampleHeight;
    int radius;
    float sigma;
    int srcPitch;
    int dstPitch;
};

struct KPShadowOffsetParamsDX
{
    unsigned int width;
    unsigned int height;
    float offsetX;
    float offsetY;
};

struct KPRenderParamsDX
{
    int samplePitch;
    int dstPitch;
    unsigned int width;
    unsigned int height;
    unsigned int sampleWidth;
    unsigned int sampleHeight;
    float refraction;
    float thickness;
    float zoom;
    float zoomCenterX;
    float zoomCenterY;
    float edgeBlur;
    float lightDirX;
    float lightDirY;
    float lightR;
    float lightG;
    float lightB;
    float lightIntensity;
    float tintR;
    float tintG;
    float tintB;
    float tintOpacity;
    float dispersion;
    int extendEdges;
    int sampleLeft;
    int sampleTop;
    int outputLeft;
    int outputTop;
    float c2s[6];
    float strokeWidth;
    float shadowR;
    float shadowG;
    float shadowB;
    float shadowOpacity;
    int shadowMode;
    int compositeOnTop;
    int underPitch;
    float outerShadowR;
    float outerShadowG;
    float outerShadowB;
    float outerShadowIntensity;
    int outerShadowMode;
    float causticsR;
    float causticsG;
    float causticsB;
    float causticsIntensity;
    int causticsMode;
    int confineToBounds;
};
#endif

#if HAS_METAL
static const char kKP_LiquidGlassMetalSource[] = R"METAL(
#include <metal_stdlib>
using namespace metal;

struct LiquidGlassKernelParams
{
    int maskPitch;
    int samplePitch;
    int dstPitch;
    uint width;
    uint height;
    uint maskWidth;
    uint maskHeight;
    uint sampleWidth;
    uint sampleHeight;
    float refraction;
    float thickness;
    float spread;
    float zoom;
    float zoomCenterX;
    float zoomCenterY;
    float edgeBlur;
    float lightDirX;
    float lightDirY;
    float lightRed;
    float lightGreen;
    float lightBlue;
    float lightIntensity;
    float tintRed;
    float tintGreen;
    float tintBlue;
    float tintOpacity;
    float dispersion;
    int extendBackground;
    int maskLeft;
    int maskTop;
    int sampleLeft;
    int sampleTop;
    int outputLeft;
    int outputTop;
    float compToSample00;
    float compToSample01;
    float compToSample02;
    float compToSample10;
    float compToSample11;
    float compToSample12;
    float compToMask00;
    float compToMask01;
    float compToMask02;
    float compToMask10;
    float compToMask11;
    float compToMask12;
    float strokeWidth;
};

struct LiquidGlassPassParams
{
    int radius;
    float sigma;
    int step;
    int srcPitch;
    int dstPitch;
};

struct LiquidGlassShadowParams
{
    int compositeOnTop;
    int underPitch;
    float shadowRed;
    float shadowGreen;
    float shadowBlue;
    float shadowOpacity;
    int shadowMode;
    float outerShadowRed;
    float outerShadowGreen;
    float outerShadowBlue;
    float outerShadowIntensity;
    int outerShadowMode;
    float causticsRed;
    float causticsGreen;
    float causticsBlue;
    float causticsIntensity;
    int causticsMode;
    int confineToBounds;
};

static float MaskAlphaAt(
    device const float4 *maskPixels,
    constant LiquidGlassKernelParams &params,
    int x,
    int y)
{
    if (x < 0 || y < 0 || x >= int(params.maskWidth) || y >= int(params.maskHeight)) {
        return 0.0f;
    }

    return clamp(maskPixels[y * params.maskPitch + x].w, 0.0f, 1.0f);
}

// Worlds are premultiplied GPU_BGRA128: x=B, y=G, z=R, w=A.
static float4 PremultipliedColor(float red, float green, float blue, float alpha)
{
    return float4(blue, green, red, 1.0f) * alpha;
}

// Premultiplied source-over.
static float4 SourceOver(float4 top, float4 bottom)
{
    return top + bottom * (1.0f - top.w);
}

// Blend one shadow/caustics color channel onto an unpremultiplied base
// channel (Normal/Multiply/Screen/Overlay/Soft Light/Hard Light).
static float ShadowBlendChannel(float base, float shadow, int mode)
{
    if (mode == 2) {          // Multiply
        return base * shadow;
    }
    if (mode == 3) {          // Screen
        return 1.0f - (1.0f - base) * (1.0f - shadow);
    }
    if (mode == 4) {          // Overlay
        return base < 0.5f
            ? 2.0f * base * shadow
            : 1.0f - 2.0f * (1.0f - base) * (1.0f - shadow);
    }
    if (mode == 5) {          // Soft Light
        return (1.0f - 2.0f * shadow) * base * base + 2.0f * shadow * base;
    }
    if (mode == 6) {          // Hard Light
        return shadow < 0.5f
            ? 2.0f * base * shadow
            : 1.0f - 2.0f * (1.0f - base) * (1.0f - shadow);
    }
    return shadow;             // Normal
}

static float3 ShadowBlend(float3 base, float3 shadow, int mode)
{
    return float3(ShadowBlendChannel(base.x, shadow.x, mode),
                  ShadowBlendChannel(base.y, shadow.y, mode),
                  ShadowBlendChannel(base.z, shadow.z, mode));
}

// One background texel. Outside the world's bounds: either the edge pixel is
// extended (Extend Background on) or the tap is transparent so the sampling
// fades to alpha.
static float4 BackgroundTap(
    device const float4 *samplePixels,
    constant LiquidGlassKernelParams &params,
    int x,
    int y)
{
    if (params.extendBackground) {
        x = clamp(x, 0, int(params.sampleWidth) - 1);
        y = clamp(y, 0, int(params.sampleHeight) - 1);
    } else if (x < 0 || y < 0 || x >= int(params.sampleWidth) || y >= int(params.sampleHeight)) {
        return float4(0.0f);
    }

    return samplePixels[y * params.samplePitch + x];
}

static float4 SampleBackgroundBilinear(
    device const float4 *samplePixels,
    constant LiquidGlassKernelParams &params,
    float sampleX,
    float sampleY)
{
    if (params.extendBackground) {
        sampleX = clamp(sampleX, 0.0f, float(params.sampleWidth - 1));
        sampleY = clamp(sampleY, 0.0f, float(params.sampleHeight - 1));
    }

    const int x0 = int(floor(sampleX));
    const int y0 = int(floor(sampleY));
    const int x1 = x0 + 1;
    const int y1 = y0 + 1;
    const float tx = sampleX - float(x0);
    const float ty = sampleY - float(y0);

    const float4 c00 = BackgroundTap(samplePixels, params, x0, y0);
    const float4 c10 = BackgroundTap(samplePixels, params, x1, y0);
    const float4 c01 = BackgroundTap(samplePixels, params, x0, y1);
    const float4 c11 = BackgroundTap(samplePixels, params, x1, y1);

    return mix(mix(c00, c10, tx), mix(c01, c11, tx), ty);
}

// Maps a point in this layer's render coordinates through the compToSample
// matrix into the sample world and returns the bilinear sample.
static float4 SampleThroughMatrix(
    device const float4 *samplePixels,
    constant LiquidGlassKernelParams &params,
    float2 p)
{
    const float sx = params.compToSample00 * p.x + params.compToSample01 * p.y + params.compToSample02 - float(params.sampleLeft);
    const float sy = params.compToSample10 * p.x + params.compToSample11 * p.y + params.compToSample12 - float(params.sampleTop);
    return SampleBackgroundBilinear(samplePixels, params, sx, sy);
}

// Same as BackgroundTap/SampleBackgroundBilinear but reads inUnder -- a
// separate raw (never roughness-blurred) copy of the sample world, so
// Composite-on-Top can see real underlying pixels even when Roughness is on.
// Its pitch can differ from samplePixels', hence the explicit argument.
static float4 UnderTap(
    device const float4 *underPixels,
    constant LiquidGlassKernelParams &params,
    int underPitch,
    int x,
    int y)
{
    if (params.extendBackground) {
        x = clamp(x, 0, int(params.sampleWidth) - 1);
        y = clamp(y, 0, int(params.sampleHeight) - 1);
    } else if (x < 0 || y < 0 || x >= int(params.sampleWidth) || y >= int(params.sampleHeight)) {
        return float4(0.0f);
    }
    return underPixels[y * underPitch + x];
}

static float4 SampleUnderThroughMatrix(
    device const float4 *underPixels,
    constant LiquidGlassKernelParams &params,
    int underPitch,
    float2 p)
{
    const float sx = params.compToSample00 * p.x + params.compToSample01 * p.y + params.compToSample02 - float(params.sampleLeft);
    const float sy = params.compToSample10 * p.x + params.compToSample11 * p.y + params.compToSample12 - float(params.sampleTop);

    float sampleX = sx;
    float sampleY = sy;
    if (params.extendBackground) {
        sampleX = clamp(sampleX, 0.0f, float(params.sampleWidth - 1));
        sampleY = clamp(sampleY, 0.0f, float(params.sampleHeight - 1));
    }
    const int x0 = int(floor(sampleX));
    const int y0 = int(floor(sampleY));
    const int x1 = x0 + 1;
    const int y1 = y0 + 1;
    const float tx = sampleX - float(x0);
    const float ty = sampleY - float(y0);

    const float4 c00 = UnderTap(underPixels, params, underPitch, x0, y0);
    const float4 c10 = UnderTap(underPixels, params, underPitch, x1, y0);
    const float4 c01 = UnderTap(underPixels, params, underPitch, x0, y1);
    const float4 c11 = UnderTap(underPixels, params, underPitch, x1, y1);

    return mix(mix(c00, c10, tx), mix(c01, c11, tx), ty);
}

// Edge blur: a 26-tap Poisson-disc blur (gaussian weights, center + 25)
// around the displaced point so the distorted rim softens without ghosting
// high-contrast content at large radii -- the earlier 5-tap version left
// visible ghost copies ("tripling"). Offsets stay in comp space (pre-
// matrix), the existing Metal convention; KP_LiquidGlass_Kernel.cu's
// KPSampleDisplaced applies the same tap table in sample space instead, a
// deliberate, accepted cosmetic-only divergence under near-uniform
// transforms.
static float4 SampleThroughMatrixBlurred(
    device const float4 *samplePixels,
    constant LiquidGlassKernelParams &params,
    float2 p,
    float radius)
{
    if (radius <= 0.25f) {
        return SampleThroughMatrix(samplePixels, params, p);
    }

    const float o = radius;
    float4 sum = SampleThroughMatrix(samplePixels, params, p);
    float weightSum = 1.0f;

#define KP_EDGE_TAP(OX, OY, WEIGHT) \
    { \
        sum += SampleThroughMatrix(samplePixels, params, p + float2(OX, OY) * o) * (WEIGHT); \
        weightSum += (WEIGHT); \
    }

    KP_EDGE_TAP( 0.691238f,  0.353789f, 0.2994f)
    KP_EDGE_TAP(-0.916361f, -0.398034f, 0.1358f)
    KP_EDGE_TAP( 0.356088f, -0.894845f, 0.1564f)
    KP_EDGE_TAP(-0.479510f,  0.795561f, 0.1780f)
    KP_EDGE_TAP(-0.121922f, -0.027974f, 0.9692f)
    KP_EDGE_TAP( 0.897908f, -0.359819f, 0.1539f)
    KP_EDGE_TAP(-0.376753f, -0.879271f, 0.1604f)
    KP_EDGE_TAP( 0.252976f,  0.940587f, 0.1500f)
    KP_EDGE_TAP(-0.881660f,  0.220244f, 0.1917f)
    KP_EDGE_TAP( 0.315311f, -0.328585f, 0.6605f)
    KP_EDGE_TAP(-0.039054f,  0.507224f, 0.5959f)
    KP_EDGE_TAP( 0.273669f,  0.194845f, 0.7979f)
    KP_EDGE_TAP(-0.430749f, -0.388759f, 0.5100f)
    KP_EDGE_TAP(-0.024568f, -0.619074f, 0.4641f)
    KP_EDGE_TAP(-0.445267f,  0.345972f, 0.5294f)
    KP_EDGE_TAP( 0.612813f,  0.735445f, 0.1600f)
    KP_EDGE_TAP( 0.964306f,  0.041737f, 0.1552f)
    KP_EDGE_TAP( 0.698508f, -0.706329f, 0.1390f)
    KP_EDGE_TAP( 0.596281f, -0.017938f, 0.4908f)
    KP_EDGE_TAP(-0.600695f, -0.054708f, 0.4830f)
    KP_EDGE_TAP(-0.118511f,  0.895520f, 0.1955f)
    KP_EDGE_TAP(-0.710748f, -0.686135f, 0.1420f)
    KP_EDGE_TAP(-0.794526f,  0.606034f, 0.1357f)
    KP_EDGE_TAP(-0.009264f, -0.991481f, 0.1400f)
    KP_EDGE_TAP( 0.318821f,  0.519225f, 0.4759f)
#undef KP_EDGE_TAP

    return sum / weightSum;
}

static float Gaussian(float x, float sigma)
{
    return exp(-0.5f * (x * x) / max(sigma * sigma, 0.0001f));
}

// The matte is sampled through the compToMask matrix so it can come either
// from the effect input (identity matrix) or from the Glass Shape layer's
// world mapped through that layer's transform.
kernel void PrepareMatteKernel(
    device const float4 *maskPixels [[buffer(0)]],
    device float *matte [[buffer(1)]],
    constant LiquidGlassKernelParams &params [[buffer(2)]],
    uint2 xy [[thread_position_in_grid]])
{
    if (xy.x >= params.width || xy.y >= params.height) {
        return;
    }

    const int x = int(xy.x);
    const int y = int(xy.y);
    const int index = y * int(params.width) + x;

    const float px = float(params.outputLeft + x);
    const float py = float(params.outputTop + y);
    const float maskX = params.compToMask00 * px + params.compToMask01 * py + params.compToMask02 - float(params.maskLeft);
    const float maskY = params.compToMask10 * px + params.compToMask11 * py + params.compToMask12 - float(params.maskTop);

    const int x0 = int(floor(maskX));
    const int y0 = int(floor(maskY));
    const float tx = maskX - float(x0);
    const float ty = maskY - float(y0);
    const float a00 = MaskAlphaAt(maskPixels, params, x0, y0);
    const float a10 = MaskAlphaAt(maskPixels, params, x0 + 1, y0);
    const float a01 = MaskAlphaAt(maskPixels, params, x0, y0 + 1);
    const float a11 = MaskAlphaAt(maskPixels, params, x0 + 1, y0 + 1);
    matte[index] = mix(mix(a00, a10, tx), mix(a01, a11, tx), ty);
}

kernel void BlurHorizontalKernel(
    device const float *src [[buffer(0)]],
    device float *dst [[buffer(1)]],
    constant LiquidGlassKernelParams &params [[buffer(2)]],
    constant LiquidGlassPassParams &pass [[buffer(3)]],
    uint2 xy [[thread_position_in_grid]])
{
    if (xy.x >= params.width || xy.y >= params.height) {
        return;
    }

    const int x = int(xy.x);
    const int y = int(xy.y);
    const int radius = max(pass.radius, 1);
    const float sigma = pass.sigma;
    float sum = 0.0f;
    float weightSum = 0.0f;

    for (int dx = -radius; dx <= radius; ++dx) {
        const int sx = clamp(x + dx, 0, int(params.width) - 1);
        const float weight = Gaussian(float(dx), sigma);
        sum += src[y * int(params.width) + sx] * weight;
        weightSum += weight;
    }

    dst[y * int(params.width) + x] = sum / weightSum;
}

kernel void BlurVerticalKernel(
    device const float *src [[buffer(0)]],
    device float *dst [[buffer(1)]],
    constant LiquidGlassKernelParams &params [[buffer(2)]],
    constant LiquidGlassPassParams &pass [[buffer(3)]],
    uint2 xy [[thread_position_in_grid]])
{
    if (xy.x >= params.width || xy.y >= params.height) {
        return;
    }

    const int x = int(xy.x);
    const int y = int(xy.y);
    const int radius = max(pass.radius, 1);
    const float sigma = pass.sigma;
    float sum = 0.0f;
    float weightSum = 0.0f;

    for (int dy = -radius; dy <= radius; ++dy) {
        const int sy = clamp(y + dy, 0, int(params.height) - 1);
        const float weight = Gaussian(float(dy), sigma);
        sum += src[sy * int(params.width) + x] * weight;
        weightSum += weight;
    }

    dst[y * int(params.width) + x] = sum / weightSum;
}

// Jump-flooding (JFA): exact euclidean distance to the shape edge. A
// gaussian pseudo-distance is curvature-biased — elongated shapes got fat
// bands at their narrow ends and thin bands on long edges — while JFA gives
// an even inset everywhere.
kernel void JFASeedKernel(
    device const float *matte [[buffer(0)]],
    device float2 *seeds [[buffer(1)]],
    constant LiquidGlassKernelParams &params [[buffer(2)]],
    uint2 xy [[thread_position_in_grid]])
{
    if (xy.x >= params.width || xy.y >= params.height) {
        return;
    }

    const int x = int(xy.x);
    const int y = int(xy.y);
    const int w = int(params.width);
    const int h = int(params.height);
    const int index = y * w + x;

    // Seeds are the inside pixels touching the outside (buffer borders count
    // as outside so clipped shapes still get a defined edge).
    const float a = matte[index];
    bool boundary = false;
    float aL = 0.0f;
    float aR = 0.0f;
    float aU = 0.0f;
    float aD = 0.0f;
    if (a >= 0.5f) {
        aL = (x > 0) ? matte[index - 1] : 0.0f;
        aR = (x < w - 1) ? matte[index + 1] : 0.0f;
        aU = (y > 0) ? matte[index - w] : 0.0f;
        aD = (y < h - 1) ? matte[index + w] : 0.0f;
        boundary = (aL < 0.5f) || (aR < 0.5f) || (aU < 0.5f) || (aD < 0.5f);
    }

    float2 seed = float2(-1.0e6f, -1.0e6f);
    if (boundary) {
        // Place the seed on the 0.5 iso-line recovered from the AA matte
        // instead of at the integer pixel center: integer seeds freeze
        // between half-pixel crossings and then lurch a whole pixel, which
        // jitters the distance field -- and displacement, light normal and
        // stroke with it -- under sub-pixel motion.
        const float gx = (aR - aL) * 0.5f;
        const float gy = (aD - aU) * 0.5f;
        const float g2 = gx * gx + gy * gy;
        float ox = 0.0f;
        float oy = 0.0f;
        if (g2 > 0.0001f) {
            const float s = (a - 0.5f) / g2;
            ox = clamp(-gx * s, -1.0f, 1.0f);
            oy = clamp(-gy * s, -1.0f, 1.0f);
        }
        seed = float2(float(x) + ox, float(y) + oy);
    }

    seeds[index] = seed;
}

kernel void JFAStepKernel(
    device const float2 *src [[buffer(0)]],
    device float2 *dst [[buffer(1)]],
    constant LiquidGlassKernelParams &params [[buffer(2)]],
    constant LiquidGlassPassParams &pass [[buffer(3)]],
    uint2 xy [[thread_position_in_grid]])
{
    if (xy.x >= params.width || xy.y >= params.height) {
        return;
    }

    const int x = int(xy.x);
    const int y = int(xy.y);
    const int w = int(params.width);
    const int h = int(params.height);
    const int index = y * w + x;
    const float2 p = float2(float(x), float(y));
    const int step = max(pass.step, 1);

    float2 best = src[index];
    float bestD = (best.x > -1.0e5f) ? distance_squared(p, best) : 1.0e30f;

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) {
                continue;
            }
            const int nx = x + dx * step;
            const int ny = y + dy * step;
            if (nx < 0 || ny < 0 || nx >= w || ny >= h) {
                continue;
            }
            const float2 cand = src[ny * w + nx];
            if (cand.x < -1.0e5f) {
                continue;
            }
            const float d = distance_squared(p, cand);
            if (d < bestD) {
                bestD = d;
                best = cand;
            }
        }
    }

    dst[index] = best;
}

// Edge proximity from the distance field: 1 at the rim, fading to 0 at
// `thickness` pixels inward; the outside stays at rim level so smoothing
// across the edge behaves.
kernel void JFAResolveKernel(
    device const float2 *seeds [[buffer(0)]],
    device const float *matte [[buffer(1)]],
    device float *field [[buffer(2)]],
    constant LiquidGlassKernelParams &params [[buffer(3)]],
    constant LiquidGlassPassParams &pass [[buffer(4)]],
    uint2 xy [[thread_position_in_grid]])
{
    if (xy.x >= params.width || xy.y >= params.height) {
        return;
    }

    const int index = int(xy.y) * int(params.width) + int(xy.x);
    if (matte[index] < 0.5f) {
        field[index] = 1.0f;
        return;
    }

    const float2 best = seeds[index];
    const float thickness = max(pass.sigma, 1.0f);
    const float d = (best.x > -1.0e5f)
        ? distance(float2(float(xy.x), float(xy.y)), best)
        : 1.0e6f;
    field[index] = clamp(1.0f - d / thickness, 0.0f, 1.0f);
}

// Lens SURFACE from the edge-proximity field: a rounded-slab bevel cross
// section, z = 0 at the rim rising to 1 where the bevel ends (and staying 1
// across the interior plateau). Displacement is taken from this surface's
// GRADIENT in the render kernel — magnitude and direction together — so it
// is maximal where the bevel is steep (rim) and exactly zero wherever the
// surface is flat (interior AND the skeleton ridge), which makes
// medial-axis seams impossible at any thickness. Spread shapes how tightly
// the curvature hugs the rim.
kernel void BuildBevelHeightKernel(
    device const float *bevelField [[buffer(0)]],
    device float *heightMap [[buffer(1)]],
    constant LiquidGlassKernelParams &params [[buffer(2)]],
    uint2 xy [[thread_position_in_grid]])
{
    if (xy.x >= params.width || xy.y >= params.height) {
        return;
    }

    const int index = int(xy.y) * int(params.width) + int(xy.x);
    const float spread = clamp(params.spread / 100.0f, 0.0f, 1.0f);
    const float k = 0.5f + spread * 3.5f;
    const float edge = clamp(bevelField[index], 0.0f, 1.0f);
    const float t = pow(edge, k);
    heightMap[index] = sqrt(max(1.0f - t * t, 0.0f));
}

static float MatteTapAt(
    device const float *matte,
    constant LiquidGlassKernelParams &params,
    int x,
    int y)
{
    if (x < 0 || y < 0 || x >= int(params.width) || y >= int(params.height)) {
        return 0.0f;
    }
    return matte[y * int(params.width) + x];
}

// Shadow/caustics field: the matte re-sampled at an angle/distance offset,
// bilinear, zero outside bounds. Run once per offset (inner shadow; outer
// shadow and caustics share one offset) then softened by the same separable
// blur pair used for Roughness, clipped/blended in LiquidGlassKernel.
kernel void ShadowOffsetKernel(
    device const float *matte [[buffer(0)]],
    device float *outShadow [[buffer(1)]],
    constant LiquidGlassKernelParams &params [[buffer(2)]],
    constant float2 &offset [[buffer(3)]],
    uint2 xy [[thread_position_in_grid]])
{
    if (xy.x >= params.width || xy.y >= params.height) {
        return;
    }

    const float sx = float(xy.x) - offset.x;
    const float sy = float(xy.y) - offset.y;
    const int x0 = int(floor(sx));
    const int y0 = int(floor(sy));
    const float tx = sx - float(x0);
    const float ty = sy - float(y0);

    const float a00 = MatteTapAt(matte, params, x0, y0);
    const float a10 = MatteTapAt(matte, params, x0 + 1, y0);
    const float a01 = MatteTapAt(matte, params, x0, y0 + 1);
    const float a11 = MatteTapAt(matte, params, x0 + 1, y0 + 1);

    outShadow[int(xy.y) * int(params.width) + int(xy.x)] =
        mix(mix(a00, a10, tx), mix(a01, a11, tx), ty);
}

// Roughness: separable gaussian over the background world (float4 pixels).
kernel void BlurBackgroundHorizontalKernel(
    device const float4 *src [[buffer(0)]],
    device float4 *dst [[buffer(1)]],
    constant LiquidGlassKernelParams &params [[buffer(2)]],
    constant LiquidGlassPassParams &pass [[buffer(3)]],
    uint2 xy [[thread_position_in_grid]])
{
    if (xy.x >= params.sampleWidth || xy.y >= params.sampleHeight) {
        return;
    }

    const int x = int(xy.x);
    const int y = int(xy.y);
    const int radius = max(pass.radius, 1);
    const float sigma = pass.sigma;
    float4 sum = float4(0.0f);
    float weightSum = 0.0f;

    for (int dx = -radius; dx <= radius; ++dx) {
        const int sx = clamp(x + dx, 0, int(params.sampleWidth) - 1);
        const float weight = Gaussian(float(dx), sigma);
        sum += src[y * pass.srcPitch + sx] * weight;
        weightSum += weight;
    }

    dst[y * pass.dstPitch + x] = sum / weightSum;
}

kernel void BlurBackgroundVerticalKernel(
    device const float4 *src [[buffer(0)]],
    device float4 *dst [[buffer(1)]],
    constant LiquidGlassKernelParams &params [[buffer(2)]],
    constant LiquidGlassPassParams &pass [[buffer(3)]],
    uint2 xy [[thread_position_in_grid]])
{
    if (xy.x >= params.sampleWidth || xy.y >= params.sampleHeight) {
        return;
    }

    const int x = int(xy.x);
    const int y = int(xy.y);
    const int radius = max(pass.radius, 1);
    const float sigma = pass.sigma;
    float4 sum = float4(0.0f);
    float weightSum = 0.0f;

    for (int dy = -radius; dy <= radius; ++dy) {
        const int sy = clamp(y + dy, 0, int(params.sampleHeight) - 1);
        const float weight = Gaussian(float(dy), sigma);
        sum += src[sy * pass.srcPitch + x] * weight;
        weightSum += weight;
    }

    dst[y * pass.dstPitch + x] = sum / weightSum;
}

kernel void LiquidGlassKernel(
    device const float *matte [[buffer(0)]],
    device const float *bevelField [[buffer(1)]],
    device const float *heightMap [[buffer(2)]],
    device const float4 *samplePixels [[buffer(3)]],
    device float4 *dstPixels [[buffer(4)]],
    constant LiquidGlassKernelParams &params [[buffer(5)]],
    device const float *innerShadowField [[buffer(6)]],
    device const float4 *underPixels [[buffer(7)]],
    device const float *outerShadowField [[buffer(8)]],
    device const float *causticsField [[buffer(9)]],
    constant LiquidGlassShadowParams &shadowParams [[buffer(10)]],
    uint2 xy [[thread_position_in_grid]])
{
    if (xy.x >= params.width || xy.y >= params.height) {
        return;
    }

    const int x = int(xy.x);
    const int y = int(xy.y);
    const int index = y * int(params.width) + x;
    const int dstIndex = y * params.dstPitch + x;
    const float maskAlpha = matte[index];

    float4 color = float4(0.0f);

    if (maskAlpha > 0.0f) {
        // Displacement comes from the GRADIENT of the lens surface: magnitude
        // and direction together. The surface is flat on the interior plateau
        // and along the skeleton ridge, so displacement fades to zero there
        // smoothly — medial-axis mirror seams cannot occur at any thickness.
        const int xL = max(x - 1, 0);
        const int xR = min(x + 1, int(params.width) - 1);
        const int yU = max(y - 1, 0);
        const int yD = min(y + 1, int(params.height) - 1);
        const float zL = heightMap[y * int(params.width) + xL];
        const float zR = heightMap[y * int(params.width) + xR];
        const float zU = heightMap[yU * int(params.width) + x];
        const float zD = heightMap[yD * int(params.width) + x];
        const float2 slope = float2(zR - zL, zD - zU) * 0.5f; // points inward: z grows away from the rim

        // Outward normal for the lighting, from the proximity field.
        const float bL = bevelField[y * int(params.width) + xL];
        const float bR = bevelField[y * int(params.width) + xR];
        const float bU = bevelField[yU * int(params.width) + x];
        const float bD = bevelField[yD * int(params.width) + x];
        const float2 fieldGradient = float2(bR - bL, bD - bU) * 0.5f;
        const float2 outward = (dot(fieldGradient, fieldGradient) > 0.000001f) ? normalize(fieldGradient) : float2(0.0f, 0.0f);

        const float zoom = max(params.zoom / 100.0f, 0.001f);
        const float2 pos = float2(float(params.outputLeft + x), float(params.outputTop + y));
        const float2 center = float2(params.zoomCenterX, params.zoomCenterY);
        const float2 zoomed = center + (pos - center) / zoom;

        const float h = heightMap[index];
        // Physical glass is infinitely thin at its outer edge: fading the
        // displacement across the antialiased boundary keeps rim pixels
        // blending with the true backdrop instead of showing displaced
        // content (this rendered as a dark 1px outline before).
        const float edgeFade = smoothstep(0.0f, 0.9f, maskAlpha);
        const float2 displacement = slope * (params.refraction * 3.0f) * edgeFade;
        const float displacementLength = length(displacement);
        const float blurRadius = params.edgeBlur * clamp(1.0f - h, 0.0f, 1.0f);
        const float dispersion = clamp(params.dispersion / 100.0f, 0.0f, 1.0f);

        float4 glass;
        if (dispersion > 0.001f && displacementLength > 0.001f) {
            // Chromatic dispersion: each channel refracts with a slightly
            // different strength (blue bends most). The separation is kept
            // small — a few percent of the displacement — so it reads as
            // narrow fringes in the fold, not rainbow bands.
            const float4 sR = SampleThroughMatrixBlurred(samplePixels, params, zoomed + displacement * (1.0f - 0.10f * dispersion), blurRadius);
            const float4 sG = SampleThroughMatrixBlurred(samplePixels, params, zoomed + displacement, blurRadius);
            const float4 sB = SampleThroughMatrixBlurred(samplePixels, params, zoomed + displacement * (1.0f + 0.10f * dispersion), blurRadius);
            glass = float4(sB.x, sG.y, sR.z, (sR.w + sG.w + sB.w) / 3.0f);
        } else {
            glass = SampleThroughMatrixBlurred(samplePixels, params, zoomed + displacement, blurRadius);
        }

        // Outer shadow & caustics live on the backdrop: sample their fields
        // at the REFRACTED position (nearest-pixel, like the CUDA/DX port)
        // so the glass visibly bends them, and blend onto the sampled
        // background color before glass coverage is applied.
        {
            const int fx = clamp(int(zoomed.x + displacement.x - float(params.outputLeft) + 0.5f), 0, int(params.width) - 1);
            const int fy = clamp(int(zoomed.y + displacement.y - float(params.outputTop) + 0.5f), 0, int(params.height) - 1);
            const int fIdx = fy * int(params.width) + fx;
            const float sAmt = clamp(outerShadowField[fIdx], 0.0f, 1.0f) * clamp(shadowParams.outerShadowIntensity / 100.0f, 0.0f, 1.0f);
            const float cAmt = clamp(causticsField[fIdx], 0.0f, 1.0f) * clamp(shadowParams.causticsIntensity / 100.0f, 0.0f, 1.0f);
            if ((sAmt > 0.0001f || cAmt > 0.0001f) && glass.w > 0.0001f) {
                float3 g = glass.xyz / max(glass.w, 0.0001f);
                if (sAmt > 0.0001f) {
                    const float3 outerColor = float3(shadowParams.outerShadowBlue, shadowParams.outerShadowGreen, shadowParams.outerShadowRed);
                    g = mix(g, ShadowBlend(g, outerColor, shadowParams.outerShadowMode), sAmt);
                }
                if (cAmt > 0.0001f) {
                    const float3 causticsColor = float3(shadowParams.causticsBlue, shadowParams.causticsGreen, shadowParams.causticsRed);
                    g = mix(g, ShadowBlend(g, causticsColor, shadowParams.causticsMode), cAmt);
                }
                glass.xyz = g * glass.w;
            }
        }

        // Premultiplied: scaling the whole sample by the mask coverage keeps
        // RGB <= A and avoids fringing at soft mask edges.
        glass *= maskAlpha;
        color = glass;

        const float tintAlpha = maskAlpha * clamp(params.tintOpacity / 100.0f, 0.0f, 1.0f);
        color = SourceOver(PremultipliedColor(params.tintRed, params.tintGreen, params.tintBlue, tintAlpha), color);

        // Light is the topmost layer (above tint), bi-directional like
        // Figma's glass: a soft directional base kept at 35% strength, plus a
        // hard glossy rim reflection on a tighter band. Both are composited
        // in Screen mode so highlights brighten without clipping the glass.
        // Bands are shaped by the edge-proximity field, so they track
        // Thickness and stay visible regardless of the Spread curve.
        const float fieldE = clamp(bevelField[index], 0.0f, 1.0f);
        const float lit = dot(outward, float2(params.lightDirX, params.lightDirY));
        const float intensity = clamp(params.lightIntensity / 100.0f, 0.0f, 2.0f);
        // Soft directional base: gentle, spread over the band.
        const float softBand = smoothstep(0.3f, 0.9f, fieldE);
        const float softLight = 0.25f * (pow(clamp(lit, 0.0f, 1.0f), 2.5f) +
                                         0.35f * pow(clamp(-lit, 0.0f, 1.0f), 2.5f)) * softBand;
        // Clear coat: a thin hard stroke hugging the very boundary, with
        // sharp angular cutoffs — reads as a specular streak on the glass
        // edge. Width is user-controlled in pixels (0 disables it).
        const float rimDistance = (1.0f - fieldE) * max(params.thickness, 1.0f);
        const float stroke = (params.strokeWidth > 0.25f)
            ? 1.0f - smoothstep(params.strokeWidth * 0.45f, params.strokeWidth, rimDistance)
            : 0.0f;
        const float gloss = 0.95f * stroke * (smoothstep(0.35f, 0.55f, lit) +
                                              0.6f * smoothstep(0.35f, 0.55f, -lit));
        const float lightAmount = clamp(intensity * (softLight + gloss) * maskAlpha, 0.0f, 0.95f);
        if (lightAmount > 0.0001f && color.w > 0.0001f) {
            // Screen blend in straight-alpha space, then re-premultiply.
            float3 base = color.xyz / max(color.w, 0.0001f);
            const float3 lightRGB = float3(params.lightBlue, params.lightGreen, params.lightRed) * lightAmount;
            base = 1.0f - (1.0f - base) * (1.0f - lightRGB);
            color.xyz = base * color.w;
        }

        // Inner shadow: the offset+blurred matte, clipped to the glass,
        // blended over the composed glass color.
        const float shadowA = clamp(innerShadowField[index], 0.0f, 1.0f)
                             * clamp(shadowParams.shadowOpacity / 100.0f, 0.0f, 1.0f) * maskAlpha;
        if (shadowA > 0.0001f && color.w > 0.0001f) {
            const float3 shadowBase = color.xyz / max(color.w, 0.0001f);
            const float3 shadowColor = float3(shadowParams.shadowBlue, shadowParams.shadowGreen, shadowParams.shadowRed);
            const float3 shadowBlend = ShadowBlend(shadowBase, shadowColor, shadowParams.shadowMode);
            color.xyz = mix(shadowBase, shadowBlend, shadowA) * color.w;
        }
    }

    // Outer shadow & caustics around the shape, at this pixel's own
    // (unrefracted) position -- runs regardless of maskAlpha since they
    // extend past the glass silhouette.
    const float ownShadowAmt = clamp(outerShadowField[index], 0.0f, 1.0f) * clamp(shadowParams.outerShadowIntensity / 100.0f, 0.0f, 1.0f);
    const float ownCausticAmt = clamp(causticsField[index], 0.0f, 1.0f) * clamp(shadowParams.causticsIntensity / 100.0f, 0.0f, 1.0f);

    if (shadowParams.compositeOnTop != 0) {
        // Adjustment-layer mode: we own the underlying, so the blends are
        // exact. Shadow/caustics apply to the surface, then the glass
        // composites over it.
        const float2 ownPos = float2(float(params.outputLeft + x), float(params.outputTop + y));
        float4 under = SampleUnderThroughMatrix(underPixels, params, shadowParams.underPitch, ownPos);
        if ((ownShadowAmt > 0.0001f || ownCausticAmt > 0.0001f) && under.w > 0.0001f) {
            float3 u = under.xyz / max(under.w, 0.0001f);
            if (ownShadowAmt > 0.0001f) {
                const float3 outerColor = float3(shadowParams.outerShadowBlue, shadowParams.outerShadowGreen, shadowParams.outerShadowRed);
                u = mix(u, ShadowBlend(u, outerColor, shadowParams.outerShadowMode), ownShadowAmt);
            }
            if (ownCausticAmt > 0.0001f) {
                const float3 causticsColor = float3(shadowParams.causticsBlue, shadowParams.causticsGreen, shadowParams.causticsRed);
                u = mix(u, ShadowBlend(u, causticsColor, shadowParams.causticsMode), ownCausticAmt);
            }
            under.xyz = u * under.w;
        }
        color = SourceOver(color, under);
    } else if (!shadowParams.confineToBounds && (ownShadowAmt > 0.0001f || ownCausticAmt > 0.0001f)) {
        // Shape-layer mode: we don't own the pixels below, so paint
        // translucent color under the glass (native Drop Shadow style):
        // caustics over shadow, then the glass over both. This can't honor
        // blend modes (no real backdrop to blend against) and shows as a
        // duplicate offset-and-blurred copy of the shape's own silhouette --
        // Confine to Layer Bounds skips it entirely instead.
        const float paintA = ownCausticAmt + ownShadowAmt * (1.0f - ownCausticAmt);
        const float3 causticsColor = float3(shadowParams.causticsBlue, shadowParams.causticsGreen, shadowParams.causticsRed);
        const float3 outerColor = float3(shadowParams.outerShadowBlue, shadowParams.outerShadowGreen, shadowParams.outerShadowRed);
        const float3 paintRGB = causticsColor * ownCausticAmt + outerColor * ownShadowAmt * (1.0f - ownCausticAmt);
        color = SourceOver(color, float4(paintRGB, paintA));
    }

    dstPixels[dstIndex] = color;
}
)METAL";

struct ScopedAutoreleasePool
{
    ScopedAutoreleasePool()
    : pool([[NSAutoreleasePool alloc] init])
    {
    }

    ~ScopedAutoreleasePool()
    {
        [pool release];
    }

    NSAutoreleasePool *pool;
};
#endif

extern "C" {
    DllExport PF_Err EffectMain(
        PF_Cmd cmd,
        PF_InData *in_data,
        PF_OutData *out_data,
        PF_ParamDef *params[],
        PF_LayerDef *output,
        void *extra);
}

#endif
