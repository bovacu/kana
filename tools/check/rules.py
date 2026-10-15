#!/usr/bin/env python3
"""Counts what docs/coding_rules.md forbids, so progress toward zero can be seen and nothing creeps back.

    python3 tools/check/rules.py              # the totals, and the worst files of each check
    python3 tools/check/rules.py --all        # every finding, file by file
    python3 tools/check/rules.py --save       # also writes the totals to docs/quality_counts.txt
    python3 tools/check/rules.py --ratchet    # fails (exit 1) if any total is higher than docs/quality_counts.txt's

The checks, each named after its section in coding_rules.md:
    libc-header     a C library or POSIX header included (the type-only ones RDE itself uses are allowed)
    libc-call       a C library or POSIX function called (comments and strings are ignored)
    void-pointer    a void* or const void* written out, where RDE writes any (comments and strings are ignored)
    layer           an #include that goes against the layer table (coding_rules.md §1)
    app-identity    an app's name or symbols inside fude/ (log prefixes, product checks, KANA_/SKETCHING_ symbols)
    file-size       a source file over FILE_LINES lines (generated files are skipped)
    function-size   a function over FUNCTION_LINES lines
    undocumented    a public function in a header with no comment above it (or above its group of declarations)

Only the repository's own code is checked: fude/ and apps/ (.c, .h, .m), not tests/, tools/, build/ or third_party/.
"""
import collections
import os
import re
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
COUNTS_FILE = os.path.join(ROOT, 'docs', 'quality_counts.txt')
FILE_LINES = 2000
FUNCTION_LINES = 150

# Headers app code may include: the type-only headers RDE itself is built on.
ALLOWED_HEADERS = {'stdint.h', 'stdbool.h', 'stdarg.h', 'stddef.h'}
# Platform frameworks and third-party libraries are not the C library: a platform file's job is to call them.
PLATFORM_HEADER = re.compile(r'^(jni\.h|android/|Foundation/|UIKit/|AppKit/|AVFoundation/|CoreFoundation/|'
                             r'CoreGraphics/|CoreText/|CoreMedia/|CoreVideo/|ImageIO/|PDFKit/|PhotosUI/|VisionKit/|'
                             r'Vision/|MLKit|MLImage|Speech/|NaturalLanguage/|UniformTypeIdentifiers/|QuartzCore/|'
                             r'TargetConditionals\.h|whisper\.h|ggml)')

LIBC_FUNCS = """
memcpy memset memmove memcmp memchr memrchr memmem
strlen strnlen strcmp strncmp strcasecmp strncasecmp strcpy strncpy strlcpy strlcat strcat strncat strstr strcasestr
strchr strrchr strpbrk strspn strcspn strdup strndup strtok strtok_r strerror strcoll strxfrm strsep stpcpy
sprintf snprintf vsnprintf vsprintf printf vprintf fprintf vfprintf puts fputs fputc putc putchar fgets fgetc getc
getchar ungetc scanf sscanf fscanf vsscanf
fopen fclose fread fwrite fseek ftell rewind fflush feof ferror clearerr fileno tmpfile tmpnam remove rename perror
setvbuf freopen fdopen popen pclose getline
malloc calloc realloc free aligned_alloc posix_memalign alloca
qsort bsearch
atoi atol atoll atof strtod strtof strtold strtol strtoll strtoul strtoull
abs labs llabs div rand srand random srandom arc4random arc4random_uniform drand48
getenv setenv unsetenv system abort exit _exit _Exit atexit
isdigit isalpha isalnum isspace isupper islower isxdigit ispunct isprint iscntrl isgraph isblank isascii tolower toupper
sqrt sqrtf sin sinf cos cosf tan tanf asin asinf acos acosf atan atanf atan2 atan2f sinh cosh tanh tanhf
floor floorf ceil ceilf round roundf lround lroundf llround trunc truncf rint rintf nearbyint lrint lrintf
fabs fabsf fmod fmodf pow powf exp expf exp2 exp2f log logf log2 log2f log10 log10f log1p hypot hypotf cbrt cbrtf
fmin fminf fmax fmaxf copysign copysignf modf modff frexp ldexp ldexpf isnan isinf isfinite signbit fpclassify erf
pthread_create pthread_join pthread_detach pthread_self pthread_equal pthread_exit pthread_mutex_init
pthread_mutex_lock pthread_mutex_unlock pthread_mutex_trylock pthread_mutex_destroy pthread_cond_init
pthread_cond_wait pthread_cond_timedwait pthread_cond_signal pthread_cond_broadcast pthread_cond_destroy
pthread_get_stackaddr_np pthread_get_stacksize_np pthread_getattr_np pthread_attr_getstack
sleep usleep nanosleep clock_gettime clock gettimeofday time difftime localtime localtime_r gmtime gmtime_r gmtime_s
mktime strftime timegm
stat lstat fstat mkdir rmdir opendir readdir closedir unlink access chdir getcwd realpath open close read write lseek
signal sigaction sigaltstack sigemptyset sigfillset sigaddset raise backtrace backtrace_symbols
dladdr dlopen dlsym dlclose
_Unwind_Backtrace _Unwind_GetIP
setlocale setjmp longjmp
assert
""".split()
# RDE's type names, not C's: a pointer to anything is `any` (rde.h: #define any void*).
VOID_POINTER = re.compile(r'\bvoid\s*\*')
LIBC_CALL = re.compile(r'(?<![\w.>])(?<!->)(' + '|'.join(sorted(LIBC_FUNCS, key=len, reverse=True)) + r')\s*\(')

