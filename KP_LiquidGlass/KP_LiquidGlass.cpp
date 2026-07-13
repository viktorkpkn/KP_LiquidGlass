/*
** KP_LiquidGlass — Copyright (c) 2026 Viktor Kopeikin.
** Licensed under the PolyForm Noncommercial License 1.0.0.
** <https://polyformproject.org/licenses/noncommercial/1.0.0>
*/

#include "KP_LiquidGlass.h"
#include "AEGP_SuiteHandler.h"
#include "AEGP_SuiteHandler.cpp"
#include "MissingSuiteError.cpp"
#include "AE_GeneralPlug.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

// Dev-time diagnostics; disabled for release builds.
#define KP_LIQUIDGLASS_ENABLE_LOG 1

#if KP_LIQUIDGLASS_ENABLE_LOG
static void KPLog(const char *format, ...)
{
#ifdef _WIN32
    char path[512];
    const char *tmp = getenv("TEMP");
    snprintf(path, sizeof(path), "%s\\KP_LiquidGlass.log", tmp ? tmp : "C:\\tmp");
    FILE *file = fopen(path, "a");
#else
    FILE *file = fopen("/tmp/KP_LiquidGlass.log", "a");
#endif
    if (!file) {
        return;
    }
    va_list args;
    va_start(args, format);
    vfprintf(file, format, args);
    va_end(args);
    fputc('\n', file);
    fclose(file);
}
#else
static inline void KPLog(const char *, ...) {}
#endif

static AEGP_PluginID s_aegpPluginID = 0;

static PF_Err About(
    PF_InData *in_data,
    PF_OutData *out_data,
    PF_ParamDef *params[],
    PF_LayerDef *output)
{
    PF_SPRINTF(out_data->return_msg,
               "%s v%d.%d\r%s\r%s",
               NAME,
               MAJOR_VERSION,
               MINOR_VERSION,
               DESCRIPTION,
               AUTHOR_NOTE);
    return PF_Err_NONE;
}

static PF_Err GlobalSetup(
    PF_InData *in_data,
    PF_OutData *out_data,
    PF_ParamDef *params[],
    PF_LayerDef *output)
{
    PF_Err err = PF_Err_NONE;

    out_data->my_version = PF_VERSION(
        MAJOR_VERSION,
        MINOR_VERSION,
        BUG_VERSION,
        STAGE_VERSION,
        BUILD_VERSION);

    out_data->out_flags = PF_OutFlag_DEEP_COLOR_AWARE |
                          PF_OutFlag_NON_PARAM_VARY |
                          PF_OutFlag_WIDE_TIME_INPUT;
    // PF_OutFlag2_SUPPORTS_THREADED_RENDERING is intentionally NOT set:
    // PreRender resolves layer transforms through AEGP suites, which are not
    // safe to call from concurrent render threads.
    out_data->out_flags2 = PF_OutFlag2_FLOAT_COLOR_AWARE |
                           PF_OutFlag2_SUPPORTS_SMART_RENDER |
                           PF_OutFlag2_SUPPORTS_GPU_RENDER_F32 |
                           PF_OutFlag2_AUTOMATIC_WIDE_TIME_INPUT |
                           PF_OutFlag2_I_MIX_GUID_DEPENDENCIES;

    if (!s_aegpPluginID && in_data && in_data->pica_basicP) {
        AEGP_SuiteHandler suites(in_data->pica_basicP);
        ERR(suites.UtilitySuite6()->AEGP_RegisterWithAEGP(
            nullptr,
            "KP_LiquidGlass",
            &s_aegpPluginID));
    }

    return err;
}

static PF_Err AddCheckboxParam(
    PF_InData *in_data,
    A_long index,
    A_long stableId,
    const char *name,
    const char *label,
    PF_Boolean defaultValue)
{
    PF_ParamDef def;
    AEFX_CLR_STRUCT(def);
    def.param_type = PF_Param_CHECKBOX;
    PF_STRCPY(def.PF_DEF_NAME, name);
    def.u.bd.dephault = defaultValue;
    def.u.bd.value = defaultValue;
    def.u.bd.u.PF_DEF_NAMEPTR = label;
    def.flags = PF_ParamFlag_CANNOT_TIME_VARY;
    def.uu.id = stableId;
    return PF_ADD_PARAM(in_data, index, &def);
}

static PF_Err AddFloatSliderParam(
    PF_InData *in_data,
    A_long index,
    A_long stableId,
    const char *name,
    float validMin,
    float validMax,
    float sliderMin,
    float sliderMax,
    float defaultValue,
    PF_ValueDisplayFlags displayFlags)
{
    PF_ParamDef def;
    AEFX_CLR_STRUCT(def);
    def.param_type = PF_Param_FLOAT_SLIDER;
    PF_STRCPY(def.PF_DEF_NAME, name);
    def.u.fs_d.valid_min = validMin;
    def.u.fs_d.valid_max = validMax;
    def.u.fs_d.slider_min = sliderMin;
    def.u.fs_d.slider_max = sliderMax;
    def.u.fs_d.value = defaultValue;
    def.u.fs_d.dephault = defaultValue;
    def.u.fs_d.precision = 1;
    def.u.fs_d.display_flags = displayFlags;
    def.u.fs_d.curve_tolerance = 0.0f;
    def.uu.id = stableId;
    return PF_ADD_PARAM(in_data, index, &def);
}

static PF_Err AddColorParam(
    PF_InData *in_data,
    A_long index,
    A_long stableId,
    const char *name,
    A_u_char red,
    A_u_char green,
    A_u_char blue)
{
    PF_ParamDef def;
    AEFX_CLR_STRUCT(def);
    def.param_type = PF_Param_COLOR;
    PF_STRCPY(def.PF_DEF_NAME, name);
    def.u.cd.dephault.alpha = 255;
    def.u.cd.dephault.red = red;
    def.u.cd.dephault.green = green;
    def.u.cd.dephault.blue = blue;
    def.u.cd.value = def.u.cd.dephault;
    def.uu.id = stableId;
    return PF_ADD_PARAM(in_data, index, &def);
}

static PF_Err AddAngleParam(
    PF_InData *in_data,
    A_long index,
    A_long stableId,
    const char *name,
    float defaultDegrees)
{
    PF_ParamDef def;
    AEFX_CLR_STRUCT(def);
    def.param_type = PF_Param_ANGLE;
    PF_STRCPY(def.PF_DEF_NAME, name);
    def.u.ad.dephault = (A_long)(defaultDegrees * 65536.0f);
    def.u.ad.value = def.u.ad.dephault;
    def.uu.id = stableId;
    return PF_ADD_PARAM(in_data, index, &def);
}

static PF_Err AddPopupParam(
    PF_InData *in_data,
    A_long index,
    A_long stableId,
    const char *name,
    const char *choices,
    A_short numChoices,
    A_short defaultChoice)
{
    PF_ParamDef def;
    AEFX_CLR_STRUCT(def);
    def.param_type = PF_Param_POPUP;
    PF_STRCPY(def.PF_DEF_NAME, name);
    def.u.pd.num_choices = numChoices;
    def.u.pd.dephault = defaultChoice;
    def.u.pd.value = defaultChoice;
    def.u.pd.u.namesptr = choices;
    def.uu.id = stableId;
    return PF_ADD_PARAM(in_data, index, &def);
}

static PF_Err AddGroupStartParam(
    PF_InData *in_data,
    A_long index,
    A_long stableId,
    const char *name)
{
    PF_ParamDef def;
    AEFX_CLR_STRUCT(def);
    def.param_type = PF_Param_GROUP_START;
    PF_STRCPY(def.PF_DEF_NAME, name);
    def.flags = PF_ParamFlag_START_COLLAPSED;
    def.uu.id = stableId;
    return PF_ADD_PARAM(in_data, index, &def);
}

static PF_Err AddGroupEndParam(
    PF_InData *in_data,
    A_long index,
    A_long stableId)
{
    PF_ParamDef def;
    AEFX_CLR_STRUCT(def);
    def.param_type = PF_Param_GROUP_END;
    def.uu.id = stableId;
    return PF_ADD_PARAM(in_data, index, &def);
}

static float ColorByteToFloat(A_u_char value)
{
    return float(value) / 255.0f;
}

static PF_Err ParamsSetup(
    PF_InData *in_data,
    PF_OutData *out_data,
    PF_ParamDef *params[],
    PF_LayerDef *output)
{
    PF_Err err = PF_Err_NONE;
    PF_ParamDef def;

    ERR(AddCheckboxParam(
        in_data, KP_LIQUIDGLASS_ADJUSTMENT_MODE, KP_ID_ADJUSTMENT_MODE,
        "Adjustment Mode", "Sample underlying layers", FALSE));

    AEFX_CLR_STRUCT(def);
    def.param_type = PF_Param_LAYER;
    PF_STRCPY(def.PF_DEF_NAME, "Background Layer");
    def.u.ld.dephault = PF_LayerDefault_NONE;
    def.uu.id = KP_ID_BACKGROUND_LAYER;
    ERR(PF_ADD_PARAM(in_data, KP_LIQUIDGLASS_BACKGROUND_LAYER, &def));

    ERR(AddCheckboxParam(
        in_data, KP_LIQUIDGLASS_EXTEND_BACKGROUND, KP_ID_EXTEND_BACKGROUND,
        "Background Edges", "Extend edge pixels", FALSE));

    // Optional explicit matte source: enables the adjustment-layer workflow
    // (input = composite below) with the glass shape taken from any layer.
    AEFX_CLR_STRUCT(def);
    def.param_type = PF_Param_LAYER;
    PF_STRCPY(def.PF_DEF_NAME, "Glass Shape");
    def.u.ld.dephault = PF_LayerDefault_NONE;
    def.uu.id = KP_ID_SHAPE_LAYER;
    ERR(PF_ADD_PARAM(in_data, KP_LIQUIDGLASS_SHAPE_LAYER, &def));

    ERR(AddCheckboxParam(
        in_data, KP_LIQUIDGLASS_COMPOSITE_ON_TOP, KP_ID_COMPOSITE_ON_TOP,
        "Underlying Composite", "Composite on top", FALSE));

    ERR(AddFloatSliderParam(
        in_data, KP_LIQUIDGLASS_REFRACTION, KP_ID_REFRACTION, "Refraction",
        REFRACTION_MIN_VALUE, REFRACTION_MAX_VALUE,
        REFRACTION_MIN_SLIDER, REFRACTION_MAX_SLIDER,
        REFRACTION_DFLT, PF_ValueDisplayFlag_PERCENT));

    ERR(AddFloatSliderParam(
        in_data, KP_LIQUIDGLASS_SOFTNESS, KP_ID_SOFTNESS, "Softness",
        SOFTNESS_MIN_VALUE, SOFTNESS_MAX_VALUE,
        SOFTNESS_MIN_SLIDER, SOFTNESS_MAX_SLIDER,
        SOFTNESS_DFLT, PF_ValueDisplayFlag_NONE));

    ERR(AddFloatSliderParam(
        in_data, KP_LIQUIDGLASS_THICKNESS, KP_ID_THICKNESS, "Thickness",
        THICKNESS_MIN_VALUE, THICKNESS_MAX_VALUE,
        THICKNESS_MIN_SLIDER, THICKNESS_MAX_SLIDER,
        THICKNESS_DFLT, PF_ValueDisplayFlag_NONE));

    ERR(AddFloatSliderParam(
        in_data, KP_LIQUIDGLASS_SPREAD, KP_ID_SPREAD, "Spread",
        SPREAD_MIN_VALUE, SPREAD_MAX_VALUE,
        SPREAD_MIN_SLIDER, SPREAD_MAX_SLIDER,
        SPREAD_DFLT, PF_ValueDisplayFlag_PERCENT));

    ERR(AddFloatSliderParam(
        in_data, KP_LIQUIDGLASS_ZOOM, KP_ID_ZOOM, "Zoom",
        ZOOM_MIN_VALUE, ZOOM_MAX_VALUE,
        ZOOM_MIN_SLIDER, ZOOM_MAX_SLIDER,
        ZOOM_DFLT, PF_ValueDisplayFlag_PERCENT));

    ERR(AddFloatSliderParam(
        in_data, KP_LIQUIDGLASS_EDGE_BLUR, KP_ID_EDGE_BLUR, "Edge Blur",
        EDGE_BLUR_MIN_VALUE, EDGE_BLUR_MAX_VALUE,
        EDGE_BLUR_MIN_SLIDER, EDGE_BLUR_MAX_SLIDER,
        EDGE_BLUR_DFLT, PF_ValueDisplayFlag_NONE));

    ERR(AddFloatSliderParam(
        in_data, KP_LIQUIDGLASS_ROUGHNESS, KP_ID_ROUGHNESS, "Roughness",
        ROUGHNESS_MIN_VALUE, ROUGHNESS_MAX_VALUE,
        ROUGHNESS_MIN_SLIDER, ROUGHNESS_MAX_SLIDER,
        ROUGHNESS_DFLT, PF_ValueDisplayFlag_NONE));

    ERR(AddFloatSliderParam(
        in_data, KP_LIQUIDGLASS_DISPERSION, KP_ID_DISPERSION, "Dispersion",
        DISPERSION_MIN_VALUE, DISPERSION_MAX_VALUE,
        DISPERSION_MIN_SLIDER, DISPERSION_MAX_SLIDER,
        DISPERSION_DFLT, PF_ValueDisplayFlag_PERCENT));

    ERR(AddColorParam(in_data, KP_LIQUIDGLASS_TINT_COLOR, KP_ID_TINT_COLOR,
                      "Tint Color", 255, 255, 255));
    ERR(AddFloatSliderParam(
        in_data, KP_LIQUIDGLASS_TINT_OPACITY, KP_ID_TINT_OPACITY, "Tint Opacity",
        TINT_OPACITY_MIN_VALUE, TINT_OPACITY_MAX_VALUE,
        TINT_OPACITY_MIN_SLIDER, TINT_OPACITY_MAX_SLIDER,
        TINT_OPACITY_DFLT, PF_ValueDisplayFlag_PERCENT));

    // --- Lighting (collapsible) ---
    ERR(AddGroupStartParam(in_data, KP_LIQUIDGLASS_GROUP_LIGHTING_START,
                           KP_ID_GROUP_LIGHTING_START, "Lighting"));

    ERR(AddAngleParam(in_data, KP_LIQUIDGLASS_LIGHT_ANGLE, KP_ID_LIGHT_ANGLE,
                      "Light Angle", LIGHT_ANGLE_DFLT));
    ERR(AddColorParam(in_data, KP_LIQUIDGLASS_LIGHT_COLOR, KP_ID_LIGHT_COLOR,
                      "Light Color", 255, 255, 255));
    ERR(AddFloatSliderParam(
        in_data, KP_LIQUIDGLASS_LIGHT_INTENSITY, KP_ID_LIGHT_INTENSITY, "Light Intensity",
        LIGHT_INTENSITY_MIN_VALUE, LIGHT_INTENSITY_MAX_VALUE,
        LIGHT_INTENSITY_MIN_SLIDER, LIGHT_INTENSITY_MAX_SLIDER,
        LIGHT_INTENSITY_DFLT, PF_ValueDisplayFlag_PERCENT));
    ERR(AddFloatSliderParam(
        in_data, KP_LIQUIDGLASS_STROKE_WIDTH, KP_ID_STROKE_WIDTH, "Stroke Width",
        STROKE_WIDTH_MIN_VALUE, STROKE_WIDTH_MAX_VALUE,
        STROKE_WIDTH_MIN_SLIDER, STROKE_WIDTH_MAX_SLIDER,
        STROKE_WIDTH_DFLT, PF_ValueDisplayFlag_NONE));

    ERR(AddGroupEndParam(in_data, KP_LIQUIDGLASS_GROUP_LIGHTING_END,
                         KP_ID_GROUP_LIGHTING_END));

    // --- Shadows (collapsible): inner (baked drop shadow), outer shadow and
    // caustics through the glass (shared Angle/Distance offset). ---
    ERR(AddGroupStartParam(in_data, KP_LIQUIDGLASS_GROUP_SHADOWS_START,
                           KP_ID_GROUP_SHADOWS_START, "Shadows"));

    ERR(AddColorParam(in_data, KP_LIQUIDGLASS_SHADOW_COLOR, KP_ID_SHADOW_COLOR,
                      "Inner Shadow Color", 0, 0, 0));
    ERR(AddFloatSliderParam(
        in_data, KP_LIQUIDGLASS_SHADOW_OPACITY, KP_ID_SHADOW_OPACITY, "Inner Shadow Opacity",
        SHADOW_OPACITY_MIN_VALUE, SHADOW_OPACITY_MAX_VALUE,
        SHADOW_OPACITY_MIN_SLIDER, SHADOW_OPACITY_MAX_SLIDER,
        SHADOW_OPACITY_DFLT, PF_ValueDisplayFlag_PERCENT));
    ERR(AddAngleParam(in_data, KP_LIQUIDGLASS_SHADOW_DIRECTION, KP_ID_SHADOW_DIRECTION,
                      "Inner Shadow Direction", SHADOW_DIRECTION_DFLT));
    ERR(AddFloatSliderParam(
        in_data, KP_LIQUIDGLASS_SHADOW_DISTANCE, KP_ID_SHADOW_DISTANCE, "Inner Shadow Distance",
        SHADOW_DISTANCE_MIN_VALUE, SHADOW_DISTANCE_MAX_VALUE,
        SHADOW_DISTANCE_MIN_SLIDER, SHADOW_DISTANCE_MAX_SLIDER,
        SHADOW_DISTANCE_DFLT, PF_ValueDisplayFlag_NONE));
    ERR(AddFloatSliderParam(
        in_data, KP_LIQUIDGLASS_SHADOW_SOFTNESS, KP_ID_SHADOW_SOFTNESS, "Inner Shadow Softness",
        SHADOW_SOFTNESS_MIN_VALUE, SHADOW_SOFTNESS_MAX_VALUE,
        SHADOW_SOFTNESS_MIN_SLIDER, SHADOW_SOFTNESS_MAX_SLIDER,
        SHADOW_SOFTNESS_DFLT, PF_ValueDisplayFlag_NONE));
    ERR(AddPopupParam(in_data, KP_LIQUIDGLASS_SHADOW_MODE, KP_ID_SHADOW_MODE,
                      "Inner Shadow Mode",
                      "Normal|Multiply|Screen|Overlay|Soft Light|Hard Light",
                      KP_ShadowMode_Count, KP_ShadowMode_Multiply));

    // Outer shadow/caustics can't honor blend modes outside the shape's own
    // silhouette without real underlying pixels (shape-layer hosting, no
    // Composite on Top) -- they fall back to painting a visible offset copy
    // of the shape. This confines that fallback to the shape's interior.
    ERR(AddCheckboxParam(
        in_data, KP_LIQUIDGLASS_CONFINE_TO_BOUNDS, KP_ID_CONFINE_TO_BOUNDS,
        "Outer Shadow / Caustics", "Confine to Layer Bounds", FALSE));

    ERR(AddAngleParam(in_data, KP_LIQUIDGLASS_OUTER_ANGLE, KP_ID_OUTER_ANGLE,
                      "Outer Direction", OUTER_ANGLE_DFLT));
    ERR(AddFloatSliderParam(
        in_data, KP_LIQUIDGLASS_OUTER_DISTANCE, KP_ID_OUTER_DISTANCE, "Outer Distance",
        OUTER_DISTANCE_MIN_VALUE, OUTER_DISTANCE_MAX_VALUE,
        OUTER_DISTANCE_MIN_SLIDER, OUTER_DISTANCE_MAX_SLIDER,
        OUTER_DISTANCE_DFLT, PF_ValueDisplayFlag_NONE));

    ERR(AddColorParam(in_data, KP_LIQUIDGLASS_OUTER_SHADOW_COLOR, KP_ID_OUTER_SHADOW_COLOR,
                      "Outer Shadow Color", 0, 0, 0));
    ERR(AddFloatSliderParam(
        in_data, KP_LIQUIDGLASS_OUTER_SHADOW_INTENSITY, KP_ID_OUTER_SHADOW_INTENSITY,
        "Outer Shadow Intensity",
        OUTER_INTENSITY_MIN_VALUE, OUTER_INTENSITY_MAX_VALUE,
        OUTER_INTENSITY_MIN_SLIDER, OUTER_INTENSITY_MAX_SLIDER,
        OUTER_SHADOW_INTENSITY_DFLT, PF_ValueDisplayFlag_PERCENT));
    ERR(AddFloatSliderParam(
        in_data, KP_LIQUIDGLASS_OUTER_SHADOW_SPREAD, KP_ID_OUTER_SHADOW_SPREAD,
        "Outer Shadow Softness",
        OUTER_SPREAD_MIN_VALUE, OUTER_SPREAD_MAX_VALUE,
        OUTER_SPREAD_MIN_SLIDER, OUTER_SPREAD_MAX_SLIDER,
        OUTER_SHADOW_SPREAD_DFLT, PF_ValueDisplayFlag_NONE));
    ERR(AddPopupParam(in_data, KP_LIQUIDGLASS_OUTER_SHADOW_MODE, KP_ID_OUTER_SHADOW_MODE,
                      "Outer Shadow Mode",
                      "Normal|Multiply|Screen|Overlay|Soft Light|Hard Light",
                      KP_ShadowMode_Count, KP_ShadowMode_Multiply));

    ERR(AddColorParam(in_data, KP_LIQUIDGLASS_CAUSTICS_COLOR, KP_ID_CAUSTICS_COLOR,
                      "Caustics Color", 255, 255, 255));
    ERR(AddFloatSliderParam(
        in_data, KP_LIQUIDGLASS_CAUSTICS_INTENSITY, KP_ID_CAUSTICS_INTENSITY,
        "Caustics Intensity",
        OUTER_INTENSITY_MIN_VALUE, OUTER_INTENSITY_MAX_VALUE,
        OUTER_INTENSITY_MIN_SLIDER, OUTER_INTENSITY_MAX_SLIDER,
        CAUSTICS_INTENSITY_DFLT, PF_ValueDisplayFlag_PERCENT));
    ERR(AddFloatSliderParam(
        in_data, KP_LIQUIDGLASS_CAUSTICS_SPREAD, KP_ID_CAUSTICS_SPREAD,
        "Caustics Softness",
        OUTER_SPREAD_MIN_VALUE, OUTER_SPREAD_MAX_VALUE,
        OUTER_SPREAD_MIN_SLIDER, OUTER_SPREAD_MAX_SLIDER,
        CAUSTICS_SPREAD_DFLT, PF_ValueDisplayFlag_NONE));
    ERR(AddPopupParam(in_data, KP_LIQUIDGLASS_CAUSTICS_MODE, KP_ID_CAUSTICS_MODE,
                      "Caustics Mode",
                      "Normal|Multiply|Screen|Overlay|Soft Light|Hard Light",
                      KP_ShadowMode_Count, KP_ShadowMode_Overlay));

    ERR(AddGroupEndParam(in_data, KP_LIQUIDGLASS_GROUP_SHADOWS_END,
                         KP_ID_GROUP_SHADOWS_END));

    out_data->num_params = KP_LIQUIDGLASS_NUM_PARAMS;
    return err;
}

