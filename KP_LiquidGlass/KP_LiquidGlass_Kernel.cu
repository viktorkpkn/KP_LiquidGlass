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
            a00 = KPClamp(ReadFloat4(inMask, y0 * inMaskPitch + x0, false).w, 0.0f, 1.0f);
        }
        if (x0 + 1 >= 0 && x0 + 1 < w && y0 >= 0 && y0 < h) {
            a10 = KPClamp(ReadFloat4(inMask, y0 * inMaskPitch + x0 + 1, false).w, 0.0f, 1.0f);
        }
        if (x0 >= 0 && x0 < w && y0 + 1 >= 0 && y0 + 1 < h) {
            a01 = KPClamp(ReadFloat4(inMask, (y0 + 1) * inMaskPitch + x0, false).w, 0.0f, 1.0f);
        }
        if (x0 + 1 >= 0 && x0 + 1 < w && y0 + 1 >= 0 && y0 + 1 < h) {
            a11 = KPClamp(ReadFloat4(inMask, (y0 + 1) * inMaskPitch + x0 + 1, false).w, 0.0f, 1.0f);
        }

        WriteFloat(KPMix(KPMix(a00, a10, tx), KPMix(a01, a11, tx), ty),
                   outMatte, (int)(inXY.y * inWidth + inXY.x), false);
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

        float a = ReadFloat(inMatte, index, false);
        bool boundary = false;
        float aL = 0.0f;
        float aR = 0.0f;
        float aU = 0.0f;
        float aD = 0.0f;
        if (a >= 0.5f)
        {
            aL = (x > 0) ? ReadFloat(inMatte, index - 1, false) : 0.0f;
            aR = (x < w - 1) ? ReadFloat(inMatte, index + 1, false) : 0.0f;
            aU = (y > 0) ? ReadFloat(inMatte, index - w, false) : 0.0f;
            aD = (y < h - 1) ? ReadFloat(inMatte, index + w, false) : 0.0f;
            boundary = (aL < 0.5f) || (aR < 0.5f) || (aU < 0.5f) || (aD < 0.5f);
        }

        float2 seed;
        seed.x = -1.0e6f;
        seed.y = -1.0e6f;
        if (boundary)
        {
            /* Place the seed on the 0.5 iso-line recovered from the AA
            ** matte instead of at the integer pixel center: integer seeds
            ** freeze between half-pixel crossings and then lurch a whole
            ** pixel, which jitters the distance field — and displacement,
            ** light normal and stroke with it — under sub-pixel motion. */
            float gx = (aR - aL) * 0.5f;
            float gy = (aD - aU) * 0.5f;
            float g2 = gx * gx + gy * gy;
            float ox = 0.0f;
            float oy = 0.0f;
            if (g2 > 0.0001f) {
                float s = (a - 0.5f) / g2;
                ox = KPClamp(-gx * s, -1.0f, 1.0f);
                oy = KPClamp(-gy * s, -1.0f, 1.0f);
            }
            seed.x = (float)x + ox;
            seed.y = (float)y + oy;
        }
        WriteFloat2(seed, outSeeds, index, false);
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

        float2 best = ReadFloat2(inSrc, index, false);
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
                float2 cand = ReadFloat2(inSrc, ny * w + nx, false);
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

        WriteFloat2(best, outDst, index, false);
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
        if (ReadFloat(inMatte, index, false) < 0.5f) {
            WriteFloat(1.0f, outField, index, false);
        } else {
            float2 best = ReadFloat2(inSeeds, index, false);
            float thickness = fmax(inThickness, 1.0f);
            float d = 1.0e6f;
            if (best.x > -1.0e5f) {
                float ddx = best.x - (float)inXY.x;
                float ddy = best.y - (float)inXY.y;
                d = sqrt(ddx * ddx + ddy * ddy);
            }
            WriteFloat(KPClamp(1.0f - d / thickness, 0.0f, 1.0f), outField, index, false);
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
            sum += ReadFloat(inSrc, y * w + sx, false) * weight;
            weightSum += weight;
        }
        WriteFloat(sum / weightSum, outDst, y * w + x, false);
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
            sum += ReadFloat(inSrc, sy * w + x, false) * weight;
            weightSum += weight;
        }
        WriteFloat(sum / weightSum, outDst, y * w + x, false);
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
        float edge = KPClamp(ReadFloat(inField, index, false), 0.0f, 1.0f);
        float t = pow(edge, k);
        WriteFloat(sqrt(fmax(1.0f - t * t, 0.0f)), outHeight, index, false);
    }
}

