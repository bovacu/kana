// Apple's and ML Kit's headers before RDE's: rde.h's `any` macro would
// otherwise reach the availability attributes in them.
#import <TargetConditionals.h>
#if TARGET_OS_IOS
#import <Foundation/Foundation.h>
#import <MLKitCommon/MLKitCommon.h>
#import <MLKitTranslate/MLKitTranslate.h>
#endif

#include "translate.h"

// ===========================================================================
// See translate.h. The iOS side: ML Kit's on-device translator, Japanese into
// the target. ML Kit keeps its models where it likes and answers on the main
// queue — where Kana's frame runs — so the answers simply wait in a queue for
// kana_translate_poll. The builder compiles Objective-C with ARC: the statics
// below keep what is kept.
// ===========================================================================

#if defined(RDE_PLATFORM_IOS)

#include <string.h>

#define KANA_TRANSLATE_QUEUE 64u   // answers waiting for a poll, at most

// The translator: one language pair at a time (the reader's), made again when
// the target changes.
static MLKTranslator*        kana_translate_translator;
static c8                    kana_translate_translator_to[8];
// A download under way, or the last one's end, for that target.
static c8                    kana_translate_download_to[8];
static KANA_TRANSLATE_STATE_ kana_translate_download = KANA_TRANSLATE_MISSING;
// The answers in, oldest first.
static u32                   kana_translate_next_ticket = 1u;
static u32                   kana_translate_head;
static u32                   kana_translate_count;
static u32                   kana_translate_tickets[KANA_TRANSLATE_QUEUE];
static c8                    kana_translate_answers[KANA_TRANSLATE_QUEUE][KANA_TRANSLATE_TEXT];