# The layer table (coding_rules.md §1): what each layer may include, besides itself and rde.h.
DRAWING = ['drawing/base', 'drawing/ink', 'drawing/doc', 'drawing/widgets', 'drawing/app']
LANG_INTERFACES = {'lang/lang.h'}
ALLOWED_LAYERS = {
    'drawing/base':    ['drawing/base'],
    'drawing/ink':     ['drawing/base', 'drawing/ink'],
    'drawing/doc':     ['drawing/base', 'drawing/ink', 'drawing/doc'],
    'drawing/widgets': ['drawing/base', 'drawing/ink', 'drawing/doc', 'drawing/widgets'],
    'drawing/app':     DRAWING,
    'sim':             ['sim'],
    'study':           DRAWING + ['study'],
    'lang':            DRAWING + ['study'],
    'zoom':            DRAWING + ['zoom', 'sim'],
}
# Headers every fude layer may include that are not in fude: the app's generated string ids (each app has its own).
APP_PROVIDED = {'text_ids.h'}

# An app's own symbols (its version.h's KANA_NAME, THAI_VERSION…) and its name as a log prefix ("kana:"). Spelt out
# in a string, only the canvas apps' names count: the study apps' (Kana, Hanzi, Hangul, Thai, Hindi, Arabic) are also
# their scripts' and languages' names, which shared code may say, and Draw is a word.
APP_SYMBOL = re.compile(r'\b(?:KANA|HANZI|HANGUL|THAI|HINDI|ARABIC|DRAW|SKETCHING|INFINOTE|INFINIWORKSHOP)_[A-Z0-9_]+')
APP_STRING = re.compile(r'"[^"\n]*(?:\b(?:kana|hanzi|hangul|thai|hindi|arabic|draw|sketching|infinote|infiniworkshop):|'
                        r'\b(?:Sketching|InfiNote|InfiniWorkshop)\b)[^"\n]*"')
PRODUCT_CHECK = re.compile(r'\bfude_zoom_product_which\s*\(')
GENERATED = re.compile(r'(^|/)text_ids\.h$')


def source_files():
    for top in ('fude', 'apps'):
        for folder, dirs, files in os.walk(os.path.join(ROOT, top)):
            dirs[:] = [d for d in dirs if d not in ('assets', 'tools', 'platform', 'site', 'store', 'docs')]
            for name in sorted(files):
                if name.endswith(('.c', '.h', '.m')):
                    yield os.path.relpath(os.path.join(folder, name), ROOT)


