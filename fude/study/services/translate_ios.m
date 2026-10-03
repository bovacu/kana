// Apple's and ML Kit's headers before RDE's: rde.h's `any` macro would
// otherwise reach the availability attributes in them.
#import <TargetConditionals.h>
#if TARGET_OS_IOS && !TARGET_OS_SIMULATOR
#import <Foundation/Foundation.h>
#import <MLKitCommon/MLKitCommon.h>
#import <MLKitTranslate/MLKitTranslate.h>
#endif

#include "study/services/translate.h"

// ===========================================================================
// See translate.h. The iOS side: ML Kit's on-device translator, for a pair of
// languages (Japanese and the reader's, either way round). ML Kit keeps its
// models where it likes and answers on the main queue — where Kana's frame runs —
// so the answers simply wait in a queue for fude_translate_platform_poll. The builder compiles Objective-C with ARC: the statics
// below keep what is kept.
// ===========================================================================

#if defined(RDE_PLATFORM_IOS) && !defined(RDE_PLATFORM_IOS_SIMULATOR)   // ML Kit has no Simulator build: the stand-ins there (.c)

#include <string.h>

#define FUDE_TRANSLATE_QUEUE 64u   // answers waiting for a poll, at most

// The translator: one language pair at a time ("ja>en"), made again when the
// pair changes.
static MLKTranslator*        fude_translate_translator;
static c8                    fude_translate_translator_pair[16];
// A download under way, or the last one's end, for that pair.
static c8                    fude_translate_download_pair[16];
static FUDE_TRANSLATE_STATE_ fude_translate_download = FUDE_TRANSLATE_MISSING;
// The answers in, oldest first.
static u32                   fude_translate_next_ticket = 1u;
static u32                   fude_translate_head;
static u32                   fude_translate_count;
static u32                   fude_translate_tickets[FUDE_TRANSLATE_QUEUE];
static c8                    fude_translate_answers[FUDE_TRANSLATE_QUEUE][FUDE_TRANSLATE_TEXT];

// _src (UTF-8) into _dst, cut at a whole character when it does not fit.
static void fude_translate_copy(c8* _dst, usize _size, const c8* _src) {
    usize _n = _src != NULL ? strlen(_src) : 0;
    if(_n >= _size) {
        _n = _size - 1u;
        while(_n > 0 && ((u8)_src[_n] & 0xC0u) == 0x80u) {
            _n--;   // not inside a character
        }
    }
    if(_n > 0) {
        memcpy(_dst, _src, _n);
    }
    _dst[_n] = 0;
}

// ML Kit's name for a language the apps translate between (NULL: not one of them):
// the one each teaches (lang.h), and the readers'.
static MLKTranslateLanguage fude_translate_language(const c8* _code) {
    if(strcmp(_code, "ja") == 0) { return MLKTranslateLanguageJapanese; }
    if(strcmp(_code, "zh") == 0) { return MLKTranslateLanguageChinese; }
    if(strcmp(_code, "ko") == 0) { return MLKTranslateLanguageKorean; }
    if(strcmp(_code, "en") == 0) { return MLKTranslateLanguageEnglish; }
    if(strcmp(_code, "es") == 0) { return MLKTranslateLanguageSpanish; }
    if(strcmp(_code, "pt") == 0) { return MLKTranslateLanguagePortuguese; }
    if(strcmp(_code, "fr") == 0) { return MLKTranslateLanguageFrench; }
    return nil;
}

// Both languages' models on the device.
static b8 fude_translate_downloaded(MLKTranslateLanguage _from, MLKTranslateLanguage _to) {
    MLKModelManager* _manager = [MLKModelManager modelManager];
    return [_manager isModelDownloaded:[MLKTranslateRemoteModel translateRemoteModelWithLanguage:_from]] &&
           [_manager isModelDownloaded:[MLKTranslateRemoteModel translateRemoteModelWithLanguage:_to]];
}

// A pair's name ("ja>en"), for the translator and the download made for it.
static void fude_translate_pair(c8* _out, usize _size, const c8* _from, const c8* _to) {
    snprintf(_out, _size, "%s>%s", _from, _to);
}

// The translator for _from into _to, made when it is another pair.
static MLKTranslator* fude_translate_for(const c8* _from, const c8* _to) {
    c8 _pair[16];
    fude_translate_pair(_pair, sizeof(_pair), _from, _to);
    if(fude_translate_translator == nil || strcmp(fude_translate_translator_pair, _pair) != 0) {
        MLKTranslatorOptions* _options = [[MLKTranslatorOptions alloc] initWithSourceLanguage:fude_translate_language(_from)
                                                                               targetLanguage:fude_translate_language(_to)];
        fude_translate_translator = [MLKTranslator translatorWithOptions:_options];
        snprintf(fude_translate_translator_pair, sizeof(fude_translate_translator_pair), "%s", _pair);
    }
    return fude_translate_translator;
}