#if HAS_HLSL
static PF_Err DXErr(bool inSuccess)
{
    return inSuccess ? PF_Err_NONE : PF_Err_INTERNAL_STRUCT_DAMAGED;
}
#define DX_ERR(FUNC) ERR(DXErr(FUNC))
#include "DirectXUtils.h"

// Same order as the Metal pipeline enum; names match the kernel functions in
// KP_LiquidGlass_Kernel.cu (the offline-compiled .cso/.rs files must be
// shipped as DirectX_Assets/<name>.cso|.rs next to the .aex).
enum {
    KP_DXShader_PrepareMatte = 0,
    KP_DXShader_JFASeed,
    KP_DXShader_JFAStep,
    KP_DXShader_JFAResolve,
    KP_DXShader_BlurHorizontal,
    KP_DXShader_BlurVertical,
    KP_DXShader_BuildBevelHeight,
    KP_DXShader_BlurBackgroundHorizontal,
    KP_DXShader_BlurBackgroundVertical,
    KP_DXShader_ShadowOffset,
    KP_DXShader_Render,
    KP_DXShader_Count
};

static const wchar_t *kKP_DXShaderNames[KP_DXShader_Count] = {
    L"KPPrepareMatteKernel",
    L"KPJFASeedKernel",
    L"KPJFAStepKernel",
    L"KPJFAResolveKernel",
    L"KPBlurHorizontalKernel",
    L"KPBlurVerticalKernel",
    L"KPBuildBevelHeightKernel",
    L"KPBlurBackgroundHorizontalKernel",
    L"KPBlurBackgroundVerticalKernel",
    L"KPShadowOffsetKernel",
    L"KPLiquidGlassKernel"
};

struct DirectXGPUData
{
    DXContextPtr mContext;
    ShaderObjectPtr mShaders[KP_DXShader_Count];
};
#endif

#if HAS_METAL
static PF_Err NSErrorToPFErr(NSError *error)
{
    return error ? PF_Err_INTERNAL_STRUCT_DAMAGED : PF_Err_NONE;
}

enum {
    KP_MetalPipeline_PrepareMatte = 0,
    KP_MetalPipeline_JFASeed,
    KP_MetalPipeline_JFAStep,
    KP_MetalPipeline_JFAResolve,
    KP_MetalPipeline_BlurHorizontal,
    KP_MetalPipeline_BlurVertical,
    KP_MetalPipeline_BuildBevelHeight,
    KP_MetalPipeline_ShadowOffset,
    KP_MetalPipeline_BlurBackgroundHorizontal,
    KP_MetalPipeline_BlurBackgroundVertical,
    KP_MetalPipeline_Render,
    KP_MetalPipeline_Count
};

static const char *kKP_MetalKernelNames[KP_MetalPipeline_Count] = {
    "PrepareMatteKernel",
    "JFASeedKernel",
    "JFAStepKernel",
    "JFAResolveKernel",
    "BlurHorizontalKernel",
    "BlurVerticalKernel",
    "BuildBevelHeightKernel",
    "ShadowOffsetKernel",
    "BlurBackgroundHorizontalKernel",
    "BlurBackgroundVerticalKernel",
    "LiquidGlassKernel"
};

struct MetalGPUData
{
    id<MTLComputePipelineState> pipelines[KP_MetalPipeline_Count];
};
#endif

static void DisposePreRenderData(void *preRenderData)
{
    if (preRenderData) {
        free(preRenderData);
    }
}

static PF_Err CheckoutParamValue(
    PF_InData *in_data,
    PF_OutData *out_data,
    PF_ParamIndex index,
    PF_ParamDef *param)
{
    PF_Err err = PF_Err_NONE;
    ERR(PF_CHECKOUT_PARAM(
        in_data,
        index,
        in_data->current_time,
        in_data->time_step,
        in_data->time_scale,
        param));
    return err;
}

static PF_Err CheckinParamValue(PF_InData *in_data, PF_ParamDef *param)
{
    return (*(in_data)->inter.checkin_param)((in_data)->effect_ref, param);
}

// Parameter/transform state that feeds the render GUID. Deliberately excludes
// time and checkout rects: pixel dependencies are tracked by AE through the
// layer checkouts, so identical state at different times can share the cache.
// The compToSample matrix covers both this layer's and the background layer's
// transforms, which is what invalidates the cache while dragging layers.
struct KP_LiquidGlassGuidState
{
    A_long adjustmentMode;
    A_long hasBackground;
    A_long extendBackground;
    A_long hasShapeLayer;
    float strokeWidth;
    float compToMask00;
    float compToMask01;
    float compToMask02;
    float compToMask10;
    float compToMask11;
    float compToMask12;
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
    float compToSample00;
    float compToSample01;
    float compToSample02;
    float compToSample10;
    float compToSample11;
    float compToSample12;
    A_long compositeOnTop;
    A_long shadowMode;
    float shadowRed;
    float shadowGreen;
    float shadowBlue;
    float shadowOpacity;
    float shadowOffsetX;
    float shadowOffsetY;
    float shadowSoftness;
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
    A_long confineToBounds;
};

static PF_Err MixRenderStateIntoGuid(
    PF_InData *in_data,
    PF_PreRenderExtra *extra,
    const LiquidGlassRenderParams *renderParams)
{
    if (!in_data || !extra || !extra->cb || !extra->cb->GuidMixInPtr || !renderParams) {
        return PF_Err_NONE;
    }

    KP_LiquidGlassGuidState guidState;
    AEFX_CLR_STRUCT(guidState);

    guidState.adjustmentMode = renderParams->adjustmentMode;
    guidState.hasBackground = renderParams->hasBackground;
    guidState.extendBackground = renderParams->extendBackground;
    guidState.hasShapeLayer = renderParams->hasShapeLayer;
    guidState.strokeWidth = renderParams->strokeWidth;
    guidState.compToMask00 = renderParams->compToMask00;
    guidState.compToMask01 = renderParams->compToMask01;
    guidState.compToMask02 = renderParams->compToMask02;
    guidState.compToMask10 = renderParams->compToMask10;
    guidState.compToMask11 = renderParams->compToMask11;
    guidState.compToMask12 = renderParams->compToMask12;
    guidState.refraction = renderParams->refraction;
    guidState.softness = renderParams->softness;
    guidState.thickness = renderParams->thickness;
    guidState.spread = renderParams->spread;
    guidState.zoom = renderParams->zoom;
    guidState.roughness = renderParams->roughness;
    guidState.dispersion = renderParams->dispersion;
    guidState.edgeBlur = renderParams->edgeBlur;
    guidState.lightDirX = renderParams->lightDirX;
    guidState.lightDirY = renderParams->lightDirY;
    guidState.lightRed = renderParams->lightRed;
    guidState.lightGreen = renderParams->lightGreen;
    guidState.lightBlue = renderParams->lightBlue;
    guidState.lightIntensity = renderParams->lightIntensity;
    guidState.tintRed = renderParams->tintRed;
    guidState.tintGreen = renderParams->tintGreen;
    guidState.tintBlue = renderParams->tintBlue;
    guidState.tintOpacity = renderParams->tintOpacity;
    guidState.zoomCenterX = renderParams->zoomCenterX;
    guidState.zoomCenterY = renderParams->zoomCenterY;
    guidState.compToSample00 = renderParams->compToSample00;
    guidState.compToSample01 = renderParams->compToSample01;
    guidState.compToSample02 = renderParams->compToSample02;
    guidState.compositeOnTop = renderParams->compositeOnTop;
    guidState.shadowMode = renderParams->shadowMode;
    guidState.shadowRed = renderParams->shadowRed;
    guidState.shadowGreen = renderParams->shadowGreen;
    guidState.shadowBlue = renderParams->shadowBlue;
    guidState.shadowOpacity = renderParams->shadowOpacity;
    guidState.shadowOffsetX = renderParams->shadowOffsetX;
    guidState.shadowOffsetY = renderParams->shadowOffsetY;
    guidState.shadowSoftness = renderParams->shadowSoftness;
    guidState.compToSample10 = renderParams->compToSample10;
    guidState.compToSample11 = renderParams->compToSample11;
    guidState.compToSample12 = renderParams->compToSample12;
    guidState.outerShadowMode = renderParams->outerShadowMode;
    guidState.causticsMode = renderParams->causticsMode;
    guidState.outerOffsetX = renderParams->outerOffsetX;
    guidState.outerOffsetY = renderParams->outerOffsetY;
    guidState.outerShadowRed = renderParams->outerShadowRed;
    guidState.outerShadowGreen = renderParams->outerShadowGreen;
    guidState.outerShadowBlue = renderParams->outerShadowBlue;
    guidState.outerShadowIntensity = renderParams->outerShadowIntensity;
    guidState.outerShadowSpread = renderParams->outerShadowSpread;
    guidState.causticsRed = renderParams->causticsRed;
    guidState.causticsGreen = renderParams->causticsGreen;
    guidState.causticsBlue = renderParams->causticsBlue;
    guidState.causticsIntensity = renderParams->causticsIntensity;
    guidState.causticsSpread = renderParams->causticsSpread;
    guidState.confineToBounds = renderParams->confineToBounds;

    return extra->cb->GuidMixInPtr(
        in_data->effect_ref,
        static_cast<A_u_long>(sizeof(guidState)),
        &guidState);
}

// Computes the mapping from this layer's render coordinates to a referenced
// layer's render coordinates: effect layer -> comp (layerToWorld), then
// comp -> target layer (inverse layerToWorld), conjugated with the downsample
// factor. Composing the effect layer's own transform is what makes the glass
// sample stay fixed in comp space while the layer is dragged. Used both for
// the Background Layer (sample source) and the Glass Shape layer (matte).
//
// NOTE: AEGP suites are main-thread oriented; calling them from PreRender is
// the reason PF_OutFlag2_SUPPORTS_THREADED_RENDERING must stay unset.
static PF_Err TryComputeLayerSampleMatrix(
    PF_InData *in_data,
    float downsampleX,
    float downsampleY,
    A_long layerParamIndex,
    const char *label,
    float outCoeffs[6],
    float outPivot[2],
    bool *outPivotValid)
{
    if (outPivotValid) {
        *outPivotValid = false;
    }
    PF_Err err = PF_Err_NONE;
    PF_Err err2 = PF_Err_NONE;

    if (!in_data || !outCoeffs || !s_aegpPluginID || !in_data->pica_basicP) {
        KPLog("Xform(%s): missing prerequisites (pluginID=%ld)", label, (long)s_aegpPluginID);
        return PF_Err_INTERNAL_STRUCT_DAMAGED;
    }

    AEGP_SuiteHandler suites(in_data->pica_basicP);

    AEGP_EffectRefH effectH = nullptr;
    AEGP_StreamRefH bgStreamH = nullptr;
    const char *failedStage = nullptr;
#define KP_XFORM_STAGE(NAME, CALL) \
    do { if (!err) { err = (CALL); if (err && !failedStage) { failedStage = (NAME); } } } while (0)

    KP_XFORM_STAGE("GetNewEffectForEffect", suites.PFInterfaceSuite1()->AEGP_GetNewEffectForEffect(
        s_aegpPluginID,
        in_data->effect_ref,
        &effectH));

    KP_XFORM_STAGE("GetNewEffectStreamByIndex", suites.StreamSuite5()->AEGP_GetNewEffectStreamByIndex(
        s_aegpPluginID,
        effectH,
        layerParamIndex,
        &bgStreamH));

    AEGP_LayerIDVal bgLayerID = 0;
    if (!err) {
        A_Time streamTime;
        streamTime.value = in_data->current_time;
        streamTime.scale = in_data->time_scale;

        AEGP_StreamValue2 streamValue;
        AEFX_CLR_STRUCT(streamValue);
        KP_XFORM_STAGE("GetNewStreamValue", suites.StreamSuite5()->AEGP_GetNewStreamValue(
            s_aegpPluginID,
            bgStreamH,
            AEGP_LTimeMode_LayerTime,
            &streamTime,
            TRUE,
            &streamValue));
        if (!err) {
            bgLayerID = streamValue.val.layer_id;
            ERR2(suites.StreamSuite5()->AEGP_DisposeStreamValue(&streamValue));
        }
    }

    AEGP_LayerH effectLayerH = nullptr;
    AEGP_CompH compH = nullptr;
    AEGP_LayerH bgLayerH = nullptr;

    KP_XFORM_STAGE("GetEffectLayer", suites.PFInterfaceSuite1()->AEGP_GetEffectLayer(in_data->effect_ref, &effectLayerH));
    KP_XFORM_STAGE("GetLayerParentComp", suites.LayerSuite9()->AEGP_GetLayerParentComp(effectLayerH, &compH));
    KP_XFORM_STAGE("GetLayerFromLayerID", suites.LayerSuite9()->AEGP_GetLayerFromLayerID(compH, bgLayerID, &bgLayerH));

    A_Time compTime;
    AEFX_CLR_STRUCT(compTime);
    KP_XFORM_STAGE("ConvertEffectToCompTime", suites.PFInterfaceSuite1()->AEGP_ConvertEffectToCompTime(
        in_data->effect_ref,
        in_data->current_time,
        in_data->time_scale,
        &compTime));

    A_Matrix4 bgToWorld;
    A_Matrix4 effectToWorld;
    AEFX_CLR_STRUCT(bgToWorld);
    AEFX_CLR_STRUCT(effectToWorld);
    KP_XFORM_STAGE("GetLayerToWorldXform(bg)", suites.LayerSuite9()->AEGP_GetLayerToWorldXform(bgLayerH, &compTime, &bgToWorld));
    KP_XFORM_STAGE("GetLayerToWorldXform(effect)", suites.LayerSuite9()->AEGP_GetLayerToWorldXform(effectLayerH, &compTime, &effectToWorld));

    // Target layer's anchor point: the natural zoom pivot. Mapped through the
    // RAW layer-to-world matrix — even when the sampling matrix treats the
    // target's world as comp-space, the transform still says where the shape
    // visually sits (the checked-out world rect does not: comp-space worlds
    // span the whole comp plus overflow, so their rect center is meaningless).
    bool gotAnchor = false;
    double anchorX = 0.0;
    double anchorY = 0.0;
    if (!err && outPivot && outPivotValid) {
        AEGP_StreamRefH anchorStreamH = nullptr;
        if (suites.StreamSuite5()->AEGP_GetNewLayerStream(
                s_aegpPluginID, bgLayerH, AEGP_LayerStream_ANCHORPOINT, &anchorStreamH) == A_Err_NONE &&
            anchorStreamH) {
            AEGP_StreamValue2 anchorValue;
            AEFX_CLR_STRUCT(anchorValue);
            if (suites.StreamSuite5()->AEGP_GetNewStreamValue(
                    s_aegpPluginID, anchorStreamH, AEGP_LTimeMode_CompTime, &compTime, TRUE, &anchorValue) == A_Err_NONE) {
                anchorX = anchorValue.val.two_d.x;
                anchorY = anchorValue.val.two_d.y;
                gotAnchor = true;
                ERR2(suites.StreamSuite5()->AEGP_DisposeStreamValue(&anchorValue));
            }
            ERR2(suites.StreamSuite5()->AEGP_DisposeStream(anchorStreamH));
        }
    }

    // Layer flags settle which space the effect input actually lives in
    // (collapse/continuously-rasterized and adjustment layers get comp-space
    // buffers rather than source-space ones).
    AEGP_LayerFlags effectLayerFlags = AEGP_LayerFlag_NONE;
    AEGP_LayerFlags bgLayerFlags = AEGP_LayerFlag_NONE;
    if (!err) {
        suites.LayerSuite9()->AEGP_GetLayerFlags(effectLayerH, &effectLayerFlags);
        suites.LayerSuite9()->AEGP_GetLayerFlags(bgLayerH, &bgLayerFlags);
        if (bgLayerFlags & AEGP_LayerFlag_ADJUSTMENT_LAYER) {
            // Layer-param checkouts of adjustment-flagged layers return the
            // white solid source, NOT the layer's visible contents — a Glass
            // Shape pointing at such a layer yields an all-white matte (no
            // edges, so every rim effect goes dead).
            KPLog("Xform(%s) WARNING: target layer has the Adjustment switch on; "
                  "its checkout is a white solid, not its shape", label);
        }
    }

    // Collapsed/continuously-rasterized and adjustment layers receive their
    // input already in comp space, so the effect layer's own transform must
    // NOT be composed in — doing so double-applies it and sends the sample
    // coordinates far outside the background world (renders as a flat color).
    const bool effectInputIsCompSpace =
        (effectLayerFlags & (AEGP_LayerFlag_COLLAPSE | AEGP_LayerFlag_ADJUSTMENT_LAYER)) != 0;
    // Same rule for the referenced layer: if its checked-out world is comp
    // space, its own transform must not be inverted out.
    const bool targetIsCompSpace =
        (bgLayerFlags & (AEGP_LayerFlag_COLLAPSE | AEGP_LayerFlag_ADJUSTMENT_LAYER)) != 0;

    if (!err) {
        // 2D affine slices, row-vector convention:
        // compX = x*m00 + y*m10 + tx; compY = x*m01 + y*m11 + ty
        const double e00 = effectInputIsCompSpace ? 1.0 : effectToWorld.mat[0][0];
        const double e01 = effectInputIsCompSpace ? 0.0 : effectToWorld.mat[0][1];
        const double e10 = effectInputIsCompSpace ? 0.0 : effectToWorld.mat[1][0];
        const double e11 = effectInputIsCompSpace ? 1.0 : effectToWorld.mat[1][1];
        const double etx = effectInputIsCompSpace ? 0.0 : effectToWorld.mat[3][0];
        const double ety = effectInputIsCompSpace ? 0.0 : effectToWorld.mat[3][1];

        const double b00 = targetIsCompSpace ? 1.0 : bgToWorld.mat[0][0];
        const double b01 = targetIsCompSpace ? 0.0 : bgToWorld.mat[0][1];
        const double b10 = targetIsCompSpace ? 0.0 : bgToWorld.mat[1][0];
        const double b11 = targetIsCompSpace ? 1.0 : bgToWorld.mat[1][1];
        const double btx = targetIsCompSpace ? 0.0 : bgToWorld.mat[3][0];
        const double bty = targetIsCompSpace ? 0.0 : bgToWorld.mat[3][1];

        const double det = (b00 * b11) - (b10 * b01);
        if (fabs(det) < 1.0e-8) {
            err = PF_Err_INTERNAL_STRUCT_DAMAGED;
            failedStage = "singular background matrix";
        } else {
            const double i00 =  b11 / det;
            const double i01 = -b01 / det;
            const double i10 = -b10 / det;
            const double i11 =  b00 / det;
            const double itx = -(btx * i00 + bty * i10);
            const double ity = -(btx * i01 + bty * i11);

            // Kernel form: sampleX = c00*x + c01*y + c02 (x, y in this
            // layer's coordinates), composed as effect->comp->background.
            double c00 = e00 * i00 + e01 * i10;
            double c01 = e10 * i00 + e11 * i10;
            double c02 = etx * i00 + ety * i10 + itx;
            double c10 = e00 * i01 + e01 * i11;
            double c11 = e10 * i01 + e11 * i11;
            double c12 = etx * i01 + ety * i11 + ity;

            // Layer-to-world matrices are in full-resolution coordinates, but
            // both worlds are rendered downsampled; conjugate with the scale.
            const double dsx = (downsampleX > 0.0f) ? downsampleX : 1.0;
            const double dsy = (downsampleY > 0.0f) ? downsampleY : 1.0;
            c01 *= dsx / dsy;
            c02 *= dsx;
            c10 *= dsy / dsx;
            c12 *= dsy;

            outCoeffs[0] = float(c00);
            outCoeffs[1] = float(c01);
            outCoeffs[2] = float(c02);
            outCoeffs[3] = float(c10);
            outCoeffs[4] = float(c11);
            outCoeffs[5] = float(c12);

            if (gotAnchor && outPivot && outPivotValid) {
                // anchor -> comp through the raw target matrix, then comp ->
                // effect input space (inverse of the effect-side matrix),
                // then conjugate to render resolution.
                const double pivotCompX = anchorX * bgToWorld.mat[0][0] + anchorY * bgToWorld.mat[1][0] + bgToWorld.mat[3][0];
                const double pivotCompY = anchorX * bgToWorld.mat[0][1] + anchorY * bgToWorld.mat[1][1] + bgToWorld.mat[3][1];
                const double effectDet = e00 * e11 - e01 * e10;
                if (fabs(effectDet) > 1.0e-8) {
                    const double relX = pivotCompX - etx;
                    const double relY = pivotCompY - ety;
                    const double pivotX = (relX * e11 - relY * e10) / effectDet;
                    const double pivotY = (-relX * e01 + relY * e00) / effectDet;
                    outPivot[0] = float(pivotX * dsx);
                    outPivot[1] = float(pivotY * dsy);
                    *outPivotValid = true;
                }
            }

            KPLog("Xform(%s) OK compSpace=%d/%d targetID=%ld effFlags=0x%lx targetFlags=0x%lx "
                  "M=[%.4f %.4f %.1f | %.4f %.4f %.1f]",
                  label,
                  effectInputIsCompSpace ? 1 : 0, targetIsCompSpace ? 1 : 0,
                  (long)bgLayerID, (unsigned long)effectLayerFlags, (unsigned long)bgLayerFlags,
                  c00, c01, c02, c10, c11, c12);
        }
    }

    if (err) {
        KPLog("Xform(%s) FAILED stage=\"%s\" err=%ld targetID=%ld",
              label, failedStage ? failedStage : "?", (long)err, (long)bgLayerID);
    }
#undef KP_XFORM_STAGE

    if (bgStreamH) {
        ERR2(suites.StreamSuite5()->AEGP_DisposeStream(bgStreamH));
    }
    if (effectH) {
        ERR2(suites.EffectSuite4()->AEGP_DisposeEffect(effectH));
    }

    return err;
}