/* ------------------------------------------------------------------ */
/* 4b. Inner shadow: the matte sampled at an offset (bilinear); the     */
/*     result is blurred by the shared gaussian kernels afterwards.     */
/* ------------------------------------------------------------------ */

GF_KERNEL_FUNCTION(KPShadowOffsetKernel,
    ((GF_PTR_READ_ONLY(float))(inMatte))
    ((GF_PTR(float))(outShadow)),
    ((unsigned int)(inWidth))
    ((unsigned int)(inHeight))
    ((float)(inOffsetX))
    ((float)(inOffsetY)),
    ((uint2)(inXY)(KERNEL_XY)))
{
    if (inXY.x < inWidth && inXY.y < inHeight)
    {
        int w = (int)inWidth;
        int h = (int)inHeight;
        float sx = (float)inXY.x - inOffsetX;
        float sy = (float)inXY.y - inOffsetY;

        int x0 = (int)floor(sx);
        int y0 = (int)floor(sy);
        float tx = sx - (float)x0;
        float ty = sy - (float)y0;

        float a00 = 0.0f;
        float a10 = 0.0f;
        float a01 = 0.0f;
        float a11 = 0.0f;
        if (x0 >= 0 && x0 < w && y0 >= 0 && y0 < h) {
            a00 = ReadFloat(inMatte, y0 * w + x0, false);
        }
        if (x0 + 1 >= 0 && x0 + 1 < w && y0 >= 0 && y0 < h) {
            a10 = ReadFloat(inMatte, y0 * w + x0 + 1, false);
        }
        if (x0 >= 0 && x0 < w && y0 + 1 >= 0 && y0 + 1 < h) {
            a01 = ReadFloat(inMatte, (y0 + 1) * w + x0, false);
        }
        if (x0 + 1 >= 0 && x0 + 1 < w && y0 + 1 >= 0 && y0 + 1 < h) {
            a11 = ReadFloat(inMatte, (y0 + 1) * w + x0 + 1, false);
        }
        WriteFloat(KPMix(KPMix(a00, a10, tx), KPMix(a01, a11, tx), ty),
                   outShadow, (int)(inXY.y * inWidth + inXY.x), false);
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
            float4 p = ReadFloat4(inSrc, y * inSrcPitch + sx, false);
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
        WriteFloat4(outPix, outDst, y * inDstPitch + x, false);
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
            float4 p = ReadFloat4(inSrc, sy * inSrcPitch + x, false);
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
        WriteFloat4(outPix, outDst, y * inDstPitch + x, false);
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
    return ReadFloat4(inSample, inY * inSamplePitch + inX, false);
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

    /* 26-tap Poisson-disc with gaussian weights (center + 25). The previous
    ** 5-tap version left visible ghost copies ("tripling") of high-contrast
    ** content at larger radii. Local macro is expanded by the C preprocessor
    ** before any of the kernel-generation steps, so it is safe for
    ** CUDA/OpenCL/HLSL. */
    float o = inBlurRadius;
    float4 c = KPSampleBilinear(inSample, sx, sy, inSampleWidth, inSampleHeight, inSamplePitch, inExtendEdges);
    float accX = c.x;
    float accY = c.y;
    float accZ = c.z;
    float accW = c.w;
    float weightSum = 1.0f;

#define KP_EDGE_TAP(OX, OY, WEIGHT) \
    { \
        float4 tap = KPSampleBilinear(inSample, sx + (OX) * o, sy + (OY) * o, \
                                      inSampleWidth, inSampleHeight, inSamplePitch, inExtendEdges); \
        accX += tap.x * (WEIGHT); \
        accY += tap.y * (WEIGHT); \
        accZ += tap.z * (WEIGHT); \
        accW += tap.w * (WEIGHT); \
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

    float4 outPix;
    outPix.x = accX / weightSum;
    outPix.y = accY / weightSum;
    outPix.z = accZ / weightSum;
    outPix.w = accW / weightSum;
    return outPix;
}

/* ------------------------------------------------------------------ */
/* 6. Composite: displacement from the surface slope, dispersion,      */
/*    tint, then the two-layer light screened on top.                  */
/* ------------------------------------------------------------------ */

/* Blend one shadow-color channel onto an unpremultiplied base channel. */
GF_DEVICE_FUNCTION float KPShadowBlendChannel(float inBase, float inShadow, int inMode)
{
    if (inMode == 2) {          /* Multiply */
        return inBase * inShadow;
    }
    if (inMode == 3) {          /* Screen */
        return 1.0f - (1.0f - inBase) * (1.0f - inShadow);
    }
    if (inMode == 4) {          /* Overlay */
        return inBase < 0.5f
            ? 2.0f * inBase * inShadow
            : 1.0f - 2.0f * (1.0f - inBase) * (1.0f - inShadow);
    }
    if (inMode == 5) {          /* Soft Light */
        return (1.0f - 2.0f * inShadow) * inBase * inBase + 2.0f * inShadow * inBase;
    }
    if (inMode == 6) {          /* Hard Light */
        return inShadow < 0.5f
            ? 2.0f * inBase * inShadow
            : 1.0f - 2.0f * (1.0f - inBase) * (1.0f - inShadow);
    }
    return inShadow;            /* Normal */
}

GF_KERNEL_FUNCTION(KPLiquidGlassKernel,
    ((GF_PTR_READ_ONLY(float))(inMatte))
    ((GF_PTR_READ_ONLY(float))(inBevel))
    ((GF_PTR_READ_ONLY(float))(inHeightMap))
    ((GF_PTR_READ_ONLY(float4))(inSample))
    ((GF_PTR_READ_ONLY(float))(inShadow))
    ((GF_PTR_READ_ONLY(float4))(inUnder))
    ((GF_PTR_READ_ONLY(float))(inOuterShadow))
    ((GF_PTR_READ_ONLY(float))(inCaustics))
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
    ((float)(inStrokeWidth))
    ((float)(inShadowR))
    ((float)(inShadowG))
    ((float)(inShadowB))
    ((float)(inShadowOpacity))
    ((int)(inShadowMode))
    ((int)(inCompositeOnTop))
    ((int)(inUnderPitch))
    ((float)(inOuterShadowR))
    ((float)(inOuterShadowG))
    ((float)(inOuterShadowB))
    ((float)(inOuterShadowIntensity))
    ((int)(inOuterShadowMode))
    ((float)(inCausticsR))
    ((float)(inCausticsG))
    ((float)(inCausticsB))
    ((float)(inCausticsIntensity))
    ((int)(inCausticsMode))
    ((int)(inConfineToBounds)),
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
        float maskAlpha = ReadFloat(inMatte, index, false);

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
            float slopeX = (ReadFloat(inHeightMap, y * w + xR, false) - ReadFloat(inHeightMap, y * w + xL, false)) * 0.5f;
            float slopeY = (ReadFloat(inHeightMap, yD * w + x, false) - ReadFloat(inHeightMap, yU * w + x, false)) * 0.5f;

            /* Outward normal for the lighting from the proximity field. */
            float gradX = (ReadFloat(inBevel, y * w + xR, false) - ReadFloat(inBevel, y * w + xL, false)) * 0.5f;
            float gradY = (ReadFloat(inBevel, yD * w + x, false) - ReadFloat(inBevel, yU * w + x, false)) * 0.5f;
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

            float hVal = ReadFloat(inHeightMap, index, false);
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

            /* Outer shadow & caustics live on the backdrop: sample their
            ** fields at the REFRACTED position so the glass visibly bends
            ** them, and blend onto the sampled background color. */
            {
                int fx = (int)(zoomedX + dispX - (float)inOutputLeft + 0.5f);
                int fy = (int)(zoomedY + dispY - (float)inOutputTop + 0.5f);
                fx = fx < 0 ? 0 : (fx > w - 1 ? w - 1 : fx);
                fy = fy < 0 ? 0 : (fy > h - 1 ? h - 1 : fy);
                int fIdx = fy * w + fx;
                float sAmt = KPClamp(ReadFloat(inOuterShadow, fIdx, false), 0.0f, 1.0f)
                           * KPClamp(inOuterShadowIntensity / 100.0f, 0.0f, 1.0f);
                float cAmt = KPClamp(ReadFloat(inCaustics, fIdx, false), 0.0f, 1.0f)
                           * KPClamp(inCausticsIntensity / 100.0f, 0.0f, 1.0f);
                if ((sAmt > 0.0001f || cAmt > 0.0001f) && glass.w > 0.0001f)
                {
                    float invGA = 1.0f / fmax(glass.w, 0.0001f);
                    float gB = glass.x * invGA;
                    float gG = glass.y * invGA;
                    float gR = glass.z * invGA;
                    if (sAmt > 0.0001f) {
                        gB = KPMix(gB, KPShadowBlendChannel(gB, inOuterShadowB, inOuterShadowMode), sAmt);
                        gG = KPMix(gG, KPShadowBlendChannel(gG, inOuterShadowG, inOuterShadowMode), sAmt);
                        gR = KPMix(gR, KPShadowBlendChannel(gR, inOuterShadowR, inOuterShadowMode), sAmt);
                    }
                    if (cAmt > 0.0001f) {
                        gB = KPMix(gB, KPShadowBlendChannel(gB, inCausticsB, inCausticsMode), cAmt);
                        gG = KPMix(gG, KPShadowBlendChannel(gG, inCausticsG, inCausticsMode), cAmt);
                        gR = KPMix(gR, KPShadowBlendChannel(gR, inCausticsR, inCausticsMode), cAmt);
                    }
                    glass.x = gB * glass.w;
                    glass.y = gG * glass.w;
                    glass.z = gR * glass.w;
                }
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
            float fieldE = KPClamp(ReadFloat(inBevel, index, false), 0.0f, 1.0f);
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

            /* Inner shadow: the offset+blurred matte, clipped to the glass,
            ** blended over the composed glass color. */
            float shadowA = KPClamp(ReadFloat(inShadow, index, false), 0.0f, 1.0f)
                          * KPClamp(inShadowOpacity / 100.0f, 0.0f, 1.0f) * maskAlpha;
            if (shadowA > 0.0001f && color.w > 0.0001f)
            {
                float invA2 = 1.0f / fmax(color.w, 0.0001f);
                float baseX2 = color.x * invA2;
                float baseY2 = color.y * invA2;
                float baseZ2 = color.z * invA2;
                float blendX = KPShadowBlendChannel(baseX2, inShadowB, inShadowMode);
                float blendY = KPShadowBlendChannel(baseY2, inShadowG, inShadowMode);
                float blendZ = KPShadowBlendChannel(baseZ2, inShadowR, inShadowMode);
                color.x = KPMix(baseX2, blendX, shadowA) * color.w;
                color.y = KPMix(baseY2, blendY, shadowA) * color.w;
                color.z = KPMix(baseZ2, blendZ, shadowA) * color.w;
            }
        }

        /* Outer shadow & caustics around the shape, at this pixel's own
        ** (unrefracted) position. */
        float ownShadowAmt = KPClamp(ReadFloat(inOuterShadow, index, false), 0.0f, 1.0f)
                           * KPClamp(inOuterShadowIntensity / 100.0f, 0.0f, 1.0f);
        float ownCausticAmt = KPClamp(ReadFloat(inCaustics, index, false), 0.0f, 1.0f)
                            * KPClamp(inCausticsIntensity / 100.0f, 0.0f, 1.0f);

        if (inCompositeOnTop != 0)
        {
            /* Adjustment-layer mode: we own the underlying, so the blends
            ** are exact. Shadow/caustics apply to the surface, then the
            ** glass composites over it. */
            float pX = (float)(inOutputLeft + x);
            float pY = (float)(inOutputTop + y);
            float4 under = KPSampleDisplaced(inUnder, pX, pY, 0.0f,
                                             inC2S00, inC2S01, inC2S02,
                                             inC2S10, inC2S11, inC2S12,
                                             inSampleLeft, inSampleTop,
                                             (int)inSampleWidth, (int)inSampleHeight,
                                             inUnderPitch, inExtendEdges);
            if ((ownShadowAmt > 0.0001f || ownCausticAmt > 0.0001f) && under.w > 0.0001f)
            {
                float invUA = 1.0f / fmax(under.w, 0.0001f);
                float uB = under.x * invUA;
                float uG = under.y * invUA;
                float uR = under.z * invUA;
                if (ownShadowAmt > 0.0001f) {
                    uB = KPMix(uB, KPShadowBlendChannel(uB, inOuterShadowB, inOuterShadowMode), ownShadowAmt);
                    uG = KPMix(uG, KPShadowBlendChannel(uG, inOuterShadowG, inOuterShadowMode), ownShadowAmt);
                    uR = KPMix(uR, KPShadowBlendChannel(uR, inOuterShadowR, inOuterShadowMode), ownShadowAmt);
                }
                if (ownCausticAmt > 0.0001f) {
                    uB = KPMix(uB, KPShadowBlendChannel(uB, inCausticsB, inCausticsMode), ownCausticAmt);
                    uG = KPMix(uG, KPShadowBlendChannel(uG, inCausticsG, inCausticsMode), ownCausticAmt);
                    uR = KPMix(uR, KPShadowBlendChannel(uR, inCausticsR, inCausticsMode), ownCausticAmt);
                }
                under.x = uB * under.w;
                under.y = uG * under.w;
                under.z = uR * under.w;
            }
            color.x = color.x + under.x * (1.0f - color.w);
            color.y = color.y + under.y * (1.0f - color.w);
            color.z = color.z + under.z * (1.0f - color.w);
            color.w = color.w + under.w * (1.0f - color.w);
        }
        else if (!inConfineToBounds && (ownShadowAmt > 0.0001f || ownCausticAmt > 0.0001f))
        {
            /* Shape-layer mode: we don't own the pixels below, so paint
            ** translucent color under the glass (native Drop Shadow style):
            ** caustics over shadow, then the glass over both. This can't
            ** honor blend modes (no real backdrop) and shows as a duplicate
            ** offset-and-blurred copy of the shape's own silhouette --
            ** Confine to Layer Bounds skips it entirely instead. */
            float paintA = ownCausticAmt + ownShadowAmt * (1.0f - ownCausticAmt);
            float paintB = inCausticsB * ownCausticAmt + inOuterShadowB * ownShadowAmt * (1.0f - ownCausticAmt);
            float paintG = inCausticsG * ownCausticAmt + inOuterShadowG * ownShadowAmt * (1.0f - ownCausticAmt);
            float paintR = inCausticsR * ownCausticAmt + inOuterShadowR * ownShadowAmt * (1.0f - ownCausticAmt);
            color.x = color.x + paintB * (1.0f - color.w);
            color.y = color.y + paintG * (1.0f - color.w);
            color.z = color.z + paintR * (1.0f - color.w);
            color.w = color.w + paintA * (1.0f - color.w);
        }

        WriteFloat4(color, outDst, dstIndex, false);
    }
}
#endif  /* GF_DEVICE_TARGET_DEVICE */

/* ------------------------------------------------------------------ */
/* CUDA launchers (host-callable, statically linked on Windows).       */
/* Scalar pack order for KPLiquidGlassRender_CUDA (float[22]):         */
/*  0 refraction  1 thickness  2 zoom       3 zoomCenterX              */
/*  4 zoomCenterY 5 edgeBlur   6 lightDirX  7 lightDirY                */
/*  8 lightR      9 lightG    10 lightB    11 lightIntensity           */
/* 12 tintR      13 tintG     14 tintB     15 tintOpacity              */
/* 16 dispersion 17 strokeWidth                                        */
/* 18 shadowR   19 shadowG   20 shadowB   21 shadowOpacity             */
/* 22 outerShR  23 outerShG  24 outerShB  25 outerShIntensity          */
/* 26 causticR  27 causticG  28 causticB  29 causticIntensity          */
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

void KPShadowOffset_CUDA(
    float const *inMatte, float *outShadow,
    unsigned int inWidth, unsigned int inHeight,
    float inOffsetX, float inOffsetY)
{
    dim3 blockDim(16, 16, 1);
    KPShadowOffsetKernel<<<KPGrid(inWidth, inHeight, blockDim), blockDim, 0>>>(
        inMatte, outShadow, inWidth, inHeight, inOffsetX, inOffsetY);
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
    float const *inMatte, float const *inBevel, float const *inHeightMap,
    float const *inSample, float const *inShadow, float const *inUnder,
    float const *inOuterShadow, float const *inCaustics, float *outDst,
    int inSamplePitch, int inDstPitch,
    unsigned int inWidth, unsigned int inHeight,
    unsigned int inSampleWidth, unsigned int inSampleHeight,
    float const *inScalars,
    int inExtendEdges,
    int inSampleLeft, int inSampleTop, int inOutputLeft, int inOutputTop,
    float const *inC2S,
    int inShadowMode, int inCompositeOnTop, int inUnderPitch,
    int inOuterShadowMode, int inCausticsMode, int inConfineToBounds)
{
    dim3 blockDim(16, 16, 1);
    KPLiquidGlassKernel<<<KPGrid(inWidth, inHeight, blockDim), blockDim, 0>>>(
        inMatte, inBevel, inHeightMap, (float4 const *)inSample, inShadow,
        (float4 const *)inUnder, inOuterShadow, inCaustics, (float4 *)outDst,
        inSamplePitch, inDstPitch, inWidth, inHeight, inSampleWidth, inSampleHeight,
        inScalars[0], inScalars[1], inScalars[2], inScalars[3], inScalars[4],
        inScalars[5], inScalars[6], inScalars[7], inScalars[8], inScalars[9],
        inScalars[10], inScalars[11], inScalars[12], inScalars[13], inScalars[14],
        inScalars[15], inScalars[16],
        inExtendEdges, inSampleLeft, inSampleTop, inOutputLeft, inOutputTop,
        inC2S[0], inC2S[1], inC2S[2], inC2S[3], inC2S[4], inC2S[5],
        inScalars[17],
        inScalars[18], inScalars[19], inScalars[20], inScalars[21],
        inShadowMode, inCompositeOnTop, inUnderPitch,
        inScalars[22], inScalars[23], inScalars[24], inScalars[25], inOuterShadowMode,
        inScalars[26], inScalars[27], inScalars[28], inScalars[29], inCausticsMode,
        inConfineToBounds);
}

#endif  /* __NVCC__ */
#endif  /* KP_LIQUIDGLASS_KERNEL_CU */