def strip(src, keep_strings=False):
    """The source with comments removed (and string literals emptied, unless keep_strings), newlines kept."""
    out, i, n = [], 0, len(src)
    while i < n:
        c = src[i]
        if src.startswith('//', i):
            j = src.find('\n', i)
            i = n if j < 0 else j
        elif src.startswith('/*', i):
            j = src.find('*/', i + 2)
            j = n if j < 0 else j
            out.append('\n' * src.count('\n', i, j))
            i = j + 2
        elif c in '"\'':
            j = i + 1
            while j < n and src[j] != c and src[j] != '\n':
                j += 2 if src[j] == '\\' else 1
            out.append(src[i:j + 1] if keep_strings else c + c)
            i = j + 1
        else:
            out.append(c)
            i += 1
    return ''.join(out)


def line_of(text, pos):
    return text.count('\n', 0, pos) + 1


def layer_of(path):
    """The layer a fude path belongs to ('drawing/base', 'study', 'lang', 'zoom'…), or None outside fude."""
    parts = path.split('/')
    if parts[0] == 'fude':
        parts = parts[1:]
    if not parts:
        return None
    if parts[0] == 'drawing' and len(parts) > 2:
        return 'drawing/' + parts[1]
    if parts[0] in ('study', 'zoom', 'sim', 'lang'):
        return parts[0]
    return None


def check_includes(path, raw, findings):
    own = layer_of(path) if path.startswith('fude/') else None
    for m in re.finditer(r'^\s*#\s*(?:include|import)\s*([<"])([^>"]+)[>"]', raw, re.M):
        kind, header = m.group(1), m.group(2)
        line = line_of(raw, m.start())
        if kind == '<':
            if header not in ALLOWED_HEADERS and not PLATFORM_HEADER.match(header):
                findings['libc-header'].append((path, line, header))
            continue
        if own is None or header == 'rde.h' or header in APP_PROVIDED or PLATFORM_HEADER.match(header):
            continue
        if header.startswith('lang/') and header in LANG_INTERFACES and own in ('study', 'lang'):
            continue
        target = layer_of(header)
        if target is None:
            # Not a fude header: the app's own (version.h…) or one beside the file, which fude must not need.
            if not os.path.exists(os.path.join(ROOT, os.path.dirname(path), header)):
                findings['layer'].append((path, line, f'includes "{header}" from outside fude'))
            continue
        if own == 'lang' and target == 'lang':
            if header.split('/')[1] != path.split('/')[2]:
                findings['layer'].append((path, line, f'includes another language: "{header}"'))
            continue
        if target != own and target not in ALLOWED_LAYERS.get(own, []):
            findings['layer'].append((path, line, f'{own} includes {target}: "{header}"'))


def check_functions(path, code, findings):
    """Function definitions at file level (code without comments and strings) longer than FUNCTION_LINES."""
    depth, start, name = 0, None, None
    header = re.compile(r'(\w+)\s*\([^;{}]*\)\s*$')
    for i, c in enumerate(code):
        if c == '{':
            if depth == 0:
                m = header.search(code[max(0, i - 400):i])
                if m and m.group(1) not in ('if', 'for', 'while', 'switch', 'sizeof', 'return'):
                    start, name = i, m.group(1)
                else:
                    start, name = None, None
            depth += 1
        elif c == '}':
            depth = max(0, depth - 1)
            if depth == 0 and start is not None:
                lines = code.count('\n', start, i) + 1
                if lines > FUNCTION_LINES:
                    findings['function-size'].append((path, line_of(code, start), f'{name}: {lines} lines'))
                start = None


