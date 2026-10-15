// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

// Apple's headers come before RDE's, because rde.h's `any` macro would otherwise reach the availability attributes
// in them.
#import <TargetConditionals.h>
#if TARGET_OS_IOS
#import <AVFoundation/AVFoundation.h>
#endif

#include "study/services/speech.h"
#include "lang/lang.h"

// ===========================================================================
// See speech.h. The iOS side: AVSpeechSynthesizer with the best voice the device has for the language taught.
//
// Asking iOS for a voice by language (Japanese: "ja-JP") gives the small built-in one, which sounds thin. The
// Enhanced and Premium ones (Japanese: Kyoko, Otoya...) are a free download in Settings › Accessibility › Spoken
// Content › Voices › <the language>, and once there any app may use them. So the best voice present is picked each
// time something is read, and one downloaded while the app is open counts at once. While only the basic one is
// there, the app says once a run where the better ones are (fude_speech_take_hint). Siri's voices are not open to
// apps.
//
// It reads at 0.85 of the default rate, a little slower, because a learner listens for each sound.
// ===========================================================================

#if defined(RDE_PLATFORM_IOS)

// Made the first time something is read, and kept for the app's life.
static AVSpeechSynthesizer* fude_speech_synth;
static b8                   fude_speech_hint_due  = false;   // A basic voice spoke: say where better ones are...
static b8                   fude_speech_hint_said = false;   // ...but only once a run.

// The language's voice code (lang.h: fude_lang_voice, "ja-JP" for Japanese).
static NSString* fude_speech_language(void) {
    return [NSString stringWithUTF8String:fude_lang_voice()];
}

// The best voice there is for the language: Premium, then Enhanced, then the basic one. Never a Personal Voice,
// which is the learner's own voice, for their own use. nil when the device has none.
static AVSpeechSynthesisVoice* fude_speech_voice(void) {
    AVSpeechSynthesisVoice* _best = nil;
    for(AVSpeechSynthesisVoice* _v in [AVSpeechSynthesisVoice speechVoices]) {
        if(![_v.language isEqualToString:fude_speech_language()]) {
            continue;
        }
        if(@available(iOS 17.0, *)) {
            if((_v.voiceTraits & AVSpeechSynthesisVoiceTraitIsPersonalVoice) != 0) {
                continue;
            }
        }
        if(_best == nil || _v.quality > _best.quality) {
            _best = _v;
        }
    }
    return _best != nil ? _best : [AVSpeechSynthesisVoice voiceWithLanguage:fude_speech_language()];
}

// See the header.
b8 fude_speech_available(void) {
    return [AVSpeechSynthesisVoice voiceWithLanguage:fude_speech_language()] != nil;
}

// Asked of iOS each time, so a voice downloaded in Settings shows as soon as the app comes back.
FUDE_SPEECH_ fude_speech_state(void) {
    return fude_speech_available() ? FUDE_SPEECH_READY : FUDE_SPEECH_MISSING;
}

// An app has no way into Settings' voices, so this is always false; the voice card says where to go instead.
b8 fude_speech_open_settings(void) {
    return false;   // The card says where the voices are.
}

// See the header. Reads with the best voice present, and notes when only the basic one was there.
void fude_speak(const c8* _text) {
    if(_text == NULL || _text[0] == 0) {
        return;
    }
    @autoreleasepool {
        NSString*               _string = [NSString stringWithUTF8String:_text];
        AVSpeechSynthesisVoice* _voice  = fude_speech_voice();
        if(_string == nil || _voice == nil) {
            return;
        }
        if(_voice.quality == AVSpeechSynthesisVoiceQualityDefault && !fude_speech_hint_said) {
            fude_speech_hint_due = true;
        }
        if(fude_speech_synth == nil) {
            fude_speech_synth = [[AVSpeechSynthesizer alloc] init];
        }
        if(fude_speech_synth.isSpeaking) {
            [fude_speech_synth stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
        }
        AVSpeechUtterance* _utterance = [AVSpeechUtterance speechUtteranceWithString:_string];
        _utterance.voice = _voice;
        _utterance.rate  = AVSpeechUtteranceDefaultSpeechRate * 0.85f;
        [fude_speech_synth speakUtterance:_utterance];
    }
}

// See the header.
void fude_speech_stop(void) {
    if(fude_speech_synth != nil && fude_speech_synth.isSpeaking) {
        [fude_speech_synth stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
    }
}

// See the header. True once a run, after a basic voice has spoken.
b8 fude_speech_take_hint(void) {
    if(!fude_speech_hint_due) {
        return false;
    }
    fude_speech_hint_due  = false;
    fude_speech_hint_said = true;
    return true;
}

#endif
