// Apple's and ML Kit's headers before RDE's: rde.h defines `any`, a word their
// availability pragmas use.
#import <TargetConditionals.h>
#if TARGET_OS_IOS && !TARGET_OS_SIMULATOR   // ML Kit has no Simulator build
#import <Foundation/Foundation.h>
#import <MLKitTextRecognitionDevanagari/MLKitTextRecognitionDevanagari.h>

#include "lang/lang.h"

// ===========================================================================
// See lang.h: Hindi's iOS side. ML Kit Text Recognition's Devanagari model (in
// the app: no download).
// ===========================================================================

MLKCommonTextRecognizerOptions* fude_lang_text_options(void) {
    return [[MLKDevanagariTextRecognizerOptions alloc] init];
}

#endif
