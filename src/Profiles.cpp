#include "Profiles.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <utility>

#include "raylib.h"

namespace {

constexpr int kMaxFileBytes = cfg::kMaxProfiles * 64;

const char* FilePath()
{
    static char path[512] = {0};
    if (path[0] == '\0') {
        const int n = std::snprintf(path, sizeof(path), "%s%s", GetApplicationDirectory(), cfg::kProfilesFile);
        assert(n > 0 && n < static_cast<int>(sizeof(path)));
        (void)n;
    }
    return path;
}

void CopyName(std::array<char, cfg::kNameMax + 1>& dst, const char* src, int srcLen)
{
    assert(src != nullptr && srcLen >= 0);
    int n = 0;
    for (; n < srcLen && n < cfg::kNameMax; ++n) {
        const char c = src[n];
        dst[static_cast<size_t>(n)] = (c >= 32 && c <= 126) ? c : '?';
    }
    dst[static_cast<size_t>(n)] = '\0';
}

bool SameName(const char* a, const char* b)
{
    assert(a != nullptr && b != nullptr);
    for (int i = 0; i <= cfg::kNameMax; ++i) {
        char ca = a[i], cb = b[i];
        if (ca >= 'a' && ca <= 'z') { ca = static_cast<char>(ca - 'a' + 'A'); }
        if (cb >= 'a' && cb <= 'z') { cb = static_cast<char>(cb - 'a' + 'A'); }
        if (ca != cb) { return false; }
        if (ca == '\0') { return true; }
    }
    return true;
}

} // namespace

void Profiles::Load()
{
    count_ = 0;
    if (FileExists(FilePath())) {
        int size = 0;
        unsigned char* data = LoadFileData(FilePath(), &size);
        if (data != nullptr) {
            if (size > kMaxFileBytes) { size = kMaxFileBytes; }
            const char* text = reinterpret_cast<const char*>(data);
            int start = 0;
            for (int i = 0; i <= size && count_ < cfg::kMaxProfiles; ++i) {
                if (i < size && text[i] != '\n') { continue; }
                int len = i - start;
                if (len > 0 && text[start + len - 1] == '\r') { --len; }
                if (len > 0) {
                    CopyName(names_[static_cast<size_t>(count_)], text + start, len);
                    ++count_;
                }
                start = i + 1;
            }
            UnloadFileData(data);
        }
    }
    if (count_ == 0) {
        // First run: friendly placeholders the girls can rename on the title screen.
        const char* defaults[3] = { "PILOT 1", "PILOT 2", "PILOT 3" };
        for (int i = 0; i < 3; ++i) { CopyName(names_[static_cast<size_t>(i)], defaults[i], static_cast<int>(std::strlen(defaults[i]))); }
        count_ = 3;
    }
    assert(count_ > 0 && count_ <= cfg::kMaxProfiles);
}

void Profiles::Save() const
{
    assert(count_ > 0 && count_ <= cfg::kMaxProfiles);
    char buf[kMaxFileBytes] = {0};
    int  used = 0;
    for (int i = 0; i < count_; ++i) {
        used += std::snprintf(buf + used, sizeof(buf) - static_cast<size_t>(used), "%s\n", names_[static_cast<size_t>(i)].data());
        assert(used < static_cast<int>(sizeof(buf)));
    }
    if (!SaveFileText(FilePath(), buf)) {
        TraceLog(LOG_WARNING, "PROFILES: could not write %s", FilePath());
    }
}

const char* Profiles::At(int i) const
{
    assert(i >= 0 && i < count_);
    return names_[static_cast<size_t>(i)].data();
}

int Profiles::Find(const char* name) const
{
    assert(name != nullptr);
    for (int i = 0; i < count_; ++i) {
        if (SameName(names_[static_cast<size_t>(i)].data(), name)) { return i; }
    }
    return -1;
}

int Profiles::Add(const char* name)
{
    assert(name != nullptr && name[0] != '\0');
    // Already known? Move it to the front so it is the default next time.
    int found = -1;
    for (int i = 0; i < count_; ++i) {
        if (SameName(names_[static_cast<size_t>(i)].data(), name)) { found = i; break; }
    }
    if (found < 0) {
        if (count_ < cfg::kMaxProfiles) { ++count_; }
        found = count_ - 1;                       // slot to overwrite (the oldest when full)
        CopyName(names_[static_cast<size_t>(found)], name, static_cast<int>(std::strlen(name)));
    }
    for (int i = found; i > 0; --i) { std::swap(names_[static_cast<size_t>(i)], names_[static_cast<size_t>(i - 1)]); }
    Save();
    assert(SameName(names_[0].data(), name));
    return 0;
}
