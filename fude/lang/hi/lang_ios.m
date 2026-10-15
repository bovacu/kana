// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

// Apple's and ML Kit's headers come before RDE's, because rde.h defines `any`, a word their availability pragmas
// use.
#import <TargetConditionals.h>
#if TARGET_OS_IOS && !TARGET_OS_SIMULATOR   // ML Kit has no build for the Simulator.
#import <Foundation/Foundation.h>
#import <MLKitTextRecognitionDevanagari/MLKitTextRecognitionDevanagari.h>

#include "lang/lang.h"

// ===========================================================================
// See lang.h. This is Hindi's iOS side: the options for ML Kit Text Recognition's Devanagari model.
// The model is built into the app, so nothing is downloaded.
// ===========================================================================

// Returns new options for ML Kit's Devanagari text recognizer.
MLKCommonTextRecognizerOptions* fude_lang_text_options(void) {
    return [[MLKDevanagariTextRecognizerOptions alloc] init];
}

#endif
