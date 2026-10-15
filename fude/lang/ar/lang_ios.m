// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

// Apple's and ML Kit's headers come before RDE's, because rde.h defines `any`, a word their availability pragmas
// use.
#import <TargetConditionals.h>
#if TARGET_OS_IOS && !TARGET_OS_SIMULATOR   // ML Kit has no build for the Simulator.
#import <Foundation/Foundation.h>
#import <MLKitTextRecognitionCommon/MLKitTextRecognitionCommon.h>   // For MLKCommonTextRecognizerOptions.

#include "lang/lang.h"

// ===========================================================================
// See lang.h. This is Arabic's iOS side. ML Kit Text Recognition has no Arabic model, so fude_lang_text_readable
// is false and Text from a photo is not offered. Nothing asks for these options, so there are none.
// ===========================================================================

// Returns nil, since ML Kit has no Arabic model to give options for.
MLKCommonTextRecognizerOptions* fude_lang_text_options(void) {
    return nil;
}

#endif