// _src (UTF-8) into _dst, cut at a whole character when it does not fit.
static void kana_translate_copy(c8* _dst, usize _size, const c8* _src) {
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

// ML Kit's name for a language Kana translates into (NULL: not one of them).
static MLKTranslateLanguage kana_translate_language(const c8* _code) {
    if(strcmp(_code, "en") == 0) { return MLKTranslateLanguageEnglish; }
    if(strcmp(_code, "es") == 0) { return MLKTranslateLanguageSpanish; }
    if(strcmp(_code, "pt") == 0) { return MLKTranslateLanguagePortuguese; }
    if(strcmp(_code, "fr") == 0) { return MLKTranslateLanguageFrench; }
    return nil;
}

// Japanese's model and the target's both on the device.
static b8 kana_translate_downloaded(MLKTranslateLanguage _to) {
    MLKModelManager* _manager = [MLKModelManager modelManager];
    return [_manager isModelDownloaded:[MLKTranslateRemoteModel translateRemoteModelWithLanguage:MLKTranslateLanguageJapanese]] &&
           [_manager isModelDownloaded:[MLKTranslateRemoteModel translateRemoteModelWithLanguage:_to]];
}

// The translator for Japanese into _code, made when it is another target.
static MLKTranslator* kana_translate_for(const c8* _code) {
    if(kana_translate_translator == nil || strcmp(kana_translate_translator_to, _code) != 0) {
        MLKTranslatorOptions* _options = [[MLKTranslatorOptions alloc] initWithSourceLanguage:MLKTranslateLanguageJapanese
                                                                               targetLanguage:kana_translate_language(_code)];
        kana_translate_translator = [MLKTranslator translatorWithOptions:_options];
        strncpy(kana_translate_translator_to, _code, sizeof(kana_translate_translator_to) - 1u);
    }
    return kana_translate_translator;
}

b8 kana_translate_platform_available(void) {
    return true;
}

KANA_TRANSLATE_STATE_ kana_translate_platform_state(const c8* _target) {
    MLKTranslateLanguage _to = kana_translate_language(_target);
    if(_to == nil) {
        return KANA_TRANSLATE_UNAVAILABLE;
    }
    const b8 _this_one = strcmp(kana_translate_download_to, _target) == 0;
    if(_this_one && kana_translate_download == KANA_TRANSLATE_DOWNLOADING) {
        return KANA_TRANSLATE_DOWNLOADING;
    }
    if(kana_translate_downloaded(_to)) {
        return KANA_TRANSLATE_READY;
    }
    return _this_one && kana_translate_download == KANA_TRANSLATE_FAILED ? KANA_TRANSLATE_FAILED : KANA_TRANSLATE_MISSING;
}

void kana_translate_platform_prepare(const c8* _target) {
    const KANA_TRANSLATE_STATE_ _state = kana_translate_platform_state(_target);
    if(_state != KANA_TRANSLATE_MISSING && _state != KANA_TRANSLATE_FAILED) {
        return;   // ready, on its way, or not a language Kana translates into
    }
    strncpy(kana_translate_download_to, _target, sizeof(kana_translate_download_to) - 1u);
    kana_translate_download_to[sizeof(kana_translate_download_to) - 1u] = 0;
    kana_translate_download = KANA_TRANSLATE_DOWNLOADING;
    // Over Wi-Fi or the phone network, as the handwriting model is: the screen
    // says how big it is. ML Kit gets whichever of the pair's models is missing.
    MLKModelDownloadConditions* _conditions = [[MLKModelDownloadConditions alloc] initWithAllowsCellularAccess:YES allowsBackgroundDownloading:YES];
    NSString* _for = [NSString stringWithUTF8String:kana_translate_download_to];   // a block cannot keep a C array
    [kana_translate_for(_target) downloadModelIfNeededWithConditions:_conditions completion:^(NSError* _Nullable _error) {
        if(strcmp(kana_translate_download_to, _for.UTF8String) != 0) {
            return;   // another target asked for since
        }
        kana_translate_download = _error == nil ? KANA_TRANSLATE_READY : KANA_TRANSLATE_FAILED;
        if(_error != nil) {
            rde_log_level(RDE_LOG_LEVEL_WARNING, "Translate: the Japanese -> %s models could not be downloaded: %s", _for.UTF8String, _error.localizedDescription.UTF8String);
        } else {
            rde_log_level(RDE_LOG_LEVEL_INFO, "Translate: the Japanese -> %s models are on the device", _for.UTF8String);
        }
    }];
}

u32 kana_translate_platform_text(const c8* _text, const c8* _target) {
    if(kana_translate_platform_state(_target) != KANA_TRANSLATE_READY) {
        return 0u;
    }
    NSString* _source = [NSString stringWithUTF8String:_text];
    if(_source == nil) {
        return 0u;
    }
    const u32 _ticket = kana_translate_next_ticket++;
    [kana_translate_for(_target) translateText:_source completion:^(NSString* _Nullable _result, NSError* _Nullable _error) {
        if(kana_translate_count == KANA_TRANSLATE_QUEUE) {
            return;   // nobody is polling: dropped
        }
        const u32 _at = (kana_translate_head + kana_translate_count++) % KANA_TRANSLATE_QUEUE;
        kana_translate_tickets[_at] = _ticket;
        kana_translate_answers[_at][0] = 0;
        if(_error == nil && _result != nil) {
            kana_translate_copy(kana_translate_answers[_at], KANA_TRANSLATE_TEXT, _result.UTF8String);
        } else if(_error != nil) {
            rde_log_level(RDE_LOG_LEVEL_WARNING, "Translate: a line could not be translated: %s", _error.localizedDescription.UTF8String);
        }
    }];
    return _ticket;
}

b8 kana_translate_platform_poll(u32* _ticket, c8* _out, usize _size) {
    if(kana_translate_count == 0u || _size == 0) {
        return false;
    }
    const u32 _at = kana_translate_head;
    kana_translate_head = (kana_translate_head + 1u) % KANA_TRANSLATE_QUEUE;
    kana_translate_count--;
    *_ticket = kana_translate_tickets[_at];
    kana_translate_copy(_out, _size, kana_translate_answers[_at]);
    return true;
}

#endif