b8 fude_translate_platform_available(void) {
    return true;
}

FUDE_TRANSLATE_STATE_ fude_translate_platform_state(const c8* _from, const c8* _to) {
    MLKTranslateLanguage _a = fude_translate_language(_from);
    MLKTranslateLanguage _b = fude_translate_language(_to);
    if(_a == nil || _b == nil) {
        return FUDE_TRANSLATE_UNAVAILABLE;
    }
    c8 _pair[16];
    fude_translate_pair(_pair, sizeof(_pair), _from, _to);
    const b8 _this_one = strcmp(fude_translate_download_pair, _pair) == 0;
    if(_this_one && fude_translate_download == FUDE_TRANSLATE_DOWNLOADING) {
        return FUDE_TRANSLATE_DOWNLOADING;
    }
    if(fude_translate_downloaded(_a, _b)) {
        return FUDE_TRANSLATE_READY;
    }
    return _this_one && fude_translate_download == FUDE_TRANSLATE_FAILED ? FUDE_TRANSLATE_FAILED : FUDE_TRANSLATE_MISSING;
}

void fude_translate_platform_prepare(const c8* _from, const c8* _to) {
    const FUDE_TRANSLATE_STATE_ _state = fude_translate_platform_state(_from, _to);
    if(_state != FUDE_TRANSLATE_MISSING && _state != FUDE_TRANSLATE_FAILED) {
        return;   // ready, on its way, or not a pair Kana translates between
    }
    fude_translate_pair(fude_translate_download_pair, sizeof(fude_translate_download_pair), _from, _to);
    fude_translate_download = FUDE_TRANSLATE_DOWNLOADING;
    // Over Wi-Fi or the phone network, as the handwriting model is: the screen
    // says how big it is. ML Kit gets whichever of the pair's models is missing.
    MLKModelDownloadConditions* _conditions = [[MLKModelDownloadConditions alloc] initWithAllowsCellularAccess:YES allowsBackgroundDownloading:YES];
    NSString* _for = [NSString stringWithUTF8String:fude_translate_download_pair];   // a block cannot keep a C array
    [fude_translate_for(_from, _to) downloadModelIfNeededWithConditions:_conditions completion:^(NSError* _Nullable _error) {
        if(strcmp(fude_translate_download_pair, _for.UTF8String) != 0) {
            return;   // another pair asked for since
        }
        fude_translate_download = _error == nil ? FUDE_TRANSLATE_READY : FUDE_TRANSLATE_FAILED;
        if(_error != nil) {
            rde_log_level(RDE_LOG_LEVEL_WARNING, "Translate: the %s models could not be downloaded: %s", _for.UTF8String, _error.localizedDescription.UTF8String);
        } else {
            rde_log_level(RDE_LOG_LEVEL_INFO, "Translate: the %s models are on the device", _for.UTF8String);
        }
    }];
}

u32 fude_translate_platform_text(const c8* _text, const c8* _from, const c8* _to) {
    if(fude_translate_platform_state(_from, _to) != FUDE_TRANSLATE_READY) {
        return 0u;
    }
    NSString* _source = [NSString stringWithUTF8String:_text];
    if(_source == nil) {
        return 0u;
    }
    const u32 _ticket = fude_translate_next_ticket++;
    [fude_translate_for(_from, _to) translateText:_source completion:^(NSString* _Nullable _result, NSError* _Nullable _error) {
        if(fude_translate_count == FUDE_TRANSLATE_QUEUE) {
            return;   // nobody is polling: dropped
        }
        const u32 _at = (fude_translate_head + fude_translate_count++) % FUDE_TRANSLATE_QUEUE;
        fude_translate_tickets[_at] = _ticket;
        fude_translate_answers[_at][0] = 0;
        if(_error == nil && _result != nil) {
            fude_translate_copy(fude_translate_answers[_at], FUDE_TRANSLATE_TEXT, _result.UTF8String);
        } else if(_error != nil) {
            rde_log_level(RDE_LOG_LEVEL_WARNING, "Translate: a line could not be translated: %s", _error.localizedDescription.UTF8String);
        }
    }];
    return _ticket;
}

b8 fude_translate_platform_poll(u32* _ticket, c8* _out, usize _size) {
    if(fude_translate_count == 0u || _size == 0) {
        return false;
    }
    const u32 _at = fude_translate_head;
    fude_translate_head = (fude_translate_head + 1u) % FUDE_TRANSLATE_QUEUE;
    fude_translate_count--;
    *_ticket = fude_translate_tickets[_at];
    fude_translate_copy(_out, _size, fude_translate_answers[_at]);
    return true;
}

#endif
