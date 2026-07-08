/*
** KP_LiquidGlass — Copyright (c) 2026 Viktor Kopeikin.
** Licensed under the PolyForm Noncommercial License 1.0.0.
** <https://polyformproject.org/licenses/noncommercial/1.0.0>
**
** KP_LiquidGlass cross-API kernels (CUDA / OpenCL / DirectX-via-OpenCL-C).
**
** This file mirrors the hand-written Metal kernels embedded in
** KP_LiquidGlass.h (which remain the source of truth for the macOS build,
** matching how SDK_Invert_ProcAmp is structured). Any change to the Metal
** kernels must be mirrored here and vice versa.
**
** Portability notes:
**  - CUDA float2/float4 have no arithmetic operators; all vector math here
**    is written component-wise.
**  - smoothstep/mix/clamp/normalize are not available in CUDA; portable
**    helpers below are used instead.
**  - AE feeds GPU effects PF_PixelFormat_GPU_BGRA128 (always 32f), so no
**    16f handling is needed (unlike Premiere-targeting samples).
**  - Pixels are premultiplied BGRA: x=B, y=G, z=R, w=A.
*/

#ifndef KP_LIQUIDGLASS_KERNEL_CU
#define KP_LIQUIDGLASS_KERNEL_CU

#include "PrGPU/KernelSupport/KernelCore.h"
#include "PrGPU/KernelSupport/KernelMemory.h"

#if GF_DEVICE_TARGET_DEVICE

#if GF_DEVICE_TARGET_HLSL
    #define fmax max
    #define fmin min
#endif

/* ------------------------------------------------------------------ */
/* Portable math helpers                                               */
/* ------------------------------------------------------------------ */

GF_DEVICE_FUNCTION float KPClamp(float inV, float inLo, float inHi)
{
    return fmax(fmin(inV, inHi), inLo);
}

GF_DEVICE_FUNCTION float KPMix(float inA, float inB, float inT)
{
    return inA + (inB - inA) * inT;
}

