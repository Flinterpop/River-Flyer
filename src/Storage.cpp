#include "Storage.h"

#include <cassert>
#include <cstdio>
#include <cstring>

#include "raylib.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>

// Browser build: records live in window.localStorage under a game prefix.
// stringToUTF8 stops at cap - 1 bytes and terminates, so a long record is
// truncated rather than overrunning the C buffer.
EM_JS(int, rf_storage_read, (const char* key, char* buf, int cap), {
    try {
        var v = window.localStorage.getItem('riverflyer/' + UTF8ToString(key));
        if (v === null) { return 0; }
        stringToUTF8(v, buf, cap);
        var n = lengthBytesUTF8(v);
        return (n < cap) ? n : cap - 1;
    } catch (e) {
        return 0;
    }
});

EM_JS(int, rf_storage_write, (const char* key, const char* text), {
    try {
        window.localStorage.setItem('riverflyer/' + UTF8ToString(key), UTF8ToString(text));
        return 1;
    } catch (e) {
        return 0;
    }
});

#else

namespace {

// Desktop: "<exe folder><key>". raylib's GetApplicationDirectory() ends in a slash.
const char* PathFor(const char* key)
{
    static char path[512] = {0};
    const int n = std::snprintf(path, sizeof(path), "%s%s", GetApplicationDirectory(), key);
    assert(n > 0 && n < static_cast<int>(sizeof(path)));
    (void)n;
    return path;
}

} // namespace

#endif

namespace storage {

int Read(const char* key, char* buf, int cap)
{
    assert(key != nullptr && key[0] != '\0');
    assert(buf != nullptr && cap > 1);
    buf[0] = '\0';
#if defined(__EMSCRIPTEN__)
    const int n = rf_storage_read(key, buf, cap);
#else
    int n = 0;
    const char* path = PathFor(key);
    if (FileExists(path)) {
        int size = 0;
        unsigned char* data = LoadFileData(path, &size);
        if (data != nullptr) {
            n = (size < cap - 1) ? size : cap - 1;   // ignore anything absurd
            std::memcpy(buf, data, static_cast<size_t>(n));
            UnloadFileData(data);
        }
    }
    buf[n] = '\0';
#endif
    assert(n >= 0 && n < cap);
    return n;
}

bool Write(const char* key, const char* text)
{
    assert(key != nullptr && key[0] != '\0');
    assert(text != nullptr);
#if defined(__EMSCRIPTEN__)
    const bool ok = rf_storage_write(key, text) != 0;
#else
    // SaveFileText takes a non-const pointer for historical reasons; it only reads.
    const bool ok = SaveFileText(PathFor(key), const_cast<char*>(text));
#endif
    if (!ok) { TraceLog(LOG_WARNING, "STORAGE: could not write %s", key); }
    return ok;
}

} // namespace storage