static PF_Err CheckoutFloatSlider(
    PF_InData *in_data,
    PF_OutData *out_data,
    PF_ParamIndex index,
    float *valueOut)
{
    PF_Err err = PF_Err_NONE;
    PF_Err err2 = PF_Err_NONE;
    PF_ParamDef param;
    AEFX_CLR_STRUCT(param);

    ERR(CheckoutParamValue(in_data, out_data, index, &param));
    if (!err) {
        *valueOut = float(param.u.fs_d.value);
        ERR2(CheckinParamValue(in_data, &param));
    }
    return err ? err : err2;
}

static PF_Err CheckoutCheckbox(
    PF_InData *in_data,
    PF_OutData *out_data,
    PF_ParamIndex index,
    A_long *valueOut)
{
    PF_Err err = PF_Err_NONE;
    PF_Err err2 = PF_Err_NONE;
    PF_ParamDef param;
    AEFX_CLR_STRUCT(param);

    ERR(CheckoutParamValue(in_data, out_data, index, &param));
    if (!err) {
        *valueOut = param.u.bd.value ? 1 : 0;
        ERR2(CheckinParamValue(in_data, &param));
    }
    return err ? err : err2;
}

static PF_Err PreRender(
    PF_InData *in_data,
    PF_OutData *out_data,
    PF_PreRenderExtra *extra)
{
    PF_Err err = PF_Err_NONE;
    PF_Err err2 = PF_Err_NONE;

    LiquidGlassRenderParams *renderParams = reinterpret_cast<LiquidGlassRenderParams *>(malloc(sizeof(LiquidGlassRenderParams)));
    if (!renderParams) {
        return PF_Err_OUT_OF_MEMORY;
    }
    AEFX_CLR_STRUCT(*renderParams);

    ERR(CheckoutCheckbox(in_data, out_data, KP_LIQUIDGLASS_ADJUSTMENT_MODE, &renderParams->adjustmentMode));
    ERR(CheckoutCheckbox(in_data, out_data, KP_LIQUIDGLASS_EXTEND_BACKGROUND, &renderParams->extendBackground));

    ERR(CheckoutFloatSlider(in_data, out_data, KP_LIQUIDGLASS_REFRACTION, &renderParams->refraction));
    ERR(CheckoutFloatSlider(in_data, out_data, KP_LIQUIDGLASS_SOFTNESS, &renderParams->softness));
    ERR(CheckoutFloatSlider(in_data, out_data, KP_LIQUIDGLASS_THICKNESS, &renderParams->thickness));
    ERR(CheckoutFloatSlider(in_data, out_data, KP_LIQUIDGLASS_SPREAD, &renderParams->spread));
    ERR(CheckoutFloatSlider(in_data, out_data, KP_LIQUIDGLASS_ZOOM, &renderParams->zoom));
    ERR(CheckoutFloatSlider(in_data, out_data, KP_LIQUIDGLASS_EDGE_BLUR, &renderParams->edgeBlur));
    ERR(CheckoutFloatSlider(in_data, out_data, KP_LIQUIDGLASS_STROKE_WIDTH, &renderParams->strokeWidth));
    ERR(CheckoutFloatSlider(in_data, out_data, KP_LIQUIDGLASS_LIGHT_INTENSITY, &renderParams->lightIntensity));
    ERR(CheckoutFloatSlider(in_data, out_data, KP_LIQUIDGLASS_TINT_OPACITY, &renderParams->tintOpacity));
    ERR(CheckoutFloatSlider(in_data, out_data, KP_LIQUIDGLASS_ROUGHNESS, &renderParams->roughness));
    ERR(CheckoutFloatSlider(in_data, out_data, KP_LIQUIDGLASS_DISPERSION, &renderParams->dispersion));

    ERR(CheckoutCheckbox(in_data, out_data, KP_LIQUIDGLASS_COMPOSITE_ON_TOP, &renderParams->compositeOnTop));
    ERR(CheckoutCheckbox(in_data, out_data, KP_LIQUIDGLASS_CONFINE_TO_BOUNDS, &renderParams->confineToBounds));
    ERR(CheckoutFloatSlider(in_data, out_data, KP_LIQUIDGLASS_SHADOW_OPACITY, &renderParams->shadowOpacity));
    ERR(CheckoutFloatSlider(in_data, out_data, KP_LIQUIDGLASS_SHADOW_SOFTNESS, &renderParams->shadowSoftness));

    float shadowDistance = 0.0f;
    ERR(CheckoutFloatSlider(in_data, out_data, KP_LIQUIDGLASS_SHADOW_DISTANCE, &shadowDistance));

    if (!err) {
        PF_ParamDef shadowDirParam;
        AEFX_CLR_STRUCT(shadowDirParam);
        ERR(CheckoutParamValue(in_data, out_data, KP_LIQUIDGLASS_SHADOW_DIRECTION, &shadowDirParam));
        if (!err) {
            // Same convention as the native Drop Shadow: 0 degrees moves the
            // shadow straight up, clockwise positive.
            const float degrees = shadowDirParam.u.ad.value / 65536.0f;
            const float radians = degrees * 3.14159265f / 180.0f;
            renderParams->shadowOffsetX = sinf(radians) * shadowDistance;
            renderParams->shadowOffsetY = -cosf(radians) * shadowDistance;
            ERR2(CheckinParamValue(in_data, &shadowDirParam));
        }
    }

    if (!err) {
        PF_ParamDef shadowColorParam;
        AEFX_CLR_STRUCT(shadowColorParam);
        ERR(CheckoutParamValue(in_data, out_data, KP_LIQUIDGLASS_SHADOW_COLOR, &shadowColorParam));
        if (!err) {
            renderParams->shadowRed = ColorByteToFloat(shadowColorParam.u.cd.value.red);
            renderParams->shadowGreen = ColorByteToFloat(shadowColorParam.u.cd.value.green);
            renderParams->shadowBlue = ColorByteToFloat(shadowColorParam.u.cd.value.blue);
            ERR2(CheckinParamValue(in_data, &shadowColorParam));
        }
    }

    if (!err) {
        PF_ParamDef shadowModeParam;
        AEFX_CLR_STRUCT(shadowModeParam);
        ERR(CheckoutParamValue(in_data, out_data, KP_LIQUIDGLASS_SHADOW_MODE, &shadowModeParam));
        if (!err) {
            renderParams->shadowMode = shadowModeParam.u.pd.value;
            ERR2(CheckinParamValue(in_data, &shadowModeParam));
        }
    }

    if (!err) {
        PF_ParamDef lightAngleParam;
        AEFX_CLR_STRUCT(lightAngleParam);
        ERR(CheckoutParamValue(in_data, out_data, KP_LIQUIDGLASS_LIGHT_ANGLE, &lightAngleParam));
        if (!err) {
            // AE angle: 0 degrees points up, clockwise positive. Convert to a
            // unit vector toward the light source (screen y grows downward).
            const float degrees = lightAngleParam.u.ad.value / 65536.0f;
            const float radians = degrees * 3.14159265f / 180.0f;
            renderParams->lightDirX = sinf(radians);
            renderParams->lightDirY = -cosf(radians);
            ERR2(CheckinParamValue(in_data, &lightAngleParam));
        }
    }

    if (!err) {
        PF_ParamDef lightColorParam;
        AEFX_CLR_STRUCT(lightColorParam);
        ERR(CheckoutParamValue(in_data, out_data, KP_LIQUIDGLASS_LIGHT_COLOR, &lightColorParam));
        if (!err) {
            renderParams->lightRed = ColorByteToFloat(lightColorParam.u.cd.value.red);
            renderParams->lightGreen = ColorByteToFloat(lightColorParam.u.cd.value.green);
            renderParams->lightBlue = ColorByteToFloat(lightColorParam.u.cd.value.blue);
            ERR2(CheckinParamValue(in_data, &lightColorParam));
        }
    }

    if (!err) {
        PF_ParamDef tintColorParam;
        AEFX_CLR_STRUCT(tintColorParam);
        ERR(CheckoutParamValue(in_data, out_data, KP_LIQUIDGLASS_TINT_COLOR, &tintColorParam));
        if (!err) {
            renderParams->tintRed = ColorByteToFloat(tintColorParam.u.cd.value.red);
            renderParams->tintGreen = ColorByteToFloat(tintColorParam.u.cd.value.green);
            renderParams->tintBlue = ColorByteToFloat(tintColorParam.u.cd.value.blue);
            ERR2(CheckinParamValue(in_data, &tintColorParam));
        }
    }

    // 1.0.0b10: outer shadow & caustics, sharing one Angle/Distance offset.
    ERR(CheckoutFloatSlider(in_data, out_data, KP_LIQUIDGLASS_OUTER_SHADOW_INTENSITY, &renderParams->outerShadowIntensity));
    ERR(CheckoutFloatSlider(in_data, out_data, KP_LIQUIDGLASS_OUTER_SHADOW_SPREAD, &renderParams->outerShadowSpread));
    ERR(CheckoutFloatSlider(in_data, out_data, KP_LIQUIDGLASS_CAUSTICS_INTENSITY, &renderParams->causticsIntensity));
    ERR(CheckoutFloatSlider(in_data, out_data, KP_LIQUIDGLASS_CAUSTICS_SPREAD, &renderParams->causticsSpread));

    float outerDistance = 0.0f;
    ERR(CheckoutFloatSlider(in_data, out_data, KP_LIQUIDGLASS_OUTER_DISTANCE, &outerDistance));

    if (!err) {
        PF_ParamDef outerAngleParam;
        AEFX_CLR_STRUCT(outerAngleParam);
        ERR(CheckoutParamValue(in_data, out_data, KP_LIQUIDGLASS_OUTER_ANGLE, &outerAngleParam));
        if (!err) {
            const float degrees = outerAngleParam.u.ad.value / 65536.0f;
            const float radians = degrees * 3.14159265f / 180.0f;
            renderParams->outerOffsetX = sinf(radians) * outerDistance;
            renderParams->outerOffsetY = -cosf(radians) * outerDistance;
            ERR2(CheckinParamValue(in_data, &outerAngleParam));
        }
    }

    if (!err) {
        PF_ParamDef outerShadowColorParam;
        AEFX_CLR_STRUCT(outerShadowColorParam);
        ERR(CheckoutParamValue(in_data, out_data, KP_LIQUIDGLASS_OUTER_SHADOW_COLOR, &outerShadowColorParam));
        if (!err) {
            renderParams->outerShadowRed = ColorByteToFloat(outerShadowColorParam.u.cd.value.red);
            renderParams->outerShadowGreen = ColorByteToFloat(outerShadowColorParam.u.cd.value.green);
            renderParams->outerShadowBlue = ColorByteToFloat(outerShadowColorParam.u.cd.value.blue);
            ERR2(CheckinParamValue(in_data, &outerShadowColorParam));
        }
    }

    if (!err) {
        PF_ParamDef outerShadowModeParam;
        AEFX_CLR_STRUCT(outerShadowModeParam);
        ERR(CheckoutParamValue(in_data, out_data, KP_LIQUIDGLASS_OUTER_SHADOW_MODE, &outerShadowModeParam));
        if (!err) {
            renderParams->outerShadowMode = outerShadowModeParam.u.pd.value;
            ERR2(CheckinParamValue(in_data, &outerShadowModeParam));
        }
    }

    if (!err) {
        PF_ParamDef causticsColorParam;
        AEFX_CLR_STRUCT(causticsColorParam);
        ERR(CheckoutParamValue(in_data, out_data, KP_LIQUIDGLASS_CAUSTICS_COLOR, &causticsColorParam));
        if (!err) {
            renderParams->causticsRed = ColorByteToFloat(causticsColorParam.u.cd.value.red);
            renderParams->causticsGreen = ColorByteToFloat(causticsColorParam.u.cd.value.green);
            renderParams->causticsBlue = ColorByteToFloat(causticsColorParam.u.cd.value.blue);
            ERR2(CheckinParamValue(in_data, &causticsColorParam));
        }
    }

    if (!err) {
        PF_ParamDef causticsModeParam;
        AEFX_CLR_STRUCT(causticsModeParam);
        ERR(CheckoutParamValue(in_data, out_data, KP_LIQUIDGLASS_CAUSTICS_MODE, &causticsModeParam));
        if (!err) {
            renderParams->causticsMode = causticsModeParam.u.pd.value;
            ERR2(CheckinParamValue(in_data, &causticsModeParam));
        }
    }

    // Registering the layer params keeps AE's dependency tracking aware of
    // them even on frames where their pixels end up not being used.
    if (!err) {
        PF_ParamDef layerParam;
        AEFX_CLR_STRUCT(layerParam);
        ERR(CheckoutParamValue(in_data, out_data, KP_LIQUIDGLASS_BACKGROUND_LAYER, &layerParam));
        if (!err) {
            ERR2(CheckinParamValue(in_data, &layerParam));
        }
        AEFX_CLR_STRUCT(layerParam);
        ERR(CheckoutParamValue(in_data, out_data, KP_LIQUIDGLASS_SHAPE_LAYER, &layerParam));
        if (!err) {
            ERR2(CheckinParamValue(in_data, &layerParam));
        }
    }

    if (err) {
        free(renderParams);
        return err;
    }

    // Pixel-unit parameters are authored at full resolution; scale them to the
    // current render resolution. Refraction is a percentage of thickness and
    // scales implicitly through it.
    const float downsampleX = (in_data->downsample_x.den != 0)
        ? float(in_data->downsample_x.num) / float(in_data->downsample_x.den) : 1.0f;
    const float downsampleY = (in_data->downsample_y.den != 0)
        ? float(in_data->downsample_y.num) / float(in_data->downsample_y.den) : 1.0f;
    renderParams->softness *= downsampleX;
    renderParams->thickness *= downsampleX;
    renderParams->roughness *= downsampleX;
    renderParams->edgeBlur *= downsampleX;
    renderParams->strokeWidth *= downsampleX;
    renderParams->shadowOffsetX *= downsampleX;
    renderParams->shadowOffsetY *= downsampleY;
    renderParams->shadowSoftness *= downsampleX;
    renderParams->outerOffsetX *= downsampleX;
    renderParams->outerOffsetY *= downsampleY;
    renderParams->outerShadowSpread *= downsampleX;
    renderParams->causticsSpread *= downsampleX;

    // The output rect is padded beyond the request; AE requires this flag for
    // result_rect to legally exceed the request rect. (GPU_RENDER_POSSIBLE is
    // decided after the layer checkouts — Adjustment Mode without a Glass
    // Shape renders as a CPU passthrough by design.)
    extra->output->flags |= PF_RenderOutputFlag_RETURNS_EXTRA_PIXELS;
    extra->output->pre_render_data = renderParams;
    extra->output->delete_pre_render_data_func = DisposePreRenderData;

    PF_RenderRequest request = extra->input->output_request;
    PF_CheckoutResult inputResult;
    AEFX_CLR_STRUCT(inputResult);

    ERR(extra->cb->checkout_layer(
        in_data->effect_ref,
        KP_LIQUIDGLASS_INPUT,
        KP_LIQUIDGLASS_INPUT,
        &request,
        in_data->current_time,
        in_data->time_step,
        in_data->time_scale,
        &inputResult));
    if (!err) {
        renderParams->maskLeft = inputResult.result_rect.left;
        renderParams->maskTop = inputResult.result_rect.top;
        renderParams->inputLeft = inputResult.result_rect.left;
        renderParams->inputTop = inputResult.result_rect.top;

        // Zoom pivot from the checked-out input bounds. (max_result_rect
        // would be RoI-stable, but for comp-space layers it reports bounds
        // unrelated to the visible content, putting the pivot far off.)
        const PF_LRect &centerRect = inputResult.result_rect;
        renderParams->zoomCenterX = 0.5f * float(centerRect.left + centerRect.right);
        renderParams->zoomCenterY = 0.5f * float(centerRect.top + centerRect.bottom);

        // The glass pipeline itself needs working margin: the bevel-field and
        // profile blurs clamp at the buffer edges, so without this margin the
        // displacement collapses along the output bounds (seen as "no glass on
        // the top/left when decoration pads were zero" before this fix). The
        // outer shadow and caustics reach further out still: their field
        // chain needs the shared offset plus their own blur radius covered.
        const float innerReach = renderParams->thickness + renderParams->softness;
        const float outerOffsetMag = sqrtf(renderParams->outerOffsetX * renderParams->outerOffsetX +
                                            renderParams->outerOffsetY * renderParams->outerOffsetY);
        const float outerSpreadMax = fmaxf(renderParams->outerShadowSpread, renderParams->causticsSpread);
        const float outerReach = outerOffsetMag + outerSpreadMax;
        const A_long margin = A_long(ceilf(fmaxf(innerReach, outerReach))) + 4;

        PF_LRect expandedRect = inputResult.result_rect;
        expandedRect.left -= margin;
        expandedRect.top -= margin;
        expandedRect.right += margin;
        expandedRect.bottom += margin;
        renderParams->outputLeft = expandedRect.left;
        renderParams->outputTop = expandedRect.top;
        UnionLRect(&expandedRect, &extra->output->result_rect);
        UnionLRect(&expandedRect, &extra->output->max_result_rect);

        UnionLRect(&inputResult.result_rect, &extra->output->result_rect);
        UnionLRect(&inputResult.max_result_rect, &extra->output->max_result_rect);
    }

    // Background layer: sampling source only, never unioned into output
    // bounds. Adjustment Mode samples the effect's own input instead, so the
    // background is neither checked out nor consulted in that case.
    if (!err && !renderParams->adjustmentMode) {
        PF_RenderRequest backgroundRequest = extra->input->output_request;
        // The output-request rect is in this layer's space and says nothing
        // about which part of the background gets sampled after the transform,
        // so request everything and let AE clip to the layer's actual bounds.
        backgroundRequest.rect.left = -30000;
        backgroundRequest.rect.top = -30000;
        backgroundRequest.rect.right = 30000;
        backgroundRequest.rect.bottom = 30000;

        PF_CheckoutResult backgroundResult;
        AEFX_CLR_STRUCT(backgroundResult);
        PF_Err bgErr = extra->cb->checkout_layer(
            in_data->effect_ref,
            KP_LIQUIDGLASS_BACKGROUND_LAYER,
            KP_LIQUIDGLASS_BACKGROUND_LAYER,
            &backgroundRequest,
            in_data->current_time,
            in_data->time_step,
            in_data->time_scale,
            &backgroundResult);

        if (bgErr == PF_Err_NONE && !IsEmptyRect(&backgroundResult.result_rect)) {
            renderParams->hasBackground = 1;
            // result_rect is in the background layer's own coordinate system;
            // its origin is where the checked-out world's pixel (0,0) sits.
            renderParams->sampleLeft = backgroundResult.result_rect.left;
            renderParams->sampleTop = backgroundResult.result_rect.top;
        }
    }

    // Glass Shape: optional explicit matte source. When set, the matte comes
    // from this layer's alpha instead of the input — which is what makes the
    // adjustment-layer workflow possible (there the input is the composite
    // below, whose alpha is just a rectangle).
    if (!err) {
        PF_RenderRequest shapeRequest = extra->input->output_request;
        shapeRequest.rect.left = -30000;
        shapeRequest.rect.top = -30000;
        shapeRequest.rect.right = 30000;
        shapeRequest.rect.bottom = 30000;

        PF_CheckoutResult shapeResult;
        AEFX_CLR_STRUCT(shapeResult);
        PF_Err shapeErr = extra->cb->checkout_layer(
            in_data->effect_ref,
            KP_LIQUIDGLASS_SHAPE_LAYER,
            KP_LIQUIDGLASS_SHAPE_LAYER,
            &shapeRequest,
            in_data->current_time,
            in_data->time_step,
            in_data->time_scale,
            &shapeResult);

        if (shapeErr == PF_Err_NONE && !IsEmptyRect(&shapeResult.result_rect)) {
            renderParams->hasShapeLayer = 1;
            renderParams->maskLeft = shapeResult.result_rect.left;
            renderParams->maskTop = shapeResult.result_rect.top;
        }
    }

    // Adjustment Mode without a Glass Shape has no meaningful glass shape
    // (the input alpha is the below-composite's rectangle) — render as a
    // plain CPU passthrough instead of glassing a bounding box.
    const bool passthrough = renderParams->adjustmentMode && !renderParams->hasShapeLayer;
    if (!passthrough && extra->input->what_gpu != PF_GPU_Framework_NONE) {
        extra->output->flags |= PF_RenderOutputFlag_GPU_RENDER_POSSIBLE;
    }

    if (!err) {
        // Identity mappings by default: sample/matte from the input world.
        renderParams->compToSample00 = 1.0f;
        renderParams->compToSample11 = 1.0f;
        renderParams->compToMask00 = 1.0f;
        renderParams->compToMask11 = 1.0f;

        if (renderParams->hasBackground) {
            float coeffs[6];
            if (TryComputeLayerSampleMatrix(in_data, downsampleX, downsampleY,
                                            KP_LIQUIDGLASS_BACKGROUND_LAYER, "sample",
                                            coeffs, nullptr, nullptr) == PF_Err_NONE) {
                renderParams->compToSample00 = coeffs[0];
                renderParams->compToSample01 = coeffs[1];
                renderParams->compToSample02 = coeffs[2];
                renderParams->compToSample10 = coeffs[3];
                renderParams->compToSample11 = coeffs[4];
                renderParams->compToSample12 = coeffs[5];
            }
        } else {
            renderParams->sampleLeft = renderParams->inputLeft;
            renderParams->sampleTop = renderParams->inputTop;
        }

        if (renderParams->hasShapeLayer) {
            float coeffs[6];
            float pivot[2];
            bool pivotValid = false;
            if (TryComputeLayerSampleMatrix(in_data, downsampleX, downsampleY,
                                            KP_LIQUIDGLASS_SHAPE_LAYER, "mask",
                                            coeffs, pivot, &pivotValid) == PF_Err_NONE) {
                renderParams->compToMask00 = coeffs[0];
                renderParams->compToMask01 = coeffs[1];
                renderParams->compToMask02 = coeffs[2];
                renderParams->compToMask10 = coeffs[3];
                renderParams->compToMask11 = coeffs[4];
                renderParams->compToMask12 = coeffs[5];
            }

            // Zoom pivot: the shape layer's anchor point in this layer's
            // render coordinates. (The shape world's rect center is useless
            // for comp-space worlds, which span the whole comp + overflow —
            // a far-off pivot turns zoom into a translation.)
            if (pivotValid) {
                renderParams->zoomCenterX = pivot[0];
                renderParams->zoomCenterY = pivot[1];
            }
        }

        KPLog("PreRender t=%ld ds=%.3f,%.3f adj=%ld hasBG=%ld extend=%ld hasShape=%ld "
              "refr=%.1f%% soft=%.1f thick=%.1f spread=%.1f eblur=%.1f "
              "zoom=%.1f rough=%.1f disp=%.1f tint=%.2f/%.2f/%.2f@%.1f "
              "light=(%.2f,%.2f)@%.0f%% stroke=%.1f "
              "mask=(%ld,%ld) out=(%ld,%ld) sampleOrigin=(%ld,%ld) center=(%.1f,%.1f) "
              "M=[%.4f %.4f %.1f | %.4f %.4f %.1f]",
              (long)in_data->current_time, downsampleX, downsampleY,
              (long)renderParams->adjustmentMode, (long)renderParams->hasBackground,
              (long)renderParams->extendBackground, (long)renderParams->hasShapeLayer,
              renderParams->refraction, renderParams->softness, renderParams->thickness,
              renderParams->spread, renderParams->edgeBlur,
              renderParams->zoom, renderParams->roughness, renderParams->dispersion,
              renderParams->tintRed, renderParams->tintGreen, renderParams->tintBlue,
              renderParams->tintOpacity,
              renderParams->lightDirX, renderParams->lightDirY, renderParams->lightIntensity,
              renderParams->strokeWidth,
              (long)renderParams->maskLeft, (long)renderParams->maskTop,
              (long)renderParams->outputLeft, (long)renderParams->outputTop,
              (long)renderParams->sampleLeft, (long)renderParams->sampleTop,
              renderParams->zoomCenterX, renderParams->zoomCenterY,
              renderParams->compToSample00, renderParams->compToSample01, renderParams->compToSample02,
              renderParams->compToSample10, renderParams->compToSample11, renderParams->compToSample12);

        ERR(MixRenderStateIntoGuid(in_data, extra, renderParams));
    }

    return err;
}