def check_documentation(path, raw, findings):
    """Every function declared in a header has a comment directly above it, or above its group of declarations."""
    lines = raw.split('\n')
    documented_group = False
    in_block_comment = False
    depth = 0
    for i, line in enumerate(lines):
        s = line.strip()
        if in_block_comment:
            in_block_comment = '*/' not in s
            documented_group = True
            continue
        if s.startswith('/*'):
            in_block_comment = '*/' not in s
            documented_group = True
            continue
        if s.startswith('//'):
            documented_group = True
            continue
        if not s or s.startswith('#'):
            documented_group = False
            continue
        code = strip(line)
        is_declaration = (depth == 0 and re.match(r'^[A-Za-z_][\w\s\*]*\b\w+\s*\(', code.strip())
                          and not code.strip().startswith(('typedef', 'return', 'static inline'))
                          and not re.match(r'^\w+\s*\(', code.strip()))
        if is_declaration:
            # A declaration may run over several lines: it is documented if a comment sits above its first line,
            # or on its own line.
            if not documented_group and '//' not in line:
                name = re.search(r'\b(\w+)\s*\(', code).group(1)
                findings['undocumented'].append((path, i + 1, name))
            # (the next declaration, with no blank line between, belongs to the same group)
        else:
            documented_group = False
        depth += code.count('{') - code.count('}')


def check_identity(path, raw, findings):
    if not path.startswith('fude/'):
        return
    with_strings = strip(raw, keep_strings=True)
    for m in APP_SYMBOL.finditer(with_strings):
        findings['app-identity'].append((path, line_of(with_strings, m.start()), m.group(0)))
    for m in APP_STRING.finditer(with_strings):
        findings['app-identity'].append((path, line_of(with_strings, m.start()), m.group(0)[:60]))
    for m in PRODUCT_CHECK.finditer(with_strings):
        findings['app-identity'].append((path, line_of(with_strings, m.start()), 'checks which product it is'))


def run():
    findings = collections.defaultdict(list)
    for path in source_files():
        raw = open(os.path.join(ROOT, path), encoding='utf-8', errors='replace').read()
        code = strip(raw)
        check_includes(path, raw, findings)
        for m in VOID_POINTER.finditer(code):
            findings['void-pointer'].append((path, line_of(code, m.start()), 'void* (RDE writes any)'))
        for m in LIBC_CALL.finditer(code):
            line_start = code.rfind('\n', 0, m.start()) + 1
            if not re.match(r'\s*#\s*define\s+$', code[line_start:m.start()]):
                findings['libc-call'].append((path, line_of(code, m.start()), m.group(1)))
        if not GENERATED.search(path):
            n = raw.count('\n') + 1
            if n > FILE_LINES:
                findings['file-size'].append((path, 1, f'{n} lines'))
        if path.endswith(('.c', '.m')):
            check_functions(path, code, findings)
        if path.endswith('.h') and not GENERATED.search(path):
            check_documentation(path, raw, findings)
        check_identity(path, raw, findings)
    return findings


CHECKS = ['libc-header', 'libc-call', 'void-pointer', 'layer', 'app-identity', 'file-size', 'function-size', 'undocumented']


def read_counts():
    counts = {}
    if os.path.exists(COUNTS_FILE):
        for line in open(COUNTS_FILE):
            parts = line.split()
            if len(parts) == 2 and parts[0] in CHECKS:
                counts[parts[0]] = int(parts[1])
    return counts


def main():
    args = set(sys.argv[1:])
    findings = run()
    totals = {check: len(findings[check]) for check in CHECKS}
    for check in CHECKS:
        items = findings[check]
        print(f'{check:15} {len(items):6}')
        if '--all' in args:
            for path, line, what in items:
                print(f'    {path}:{line}  {what}')
        else:
            worst = collections.Counter(path for path, _, _ in items).most_common(5)
            for path, n in worst:
                print(f'    {n:5}  {path}')
    if '--save' in args:
        with open(COUNTS_FILE, 'w') as out:
            out.write('# Totals of tools/check/rules.py (docs/coding_rules.md §9). They only ever go down.\n')
            for check in CHECKS:
                out.write(f'{check} {totals[check]}\n')
        print(f'saved to {os.path.relpath(COUNTS_FILE, ROOT)}')
    if '--ratchet' in args:
        before = read_counts()
        worse = [c for c in CHECKS if c in before and totals[c] > before[c]]
        for c in worse:
            print(f'WORSE: {c} {before[c]} -> {totals[c]}')
        if worse:
            sys.exit(1)
        print('no count went up')


if __name__ == '__main__':
    main()
