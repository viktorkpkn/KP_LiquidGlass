/*
** KP_LiquidGlass — Copyright (c) 2026 Viktor Kopeikin.
** Licensed under the PolyForm Noncommercial License 1.0.0.
*/

#include "AEConfig.h"
#include "AE_EffectVers.h"

#ifndef AE_OS_WIN
    #include "AE_General.r"
#endif
resource 'PiPL' (16000) {
    {    /* array properties: 12 elements */
        /* [1] */
        Kind {
            AEEffect
        },
        /* [2] */
        Name {
            "KP_LiquidGlass"
        },
        /* [3] */
        Category {
            "KP Effects"
        },
#ifdef AE_OS_WIN
    #if defined(AE_PROC_INTELx64)
        CodeWin64X86 {"EffectMain"},
    #elif defined(AE_PROC_ARM64)
        CodeWinARM64 {"EffectMain"},
    #endif
#elif defined(AE_OS_MAC)
        CodeMacIntel64 {"EffectMain"},
        CodeMacARM64 {"EffectMain"},
#endif
        /* [6] */
        AE_PiPL_Version {
            2,
            0
        },
        /* [7] */
        AE_Effect_Spec_Version {
            PF_PLUG_IN_VERSION,
            PF_PLUG_IN_SUBVERS
        },
        /* [8] */
        AE_Effect_Version {
            525324    /* 1.0.0 beta build 12 = (1<<19)|(PF_Stage_BETA<<9)|12 */
        },
        /* [9] */
        AE_Effect_Info_Flags {
            0
        },
        /* [10] */
        AE_Effect_Global_OutFlags {
            0x2000006
        },
        AE_Effect_Global_OutFlags_2 {
            0x02221400
        },
        /* [11] */
        AE_Effect_Match_Name {
            "KP KP_LiquidGlass"
        },
        /* [12] */
        AE_Reserved_Info {
            0
        },
        /* [13] */
        AE_Effect_Support_URL {
            "https://www.adobe.com"
        }
    }
};