static PF_Err GPUDeviceSetup(
    PF_InData *in_data,
    PF_OutData *out_data,
    PF_GPUDeviceSetupExtra *extra)
{
    PF_Err err = PF_Err_NONE;

    if (!extra || !extra->input || !extra->output) {
        return PF_Err_INTERNAL_STRUCT_DAMAGED;
    }

    PF_GPUDeviceInfo deviceInfo;
    AEFX_CLR_STRUCT(deviceInfo);

    AEFX_SuiteScoper<PF_GPUDeviceSuite1> gpuSuite(
        in_data,
        kPFGPUDeviceSuite,
        kPFGPUDeviceSuiteVersion1,
        out_data);
    ERR(gpuSuite->GetDeviceInfo(in_data->effect_ref, extra->input->device_index, &deviceInfo));
    if (err) {
        return err;
    }
    // For CUDA, devicePV holds a CUdevice — an integer handle that is 0 for
    // the primary GPU — so the null-check must not gate the CUDA branch.
    if (extra->input->what_gpu != PF_GPU_Framework_CUDA && !deviceInfo.devicePV) {
        return PF_Err_INTERNAL_STRUCT_DAMAGED;
    }

    AEFX_SuiteScoper<PF_HandleSuite1> handleSuite(
        in_data,
        kPFHandleSuite,
        kPFHandleSuiteVersion1,
        out_data);

#if HAS_CUDA
    if (extra->input->what_gpu == PF_GPU_Framework_CUDA) {
        // CUDA kernels are statically linked by nvcc; nothing to compile.
        out_data->out_flags2 |= PF_OutFlag2_SUPPORTS_GPU_RENDER_F32;
        return PF_Err_NONE;
    }
#endif

#if HAS_HLSL
    if (extra->input->what_gpu == PF_GPU_Framework_DIRECTX) {
        PF_Handle dxHandle = handleSuite->host_new_handle(sizeof(DirectXGPUData));
        if (!dxHandle) {
            return PF_Err_OUT_OF_MEMORY;
        }
        DirectXGPUData *dxData = reinterpret_cast<DirectXGPUData *>(*dxHandle);
        memset(dxData, 0, sizeof(DirectXGPUData));

        dxData->mContext = std::make_shared<DXContext>();
        DX_ERR(dxData->mContext->Initialize(
            (ID3D12Device *)deviceInfo.devicePV,
            (ID3D12CommandQueue *)deviceInfo.command_queuePV));

        for (int i = 0; i < KP_DXShader_Count && !err; ++i) {
            dxData->mShaders[i] = std::make_shared<ShaderObject>();
            std::wstring csoPath;
            std::wstring sigPath;
            DX_ERR(GetShaderPath(kKP_DXShaderNames[i], csoPath, sigPath));
            DX_ERR(dxData->mContext->LoadShader(csoPath.c_str(), sigPath.c_str(), dxData->mShaders[i]));
        }

        if (err) {
            dxData->mContext.reset();
            for (int i = 0; i < KP_DXShader_Count; ++i) {
                dxData->mShaders[i].reset();
            }
            handleSuite->host_dispose_handle(dxHandle);
            return err;
        }

        extra->output->gpu_data = dxHandle;
        out_data->out_flags2 |= PF_OutFlag2_SUPPORTS_GPU_RENDER_F32;
        return PF_Err_NONE;
    }
#endif

#if HAS_METAL
    if (extra->input->what_gpu != PF_GPU_Framework_METAL) {
        return PF_Err_UNRECOGNIZED_PARAM_TYPE;
    }

    ScopedAutoreleasePool pool;

    PF_Handle gpuDataHandle = handleSuite->host_new_handle(sizeof(MetalGPUData));
    if (!gpuDataHandle) {
        return PF_Err_OUT_OF_MEMORY;
    }

    MetalGPUData *gpuData = reinterpret_cast<MetalGPUData *>(*gpuDataHandle);
    AEFX_CLR_STRUCT(*gpuData);

    id<MTLDevice> device = (id<MTLDevice>)deviceInfo.devicePV;
    NSError *error = nil;
    NSString *source = [NSString stringWithUTF8String:kKP_LiquidGlassMetalSource];
    id<MTLLibrary> library = [[device newLibraryWithSource:source options:nil error:&error] autorelease];
    if (!library) {
        handleSuite->host_dispose_handle(gpuDataHandle);
        return NSErrorToPFErr(error);
    }

    for (int i = 0; i < KP_MetalPipeline_Count && !err; ++i) {
        NSString *name = [NSString stringWithUTF8String:kKP_MetalKernelNames[i]];
        id<MTLFunction> function = [[library newFunctionWithName:name] autorelease];
        if (!function) {
            err = PF_Err_INTERNAL_STRUCT_DAMAGED;
            break;
        }
        gpuData->pipelines[i] = [device newComputePipelineStateWithFunction:function error:&error];
        if (!gpuData->pipelines[i]) {
            err = error ? NSErrorToPFErr(error) : PF_Err_INTERNAL_STRUCT_DAMAGED;
        }
    }

    if (err) {
        for (int i = 0; i < KP_MetalPipeline_Count; ++i) {
            if (gpuData->pipelines[i]) {
                [gpuData->pipelines[i] release];
                gpuData->pipelines[i] = nil;
            }
        }
        handleSuite->host_dispose_handle(gpuDataHandle);
        return err;
    }

    extra->output->gpu_data = gpuDataHandle;
    out_data->out_flags2 |= PF_OutFlag2_SUPPORTS_GPU_RENDER_F32;
    return PF_Err_NONE;
#endif  // HAS_METAL

    return PF_Err_UNRECOGNIZED_PARAM_TYPE;
}

static PF_Err GPUDeviceSetdown(
    PF_InData *in_data,
    PF_OutData *out_data,
    PF_GPUDeviceSetdownExtra *extra)
{
    if (!extra || !extra->input || !extra->input->gpu_data) {
        return PF_Err_NONE;
    }

    PF_Handle gpuDataHandle = (PF_Handle)extra->input->gpu_data;
    if (!gpuDataHandle || !*gpuDataHandle) {
        return PF_Err_NONE;
    }

    AEFX_SuiteScoper<PF_HandleSuite1> handleSuite(
        in_data,
        kPFHandleSuite,
        kPFHandleSuiteVersion1,
        out_data);

#if HAS_HLSL
    if (extra->input->what_gpu == PF_GPU_Framework_DIRECTX) {
        DirectXGPUData *dxData = reinterpret_cast<DirectXGPUData *>(*gpuDataHandle);
        if (dxData) {
            dxData->mContext.reset();
            for (int i = 0; i < KP_DXShader_Count; ++i) {
                dxData->mShaders[i].reset();
            }
        }
        handleSuite->host_dispose_handle(gpuDataHandle);
        return PF_Err_NONE;
    }
#endif

#if HAS_METAL
    if (extra->input->what_gpu == PF_GPU_Framework_METAL) {
        MetalGPUData *gpuData = reinterpret_cast<MetalGPUData *>(*gpuDataHandle);
        if (gpuData) {
            for (int i = 0; i < KP_MetalPipeline_Count; ++i) {
                if (gpuData->pipelines[i]) {
                    [gpuData->pipelines[i] release];
                    gpuData->pipelines[i] = nil;
                }
            }
        }
        handleSuite->host_dispose_handle(gpuDataHandle);
    }
#endif
    return PF_Err_NONE;
}

static size_t DivideRoundUp(size_t value, size_t divisor)
{
    return value ? (value + divisor - 1) / divisor : 0;
}

#if HAS_METAL
static void EncodeKernel(
    id<MTLComputeCommandEncoder> encoder,
    id<MTLComputePipelineState> pipeline,
    NSUInteger gridWidth,
    NSUInteger gridHeight)
{
    NSUInteger threadWidth = pipeline.threadExecutionWidth ? pipeline.threadExecutionWidth : 16;
    NSUInteger threadHeight = pipeline.maxTotalThreadsPerThreadgroup / threadWidth;
    if (threadHeight > 8) {
        threadHeight = 8;
    }
    if (threadHeight < 1) {
        threadHeight = 1;
    }
    MTLSize threadsPerGroup = MTLSizeMake(threadWidth, threadHeight, 1);
    MTLSize threadgroups = MTLSizeMake(
        DivideRoundUp(gridWidth, threadWidth),
        DivideRoundUp(gridHeight, threadHeight),
        1);
    [encoder setComputePipelineState:pipeline];
    [encoder dispatchThreadgroups:threadgroups threadsPerThreadgroup:threadsPerGroup];
}
#endif

