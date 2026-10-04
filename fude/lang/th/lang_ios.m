// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

// Apple's headers before RDE's: rde.h defines `any`, a word their availability
// pragmas use.
#import <TargetConditionals.h>
#if TARGET_OS_IOS && !TARGET_OS_SIMULATOR   // ML Kit has no Simulator build
#import <Foundation/Foundation.h>
#import <MLKitTextRecognitionCommon/MLKitTextRecognitionCommon.h>   // the options' type, MLKCommonTextRecognizerOptions

#include "lang/lang.h"

// ===========================================================================
// See lang.h: Thai's iOS side. ML Kit Text Recognition has no Thai model
// (fude_lang_text_readable is false: Text from a photo is not offered), so no
// options: nothing asks for them.
// ===========================================================================

MLKCommonTextRecognizerOptions* fude_lang_text_options(void) {
    return nil;
}

#endif