GF_DEVICE_FUNCTION float KPSmoothstep(float inEdge0, float inEdge1, float inX)
{
    float t = KPClamp((inX - inEdge0) / (inEdge1 - inEdge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

GF_DEVICE_FUNCTION float KPGaussian(float inX, float inSigma)
{
    return exp(-0.5f * (inX * inX) / fmax(inSigma * inSigma, 0.0001f));
}

/* ------------------------------------------------------------------ */
/* 1. Matte: sample the mask world's alpha through the compToMask      */
/*    matrix (identity + origin when the matte comes from the input).  */
/* ------------------------------------------------------------------ */

GF_KERNEL_FUNCTION(KPPrepareMatteKernel,
    ((GF_PTR_READ_ONLY(float4))(inMask))
    ((GF_PTR(float))(outMatte)),
    ((int)(inMaskPitch))
    ((unsigned int)(inMaskWidth))
    ((unsigned int)(inMaskHeight))
    ((unsigned int)(inWidth))
    ((unsigned int)(inHeight))
    ((int)(inOutputLeft))
    ((int)(inOutputTop))
    ((int)(inMaskLeft))
    ((int)(inMaskTop))
    ((float)(inC2M00))
    ((float)(inC2M01))
    ((float)(inC2M02))
    ((float)(inC2M10))
    ((float)(inC2M11))
    ((float)(inC2M12)),
    ((uint2)(inXY)(KERNEL_XY)))
{
    if (inXY.x < inWidth && inXY.y < inHeight)
    {
        float px = (float)(inOutputLeft + (int)inXY.x);
        float py = (float)(inOutputTop + (int)inXY.y);
        float maskX = inC2M00 * px + inC2M01 * py + inC2M02 - (float)inMaskLeft;
        float maskY = inC2M10 * px + inC2M11 * py + inC2M12 - (float)inMaskTop;

        int x0 = (int)floor(maskX);
        int y0 = (int)floor(maskY);
        float tx = maskX - (float)x0;
        float ty = maskY - (float)y0;

        int w = (int)inMaskWidth;
        int h = (int)inMaskHeight;
        float a00 = 0.0f;
        float a10 = 0.0f;
        float a01 = 0.0f;
        float a11 = 0.0f;
        if (x0 >= 0 && x0 < w && y0 >= 0 && y0 < h) {
            a00 = KPClamp(inMask[y0 * inMaskPitch + x0].w, 0.0f, 1.0f);
        }
        if (x0 + 1 >= 0 && x0 + 1 < w && y0 >= 0 && y0 < h) {
            a10 = KPClamp(inMask[y0 * inMaskPitch + x0 + 1].w, 0.0f, 1.0f);
        }
        if (x0 >= 0 && x0 < w && y0 + 1 >= 0 && y0 + 1 < h) {
            a01 = KPClamp(inMask[(y0 + 1) * inMaskPitch + x0].w, 0.0f, 1.0f);
        }
        if (x0 + 1 >= 0 && x0 + 1 < w && y0 + 1 >= 0 && y0 + 1 < h) {
            a11 = KPClamp(inMask[(y0 + 1) * inMaskPitch + x0 + 1].w, 0.0f, 1.0f);
        }

        outMatte[inXY.y * inWidth + inXY.x] = KPMix(KPMix(a00, a10, tx), KPMix(a01, a11, tx), ty);
    }
}

/* ------------------------------------------------------------------ */
/* 2. Jump flooding: exact distance to the matte boundary.             */
/* ------------------------------------------------------------------ */

GF_KERNEL_FUNCTION(KPJFASeedKernel,
    ((GF_PTR_READ_ONLY(float))(inMatte))
    ((GF_PTR(float2))(outSeeds)),
    ((unsigned int)(inWidth))
    ((unsigned int)(inHeight)),
    ((uint2)(inXY)(KERNEL_XY)))
{
    if (inXY.x < inWidth && inXY.y < inHeight)
    {
        int x = (int)inXY.x;
        int y = (int)inXY.y;
        int w = (int)inWidth;
        int h = (int)inHeight;
        int index = y * w + x;

        bool boundary = false;
        if (inMatte[index] >= 0.5f)
        {
            bool outL = (x > 0) ? (inMatte[index - 1] < 0.5f) : true;
            bool outR = (x < w - 1) ? (inMatte[index + 1] < 0.5f) : true;
            bool outU = (y > 0) ? (inMatte[index - w] < 0.5f) : true;
            bool outD = (y < h - 1) ? (inMatte[index + w] < 0.5f) : true;
            boundary = outL || outR || outU || outD;
        }

        float2 seed;
        seed.x = boundary ? (float)x : -1.0e6f;
        seed.y = boundary ? (float)y : -1.0e6f;
        outSeeds[index] = seed;
    }
}

GF_KERNEL_FUNCTION(KPJFAStepKernel,
    ((GF_PTR_READ_ONLY(float2))(inSrc))
    ((GF_PTR(float2))(outDst)),
    ((unsigned int)(inWidth))
    ((unsigned int)(inHeight))
    ((int)(inStep)),
    ((uint2)(inXY)(KERNEL_XY)))
{
    if (inXY.x < inWidth && inXY.y < inHeight)
    {
        int x = (int)inXY.x;
        int y = (int)inXY.y;
        int w = (int)inWidth;
        int h = (int)inHeight;
        int index = y * w + x;
        int step = inStep > 1 ? inStep : 1;

        float2 best = inSrc[index];
        float bestD = 1.0e30f;
        if (best.x > -1.0e5f) {
            float ddx = best.x - (float)x;
            float ddy = best.y - (float)y;
            bestD = ddx * ddx + ddy * ddy;
        }

        int dy;
        int dx;
        for (dy = -1; dy <= 1; ++dy) {
            for (dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) {
                    continue;
                }
                int nx = x + dx * step;
                int ny = y + dy * step;
                if (nx < 0 || ny < 0 || nx >= w || ny >= h) {
                    continue;
                }
                float2 cand = inSrc[ny * w + nx];
                if (cand.x < -1.0e5f) {
                    continue;
                }
                float cdx = cand.x - (float)x;
                float cdy = cand.y - (float)y;
                float d = cdx * cdx + cdy * cdy;
                if (d < bestD) {
                    bestD = d;
                    best = cand;
                }
            }
        }

        outDst[index] = best;
    }
}

GF_KERNEL_FUNCTION(KPJFAResolveKernel,
    ((GF_PTR_READ_ONLY(float2))(inSeeds))
    ((GF_PTR_READ_ONLY(float))(inMatte))
    ((GF_PTR(float))(outField)),
    ((unsigned int)(inWidth))
    ((unsigned int)(inHeight))
    ((float)(inThickness)),
    ((uint2)(inXY)(KERNEL_XY)))
{
    if (inXY.x < inWidth && inXY.y < inHeight)
    {
        int index = (int)inXY.y * (int)inWidth + (int)inXY.x;
        if (inMatte[index] < 0.5f) {
            outField[index] = 1.0f;
        } else {
            float2 best = inSeeds[index];
            float thickness = fmax(inThickness, 1.0f);
            float d = 1.0e6f;
            if (best.x > -1.0e5f) {
                float ddx = best.x - (float)inXY.x;
                float ddy = best.y - (float)inXY.y;
                d = sqrt(ddx * ddx + ddy * ddy);
            }
            outField[index] = KPClamp(1.0f - d / thickness, 0.0f, 1.0f);
        }
    }
}

/* ------------------------------------------------------------------ */
/* 3. Separable gaussian blurs over scalar buffers.                    */
/* ------------------------------------------------------------------ */

GF_KERNEL_FUNCTION(KPBlurHorizontalKernel,
    ((GF_PTR_READ_ONLY(float))(inSrc))
    ((GF_PTR(float))(outDst)),
    ((unsigned int)(inWidth))
    ((unsigned int)(inHeight))
    ((int)(inRadius))
    ((float)(inSigma)),
    ((uint2)(inXY)(KERNEL_XY)))
{
    if (inXY.x < inWidth && inXY.y < inHeight)
    {
        int x = (int)inXY.x;
        int y = (int)inXY.y;
        int w = (int)inWidth;
        int radius = inRadius > 1 ? inRadius : 1;
        float sum = 0.0f;
        float weightSum = 0.0f;
        int dx;
        for (dx = -radius; dx <= radius; ++dx) {
            int sx = x + dx;
            sx = sx < 0 ? 0 : (sx > w - 1 ? w - 1 : sx);
            float weight = KPGaussian((float)dx, inSigma);
            sum += inSrc[y * w + sx] * weight;
            weightSum += weight;
        }
        outDst[y * w + x] = sum / weightSum;
    }
}

GF_KERNEL_FUNCTION(KPBlurVerticalKernel,
    ((GF_PTR_READ_ONLY(float))(inSrc))
    ((GF_PTR(float))(outDst)),
    ((unsigned int)(inWidth))
    ((unsigned int)(inHeight))
    ((int)(inRadius))
    ((float)(inSigma)),
    ((uint2)(inXY)(KERNEL_XY)))
{
    if (inXY.x < inWidth && inXY.y < inHeight)
    {
        int x = (int)inXY.x;
        int y = (int)inXY.y;
        int w = (int)inWidth;
        int h = (int)inHeight;
        int radius = inRadius > 1 ? inRadius : 1;
        float sum = 0.0f;
        float weightSum = 0.0f;
        int dy;
        for (dy = -radius; dy <= radius; ++dy) {
            int sy = y + dy;
            sy = sy < 0 ? 0 : (sy > h - 1 ? h - 1 : sy);
            float weight = KPGaussian((float)dy, inSigma);
            sum += inSrc[sy * w + x] * weight;
            weightSum += weight;
        }
        outDst[y * w + x] = sum / weightSum;
    }
}

/* ------------------------------------------------------------------ */
/* 4. Lens surface from the proximity field.                           */
/* ------------------------------------------------------------------ */

GF_KERNEL_FUNCTION(KPBuildBevelHeightKernel,
    ((GF_PTR_READ_ONLY(float))(inField))
    ((GF_PTR(float))(outHeight)),
    ((unsigned int)(inWidth))
    ((unsigned int)(inHeight))
    ((float)(inSpread)),
    ((uint2)(inXY)(KERNEL_XY)))
{
    if (inXY.x < inWidth && inXY.y < inHeight)
    {
        int index = (int)inXY.y * (int)inWidth + (int)inXY.x;
        float spread = KPClamp(inSpread / 100.0f, 0.0f, 1.0f);
        float k = 0.5f + spread * 3.5f;
        float edge = KPClamp(inField[index], 0.0f, 1.0f);
        float t = pow(edge, k);
        outHeight[index] = sqrt(fmax(1.0f - t * t, 0.0f));
    }
}

/* ------------------------------------------------------------------ */
/* 5. Roughness: separable gaussian over the background world.         */
/* ------------------------------------------------------------------ */

GF_KERNEL_FUNCTION(KPBlurBackgroundHorizontalKernel,
    ((GF_PTR_READ_ONLY(float4))(inSrc))
    ((GF_PTR(float4))(outDst)),
    ((unsigned int)(inSampleWidth))
    ((unsigned int)(inSampleHeight))
    ((int)(inRadius))
    ((float)(inSigma))
    ((int)(inSrcPitch))
    ((int)(inDstPitch)),
    ((uint2)(inXY)(KERNEL_XY)))
{
    if (inXY.x < inSampleWidth && inXY.y < inSampleHeight)
    {
        int x = (int)inXY.x;
        int y = (int)inXY.y;
        int w = (int)inSampleWidth;
        int radius = inRadius > 1 ? inRadius : 1;
        float sumX = 0.0f;
        float sumY = 0.0f;
        float sumZ = 0.0f;
        float sumW = 0.0f;
        float weightSum = 0.0f;
        int dx;
        for (dx = -radius; dx <= radius; ++dx) {
            int sx = x + dx;
            sx = sx < 0 ? 0 : (sx > w - 1 ? w - 1 : sx);
            float weight = KPGaussian((float)dx, inSigma);
            float4 p = inSrc[y * inSrcPitch + sx];
            sumX += p.x * weight;
            sumY += p.y * weight;
            sumZ += p.z * weight;
            sumW += p.w * weight;
            weightSum += weight;
        }
        float4 outPix;
        outPix.x = sumX / weightSum;
        outPix.y = sumY / weightSum;
        outPix.z = sumZ / weightSum;
        outPix.w = sumW / weightSum;
        outDst[y * inDstPitch + x] = outPix;
    }
}

GF_KERNEL_FUNCTION(KPBlurBackgroundVerticalKernel,
    ((GF_PTR_READ_ONLY(float4))(inSrc))
    ((GF_PTR(float4))(outDst)),
    ((unsigned int)(inSampleWidth))
    ((unsigned int)(inSampleHeight))
    ((int)(inRadius))
    ((float)(inSigma))
    ((int)(inSrcPitch))
    ((int)(inDstPitch)),
    ((uint2)(inXY)(KERNEL_XY)))
{
    if (inXY.x < inSampleWidth && inXY.y < inSampleHeight)
    {
        int x = (int)inXY.x;
        int y = (int)inXY.y;
        int h = (int)inSampleHeight;
        int radius = inRadius > 1 ? inRadius : 1;
        float sumX = 0.0f;
        float sumY = 0.0f;
        float sumZ = 0.0f;
        float sumW = 0.0f;
        float weightSum = 0.0f;
        int dy;
        for (dy = -radius; dy <= radius; ++dy) {
            int sy = y + dy;
            sy = sy < 0 ? 0 : (sy > h - 1 ? h - 1 : sy);
            float weight = KPGaussian((float)dy, inSigma);
            float4 p = inSrc[sy * inSrcPitch + x];
            sumX += p.x * weight;
            sumY += p.y * weight;
            sumZ += p.z * weight;
            sumW += p.w * weight;
            weightSum += weight;
        }
        float4 outPix;
        outPix.x = sumX / weightSum;
        outPix.y = sumY / weightSum;
        outPix.z = sumZ / weightSum;
        outPix.w = sumW / weightSum;
        outDst[y * inDstPitch + x] = outPix;
    }
}

/* ------------------------------------------------------------------ */
/* Sampling helpers for the composite kernel.                          */
/* ------------------------------------------------------------------ */

/* One background texel; extend-edges clamps, fade returns transparent. */
GF_DEVICE_FUNCTION float4 KPBackgroundTap(
    GF_PTR_READ_ONLY(float4) inSample,
    int inX, int inY,
    int inSampleWidth, int inSampleHeight, int inSamplePitch,
    int inExtendEdges)
{
    if (inExtendEdges != 0) {
        inX = inX < 0 ? 0 : (inX > inSampleWidth - 1 ? inSampleWidth - 1 : inX);
        inY = inY < 0 ? 0 : (inY > inSampleHeight - 1 ? inSampleHeight - 1 : inY);
    } else if (inX < 0 || inY < 0 || inX >= inSampleWidth || inY >= inSampleHeight) {
        float4 zero;
        zero.x = 0.0f;
        zero.y = 0.0f;
        zero.z = 0.0f;
        zero.w = 0.0f;
        return zero;
    }
    return inSample[inY * inSamplePitch + inX];
}

GF_DEVICE_FUNCTION float4 KPSampleBilinear(
    GF_PTR_READ_ONLY(float4) inSample,
    float inX, float inY,
    int inSampleWidth, int inSampleHeight, int inSamplePitch,
    int inExtendEdges)
{
    if (inExtendEdges != 0) {
        inX = KPClamp(inX, 0.0f, (float)(inSampleWidth - 1));
        inY = KPClamp(inY, 0.0f, (float)(inSampleHeight - 1));
    }
    int x0 = (int)floor(inX);
    int y0 = (int)floor(inY);
    float tx = inX - (float)x0;
    float ty = inY - (float)y0;

    float4 c00 = KPBackgroundTap(inSample, x0, y0, inSampleWidth, inSampleHeight, inSamplePitch, inExtendEdges);
    float4 c10 = KPBackgroundTap(inSample, x0 + 1, y0, inSampleWidth, inSampleHeight, inSamplePitch, inExtendEdges);
    float4 c01 = KPBackgroundTap(inSample, x0, y0 + 1, inSampleWidth, inSampleHeight, inSamplePitch, inExtendEdges);
    float4 c11 = KPBackgroundTap(inSample, x0 + 1, y0 + 1, inSampleWidth, inSampleHeight, inSamplePitch, inExtendEdges);

    float4 outPix;
    outPix.x = KPMix(KPMix(c00.x, c10.x, tx), KPMix(c01.x, c11.x, tx), ty);
    outPix.y = KPMix(KPMix(c00.y, c10.y, tx), KPMix(c01.y, c11.y, tx), ty);
    outPix.z = KPMix(KPMix(c00.z, c10.z, tx), KPMix(c01.z, c11.z, tx), ty);
    outPix.w = KPMix(KPMix(c00.w, c10.w, tx), KPMix(c01.w, c11.w, tx), ty);
    return outPix;
}

/* Sample through the compToSample matrix with the 5-tap edge blur.
** NOTE: the blur offsets are applied in sample space, whereas the Metal
** kernel offsets pre-matrix; for near-uniform transforms the difference is
** negligible — revisit if strongly anisotropic transforms matter. */
GF_DEVICE_FUNCTION float4 KPSampleDisplaced(
    GF_PTR_READ_ONLY(float4) inSample,
    float inPX, float inPY, float inBlurRadius,
    float inC2S00, float inC2S01, float inC2S02,
    float inC2S10, float inC2S11, float inC2S12,
    int inSampleLeft, int inSampleTop,
    int inSampleWidth, int inSampleHeight, int inSamplePitch,
    int inExtendEdges)
{
    float sx = inC2S00 * inPX + inC2S01 * inPY + inC2S02 - (float)inSampleLeft;
    float sy = inC2S10 * inPX + inC2S11 * inPY + inC2S12 - (float)inSampleTop;

    if (inBlurRadius <= 0.25f) {
        return KPSampleBilinear(inSample, sx, sy, inSampleWidth, inSampleHeight, inSamplePitch, inExtendEdges);
    }

    float o = inBlurRadius * 0.7071f;
    float4 c = KPSampleBilinear(inSample, sx, sy, inSampleWidth, inSampleHeight, inSamplePitch, inExtendEdges);
    float4 c1 = KPSampleBilinear(inSample, sx + o, sy + o, inSampleWidth, inSampleHeight, inSamplePitch, inExtendEdges);
    float4 c2 = KPSampleBilinear(inSample, sx - o, sy + o, inSampleWidth, inSampleHeight, inSamplePitch, inExtendEdges);
    float4 c3 = KPSampleBilinear(inSample, sx + o, sy - o, inSampleWidth, inSampleHeight, inSamplePitch, inExtendEdges);
    float4 c4 = KPSampleBilinear(inSample, sx - o, sy - o, inSampleWidth, inSampleHeight, inSamplePitch, inExtendEdges);

    float4 outPix;
    outPix.x = (c.x * 2.0f + c1.x + c2.x + c3.x + c4.x) / 6.0f;
    outPix.y = (c.y * 2.0f + c1.y + c2.y + c3.y + c4.y) / 6.0f;
    outPix.z = (c.z * 2.0f + c1.z + c2.z + c3.z + c4.z) / 6.0f;
    outPix.w = (c.w * 2.0f + c1.w + c2.w + c3.w + c4.w) / 6.0f;
    return outPix;
}

/* ------------------------------------------------------------------ */
/* 6. Composite: displacement from the surface slope, dispersion,      */
/*    tint, then the two-layer light screened on top.                  */
/* ------------------------------------------------------------------ */

GF_KERNEL_FUNCTION(KPLiquidGlassKernel,
    ((GF_PTR_READ_ONLY(float))(inMatte))
    ((GF_PTR_READ_ONLY(float))(inBevel))
    ((GF_PTR_READ_ONLY(float))(inHeightMap))
    ((GF_PTR_READ_ONLY(float4))(inSample))
    ((GF_PTR(float4))(outDst)),
    ((int)(inSamplePitch))
    ((int)(inDstPitch))
    ((unsigned int)(inWidth))
    ((unsigned int)(inHeight))
    ((unsigned int)(inSampleWidth))
    ((unsigned int)(inSampleHeight))
    ((float)(inRefraction))
    ((float)(inThickness))
    ((float)(inZoom))
    ((float)(inZoomCenterX))
    ((float)(inZoomCenterY))
    ((float)(inEdgeBlur))
    ((float)(inLightDirX))
    ((float)(inLightDirY))
    ((float)(inLightR))
    ((float)(inLightG))
    ((float)(inLightB))
    ((float)(inLightIntensity))
    ((float)(inTintR))
    ((float)(inTintG))
    ((float)(inTintB))
    ((float)(inTintOpacity))
    ((float)(inDispersion))
    ((int)(inExtendEdges))
    ((int)(inSampleLeft))
    ((int)(inSampleTop))
    ((int)(inOutputLeft))
    ((int)(inOutputTop))
    ((float)(inC2S00))
    ((float)(inC2S01))
    ((float)(inC2S02))
    ((float)(inC2S10))
    ((float)(inC2S11))
    ((float)(inC2S12))
    ((float)(inStrokeWidth)),
    ((uint2)(inXY)(KERNEL_XY)))
{
    if (inXY.x < inWidth && inXY.y < inHeight)
    {
        int x = (int)inXY.x;
        int y = (int)inXY.y;
        int w = (int)inWidth;
        int h = (int)inHeight;
        int index = y * w + x;
        int dstIndex = y * inDstPitch + x;
        float maskAlpha = inMatte[index];

        float4 color;
        color.x = 0.0f;
        color.y = 0.0f;
        color.z = 0.0f;
        color.w = 0.0f;

        if (maskAlpha > 0.0f)
        {
            int xL = x > 0 ? x - 1 : 0;
            int xR = x < w - 1 ? x + 1 : w - 1;
            int yU = y > 0 ? y - 1 : 0;
            int yD = y < h - 1 ? y + 1 : h - 1;

            /* Displacement vector = gradient of the lens surface. */
            float slopeX = (inHeightMap[y * w + xR] - inHeightMap[y * w + xL]) * 0.5f;
            float slopeY = (inHeightMap[yD * w + x] - inHeightMap[yU * w + x]) * 0.5f;

            /* Outward normal for the lighting from the proximity field. */
            float gradX = (inBevel[y * w + xR] - inBevel[y * w + xL]) * 0.5f;
            float gradY = (inBevel[yD * w + x] - inBevel[yU * w + x]) * 0.5f;
            float gradLen2 = gradX * gradX + gradY * gradY;
            float outwardX = 0.0f;
            float outwardY = 0.0f;
            if (gradLen2 > 0.000001f) {
                float invLen = 1.0f / sqrt(gradLen2);
                outwardX = gradX * invLen;
                outwardY = gradY * invLen;
            }

            float zoom = fmax(inZoom / 100.0f, 0.001f);
            float posX = (float)(inOutputLeft + x);
            float posY = (float)(inOutputTop + y);
            float zoomedX = inZoomCenterX + (posX - inZoomCenterX) / zoom;
            float zoomedY = inZoomCenterY + (posY - inZoomCenterY) / zoom;

            float hVal = inHeightMap[index];
            float edgeFade = KPSmoothstep(0.0f, 0.9f, maskAlpha);
            float dispX = slopeX * (inRefraction * 3.0f) * edgeFade;
            float dispY = slopeY * (inRefraction * 3.0f) * edgeFade;
            float dispLen = sqrt(dispX * dispX + dispY * dispY);
            float blurRadius = inEdgeBlur * KPClamp(1.0f - hVal, 0.0f, 1.0f);
            float dispersion = KPClamp(inDispersion / 100.0f, 0.0f, 1.0f);

            float4 glass;
            if (dispersion > 0.001f && dispLen > 0.001f)
            {
                float sR = 1.0f - 0.10f * dispersion;
                float sB = 1.0f + 0.10f * dispersion;
                float4 pr = KPSampleDisplaced(inSample, zoomedX + dispX * sR, zoomedY + dispY * sR, blurRadius,
                                              inC2S00, inC2S01, inC2S02, inC2S10, inC2S11, inC2S12,
                                              inSampleLeft, inSampleTop,
                                              (int)inSampleWidth, (int)inSampleHeight, inSamplePitch, inExtendEdges);
                float4 pg = KPSampleDisplaced(inSample, zoomedX + dispX, zoomedY + dispY, blurRadius,
                                              inC2S00, inC2S01, inC2S02, inC2S10, inC2S11, inC2S12,
                                              inSampleLeft, inSampleTop,
                                              (int)inSampleWidth, (int)inSampleHeight, inSamplePitch, inExtendEdges);
                float4 pb = KPSampleDisplaced(inSample, zoomedX + dispX * sB, zoomedY + dispY * sB, blurRadius,
                                              inC2S00, inC2S01, inC2S02, inC2S10, inC2S11, inC2S12,
                                              inSampleLeft, inSampleTop,
                                              (int)inSampleWidth, (int)inSampleHeight, inSamplePitch, inExtendEdges);
                glass.x = pb.x;
                glass.y = pg.y;
                glass.z = pr.z;
                glass.w = (pr.w + pg.w + pb.w) / 3.0f;
            }
            else
            {
                glass = KPSampleDisplaced(inSample, zoomedX + dispX, zoomedY + dispY, blurRadius,
                                          inC2S00, inC2S01, inC2S02, inC2S10, inC2S11, inC2S12,
                                          inSampleLeft, inSampleTop,
                                          (int)inSampleWidth, (int)inSampleHeight, inSamplePitch, inExtendEdges);
            }

            /* Premultiplied coverage. */
            glass.x *= maskAlpha;
            glass.y *= maskAlpha;
            glass.z *= maskAlpha;
            glass.w *= maskAlpha;
            color = glass;

            /* Tint (premultiplied source-over; BGRA so x=B, z=R). */
            float tintAlpha = maskAlpha * KPClamp(inTintOpacity / 100.0f, 0.0f, 1.0f);
            color.x = inTintB * tintAlpha + color.x * (1.0f - tintAlpha);
            color.y = inTintG * tintAlpha + color.y * (1.0f - tintAlpha);
            color.z = inTintR * tintAlpha + color.z * (1.0f - tintAlpha);
            color.w = tintAlpha + color.w * (1.0f - tintAlpha);

            /* Light: soft base + hard clear-coat stroke, screened on top. */
            float fieldE = KPClamp(inBevel[index], 0.0f, 1.0f);
            float lit = outwardX * inLightDirX + outwardY * inLightDirY;
            float intensity = KPClamp(inLightIntensity / 100.0f, 0.0f, 2.0f);
            float softBand = KPSmoothstep(0.3f, 0.9f, fieldE);
            float softLight = 0.25f * (pow(KPClamp(lit, 0.0f, 1.0f), 2.5f) +
                                       0.35f * pow(KPClamp(-lit, 0.0f, 1.0f), 2.5f)) * softBand;
            float rimDistance = (1.0f - fieldE) * fmax(inThickness, 1.0f);
            float stroke = (inStrokeWidth > 0.25f)
                ? 1.0f - KPSmoothstep(inStrokeWidth * 0.45f, inStrokeWidth, rimDistance)
                : 0.0f;
            float gloss = 0.95f * stroke * (KPSmoothstep(0.35f, 0.55f, lit) +
                                            0.6f * KPSmoothstep(0.35f, 0.55f, -lit));
            float lightAmount = KPClamp(intensity * (softLight + gloss) * maskAlpha, 0.0f, 0.95f);
            if (lightAmount > 0.0001f && color.w > 0.0001f)
            {
                float invA = 1.0f / fmax(color.w, 0.0001f);
                float baseX = color.x * invA;
                float baseY = color.y * invA;
                float baseZ = color.z * invA;
                baseX = 1.0f - (1.0f - baseX) * (1.0f - inLightB * lightAmount);
                baseY = 1.0f - (1.0f - baseY) * (1.0f - inLightG * lightAmount);
                baseZ = 1.0f - (1.0f - baseZ) * (1.0f - inLightR * lightAmount);
                color.x = baseX * color.w;
                color.y = baseY * color.w;
                color.z = baseZ * color.w;
            }
        }

        outDst[dstIndex] = color;
    }
}
#endif  /* GF_DEVICE_TARGET_DEVICE */

/* ------------------------------------------------------------------ */
/* CUDA launchers (host-callable, statically linked on Windows).       */
/* Scalar pack order for KPLiquidGlassRender_CUDA (float[18]):         */
/*  0 refraction  1 thickness  2 zoom       3 zoomCenterX              */
/*  4 zoomCenterY 5 edgeBlur   6 lightDirX  7 lightDirY                */
/*  8 lightR      9 lightG    10 lightB    11 lightIntensity           */
/* 12 tintR      13 tintG     14 tintB     15 tintOpacity              */
/* 16 dispersion 17 strokeWidth                                        */
/* ------------------------------------------------------------------ */
#if __NVCC__

static dim3 KPGrid(unsigned int inWidth, unsigned int inHeight, dim3 inBlock)
{
    return dim3((inWidth + inBlock.x - 1) / inBlock.x,
                (inHeight + inBlock.y - 1) / inBlock.y, 1);
}

void KPPrepareMatte_CUDA(
    float const *inMask, float *outMatte,
    int inMaskPitch, unsigned int inMaskWidth, unsigned int inMaskHeight,
    unsigned int inWidth, unsigned int inHeight,
    int inOutputLeft, int inOutputTop, int inMaskLeft, int inMaskTop,
    float const *inC2M)
{
    dim3 blockDim(16, 16, 1);
    KPPrepareMatteKernel<<<KPGrid(inWidth, inHeight, blockDim), blockDim, 0>>>(
        (float4 const *)inMask, outMatte,
        inMaskPitch, inMaskWidth, inMaskHeight, inWidth, inHeight,
        inOutputLeft, inOutputTop, inMaskLeft, inMaskTop,
        inC2M[0], inC2M[1], inC2M[2], inC2M[3], inC2M[4], inC2M[5]);
}

void KPJFASeed_CUDA(
    float const *inMatte, float *outSeeds,
    unsigned int inWidth, unsigned int inHeight)
{
    dim3 blockDim(16, 16, 1);
    KPJFASeedKernel<<<KPGrid(inWidth, inHeight, blockDim), blockDim, 0>>>(
        inMatte, (float2 *)outSeeds, inWidth, inHeight);
}

void KPJFAStep_CUDA(
    float const *inSrc, float *outDst,
    unsigned int inWidth, unsigned int inHeight, int inStep)
{
    dim3 blockDim(16, 16, 1);
    KPJFAStepKernel<<<KPGrid(inWidth, inHeight, blockDim), blockDim, 0>>>(
        (float2 const *)inSrc, (float2 *)outDst, inWidth, inHeight, inStep);
}

void KPJFAResolve_CUDA(
    float const *inSeeds, float const *inMatte, float *outField,
    unsigned int inWidth, unsigned int inHeight, float inThickness)
{
    dim3 blockDim(16, 16, 1);
    KPJFAResolveKernel<<<KPGrid(inWidth, inHeight, blockDim), blockDim, 0>>>(
        (float2 const *)inSeeds, inMatte, outField, inWidth, inHeight, inThickness);
}

void KPBlurHorizontal_CUDA(
    float const *inSrc, float *outDst,
    unsigned int inWidth, unsigned int inHeight, int inRadius, float inSigma)
{
    dim3 blockDim(16, 16, 1);
    KPBlurHorizontalKernel<<<KPGrid(inWidth, inHeight, blockDim), blockDim, 0>>>(
        inSrc, outDst, inWidth, inHeight, inRadius, inSigma);
}

void KPBlurVertical_CUDA(
    float const *inSrc, float *outDst,
    unsigned int inWidth, unsigned int inHeight, int inRadius, float inSigma)
{
    dim3 blockDim(16, 16, 1);
    KPBlurVerticalKernel<<<KPGrid(inWidth, inHeight, blockDim), blockDim, 0>>>(
        inSrc, outDst, inWidth, inHeight, inRadius, inSigma);
}

void KPBuildBevelHeight_CUDA(
    float const *inField, float *outHeight,
    unsigned int inWidth, unsigned int inHeight, float inSpread)
{
    dim3 blockDim(16, 16, 1);
    KPBuildBevelHeightKernel<<<KPGrid(inWidth, inHeight, blockDim), blockDim, 0>>>(
        inField, outHeight, inWidth, inHeight, inSpread);
}

void KPBlurBackgroundHorizontal_CUDA(
    float const *inSrc, float *outDst,
    unsigned int inSampleWidth, unsigned int inSampleHeight,
    int inRadius, float inSigma, int inSrcPitch, int inDstPitch)
{
    dim3 blockDim(16, 16, 1);
    KPBlurBackgroundHorizontalKernel<<<KPGrid(inSampleWidth, inSampleHeight, blockDim), blockDim, 0>>>(
        (float4 const *)inSrc, (float4 *)outDst,
        inSampleWidth, inSampleHeight, inRadius, inSigma, inSrcPitch, inDstPitch);
}

void KPBlurBackgroundVertical_CUDA(
    float const *inSrc, float *outDst,
    unsigned int inSampleWidth, unsigned int inSampleHeight,
    int inRadius, float inSigma, int inSrcPitch, int inDstPitch)
{
    dim3 blockDim(16, 16, 1);
    KPBlurBackgroundVerticalKernel<<<KPGrid(inSampleWidth, inSampleHeight, blockDim), blockDim, 0>>>(
        (float4 const *)inSrc, (float4 *)outDst,
        inSampleWidth, inSampleHeight, inRadius, inSigma, inSrcPitch, inDstPitch);
}

void KPLiquidGlassRender_CUDA(
    float const *inMatte, float const *inBevel, float const *inHeight,
    float const *inSample, float *outDst,
    int inSamplePitch, int inDstPitch,
    unsigned int inWidth, unsigned int inHeight,
    unsigned int inSampleWidth, unsigned int inSampleHeight,
    float const *inScalars,
    int inExtendEdges,
    int inSampleLeft, int inSampleTop, int inOutputLeft, int inOutputTop,
    float const *inC2S)
{
    dim3 blockDim(16, 16, 1);
    KPLiquidGlassKernel<<<KPGrid(inWidth, inHeight, blockDim), blockDim, 0>>>(
        inMatte, inBevel, inHeight, (float4 const *)inSample, (float4 *)outDst,
        inSamplePitch, inDstPitch, inWidth, inHeight, inSampleWidth, inSampleHeight,
        inScalars[0], inScalars[1], inScalars[2], inScalars[3], inScalars[4],
        inScalars[5], inScalars[6], inScalars[7], inScalars[8], inScalars[9],
        inScalars[10], inScalars[11], inScalars[12], inScalars[13], inScalars[14],
        inScalars[15], inScalars[16],
        inExtendEdges, inSampleLeft, inSampleTop, inOutputLeft, inOutputTop,
        inC2S[0], inC2S[1], inC2S[2], inC2S[3], inC2S[4], inC2S[5],
        inScalars[17]);
}

#endif  /* __NVCC__ */
#endif  /* KP_LIQUIDGLASS_KERNEL_CU */
