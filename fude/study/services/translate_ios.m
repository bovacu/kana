// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

// Apple's and ML Kit's headers come before RDE's, because rde.h's `any` macro would otherwise reach the availability
// attributes in them.
#import <TargetConditionals.h>
#if TARGET_OS_IOS && !TARGET_OS_SIMULATOR
#import <Foundation/Foundation.h>
#import <MLKitCommon/MLKitCommon.h>
#import <MLKitTranslate/MLKitTranslate.h>
#endif

#include "study/services/translate.h"
#include "drawing/base/utf8.h"

// ===========================================================================
// See translate.h. The iOS side: ML Kit's on-device translator, for a pair of languages (the language taught and the
// reader's, either way round). ML Kit keeps its models where it likes and answers on the main queue, where the app's
// frame runs, so the answers simply wait in a queue here for fude_translate_platform_poll. The builder compiles
// Objective-C with ARC, so the statics below are what keeps each object alive.
// ===========================================================================

// ML Kit has no Simulator build, so the Simulator uses the stand-ins in translate.c.
#if defined(RDE_PLATFORM_IOS) && !defined(RDE_PLATFORM_IOS_SIMULATOR)   // A device only.

// The translator: one language pair at a time ("ja>en"), made again when the pair changes.
static MLKTranslator*        fude_translate_translator;
static c8                    fude_translate_translator_pair[16];
// The pair whose models are being downloaded, or were last, and how that download stands.
static c8                    fude_translate_download_pair[16];
static FUDE_TRANSLATE_STATE_ fude_translate_download = FUDE_TRANSLATE_MISSING;
// An answer that has come in: its ticket and its text (empty when the line could not be translated).
typedef struct {
    u32 ticket;
    c8  text[FUDE_TRANSLATE_TEXT];
} fude_translate_answer;
// The answers that have come in, oldest first, until a poll takes them (an rde_arr on the std allocator, made with the
// first answer, kept for the run). It has no bound: every poll takes them all into translate.c's box, which keeps
// what is waited for. Tickets count up from 1.
static u32                   fude_translate_next_ticket = 1u;
static rde_arr TYPE(fude_translate_answer) fude_translate_answers;

// ML Kit's name for a language code the apps translate between: the one each teaches (lang.h) and the readers'.
// nil when it is not one of those.
static MLKTranslateLanguage fude_translate_language(const c8* _code) {
    if(rde_strcmp(_code, "ja") == 0) { return MLKTranslateLanguageJapanese; }
    if(rde_strcmp(_code, "zh") == 0) { return MLKTranslateLanguageChinese; }
    if(rde_strcmp(_code, "ko") == 0) { return MLKTranslateLanguageKorean; }
    if(rde_strcmp(_code, "th") == 0) { return MLKTranslateLanguageThai; }
    if(rde_strcmp(_code, "hi") == 0) { return MLKTranslateLanguageHindi; }
    if(rde_strcmp(_code, "ar") == 0) { return MLKTranslateLanguageArabic; }
    if(rde_strcmp(_code, "en") == 0) { return MLKTranslateLanguageEnglish; }
    if(rde_strcmp(_code, "es") == 0) { return MLKTranslateLanguageSpanish; }
    if(rde_strcmp(_code, "pt") == 0) { return MLKTranslateLanguagePortuguese; }
    if(rde_strcmp(_code, "fr") == 0) { return MLKTranslateLanguageFrench; }
    return nil;
}

// Whether both languages' models are on the device.
static b8 fude_translate_downloaded(MLKTranslateLanguage _from, MLKTranslateLanguage _to) {
    MLKModelManager* _manager = [MLKModelManager modelManager];
    return [_manager isModelDownloaded:[MLKTranslateRemoteModel translateRemoteModelWithLanguage:_from]] &&
           [_manager isModelDownloaded:[MLKTranslateRemoteModel translateRemoteModelWithLanguage:_to]];
}

// Writes a pair's name ("ja>en") into _out, to tell which pair the translator and the download were made for.
static void fude_translate_pair(c8* _out, usize _size, const c8* _from, const c8* _to) {
    rde_snprintf(_out, _size, "%s>%s", _from, _to);
}

// The translator for _from into _to, made anew when the pair is not the one it was made for.
static MLKTranslator* fude_translate_for(const c8* _from, const c8* _to) {
    c8 _pair[16];
    fude_translate_pair(_pair, sizeof(_pair), _from, _to);
    if(fude_translate_translator == nil || rde_strcmp(fude_translate_translator_pair, _pair) != 0) {
        MLKTranslatorOptions* _options = [[MLKTranslatorOptions alloc] initWithSourceLanguage:fude_translate_language(_from)
                                                                               targetLanguage:fude_translate_language(_to)];
        fude_translate_translator = [MLKTranslator translatorWithOptions:_options];
        rde_snprintf(fude_translate_translator_pair, sizeof(fude_translate_translator_pair), "%s", _pair);
    }
    return fude_translate_translator;
}

// An iOS device always has ML Kit's translator.
b8 fude_translate_platform_available(void) {
    return true;
}