static LiquidGlassPassParams MakeBlurPass(float radiusPx)
{
    LiquidGlassPassParams pass;
    memset(&pass, 0, sizeof(pass));
    pass.radius = int(ceilf(fmaxf(radiusPx, 1.0f)));
    pass.sigma = fmaxf(0.5f * radiusPx, 1.0f);
    return pass;
}

// Largest power of two below the bigger grid dimension: the initial jump
// flooding step size.
static int InitialJFAStep(unsigned int gridWidth, unsigned int gridHeight)
{
    unsigned int maxDimension = gridWidth > gridHeight ? gridWidth : gridHeight;
    int step = 1;
    while ((unsigned int)step < maxDimension) {
        step <<= 1;
    }
    return step >> 1;
}

static PF_Err BuildKernelParams(
    PF_EffectWorld *maskWorld,
    PF_EffectWorld *sampleWorld,
    PF_EffectWorld *outputWorld,
    const LiquidGlassRenderParams *renderParams,
    bool sampleFromBlurredBackground,
    LiquidGlassKernelParams *kernelParams)
{
    if (!maskWorld || !sampleWorld || !outputWorld || !renderParams || !kernelParams) {
        return PF_Err_INTERNAL_STRUCT_DAMAGED;
    }

    AEFX_CLR_STRUCT(*kernelParams);

    kernelParams->maskPitch = maskWorld->rowbytes / sizeof(PF_PixelFloat);
    // The blurred-background buffer is tightly packed, unlike the AE world.
    kernelParams->samplePitch = sampleFromBlurredBackground
        ? sampleWorld->width
        : sampleWorld->rowbytes / sizeof(PF_PixelFloat);
    kernelParams->dstPitch = outputWorld->rowbytes / sizeof(PF_PixelFloat);
    kernelParams->width = outputWorld->width;
    kernelParams->height = outputWorld->height;
    kernelParams->maskWidth = maskWorld->width;
    kernelParams->maskHeight = maskWorld->height;
    kernelParams->sampleWidth = sampleWorld->width;
    kernelParams->sampleHeight = sampleWorld->height;
    // Rim displacement in render pixels: refraction% of the bevel width.
    kernelParams->refraction = renderParams->refraction * 0.01f * renderParams->thickness;
    kernelParams->thickness = renderParams->thickness;
    kernelParams->spread = renderParams->spread;
    kernelParams->zoom = renderParams->zoom;
    kernelParams->zoomCenterX = renderParams->zoomCenterX;
    kernelParams->zoomCenterY = renderParams->zoomCenterY;
    kernelParams->edgeBlur = renderParams->edgeBlur;
    kernelParams->lightDirX = renderParams->lightDirX;
    kernelParams->lightDirY = renderParams->lightDirY;
    kernelParams->lightRed = renderParams->lightRed;
    kernelParams->lightGreen = renderParams->lightGreen;
    kernelParams->lightBlue = renderParams->lightBlue;
    kernelParams->lightIntensity = renderParams->lightIntensity;
    kernelParams->tintRed = renderParams->tintRed;
    kernelParams->tintGreen = renderParams->tintGreen;
    kernelParams->tintBlue = renderParams->tintBlue;
    kernelParams->tintOpacity = renderParams->tintOpacity;
    kernelParams->dispersion = renderParams->dispersion;
    // Extend-edges only makes sense against an explicit Background Layer;
    // the below-composite's union rect can cut through visible content
    // mid-frame, where extending smears it. Force fade otherwise.
    kernelParams->extendBackground = int(renderParams->extendBackground && renderParams->hasBackground);
    kernelParams->maskLeft = renderParams->maskLeft;
    kernelParams->maskTop = renderParams->maskTop;
    kernelParams->sampleLeft = renderParams->sampleLeft;
    kernelParams->sampleTop = renderParams->sampleTop;
    kernelParams->outputLeft = renderParams->outputLeft;
    kernelParams->outputTop = renderParams->outputTop;
    kernelParams->compToSample00 = renderParams->compToSample00;
    kernelParams->compToSample01 = renderParams->compToSample01;
    kernelParams->compToSample02 = renderParams->compToSample02;
    kernelParams->compToSample10 = renderParams->compToSample10;
    kernelParams->compToSample11 = renderParams->compToSample11;
    kernelParams->compToSample12 = renderParams->compToSample12;
    kernelParams->compToMask00 = renderParams->compToMask00;
    kernelParams->compToMask01 = renderParams->compToMask01;
    kernelParams->compToMask02 = renderParams->compToMask02;
    kernelParams->compToMask10 = renderParams->compToMask10;
    kernelParams->compToMask11 = renderParams->compToMask11;
    kernelParams->compToMask12 = renderParams->compToMask12;
    kernelParams->strokeWidth = renderParams->strokeWidth;

    return PF_Err_NONE;
}

#if HAS_CUDA || HAS_HLSL
// Framework-agnostic scratch memory for the CUDA/DirectX paths: GPU worlds
// used as plain linear device buffers. A BGRA128 world of the output's
// dimensions (16 bytes/px) comfortably holds any of the working sets
// (scalar fields need 4 bytes/px, JFA seed pairs need 8).
struct KPScratch
{
    PF_EffectWorld *world;
    void *memory;
};

static PF_Err KPAllocScratch(
    PF_InData *in_data,
    PF_SmartRenderExtra *extra,
    AEFX_SuiteScoper<PF_GPUDeviceSuite1> &gpuSuite,
    PF_EffectWorld *templateWorld,
    A_long width,
    A_long height,
    KPScratch *outScratch)
{
    PF_Err err = PF_Err_NONE;
    outScratch->world = nullptr;
    outScratch->memory = nullptr;
    ERR(gpuSuite->CreateGPUWorld(
        in_data->effect_ref,
        extra->input->device_index,
        width,
        height,
        templateWorld->pix_aspect_ratio,
        in_data->field,
        PF_PixelFormat_GPU_BGRA128,
        FALSE,
        &outScratch->world));
    if (!err) {
        ERR(gpuSuite->GetGPUWorldData(in_data->effect_ref, outScratch->world, &outScratch->memory));
    }
    return err;
}

// Scalar pack for the render kernel; order documented in the kernel file.
// The shadow entries come from renderParams: the kernel-params struct is
// Metal-mirrored and must not grow on this side.
static void KPFillRenderScalars(const LiquidGlassKernelParams &kp,
                                const LiquidGlassRenderParams *renderParams,
                                float outScalars[30])
{
    outScalars[0] = kp.refraction;
    outScalars[1] = kp.thickness;
    outScalars[2] = kp.zoom;
    outScalars[3] = kp.zoomCenterX;
    outScalars[4] = kp.zoomCenterY;
    outScalars[5] = kp.edgeBlur;
    outScalars[6] = kp.lightDirX;
    outScalars[7] = kp.lightDirY;
    outScalars[8] = kp.lightRed;
    outScalars[9] = kp.lightGreen;
    outScalars[10] = kp.lightBlue;
    outScalars[11] = kp.lightIntensity;
    outScalars[12] = kp.tintRed;
    outScalars[13] = kp.tintGreen;
    outScalars[14] = kp.tintBlue;
    outScalars[15] = kp.tintOpacity;
    outScalars[16] = kp.dispersion;
    outScalars[17] = kp.strokeWidth;
    outScalars[18] = renderParams->shadowRed;
    outScalars[19] = renderParams->shadowGreen;
    outScalars[20] = renderParams->shadowBlue;
    outScalars[21] = renderParams->shadowOpacity;
    outScalars[22] = renderParams->outerShadowRed;
    outScalars[23] = renderParams->outerShadowGreen;
    outScalars[24] = renderParams->outerShadowBlue;
    outScalars[25] = renderParams->outerShadowIntensity;
    outScalars[26] = renderParams->causticsRed;
    outScalars[27] = renderParams->causticsGreen;
    outScalars[28] = renderParams->causticsBlue;
    outScalars[29] = renderParams->causticsIntensity;
}
#endif

#if HAS_CUDA
static PF_Err SmartRenderGPU_CUDA(
    PF_InData *in_data,
    PF_OutData *out_data,
    PF_SmartRenderExtra *extra,
    PF_EffectWorld *maskWorld,
    PF_EffectWorld *sampleWorld,
    PF_EffectWorld *outputWorld,
    const LiquidGlassRenderParams *renderParams,
    const LiquidGlassKernelParams &kp,
    void *maskMemory,
    void *sampleMemory,
    void *outputMemory,
    bool blurBackground)
{
    PF_Err err = PF_Err_NONE;

    AEFX_SuiteScoper<PF_GPUDeviceSuite1> gpuSuite(
        in_data,
        kPFGPUDeviceSuite,
        kPFGPUDeviceSuiteVersion1,
        out_data);

    // 0 matte, 1 bevel, 2 height, 3 scratchA, 4 scratchB, 5 jfaA, 6 jfaB,
    // 7 outer shadow field, 8 caustics field
    KPScratch scratch[9] = {};
    for (int i = 0; i < 9 && !err; ++i) {
        ERR(KPAllocScratch(in_data, extra, gpuSuite, outputWorld,
                           (A_long)kp.width, (A_long)kp.height, &scratch[i]));
    }
    KPScratch bgScratch[2] = {};
    if (blurBackground) {
        for (int i = 0; i < 2 && !err; ++i) {
            ERR(KPAllocScratch(in_data, extra, gpuSuite, sampleWorld,
                               (A_long)kp.sampleWidth, (A_long)kp.sampleHeight, &bgScratch[i]));
        }
    }

    if (!err) {
        float *matte = (float *)scratch[0].memory;
        float *bevel = (float *)scratch[1].memory;
        float *height = (float *)scratch[2].memory;
        float *scratchA = (float *)scratch[3].memory;
        float *scratchB = (float *)scratch[4].memory;
        float *jfaCurrent = (float *)scratch[5].memory;
        float *jfaNext = (float *)scratch[6].memory;

        const float c2m[6] = { kp.compToMask00, kp.compToMask01, kp.compToMask02,
                               kp.compToMask10, kp.compToMask11, kp.compToMask12 };
        const float c2s[6] = { kp.compToSample00, kp.compToSample01, kp.compToSample02,
                               kp.compToSample10, kp.compToSample11, kp.compToSample12 };
        float scalars[30];
        KPFillRenderScalars(kp, renderParams, scalars);

        KPPrepareMatte_CUDA((const float *)maskMemory, matte,
                            kp.maskPitch, kp.maskWidth, kp.maskHeight, kp.width, kp.height,
                            kp.outputLeft, kp.outputTop, kp.maskLeft, kp.maskTop, c2m);

        KPJFASeed_CUDA(matte, jfaCurrent, kp.width, kp.height);
        for (int step = InitialJFAStep(kp.width, kp.height); step >= 1; step >>= 1) {
            KPJFAStep_CUDA(jfaCurrent, jfaNext, kp.width, kp.height, step);
            float *swap = jfaCurrent;
            jfaCurrent = jfaNext;
            jfaNext = swap;
        }
        KPJFAResolve_CUDA(jfaCurrent, matte, scratchA, kp.width, kp.height,
                          fmaxf(renderParams->thickness, 1.0f));

        const LiquidGlassPassParams fieldPass = MakeBlurPass(fmaxf(3.0f, renderParams->thickness * 0.15f));
        KPBlurHorizontal_CUDA(scratchA, scratchB, kp.width, kp.height, fieldPass.radius, fieldPass.sigma);
        KPBlurVertical_CUDA(scratchB, bevel, kp.width, kp.height, fieldPass.radius, fieldPass.sigma);

        KPBuildBevelHeight_CUDA(bevel, scratchA, kp.width, kp.height, renderParams->spread);

        const LiquidGlassPassParams softnessPass = MakeBlurPass(renderParams->softness);
        KPBlurHorizontal_CUDA(scratchA, scratchB, kp.width, kp.height, softnessPass.radius, softnessPass.sigma);
        KPBlurVertical_CUDA(scratchB, height, kp.width, kp.height, softnessPass.radius, softnessPass.sigma);

        // Inner shadow field: matte shifted, then blurred by the shared
        // gaussians. The JFA ping-pong buffers are free by now; the final
        // shadow lives in the one the composite kernel reads. With opacity
        // at zero the kernel ignores the buffer entirely.
        float *shadow = jfaNext;
        if (renderParams->shadowOpacity > 0.01f) {
            KPShadowOffset_CUDA(matte, scratchA, kp.width, kp.height,
                                renderParams->shadowOffsetX, renderParams->shadowOffsetY);
            const LiquidGlassPassParams shadowPass = MakeBlurPass(fmaxf(renderParams->shadowSoftness * 0.5f, 1.0f));
            KPBlurHorizontal_CUDA(scratchA, scratchB, kp.width, kp.height, shadowPass.radius, shadowPass.sigma);
            KPBlurVertical_CUDA(scratchB, shadow, kp.width, kp.height, shadowPass.radius, shadowPass.sigma);
        }

        // Outer shadow & caustics fields: matte shifted by the shared
        // Angle/Distance offset, blurred by each effect's own Softness.
        // scratchA/scratchB are free again (the inner shadow chain is done).
        float *outerShadowField = (float *)scratch[7].memory;
        float *causticsField = (float *)scratch[8].memory;
        if (renderParams->outerShadowIntensity > 0.01f) {
            KPShadowOffset_CUDA(matte, scratchA, kp.width, kp.height,
                                renderParams->outerOffsetX, renderParams->outerOffsetY);
            const LiquidGlassPassParams outerPass = MakeBlurPass(fmaxf(renderParams->outerShadowSpread * 0.5f, 1.0f));
            KPBlurHorizontal_CUDA(scratchA, scratchB, kp.width, kp.height, outerPass.radius, outerPass.sigma);
            KPBlurVertical_CUDA(scratchB, outerShadowField, kp.width, kp.height, outerPass.radius, outerPass.sigma);
        }
        if (renderParams->causticsIntensity > 0.01f) {
            KPShadowOffset_CUDA(matte, scratchA, kp.width, kp.height,
                                renderParams->outerOffsetX, renderParams->outerOffsetY);
            const LiquidGlassPassParams causticsPass = MakeBlurPass(fmaxf(renderParams->causticsSpread * 0.5f, 1.0f));
            KPBlurHorizontal_CUDA(scratchA, scratchB, kp.width, kp.height, causticsPass.radius, causticsPass.sigma);
            KPBlurVertical_CUDA(scratchB, causticsField, kp.width, kp.height, causticsPass.radius, causticsPass.sigma);
        }

        const float *samplePtr = (const float *)sampleMemory;
        int samplePitch = (int)(sampleWorld->rowbytes / sizeof(PF_PixelFloat));
        if (blurBackground) {
            const LiquidGlassPassParams bgPass = MakeBlurPass(renderParams->roughness);
            const int bgPitch0 = (int)(bgScratch[0].world->rowbytes / sizeof(PF_PixelFloat));
            const int bgPitch1 = (int)(bgScratch[1].world->rowbytes / sizeof(PF_PixelFloat));
            KPBlurBackgroundHorizontal_CUDA((const float *)sampleMemory, (float *)bgScratch[0].memory,
                                            kp.sampleWidth, kp.sampleHeight,
                                            bgPass.radius, bgPass.sigma, samplePitch, bgPitch0);
            KPBlurBackgroundVertical_CUDA((const float *)bgScratch[0].memory, (float *)bgScratch[1].memory,
                                          kp.sampleWidth, kp.sampleHeight,
                                          bgPass.radius, bgPass.sigma, bgPitch0, bgPitch1);
            samplePtr = (const float *)bgScratch[1].memory;
            samplePitch = bgPitch1;
        }

        const int compositeOnTop =
            (renderParams->adjustmentMode && renderParams->compositeOnTop) ? 1 : 0;
        const int underPitch = (int)(sampleWorld->rowbytes / sizeof(PF_PixelFloat));
        KPLiquidGlassRender_CUDA(matte, bevel, height, samplePtr, shadow,
                                 (const float *)sampleMemory, outerShadowField, causticsField,
                                 (float *)outputMemory,
                                 samplePitch, kp.dstPitch,
                                 kp.width, kp.height, kp.sampleWidth, kp.sampleHeight,
                                 scalars, kp.extendBackground,
                                 kp.sampleLeft, kp.sampleTop, kp.outputLeft, kp.outputTop, c2s,
                                 (int)renderParams->shadowMode, compositeOnTop, underPitch,
                                 (int)renderParams->outerShadowMode, (int)renderParams->causticsMode,
                                 renderParams->confineToBounds ? 1 : 0);

        cudaError_t syncErr = cudaDeviceSynchronize();
        cudaError_t lastErr = cudaPeekAtLastError();
        if (syncErr != cudaSuccess || lastErr != cudaSuccess) {
            KPLog("SmartRenderGPU_CUDA FAILED sync=%d last=%d (%s)",
                  (int)syncErr, (int)lastErr,
                  cudaGetErrorString(lastErr != cudaSuccess ? lastErr : syncErr));
            err = PF_Err_INTERNAL_STRUCT_DAMAGED;
        } else {
            KPLog("SmartRenderGPU_CUDA OK %ux%u sample=%ux%u", kp.width, kp.height,
                  kp.sampleWidth, kp.sampleHeight);
        }
    }
    if (err) {
        KPLog("SmartRenderGPU_CUDA exit err=%ld", (long)err);
    }

    for (int i = 0; i < 9; ++i) {
        if (scratch[i].world) {
            gpuSuite->DisposeGPUWorld(in_data->effect_ref, scratch[i].world);
        }
    }
    for (int i = 0; i < 2; ++i) {
        if (bgScratch[i].world) {
            gpuSuite->DisposeGPUWorld(in_data->effect_ref, bgScratch[i].world);
        }
    }

    return err;
}
#endif  // HAS_CUDA

