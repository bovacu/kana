// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

// Apple's and ML Kit's headers before RDE's: rde.h defines `any`, a word their
// availability pragmas use.
#import <TargetConditionals.h>
#if TARGET_OS_IOS && !TARGET_OS_SIMULATOR   // ML Kit has no Simulator build
#import <Foundation/Foundation.h>
#import <MLKitTextRecognitionChinese/MLKitTextRecognitionChinese.h>

#include "lang/lang.h"

// ===========================================================================
// See lang.h: Chinese's iOS side. ML Kit Text Recognition's Chinese model, which
// reads simplified and traditional alike (in the app: no download).
// ===========================================================================

MLKCommonTextRecognizerOptions* fude_lang_text_options(void) {
    return [[MLKChineseTextRecognizerOptions alloc] init];
}

#endif