// A pair's state: unavailable when ML Kit does not know a language, downloading while this pair's download is under
// way, ready once both models are on the device, failed when this pair's last download failed, else missing.
FUDE_TRANSLATE_STATE_ fude_translate_platform_state(const c8* _from, const c8* _to) {
    MLKTranslateLanguage _a = fude_translate_language(_from);
    MLKTranslateLanguage _b = fude_translate_language(_to);
    if(_a == nil || _b == nil) {
        return FUDE_TRANSLATE_UNAVAILABLE;
    }
    c8 _pair[16];
    fude_translate_pair(_pair, sizeof(_pair), _from, _to);
    const b8 _this_one = rde_strcmp(fude_translate_download_pair, _pair) == 0;
    if(_this_one && fude_translate_download == FUDE_TRANSLATE_DOWNLOADING) {
        return FUDE_TRANSLATE_DOWNLOADING;
    }
    if(fude_translate_downloaded(_a, _b)) {
        return FUDE_TRANSLATE_READY;
    }
    return _this_one && fude_translate_download == FUDE_TRANSLATE_FAILED ? FUDE_TRANSLATE_FAILED : FUDE_TRANSLATE_MISSING;
}

// Downloads the pair's models when they are missing or the last download failed.
void fude_translate_platform_prepare(const c8* _from, const c8* _to) {
    const FUDE_TRANSLATE_STATE_ _state = fude_translate_platform_state(_from, _to);
    if(_state != FUDE_TRANSLATE_MISSING && _state != FUDE_TRANSLATE_FAILED) {
        return;   // Ready, on its way, or not a pair the app translates between.
    }
    fude_translate_pair(fude_translate_download_pair, sizeof(fude_translate_download_pair), _from, _to);
    fude_translate_download = FUDE_TRANSLATE_DOWNLOADING;
    // It may download over Wi-Fi or the phone network, as the handwriting model does, since the screen says how big
    // it is. ML Kit gets whichever of the pair's models is missing.
    MLKModelDownloadConditions* _conditions = [[MLKModelDownloadConditions alloc] initWithAllowsCellularAccess:YES allowsBackgroundDownloading:YES];
    NSString* _for = [NSString stringWithUTF8String:fude_translate_download_pair];   // A block cannot keep a C array.
    [fude_translate_for(_from, _to) downloadModelIfNeededWithConditions:_conditions completion:^(NSError* _Nullable _error) {
        if(rde_strcmp(fude_translate_download_pair, _for.UTF8String) != 0) {
            return;   // Another pair was asked for since.
        }
        fude_translate_download = _error == nil ? FUDE_TRANSLATE_READY : FUDE_TRANSLATE_FAILED;
        if(_error != nil) {
            rde_log_level(RDE_LOG_LEVEL_WARNING, "Translate: the %s models could not be downloaded: %s", _for.UTF8String, _error.localizedDescription.UTF8String);
        } else {
            rde_log_level(RDE_LOG_LEVEL_INFO, "Translate: the %s models are on the device", _for.UTF8String);
        }
    }];
}

// Hands _text to the pair's translator and returns its ticket; 0 when the pair is not ready. The answer joins the
// queue when it comes (an empty text when it failed).
u32 fude_translate_platform_text(const c8* _text, const c8* _from, const c8* _to) {
    if(fude_translate_platform_state(_from, _to) != FUDE_TRANSLATE_READY) {
        return 0u;
    }
    NSString* _source = [NSString stringWithUTF8String:_text];
    if(_source == nil) {
        return 0u;
    }
    // Tickets count up from 1 and skip 0 when they wrap, since 0 means no ticket (as on Android).
    const u32 _ticket = fude_translate_next_ticket != 0u ? fude_translate_next_ticket : 1u;
    fude_translate_next_ticket = _ticket + 1u;
    [fude_translate_for(_from, _to) translateText:_source completion:^(NSString* _Nullable _result, NSError* _Nullable _error) {
        // ML Kit answers on the main queue, the app's thread, so the queue is only ever touched there.
        if(!rde_arr_is_inited(&fude_translate_answers)) {
            fude_translate_answers = rde_arr_new(sizeof(fude_translate_answer), rde_memory_allocator_get_default_std());
        }
        fude_translate_answer* _answer = (fude_translate_answer*)rde_arr_add_n(&fude_translate_answers, 1u);
        _answer->ticket  = _ticket;
        _answer->text[0] = 0;
        if(_error == nil && _result != nil) {
            fude_utf8_set(_answer->text, FUDE_TRANSLATE_TEXT, _result.UTF8String);   // cut, if at all, at a whole character
        } else if(_error != nil) {
            rde_log_level(RDE_LOG_LEVEL_WARNING, "Translate: a line could not be translated: %s", _error.localizedDescription.UTF8String);
        }
    }];
    return _ticket;
}

// Takes the oldest answer in the queue: its ticket into *_ticket and its text into _out. False when none is waiting.
b8 fude_translate_platform_poll(u32* _ticket, c8* _out, usize _size) {
    if(!rde_arr_is_inited(&fude_translate_answers) || rde_arr_length(&fude_translate_answers) == 0u || _size == 0) {
        return false;
    }
    const fude_translate_answer* _oldest = (const fude_translate_answer*)rde_arr_first(&fude_translate_answers);
    *_ticket = _oldest->ticket;
    fude_utf8_set(_out, _size, _oldest->text);
    rde_arr_remove(&fude_translate_answers, 0u);
    return true;
}

#endif