#if HAS_HLSL
static PF_Err SmartRenderGPU_DirectX(
    PF_InData *in_data,
    PF_OutData *out_data,
    PF_SmartRenderExtra *extra,
    PF_EffectWorld *maskWorld,
    PF_EffectWorld *sampleWorld,
    PF_EffectWorld *outputWorld,
    const LiquidGlassRenderParams *renderParams,
    const LiquidGlassKernelParams &kp,
    void *maskMemory,
    void *sampleMemory,
    void *outputMemory,
    bool blurBackground)
{
    PF_Err err = PF_Err_NONE;

    if (!extra->input->gpu_data) {
        return PF_Err_INTERNAL_STRUCT_DAMAGED;
    }
    PF_Handle gpuDataHandle = (PF_Handle)extra->input->gpu_data;
    DirectXGPUData *dxData = reinterpret_cast<DirectXGPUData *>(*gpuDataHandle);
    if (!dxData || !dxData->mContext) {
        return PF_Err_INTERNAL_STRUCT_DAMAGED;
    }

    AEFX_SuiteScoper<PF_GPUDeviceSuite1> gpuSuite(
        in_data,
        kPFGPUDeviceSuite,
        kPFGPUDeviceSuiteVersion1,
        out_data);

    // 0 matte, 1 bevel, 2 height, 3 scratchA, 4 scratchB, 5 jfaA, 6 jfaB,
    // 7 outer shadow field, 8 caustics field
    KPScratch scratch[9] = {};
    for (int i = 0; i < 9 && !err; ++i) {
        ERR(KPAllocScratch(in_data, extra, gpuSuite, outputWorld,
                           (A_long)kp.width, (A_long)kp.height, &scratch[i]));
    }
    KPScratch bgScratch[2] = {};
    if (blurBackground) {
        for (int i = 0; i < 2 && !err; ++i) {
            ERR(KPAllocScratch(in_data, extra, gpuSuite, sampleWorld,
                               (A_long)kp.sampleWidth, (A_long)kp.sampleHeight, &bgScratch[i]));
        }
    }

    const UINT gridX = (UINT)DivideRoundUp(kp.width, 16);
    const UINT gridY = (UINT)DivideRoundUp(kp.height, 16);
    const UINT scalarBytes = kp.width * kp.height * (UINT)sizeof(float);
    const UINT seedBytes = scalarBytes * 2;
    const UINT maskBytes = (UINT)(maskWorld->height * maskWorld->rowbytes);
    const UINT sampleBytes = (UINT)(sampleWorld->height * sampleWorld->rowbytes);
    const UINT outputBytes = (UINT)(outputWorld->height * outputWorld->rowbytes);

    if (!err) {
        // 1. Matte.
        KPPrepareMatteParamsDX matteParams;
        matteParams.maskPitch = kp.maskPitch;
        matteParams.maskWidth = kp.maskWidth;
        matteParams.maskHeight = kp.maskHeight;
        matteParams.width = kp.width;
        matteParams.height = kp.height;
        matteParams.outputLeft = kp.outputLeft;
        matteParams.outputTop = kp.outputTop;
        matteParams.maskLeft = kp.maskLeft;
        matteParams.maskTop = kp.maskTop;
        matteParams.c2m[0] = kp.compToMask00;
        matteParams.c2m[1] = kp.compToMask01;
        matteParams.c2m[2] = kp.compToMask02;
        matteParams.c2m[3] = kp.compToMask10;
        matteParams.c2m[4] = kp.compToMask11;
        matteParams.c2m[5] = kp.compToMask12;

        DXShaderExecution exec(dxData->mContext, dxData->mShaders[KP_DXShader_PrepareMatte], 3);
        DX_ERR(exec.SetParamBuffer(&matteParams, sizeof(matteParams)));
        DX_ERR(exec.SetUnorderedAccessView((ID3D12Resource *)scratch[0].memory, scalarBytes));
        DX_ERR(exec.SetShaderResourceView((ID3D12Resource *)maskMemory, maskBytes));
        DX_ERR(exec.Execute(gridX, gridY));
    }

    if (!err) {
        // 2. JFA seed.
        KPJFASeedParamsDX seedParams;
        seedParams.width = kp.width;
        seedParams.height = kp.height;

        DXShaderExecution exec(dxData->mContext, dxData->mShaders[KP_DXShader_JFASeed], 3);
        DX_ERR(exec.SetParamBuffer(&seedParams, sizeof(seedParams)));
        DX_ERR(exec.SetUnorderedAccessView((ID3D12Resource *)scratch[5].memory, seedBytes));
        DX_ERR(exec.SetShaderResourceView((ID3D12Resource *)scratch[0].memory, scalarBytes));
        DX_ERR(exec.Execute(gridX, gridY));
    }

    int jfaCurrent = 5;
    int jfaNext = 6;
    for (int step = InitialJFAStep(kp.width, kp.height); step >= 1 && !err; step >>= 1) {
        KPJFAStepParamsDX stepParams;
        stepParams.width = kp.width;
        stepParams.height = kp.height;
        stepParams.step = step;

        DXShaderExecution exec(dxData->mContext, dxData->mShaders[KP_DXShader_JFAStep], 3);
        DX_ERR(exec.SetParamBuffer(&stepParams, sizeof(stepParams)));
        DX_ERR(exec.SetUnorderedAccessView((ID3D12Resource *)scratch[jfaNext].memory, seedBytes));
        DX_ERR(exec.SetShaderResourceView((ID3D12Resource *)scratch[jfaCurrent].memory, seedBytes));
        DX_ERR(exec.Execute(gridX, gridY));

        const int swap = jfaCurrent;
        jfaCurrent = jfaNext;
        jfaNext = swap;
    }

    if (!err) {
        // 3. Resolve to the proximity field (into scratchA).
        KPJFAResolveParamsDX resolveParams;
        resolveParams.width = kp.width;
        resolveParams.height = kp.height;
        resolveParams.thickness = fmaxf(renderParams->thickness, 1.0f);

        DXShaderExecution exec(dxData->mContext, dxData->mShaders[KP_DXShader_JFAResolve], 4);
        DX_ERR(exec.SetParamBuffer(&resolveParams, sizeof(resolveParams)));
        DX_ERR(exec.SetUnorderedAccessView((ID3D12Resource *)scratch[3].memory, scalarBytes));
        DX_ERR(exec.SetShaderResourceView((ID3D12Resource *)scratch[jfaCurrent].memory, seedBytes));
        DX_ERR(exec.SetShaderResourceView((ID3D12Resource *)scratch[0].memory, scalarBytes));
        DX_ERR(exec.Execute(gridX, gridY));
    }

    const LiquidGlassPassParams fieldPass = MakeBlurPass(fmaxf(3.0f, renderParams->thickness * 0.15f));
    const LiquidGlassPassParams softnessPass = MakeBlurPass(renderParams->softness);

    struct KPScalarBlurStep
    {
        int shader;
        int srcIndex;
        int dstIndex;
        const LiquidGlassPassParams *pass;
    };
    // Field smoothing scratchA->scratchB->bevel, profile bevel->scratchA,
    // softness scratchA->scratchB->height (profile handled separately below).
    const KPScalarBlurStep blurSteps[4] = {
        { KP_DXShader_BlurHorizontal, 3, 4, &fieldPass },
        { KP_DXShader_BlurVertical, 4, 1, &fieldPass },
        { KP_DXShader_BlurHorizontal, 3, 4, &softnessPass },
        { KP_DXShader_BlurVertical, 4, 2, &softnessPass },
    };

    for (int i = 0; i < 4 && !err; ++i) {
        if (i == 2 && !err) {
            // Between the two blur pairs: lens profile bevel -> scratchA.
            KPBuildBevelParamsDX bevelParams;
            bevelParams.width = kp.width;
            bevelParams.height = kp.height;
            bevelParams.spread = renderParams->spread;

            DXShaderExecution exec(dxData->mContext, dxData->mShaders[KP_DXShader_BuildBevelHeight], 3);
            DX_ERR(exec.SetParamBuffer(&bevelParams, sizeof(bevelParams)));
            DX_ERR(exec.SetUnorderedAccessView((ID3D12Resource *)scratch[3].memory, scalarBytes));
            DX_ERR(exec.SetShaderResourceView((ID3D12Resource *)scratch[1].memory, scalarBytes));
            DX_ERR(exec.Execute(gridX, gridY));
            if (err) {
                break;
            }
        }

        KPBlurParamsDX blurParams;
        blurParams.width = kp.width;
        blurParams.height = kp.height;
        blurParams.radius = blurSteps[i].pass->radius;
        blurParams.sigma = blurSteps[i].pass->sigma;

        DXShaderExecution exec(dxData->mContext, dxData->mShaders[blurSteps[i].shader], 3);
        DX_ERR(exec.SetParamBuffer(&blurParams, sizeof(blurParams)));
        DX_ERR(exec.SetUnorderedAccessView((ID3D12Resource *)scratch[blurSteps[i].dstIndex].memory, scalarBytes));
        DX_ERR(exec.SetShaderResourceView((ID3D12Resource *)scratch[blurSteps[i].srcIndex].memory, scalarBytes));
        DX_ERR(exec.Execute(gridX, gridY));
    }

    // Inner shadow field: offset matte -> scratchA, blur -> scratch[6]
    // (the JFA buffers are free after the resolve). Skipped at zero opacity;
    // the composite kernel ignores the buffer in that case.
    if (!err && renderParams->shadowOpacity > 0.01f) {
        const LiquidGlassPassParams shadowPass =
            MakeBlurPass(fmaxf(renderParams->shadowSoftness * 0.5f, 1.0f));
        {
            KPShadowOffsetParamsDX shadowParams;
            shadowParams.width = kp.width;
            shadowParams.height = kp.height;
            shadowParams.offsetX = renderParams->shadowOffsetX;
            shadowParams.offsetY = renderParams->shadowOffsetY;

            DXShaderExecution exec(dxData->mContext, dxData->mShaders[KP_DXShader_ShadowOffset], 3);
            DX_ERR(exec.SetParamBuffer(&shadowParams, sizeof(shadowParams)));
            DX_ERR(exec.SetUnorderedAccessView((ID3D12Resource *)scratch[3].memory, scalarBytes));
            DX_ERR(exec.SetShaderResourceView((ID3D12Resource *)scratch[0].memory, scalarBytes));
            DX_ERR(exec.Execute(gridX, gridY));
        }
        if (!err) {
            KPBlurParamsDX blurParams;
            blurParams.width = kp.width;
            blurParams.height = kp.height;
            blurParams.radius = shadowPass.radius;
            blurParams.sigma = shadowPass.sigma;

            DXShaderExecution exec(dxData->mContext, dxData->mShaders[KP_DXShader_BlurHorizontal], 3);
            DX_ERR(exec.SetParamBuffer(&blurParams, sizeof(blurParams)));
            DX_ERR(exec.SetUnorderedAccessView((ID3D12Resource *)scratch[4].memory, scalarBytes));
            DX_ERR(exec.SetShaderResourceView((ID3D12Resource *)scratch[3].memory, scalarBytes));
            DX_ERR(exec.Execute(gridX, gridY));
        }
        if (!err) {
            KPBlurParamsDX blurParams;
            blurParams.width = kp.width;
            blurParams.height = kp.height;
            blurParams.radius = shadowPass.radius;
            blurParams.sigma = shadowPass.sigma;

            DXShaderExecution exec(dxData->mContext, dxData->mShaders[KP_DXShader_BlurVertical], 3);
            DX_ERR(exec.SetParamBuffer(&blurParams, sizeof(blurParams)));
            DX_ERR(exec.SetUnorderedAccessView((ID3D12Resource *)scratch[6].memory, scalarBytes));
            DX_ERR(exec.SetShaderResourceView((ID3D12Resource *)scratch[4].memory, scalarBytes));
            DX_ERR(exec.Execute(gridX, gridY));
        }
    }

    // Outer shadow & caustics fields: offset matte -> scratchA, blur -> the
    // dedicated scratch[7]/scratch[8] buffers. scratchA/scratchB are free
    // again (the inner shadow chain above is done). Both share the same
    // Angle/Distance offset; each has its own Softness (blur radius).
    if (!err && renderParams->outerShadowIntensity > 0.01f) {
        const LiquidGlassPassParams outerPass =
            MakeBlurPass(fmaxf(renderParams->outerShadowSpread * 0.5f, 1.0f));
        {
            KPShadowOffsetParamsDX offsetParams;
            offsetParams.width = kp.width;
            offsetParams.height = kp.height;
            offsetParams.offsetX = renderParams->outerOffsetX;
            offsetParams.offsetY = renderParams->outerOffsetY;

            DXShaderExecution exec(dxData->mContext, dxData->mShaders[KP_DXShader_ShadowOffset], 3);
            DX_ERR(exec.SetParamBuffer(&offsetParams, sizeof(offsetParams)));
            DX_ERR(exec.SetUnorderedAccessView((ID3D12Resource *)scratch[3].memory, scalarBytes));
            DX_ERR(exec.SetShaderResourceView((ID3D12Resource *)scratch[0].memory, scalarBytes));
            DX_ERR(exec.Execute(gridX, gridY));
        }
        if (!err) {
            KPBlurParamsDX blurParams;
            blurParams.width = kp.width;
            blurParams.height = kp.height;
            blurParams.radius = outerPass.radius;
            blurParams.sigma = outerPass.sigma;

            DXShaderExecution exec(dxData->mContext, dxData->mShaders[KP_DXShader_BlurHorizontal], 3);
            DX_ERR(exec.SetParamBuffer(&blurParams, sizeof(blurParams)));
            DX_ERR(exec.SetUnorderedAccessView((ID3D12Resource *)scratch[4].memory, scalarBytes));
            DX_ERR(exec.SetShaderResourceView((ID3D12Resource *)scratch[3].memory, scalarBytes));
            DX_ERR(exec.Execute(gridX, gridY));
        }
        if (!err) {
            KPBlurParamsDX blurParams;
            blurParams.width = kp.width;
            blurParams.height = kp.height;
            blurParams.radius = outerPass.radius;
            blurParams.sigma = outerPass.sigma;

            DXShaderExecution exec(dxData->mContext, dxData->mShaders[KP_DXShader_BlurVertical], 3);
            DX_ERR(exec.SetParamBuffer(&blurParams, sizeof(blurParams)));
            DX_ERR(exec.SetUnorderedAccessView((ID3D12Resource *)scratch[7].memory, scalarBytes));
            DX_ERR(exec.SetShaderResourceView((ID3D12Resource *)scratch[4].memory, scalarBytes));
            DX_ERR(exec.Execute(gridX, gridY));
        }
    }

    if (!err && renderParams->causticsIntensity > 0.01f) {
        const LiquidGlassPassParams causticsPass =
            MakeBlurPass(fmaxf(renderParams->causticsSpread * 0.5f, 1.0f));
        {
            KPShadowOffsetParamsDX offsetParams;
            offsetParams.width = kp.width;
            offsetParams.height = kp.height;
            offsetParams.offsetX = renderParams->outerOffsetX;
            offsetParams.offsetY = renderParams->outerOffsetY;

            DXShaderExecution exec(dxData->mContext, dxData->mShaders[KP_DXShader_ShadowOffset], 3);
            DX_ERR(exec.SetParamBuffer(&offsetParams, sizeof(offsetParams)));
            DX_ERR(exec.SetUnorderedAccessView((ID3D12Resource *)scratch[3].memory, scalarBytes));
            DX_ERR(exec.SetShaderResourceView((ID3D12Resource *)scratch[0].memory, scalarBytes));
            DX_ERR(exec.Execute(gridX, gridY));
        }
        if (!err) {
            KPBlurParamsDX blurParams;
            blurParams.width = kp.width;
            blurParams.height = kp.height;
            blurParams.radius = causticsPass.radius;
            blurParams.sigma = causticsPass.sigma;

            DXShaderExecution exec(dxData->mContext, dxData->mShaders[KP_DXShader_BlurHorizontal], 3);
            DX_ERR(exec.SetParamBuffer(&blurParams, sizeof(blurParams)));
            DX_ERR(exec.SetUnorderedAccessView((ID3D12Resource *)scratch[4].memory, scalarBytes));
            DX_ERR(exec.SetShaderResourceView((ID3D12Resource *)scratch[3].memory, scalarBytes));
            DX_ERR(exec.Execute(gridX, gridY));
        }
        if (!err) {
            KPBlurParamsDX blurParams;
            blurParams.width = kp.width;
            blurParams.height = kp.height;
            blurParams.radius = causticsPass.radius;
            blurParams.sigma = causticsPass.sigma;

            DXShaderExecution exec(dxData->mContext, dxData->mShaders[KP_DXShader_BlurVertical], 3);
            DX_ERR(exec.SetParamBuffer(&blurParams, sizeof(blurParams)));
            DX_ERR(exec.SetUnorderedAccessView((ID3D12Resource *)scratch[8].memory, scalarBytes));
            DX_ERR(exec.SetShaderResourceView((ID3D12Resource *)scratch[4].memory, scalarBytes));
            DX_ERR(exec.Execute(gridX, gridY));
        }
    }

    void *renderSampleMemory = sampleMemory;
    int renderSamplePitch = (int)(sampleWorld->rowbytes / sizeof(PF_PixelFloat));
    UINT renderSampleBytes = sampleBytes;
    if (blurBackground && !err) {
        const LiquidGlassPassParams bgPass = MakeBlurPass(renderParams->roughness);
        const int bgPitch0 = (int)(bgScratch[0].world->rowbytes / sizeof(PF_PixelFloat));
        const int bgPitch1 = (int)(bgScratch[1].world->rowbytes / sizeof(PF_PixelFloat));
        const UINT bgBytes0 = (UINT)(bgScratch[0].world->height * bgScratch[0].world->rowbytes);
        const UINT bgBytes1 = (UINT)(bgScratch[1].world->height * bgScratch[1].world->rowbytes);
        const UINT bgGridX = (UINT)DivideRoundUp(kp.sampleWidth, 16);
        const UINT bgGridY = (UINT)DivideRoundUp(kp.sampleHeight, 16);

        {
            KPBlurBackgroundParamsDX bgParams;
            bgParams.sampleWidth = kp.sampleWidth;
            bgParams.sampleHeight = kp.sampleHeight;
            bgParams.radius = bgPass.radius;
            bgParams.sigma = bgPass.sigma;
            bgParams.srcPitch = renderSamplePitch;
            bgParams.dstPitch = bgPitch0;

            DXShaderExecution exec(dxData->mContext, dxData->mShaders[KP_DXShader_BlurBackgroundHorizontal], 3);
            DX_ERR(exec.SetParamBuffer(&bgParams, sizeof(bgParams)));
            DX_ERR(exec.SetUnorderedAccessView((ID3D12Resource *)bgScratch[0].memory, bgBytes0));
            DX_ERR(exec.SetShaderResourceView((ID3D12Resource *)sampleMemory, sampleBytes));
            DX_ERR(exec.Execute(bgGridX, bgGridY));
        }
        if (!err) {
            KPBlurBackgroundParamsDX bgParams;
            bgParams.sampleWidth = kp.sampleWidth;
            bgParams.sampleHeight = kp.sampleHeight;
            bgParams.radius = bgPass.radius;
            bgParams.sigma = bgPass.sigma;
            bgParams.srcPitch = bgPitch0;
            bgParams.dstPitch = bgPitch1;

            DXShaderExecution exec(dxData->mContext, dxData->mShaders[KP_DXShader_BlurBackgroundVertical], 3);
            DX_ERR(exec.SetParamBuffer(&bgParams, sizeof(bgParams)));
            DX_ERR(exec.SetUnorderedAccessView((ID3D12Resource *)bgScratch[1].memory, bgBytes1));
            DX_ERR(exec.SetShaderResourceView((ID3D12Resource *)bgScratch[0].memory, bgBytes0));
            DX_ERR(exec.Execute(bgGridX, bgGridY));
        }
        renderSampleMemory = bgScratch[1].memory;
        renderSamplePitch = bgPitch1;
        renderSampleBytes = bgBytes1;
    }

    if (!err) {
        // Composite.
        KPRenderParamsDX renderParamsDX;
        renderParamsDX.samplePitch = renderSamplePitch;
        renderParamsDX.dstPitch = kp.dstPitch;
        renderParamsDX.width = kp.width;
        renderParamsDX.height = kp.height;
        renderParamsDX.sampleWidth = kp.sampleWidth;
        renderParamsDX.sampleHeight = kp.sampleHeight;
        float scalars[30];
        KPFillRenderScalars(kp, renderParams, scalars);
        renderParamsDX.refraction = scalars[0];
        renderParamsDX.thickness = scalars[1];
        renderParamsDX.zoom = scalars[2];
        renderParamsDX.zoomCenterX = scalars[3];
        renderParamsDX.zoomCenterY = scalars[4];
        renderParamsDX.edgeBlur = scalars[5];
        renderParamsDX.lightDirX = scalars[6];
        renderParamsDX.lightDirY = scalars[7];
        renderParamsDX.lightR = scalars[8];
        renderParamsDX.lightG = scalars[9];
        renderParamsDX.lightB = scalars[10];
        renderParamsDX.lightIntensity = scalars[11];
        renderParamsDX.tintR = scalars[12];
        renderParamsDX.tintG = scalars[13];
        renderParamsDX.tintB = scalars[14];
        renderParamsDX.tintOpacity = scalars[15];
        renderParamsDX.dispersion = scalars[16];
        renderParamsDX.strokeWidth = scalars[17];
        renderParamsDX.extendEdges = kp.extendBackground;
        renderParamsDX.sampleLeft = kp.sampleLeft;
        renderParamsDX.sampleTop = kp.sampleTop;
        renderParamsDX.outputLeft = kp.outputLeft;
        renderParamsDX.outputTop = kp.outputTop;
        renderParamsDX.c2s[0] = kp.compToSample00;
        renderParamsDX.c2s[1] = kp.compToSample01;
        renderParamsDX.c2s[2] = kp.compToSample02;
        renderParamsDX.c2s[3] = kp.compToSample10;
        renderParamsDX.c2s[4] = kp.compToSample11;
        renderParamsDX.c2s[5] = kp.compToSample12;
        renderParamsDX.shadowR = renderParams->shadowRed;
        renderParamsDX.shadowG = renderParams->shadowGreen;
        renderParamsDX.shadowB = renderParams->shadowBlue;
        renderParamsDX.shadowOpacity = renderParams->shadowOpacity;
        renderParamsDX.shadowMode = (int)renderParams->shadowMode;
        renderParamsDX.compositeOnTop =
            (renderParams->adjustmentMode && renderParams->compositeOnTop) ? 1 : 0;
        renderParamsDX.underPitch = (int)(sampleWorld->rowbytes / sizeof(PF_PixelFloat));
        renderParamsDX.outerShadowR = scalars[22];
        renderParamsDX.outerShadowG = scalars[23];
        renderParamsDX.outerShadowB = scalars[24];
        renderParamsDX.outerShadowIntensity = scalars[25];
        renderParamsDX.outerShadowMode = (int)renderParams->outerShadowMode;
        renderParamsDX.causticsR = scalars[26];
        renderParamsDX.causticsG = scalars[27];
        renderParamsDX.causticsB = scalars[28];
        renderParamsDX.causticsIntensity = scalars[29];
        renderParamsDX.causticsMode = (int)renderParams->causticsMode;
        renderParamsDX.confineToBounds = renderParams->confineToBounds ? 1 : 0;

        DXShaderExecution exec(dxData->mContext, dxData->mShaders[KP_DXShader_Render], 10);
        DX_ERR(exec.SetParamBuffer(&renderParamsDX, sizeof(renderParamsDX)));
        DX_ERR(exec.SetUnorderedAccessView((ID3D12Resource *)outputMemory, outputBytes));
        DX_ERR(exec.SetShaderResourceView((ID3D12Resource *)scratch[0].memory, scalarBytes));
        DX_ERR(exec.SetShaderResourceView((ID3D12Resource *)scratch[1].memory, scalarBytes));
        DX_ERR(exec.SetShaderResourceView((ID3D12Resource *)scratch[2].memory, scalarBytes));
        DX_ERR(exec.SetShaderResourceView((ID3D12Resource *)renderSampleMemory, renderSampleBytes));
        DX_ERR(exec.SetShaderResourceView((ID3D12Resource *)scratch[6].memory, scalarBytes));
        DX_ERR(exec.SetShaderResourceView((ID3D12Resource *)sampleMemory, sampleBytes));
        DX_ERR(exec.SetShaderResourceView((ID3D12Resource *)scratch[7].memory, scalarBytes));
        DX_ERR(exec.SetShaderResourceView((ID3D12Resource *)scratch[8].memory, scalarBytes));
        DX_ERR(exec.Execute(gridX, gridY));
    }

    for (int i = 0; i < 9; ++i) {
        if (scratch[i].world) {
            gpuSuite->DisposeGPUWorld(in_data->effect_ref, scratch[i].world);
        }
    }
    for (int i = 0; i < 2; ++i) {
        if (bgScratch[i].world) {
            gpuSuite->DisposeGPUWorld(in_data->effect_ref, bgScratch[i].world);
        }
    }

    return err;
}
#endif  // HAS_HLSL

