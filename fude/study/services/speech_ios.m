// Apple's headers before RDE's: rde.h's `any` macro would otherwise reach the
// availability attributes in them.
#import <TargetConditionals.h>
#if TARGET_OS_IOS
#import <AVFoundation/AVFoundation.h>
#endif

#include "study/services/speech.h"
#include "lang/lang.h"

// ===========================================================================
// See speech.h. The iOS side: AVSpeechSynthesizer with the best Japanese voice
// the device has. Asking iOS for "a ja-JP voice" gives the small built-in one,
// which sounds thin; the Enhanced and Premium ones (Kyoko, Otoya...) are a free
// download in Settings › Accessibility › Spoken Content › Voices › Japanese,
// and once there any app may use them — so each time the best one present is
// picked (one downloaded while Kana is open counts at once), and while only
// the basic one is there Kana says, once, where the better ones are. (Siri's
// voices are not open to apps.) A little slower than the default: a learner
// listens for each sound.
// ===========================================================================

#if defined(RDE_PLATFORM_IOS)

static AVSpeechSynthesizer* fude_speech_synth;
static b8                   fude_speech_hint_due  = false;   // a basic voice spoke: say where better ones are...
static b8                   fude_speech_hint_said = false;   // ...once a run

// The language's voice (lang.h: "ja-JP").
static NSString* fude_speech_language(void) {
    return [NSString stringWithUTF8String:fude_lang_voice()];
}

// The best voice there is for it: Premium, then Enhanced, then the basic one
// (never a Personal Voice — the learner's own, for their own use).
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

b8 fude_speech_available(void) {
    return [AVSpeechSynthesisVoice voiceWithLanguage:fude_speech_language()] != nil;
}

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

void fude_speech_stop(void) {
    if(fude_speech_synth != nil && fude_speech_synth.isSpeaking) {
        [fude_speech_synth stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
    }
}

b8 fude_speech_take_hint(void) {
    if(!fude_speech_hint_due) {
        return false;
    }
    fude_speech_hint_due  = false;
    fude_speech_hint_said = true;
    return true;
}

#endif