static PF_Err SmartRenderGPU(
    PF_InData *in_data,
    PF_OutData *out_data,
    PF_SmartRenderExtra *extra,
    PF_EffectWorld *maskWorld,
    PF_EffectWorld *sampleWorld,
    PF_EffectWorld *outputWorld,
    const LiquidGlassRenderParams *renderParams)
{
    PF_Err err = PF_Err_NONE;
    if (!extra || !extra->input || !maskWorld || !sampleWorld || !outputWorld || !renderParams) {
        return PF_Err_INTERNAL_STRUCT_DAMAGED;
    }
    if (maskWorld->width <= 0 || maskWorld->height <= 0 ||
        sampleWorld->width <= 0 || sampleWorld->height <= 0 ||
        outputWorld->width <= 0 || outputWorld->height <= 0) {
        return PF_Err_NONE;
    }

    AEFX_SuiteScoper<PF_WorldSuite2> worldSuite(
        in_data,
        kPFWorldSuite,
        kPFWorldSuiteVersion2,
        out_data);

    PF_PixelFormat maskPixelFormat = PF_PixelFormat_INVALID;
    PF_PixelFormat samplePixelFormat = PF_PixelFormat_INVALID;
    PF_PixelFormat outputPixelFormat = PF_PixelFormat_INVALID;
    ERR(worldSuite->PF_GetPixelFormat(maskWorld, &maskPixelFormat));
    ERR(worldSuite->PF_GetPixelFormat(sampleWorld, &samplePixelFormat));
    ERR(worldSuite->PF_GetPixelFormat(outputWorld, &outputPixelFormat));
    if (err) {
        return err;
    }
    if (maskPixelFormat != PF_PixelFormat_GPU_BGRA128 ||
        samplePixelFormat != PF_PixelFormat_GPU_BGRA128 ||
        outputPixelFormat != PF_PixelFormat_GPU_BGRA128) {
        return PF_Err_UNRECOGNIZED_PARAM_TYPE;
    }

    AEFX_SuiteScoper<PF_GPUDeviceSuite1> gpuSuite(
        in_data,
        kPFGPUDeviceSuite,
        kPFGPUDeviceSuiteVersion1,
        out_data);

    PF_GPUDeviceInfo deviceInfo;
    AEFX_CLR_STRUCT(deviceInfo);
    ERR(gpuSuite->GetDeviceInfo(in_data->effect_ref, extra->input->device_index, &deviceInfo));
    if (err) {
        return err;
    }

    void *maskMemory = nullptr;
    void *sampleMemory = nullptr;
    void *outputMemory = nullptr;
    ERR(gpuSuite->GetGPUWorldData(in_data->effect_ref, maskWorld, &maskMemory));
    ERR(gpuSuite->GetGPUWorldData(in_data->effect_ref, sampleWorld, &sampleMemory));
    ERR(gpuSuite->GetGPUWorldData(in_data->effect_ref, outputWorld, &outputMemory));
    if (err) {
        return err;
    }
    if (!maskMemory || !sampleMemory || !outputMemory) {
        return PF_Err_INTERNAL_STRUCT_DAMAGED;
    }

    const bool blurBackground = renderParams->roughness > 0.01f;

    LiquidGlassKernelParams kernelParams;
    ERR(BuildKernelParams(
        maskWorld,
        sampleWorld,
        outputWorld,
        renderParams,
        blurBackground,
        &kernelParams));
    if (err) {
        return err;
    }

    KPLog("SmartRenderGPU out=%ldx%ld mask=%ldx%ld sample=%ldx%ld samplePitch=%ld sameWorld=%d",
          (long)outputWorld->width, (long)outputWorld->height,
          (long)maskWorld->width, (long)maskWorld->height,
          (long)sampleWorld->width, (long)sampleWorld->height,
          (long)(sampleWorld->rowbytes / sizeof(PF_PixelFloat)),
          maskWorld == sampleWorld ? 1 : 0);

#if HAS_CUDA
    if (extra->input->what_gpu == PF_GPU_Framework_CUDA) {
        return SmartRenderGPU_CUDA(in_data, out_data, extra,
                                   maskWorld, sampleWorld, outputWorld,
                                   renderParams, kernelParams,
                                   maskMemory, sampleMemory, outputMemory,
                                   blurBackground);
    }
#endif
#if HAS_HLSL
    if (extra->input->what_gpu == PF_GPU_Framework_DIRECTX) {
        return SmartRenderGPU_DirectX(in_data, out_data, extra,
                                      maskWorld, sampleWorld, outputWorld,
                                      renderParams, kernelParams,
                                      maskMemory, sampleMemory, outputMemory,
                                      blurBackground);
    }
#endif

#if HAS_METAL
    if (extra->input->what_gpu != PF_GPU_Framework_METAL) {
        return PF_Err_UNRECOGNIZED_PARAM_TYPE;
    }
    if (!deviceInfo.devicePV || !deviceInfo.command_queuePV) {
        return PF_Err_INTERNAL_STRUCT_DAMAGED;
    }

    ScopedAutoreleasePool pool;
    PF_Handle gpuDataHandle = (PF_Handle)extra->input->gpu_data;
    if (!gpuDataHandle || !*gpuDataHandle) {
        return PF_Err_INTERNAL_STRUCT_DAMAGED;
    }
    MetalGPUData *gpuData = reinterpret_cast<MetalGPUData *>(*gpuDataHandle);
    if (!gpuData) {
        return PF_Err_INTERNAL_STRUCT_DAMAGED;
    }
    for (int i = 0; i < KP_MetalPipeline_Count; ++i) {
        if (!gpuData->pipelines[i]) {
            return PF_Err_INTERNAL_STRUCT_DAMAGED;
        }
    }

    id<MTLDevice> device = (id<MTLDevice>)deviceInfo.devicePV;
    id<MTLCommandQueue> queue = (id<MTLCommandQueue>)deviceInfo.command_queuePV;

    const NSUInteger scalarBufferLength = NSUInteger(kernelParams.width) * NSUInteger(kernelParams.height) * sizeof(float);
    id<MTLBuffer> matteBuffer = [[device newBufferWithLength:scalarBufferLength
                                                     options:MTLResourceStorageModePrivate] autorelease];
    id<MTLBuffer> bevelBuffer = [[device newBufferWithLength:scalarBufferLength
                                                     options:MTLResourceStorageModePrivate] autorelease];
    id<MTLBuffer> heightBuffer = [[device newBufferWithLength:scalarBufferLength
                                                      options:MTLResourceStorageModePrivate] autorelease];
    id<MTLBuffer> scratchABuffer = [[device newBufferWithLength:scalarBufferLength
                                                        options:MTLResourceStorageModePrivate] autorelease];
    id<MTLBuffer> scratchBBuffer = [[device newBufferWithLength:scalarBufferLength
                                                        options:MTLResourceStorageModePrivate] autorelease];
    // Jump-flooding ping-pong buffers hold float2 seed coordinates.
    const NSUInteger seedBufferLength = scalarBufferLength * 2;
    id<MTLBuffer> jfaABuffer = [[device newBufferWithLength:seedBufferLength
                                                    options:MTLResourceStorageModePrivate] autorelease];
    id<MTLBuffer> jfaBBuffer = [[device newBufferWithLength:seedBufferLength
                                                    options:MTLResourceStorageModePrivate] autorelease];
    // 1.0.0b8-b10: inner shadow, outer shadow, and caustics fields -- each
    // the matte re-sampled at an offset and blurred, same shape as matte.
    id<MTLBuffer> innerShadowBuffer = [[device newBufferWithLength:scalarBufferLength
                                                           options:MTLResourceStorageModePrivate] autorelease];
    id<MTLBuffer> outerShadowBuffer = [[device newBufferWithLength:scalarBufferLength
                                                           options:MTLResourceStorageModePrivate] autorelease];
    id<MTLBuffer> causticsBuffer = [[device newBufferWithLength:scalarBufferLength
                                                        options:MTLResourceStorageModePrivate] autorelease];
    if (!matteBuffer || !bevelBuffer || !heightBuffer || !scratchABuffer || !scratchBBuffer ||
        !jfaABuffer || !jfaBBuffer || !innerShadowBuffer || !outerShadowBuffer || !causticsBuffer) {
        return PF_Err_OUT_OF_MEMORY;
    }

    id<MTLBuffer> backgroundScratchBuffer = nil;
    id<MTLBuffer> backgroundBlurredBuffer = nil;
    if (blurBackground) {
        const NSUInteger backgroundBufferLength =
            NSUInteger(kernelParams.sampleWidth) * NSUInteger(kernelParams.sampleHeight) * sizeof(float) * 4;
        backgroundScratchBuffer = [[device newBufferWithLength:backgroundBufferLength
                                                       options:MTLResourceStorageModePrivate] autorelease];
        backgroundBlurredBuffer = [[device newBufferWithLength:backgroundBufferLength
                                                       options:MTLResourceStorageModePrivate] autorelease];
        if (!backgroundScratchBuffer || !backgroundBlurredBuffer) {
            return PF_Err_OUT_OF_MEMORY;
        }
    }

    id<MTLBuffer> paramsBuffer = [[device newBufferWithBytes:&kernelParams
                                                      length:sizeof(LiquidGlassKernelParams)
                                                     options:MTLResourceStorageModeShared] autorelease];
    if (!paramsBuffer) {
        return PF_Err_OUT_OF_MEMORY;
    }

    id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
    if (!commandBuffer) {
        return PF_Err_INTERNAL_STRUCT_DAMAGED;
    }
    id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
    if (!encoder) {
        return PF_Err_INTERNAL_STRUCT_DAMAGED;
    }

    const NSUInteger gridWidth = kernelParams.width;
    const NSUInteger gridHeight = kernelParams.height;

    // 1. Matte from the input's alpha.
    [encoder setBuffer:(id<MTLBuffer>)maskMemory offset:0 atIndex:0];
    [encoder setBuffer:matteBuffer offset:0 atIndex:1];
    [encoder setBuffer:paramsBuffer offset:0 atIndex:2];
    EncodeKernel(encoder, gpuData->pipelines[KP_MetalPipeline_PrepareMatte], gridWidth, gridHeight);

    // 2. Exact distance-to-edge field via jump flooding: seed the shape
    // boundary, propagate nearest-seed coordinates in log-step passes, then
    // resolve to an edge-proximity field normalized by Thickness. This keeps
    // the inset even on elongated shapes where a blur-based field is
    // curvature-biased.
    [encoder setBuffer:matteBuffer offset:0 atIndex:0];
    [encoder setBuffer:jfaABuffer offset:0 atIndex:1];
    [encoder setBuffer:paramsBuffer offset:0 atIndex:2];
    EncodeKernel(encoder, gpuData->pipelines[KP_MetalPipeline_JFASeed], gridWidth, gridHeight);

    id<MTLBuffer> jfaCurrent = jfaABuffer;
    id<MTLBuffer> jfaNext = jfaBBuffer;
    for (int jfaStep = InitialJFAStep(kernelParams.width, kernelParams.height); jfaStep >= 1; jfaStep >>= 1) {
        LiquidGlassPassParams jfaPass;
        memset(&jfaPass, 0, sizeof(jfaPass));
        jfaPass.step = jfaStep;
        [encoder setBuffer:jfaCurrent offset:0 atIndex:0];
        [encoder setBuffer:jfaNext offset:0 atIndex:1];
        [encoder setBuffer:paramsBuffer offset:0 atIndex:2];
        [encoder setBytes:&jfaPass length:sizeof(jfaPass) atIndex:3];
        EncodeKernel(encoder, gpuData->pipelines[KP_MetalPipeline_JFAStep], gridWidth, gridHeight);
        id<MTLBuffer> swap = jfaCurrent;
        jfaCurrent = jfaNext;
        jfaNext = swap;
    }

    LiquidGlassPassParams resolvePass;
    memset(&resolvePass, 0, sizeof(resolvePass));
    resolvePass.sigma = fmaxf(renderParams->thickness, 1.0f);
    [encoder setBuffer:jfaCurrent offset:0 atIndex:0];
    [encoder setBuffer:matteBuffer offset:0 atIndex:1];
    [encoder setBuffer:scratchABuffer offset:0 atIndex:2];
    [encoder setBuffer:paramsBuffer offset:0 atIndex:3];
    [encoder setBytes:&resolvePass length:sizeof(resolvePass) atIndex:4];
    EncodeKernel(encoder, gpuData->pipelines[KP_MetalPipeline_JFAResolve], gridWidth, gridHeight);

    // Smoothing of the proximity field: the displacement direction comes
    // from its gradient, and an exact distance field is only C0 — it creases
    // along the shape's skeleton where the nearest edge flips. Scaling the
    // smoothing with thickness hides that seam while barely affecting the
    // even inset near the rim.
    const LiquidGlassPassParams fieldPass = MakeBlurPass(fmaxf(3.0f, renderParams->thickness * 0.15f));
    [encoder setBuffer:scratchABuffer offset:0 atIndex:0];
    [encoder setBuffer:scratchBBuffer offset:0 atIndex:1];
    [encoder setBuffer:paramsBuffer offset:0 atIndex:2];
    [encoder setBytes:&fieldPass length:sizeof(fieldPass) atIndex:3];
    EncodeKernel(encoder, gpuData->pipelines[KP_MetalPipeline_BlurHorizontal], gridWidth, gridHeight);

    [encoder setBuffer:scratchBBuffer offset:0 atIndex:0];
    [encoder setBuffer:bevelBuffer offset:0 atIndex:1];
    [encoder setBuffer:paramsBuffer offset:0 atIndex:2];
    [encoder setBytes:&fieldPass length:sizeof(fieldPass) atIndex:3];
    EncodeKernel(encoder, gpuData->pipelines[KP_MetalPipeline_BlurVertical], gridWidth, gridHeight);

    // 3. Lens profile (Spread curve + smootherstep punch) from the field,
    // smoothed by Softness into the height map driving displacement.
    [encoder setBuffer:bevelBuffer offset:0 atIndex:0];
    [encoder setBuffer:scratchABuffer offset:0 atIndex:1];
    [encoder setBuffer:paramsBuffer offset:0 atIndex:2];
    EncodeKernel(encoder, gpuData->pipelines[KP_MetalPipeline_BuildBevelHeight], gridWidth, gridHeight);

    const LiquidGlassPassParams softnessPass = MakeBlurPass(renderParams->softness);
    [encoder setBuffer:scratchABuffer offset:0 atIndex:0];
    [encoder setBuffer:scratchBBuffer offset:0 atIndex:1];
    [encoder setBuffer:paramsBuffer offset:0 atIndex:2];
    [encoder setBytes:&softnessPass length:sizeof(softnessPass) atIndex:3];
    EncodeKernel(encoder, gpuData->pipelines[KP_MetalPipeline_BlurHorizontal], gridWidth, gridHeight);

    [encoder setBuffer:scratchBBuffer offset:0 atIndex:0];
    [encoder setBuffer:heightBuffer offset:0 atIndex:1];
    [encoder setBuffer:paramsBuffer offset:0 atIndex:2];
    [encoder setBytes:&softnessPass length:sizeof(softnessPass) atIndex:3];
    EncodeKernel(encoder, gpuData->pipelines[KP_MetalPipeline_BlurVertical], gridWidth, gridHeight);

    // 1.0.0b8-b10: inner shadow, outer shadow, and caustics fields -- the
    // matte re-sampled at an angle/distance offset (ShadowOffsetKernel),
    // then softened by the same separable blur pair used above. Outer
    // shadow and caustics share one offset; scratchA/scratchB are free
    // again since the height-map chain above is done with them. With the
    // opacity/intensity at zero the composite kernel ignores the buffer
    // entirely, so the dispatch is skipped and the buffer is left unused.
    if (renderParams->shadowOpacity > 0.01f) {
        float shadowOffset[2] = { renderParams->shadowOffsetX, renderParams->shadowOffsetY };
        [encoder setBuffer:matteBuffer offset:0 atIndex:0];
        [encoder setBuffer:scratchABuffer offset:0 atIndex:1];
        [encoder setBuffer:paramsBuffer offset:0 atIndex:2];
        [encoder setBytes:shadowOffset length:sizeof(shadowOffset) atIndex:3];
        EncodeKernel(encoder, gpuData->pipelines[KP_MetalPipeline_ShadowOffset], gridWidth, gridHeight);

        const LiquidGlassPassParams shadowPass = MakeBlurPass(fmaxf(renderParams->shadowSoftness * 0.5f, 1.0f));
        [encoder setBuffer:scratchABuffer offset:0 atIndex:0];
        [encoder setBuffer:scratchBBuffer offset:0 atIndex:1];
        [encoder setBuffer:paramsBuffer offset:0 atIndex:2];
        [encoder setBytes:&shadowPass length:sizeof(shadowPass) atIndex:3];
        EncodeKernel(encoder, gpuData->pipelines[KP_MetalPipeline_BlurHorizontal], gridWidth, gridHeight);

        [encoder setBuffer:scratchBBuffer offset:0 atIndex:0];
        [encoder setBuffer:innerShadowBuffer offset:0 atIndex:1];
        [encoder setBuffer:paramsBuffer offset:0 atIndex:2];
        [encoder setBytes:&shadowPass length:sizeof(shadowPass) atIndex:3];
        EncodeKernel(encoder, gpuData->pipelines[KP_MetalPipeline_BlurVertical], gridWidth, gridHeight);
    }

    if (renderParams->outerShadowIntensity > 0.01f) {
        float outerOffset[2] = { renderParams->outerOffsetX, renderParams->outerOffsetY };
        [encoder setBuffer:matteBuffer offset:0 atIndex:0];
        [encoder setBuffer:scratchABuffer offset:0 atIndex:1];
        [encoder setBuffer:paramsBuffer offset:0 atIndex:2];
        [encoder setBytes:outerOffset length:sizeof(outerOffset) atIndex:3];
        EncodeKernel(encoder, gpuData->pipelines[KP_MetalPipeline_ShadowOffset], gridWidth, gridHeight);

        const LiquidGlassPassParams outerPass = MakeBlurPass(fmaxf(renderParams->outerShadowSpread * 0.5f, 1.0f));
        [encoder setBuffer:scratchABuffer offset:0 atIndex:0];
        [encoder setBuffer:scratchBBuffer offset:0 atIndex:1];
        [encoder setBuffer:paramsBuffer offset:0 atIndex:2];
        [encoder setBytes:&outerPass length:sizeof(outerPass) atIndex:3];
        EncodeKernel(encoder, gpuData->pipelines[KP_MetalPipeline_BlurHorizontal], gridWidth, gridHeight);

        [encoder setBuffer:scratchBBuffer offset:0 atIndex:0];
        [encoder setBuffer:outerShadowBuffer offset:0 atIndex:1];
        [encoder setBuffer:paramsBuffer offset:0 atIndex:2];
        [encoder setBytes:&outerPass length:sizeof(outerPass) atIndex:3];
        EncodeKernel(encoder, gpuData->pipelines[KP_MetalPipeline_BlurVertical], gridWidth, gridHeight);
    }

    if (renderParams->causticsIntensity > 0.01f) {
        float causticsOffset[2] = { renderParams->outerOffsetX, renderParams->outerOffsetY };
        [encoder setBuffer:matteBuffer offset:0 atIndex:0];
        [encoder setBuffer:scratchABuffer offset:0 atIndex:1];
        [encoder setBuffer:paramsBuffer offset:0 atIndex:2];
        [encoder setBytes:causticsOffset length:sizeof(causticsOffset) atIndex:3];
        EncodeKernel(encoder, gpuData->pipelines[KP_MetalPipeline_ShadowOffset], gridWidth, gridHeight);

        const LiquidGlassPassParams causticsPass = MakeBlurPass(fmaxf(renderParams->causticsSpread * 0.5f, 1.0f));
        [encoder setBuffer:scratchABuffer offset:0 atIndex:0];
        [encoder setBuffer:scratchBBuffer offset:0 atIndex:1];
        [encoder setBuffer:paramsBuffer offset:0 atIndex:2];
        [encoder setBytes:&causticsPass length:sizeof(causticsPass) atIndex:3];
        EncodeKernel(encoder, gpuData->pipelines[KP_MetalPipeline_BlurHorizontal], gridWidth, gridHeight);

        [encoder setBuffer:scratchBBuffer offset:0 atIndex:0];
        [encoder setBuffer:causticsBuffer offset:0 atIndex:1];
        [encoder setBuffer:paramsBuffer offset:0 atIndex:2];
        [encoder setBytes:&causticsPass length:sizeof(causticsPass) atIndex:3];
        EncodeKernel(encoder, gpuData->pipelines[KP_MetalPipeline_BlurVertical], gridWidth, gridHeight);
    }

    // 4. Roughness: separable blur over the background world.
    if (blurBackground) {
        LiquidGlassPassParams bgPass = MakeBlurPass(renderParams->roughness);
        bgPass.srcPitch = sampleWorld->rowbytes / sizeof(PF_PixelFloat);
        bgPass.dstPitch = int(kernelParams.sampleWidth);
        [encoder setBuffer:(id<MTLBuffer>)sampleMemory offset:0 atIndex:0];
        [encoder setBuffer:backgroundScratchBuffer offset:0 atIndex:1];
        [encoder setBuffer:paramsBuffer offset:0 atIndex:2];
        [encoder setBytes:&bgPass length:sizeof(bgPass) atIndex:3];
        EncodeKernel(encoder, gpuData->pipelines[KP_MetalPipeline_BlurBackgroundHorizontal],
                     kernelParams.sampleWidth, kernelParams.sampleHeight);

        bgPass.srcPitch = int(kernelParams.sampleWidth);
        [encoder setBuffer:backgroundScratchBuffer offset:0 atIndex:0];
        [encoder setBuffer:backgroundBlurredBuffer offset:0 atIndex:1];
        [encoder setBuffer:paramsBuffer offset:0 atIndex:2];
        [encoder setBytes:&bgPass length:sizeof(bgPass) atIndex:3];
        EncodeKernel(encoder, gpuData->pipelines[KP_MetalPipeline_BlurBackgroundVertical],
                     kernelParams.sampleWidth, kernelParams.sampleHeight);
    }

    // 5. Composite.
    LiquidGlassShadowParams shadowParams;
    memset(&shadowParams, 0, sizeof(shadowParams));
    shadowParams.compositeOnTop = (renderParams->adjustmentMode && renderParams->compositeOnTop) ? 1 : 0;
    shadowParams.underPitch = int(sampleWorld->rowbytes / sizeof(PF_PixelFloat));
    shadowParams.shadowRed = renderParams->shadowRed;
    shadowParams.shadowGreen = renderParams->shadowGreen;
    shadowParams.shadowBlue = renderParams->shadowBlue;
    shadowParams.shadowOpacity = renderParams->shadowOpacity;
    shadowParams.shadowMode = (int)renderParams->shadowMode;
    shadowParams.outerShadowRed = renderParams->outerShadowRed;
    shadowParams.outerShadowGreen = renderParams->outerShadowGreen;
    shadowParams.outerShadowBlue = renderParams->outerShadowBlue;
    shadowParams.outerShadowIntensity = renderParams->outerShadowIntensity;
    shadowParams.outerShadowMode = (int)renderParams->outerShadowMode;
    shadowParams.causticsRed = renderParams->causticsRed;
    shadowParams.causticsGreen = renderParams->causticsGreen;
    shadowParams.causticsBlue = renderParams->causticsBlue;
    shadowParams.causticsIntensity = renderParams->causticsIntensity;
    shadowParams.causticsMode = (int)renderParams->causticsMode;
    shadowParams.confineToBounds = renderParams->confineToBounds ? 1 : 0;

    [encoder setBuffer:matteBuffer offset:0 atIndex:0];
    [encoder setBuffer:bevelBuffer offset:0 atIndex:1];
    [encoder setBuffer:heightBuffer offset:0 atIndex:2];
    [encoder setBuffer:(blurBackground ? backgroundBlurredBuffer : (id<MTLBuffer>)sampleMemory) offset:0 atIndex:3];
    [encoder setBuffer:(id<MTLBuffer>)outputMemory offset:0 atIndex:4];
    [encoder setBuffer:paramsBuffer offset:0 atIndex:5];
    [encoder setBuffer:innerShadowBuffer offset:0 atIndex:6];
    [encoder setBuffer:(id<MTLBuffer>)sampleMemory offset:0 atIndex:7];
    [encoder setBuffer:outerShadowBuffer offset:0 atIndex:8];
    [encoder setBuffer:causticsBuffer offset:0 atIndex:9];
    [encoder setBytes:&shadowParams length:sizeof(shadowParams) atIndex:10];
    EncodeKernel(encoder, gpuData->pipelines[KP_MetalPipeline_Render], gridWidth, gridHeight);

    [encoder endEncoding];
    // AE shares this command queue, so downstream work is ordered after ours;
    // blocking on completion here would only stall the render thread.
    [commandBuffer commit];

    return NSErrorToPFErr(commandBuffer.error);
#endif  // HAS_METAL

    return PF_Err_UNRECOGNIZED_PARAM_TYPE;
}

// CPU fallback: passthrough copy of the input, so comps stay renderable when
// AE renders on CPU. The full CPU implementation of the effect is still TODO.
static PF_Err SmartRenderCPU(
    PF_InData *in_data,
    PF_OutData *out_data,
    PF_EffectWorld *inputWorld,
    PF_EffectWorld *outputWorld,
    const LiquidGlassRenderParams *renderParams)
{
    PF_Err err = PF_Err_NONE;

    if (!inputWorld || !outputWorld || !renderParams) {
        return PF_Err_INTERNAL_STRUCT_DAMAGED;
    }

    AEFX_SuiteScoper<PF_WorldSuite2> worldSuite(
        in_data,
        kPFWorldSuite,
        kPFWorldSuiteVersion2,
        out_data);

    PF_PixelFormat pixelFormat = PF_PixelFormat_INVALID;
    ERR(worldSuite->PF_GetPixelFormat(outputWorld, &pixelFormat));
    if (err) {
        return err;
    }

    size_t bytesPerPixel = 0;
    switch (pixelFormat) {
        case PF_PixelFormat_ARGB32:
            bytesPerPixel = 4;
            break;
        case PF_PixelFormat_ARGB64:
            bytesPerPixel = 8;
            break;
        case PF_PixelFormat_ARGB128:
            bytesPerPixel = 16;
            break;
        default:
            return PF_Err_UNRECOGNIZED_PARAM_TYPE;
    }

    char *dstBase = reinterpret_cast<char *>(outputWorld->data);
    const char *srcBase = reinterpret_cast<const char *>(inputWorld->data);
    if (!dstBase || !srcBase) {
        return PF_Err_INTERNAL_STRUCT_DAMAGED;
    }

    KPLog("SmartRenderCPU passthrough out=%ldx%ld in=%ldx%ld bpp=%ld",
          (long)outputWorld->width, (long)outputWorld->height,
          (long)inputWorld->width, (long)inputWorld->height, (long)bytesPerPixel);

    for (A_long y = 0; y < outputWorld->height; ++y) {
        memset(dstBase + size_t(y) * outputWorld->rowbytes, 0, size_t(outputWorld->width) * bytesPerPixel);
    }

    // Place the input at its layer position within the (padded) output.
    const A_long offsetX = renderParams->inputLeft - renderParams->outputLeft;
    const A_long offsetY = renderParams->inputTop - renderParams->outputTop;
    const A_long srcX0 = (offsetX < 0) ? -offsetX : 0;
    const A_long dstX0 = (offsetX > 0) ? offsetX : 0;
    const A_long srcY0 = (offsetY < 0) ? -offsetY : 0;
    const A_long dstY0 = (offsetY > 0) ? offsetY : 0;
    const A_long copyWidth = MIN(inputWorld->width - srcX0, outputWorld->width - dstX0);
    const A_long copyHeight = MIN(inputWorld->height - srcY0, outputWorld->height - dstY0);

    for (A_long y = 0; y < copyHeight; ++y) {
        memcpy(
            dstBase + size_t(dstY0 + y) * outputWorld->rowbytes + size_t(dstX0) * bytesPerPixel,
            srcBase + size_t(srcY0 + y) * inputWorld->rowbytes + size_t(srcX0) * bytesPerPixel,
            size_t(copyWidth) * bytesPerPixel);
    }

    return PF_Err_NONE;
}

static PF_Err SmartRender(
    PF_InData *in_data,
    PF_OutData *out_data,
    PF_SmartRenderExtra *extra,
    bool isGPU)
{
    if (!extra || !extra->input || !extra->cb) {
        return PF_Err_INTERNAL_STRUCT_DAMAGED;
    }

    PF_Err err = PF_Err_NONE;
    PF_Err err2 = PF_Err_NONE;
    LiquidGlassRenderParams *renderParams = reinterpret_cast<LiquidGlassRenderParams *>(extra->input->pre_render_data);
    if (!renderParams) {
        return PF_Err_INTERNAL_STRUCT_DAMAGED;
    }

    PF_EffectWorld *maskWorld = nullptr;
    PF_EffectWorld *sampleWorld = nullptr;
    PF_EffectWorld *matteWorld = nullptr;
    PF_EffectWorld *outputWorld = nullptr;
    bool checkedOutInput = false;
    bool checkedOutBackground = false;
    bool checkedOutShape = false;

    ERR(extra->cb->checkout_layer_pixels(in_data->effect_ref, KP_LIQUIDGLASS_INPUT, &maskWorld));
    if (!err && maskWorld) {
        checkedOutInput = true;
        sampleWorld = maskWorld;
        matteWorld = maskWorld;
    }

    if (!err && isGPU && renderParams->hasBackground) {
        PF_EffectWorld *backgroundWorld = nullptr;
        PF_Err bgErr = extra->cb->checkout_layer_pixels(in_data->effect_ref, KP_LIQUIDGLASS_BACKGROUND_LAYER, &backgroundWorld);
        if (bgErr == PF_Err_NONE && backgroundWorld) {
            checkedOutBackground = true;
            sampleWorld = backgroundWorld;
        }
    }

    if (!err && isGPU && renderParams->hasShapeLayer) {
        PF_EffectWorld *shapeWorld = nullptr;
        PF_Err shapeErr = extra->cb->checkout_layer_pixels(in_data->effect_ref, KP_LIQUIDGLASS_SHAPE_LAYER, &shapeWorld);
        if (shapeErr == PF_Err_NONE && shapeWorld) {
            checkedOutShape = true;
            matteWorld = shapeWorld;
        }
    }

    if (!err) {
        ERR(extra->cb->checkout_output(in_data->effect_ref, &outputWorld));
    }

    if (!err && maskWorld && sampleWorld && matteWorld && outputWorld) {
        if (isGPU) {
            ERR(SmartRenderGPU(in_data, out_data, extra, matteWorld, sampleWorld, outputWorld, renderParams));
        } else {
            ERR(SmartRenderCPU(in_data, out_data, maskWorld, outputWorld, renderParams));
        }
    } else if (!err) {
        err = PF_Err_INTERNAL_STRUCT_DAMAGED;
    }

    if (checkedOutShape) {
        ERR2(extra->cb->checkin_layer_pixels(in_data->effect_ref, KP_LIQUIDGLASS_SHAPE_LAYER));
    }
    if (checkedOutBackground) {
        ERR2(extra->cb->checkin_layer_pixels(in_data->effect_ref, KP_LIQUIDGLASS_BACKGROUND_LAYER));
    }
    if (checkedOutInput) {
        ERR2(extra->cb->checkin_layer_pixels(in_data->effect_ref, KP_LIQUIDGLASS_INPUT));
    }

    return err ? err : err2;
}

extern "C" DllExport PF_Err PluginDataEntryFunction2(
    PF_PluginDataPtr inPtr,
    PF_PluginDataCB2 inPluginDataCallBackPtr,
    SPBasicSuite *inSPBasicSuitePtr,
    const char *inHostName,
    const char *inHostVersion)
{
    PF_Err result = PF_Err_INVALID_CALLBACK;

    result = PF_REGISTER_EFFECT_EXT2(
        inPtr,
        inPluginDataCallBackPtr,
        "KP_LiquidGlass",
        "KP KP_LiquidGlass",
        "KP Effects",
        AE_RESERVED_INFO,
        "EffectMain",
        "https://www.adobe.com");

    return result;
}

PF_Err EffectMain(
    PF_Cmd cmd,
    PF_InData *in_data,
    PF_OutData *out_data,
    PF_ParamDef *params[],
    PF_LayerDef *output,
    void *extra)
{
    PF_Err err = PF_Err_NONE;

    try {
        switch (cmd) {
            case PF_Cmd_ABOUT:
                err = About(in_data, out_data, params, output);
                break;
            case PF_Cmd_GLOBAL_SETUP:
                err = GlobalSetup(in_data, out_data, params, output);
                break;
            case PF_Cmd_PARAMS_SETUP:
                err = ParamsSetup(in_data, out_data, params, output);
                break;
            case PF_Cmd_GPU_DEVICE_SETUP:
                err = GPUDeviceSetup(in_data, out_data, reinterpret_cast<PF_GPUDeviceSetupExtra *>(extra));
                break;
            case PF_Cmd_GPU_DEVICE_SETDOWN:
                err = GPUDeviceSetdown(in_data, out_data, reinterpret_cast<PF_GPUDeviceSetdownExtra *>(extra));
                break;
            case PF_Cmd_RENDER:
                err = PF_Err_UNRECOGNIZED_PARAM_TYPE;
                break;
            case PF_Cmd_SMART_PRE_RENDER:
                err = PreRender(in_data, out_data, reinterpret_cast<PF_PreRenderExtra *>(extra));
                break;
            case PF_Cmd_SMART_RENDER:
                err = SmartRender(in_data, out_data, reinterpret_cast<PF_SmartRenderExtra *>(extra), false);
                break;
            case PF_Cmd_SMART_RENDER_GPU:
                err = SmartRender(in_data, out_data, reinterpret_cast<PF_SmartRenderExtra *>(extra), true);
                break;
            default:
                break;
        }
    } catch (PF_Err &thrownErr) {
        err = thrownErr;
    }

    return err;
}
