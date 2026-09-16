#include "HighScores.h"

#include <cassert>
#include <cstdio>
#include <cstring>

#include "raylib.h"

namespace {

// Longest sensible file: N entries plus the "#last" line, each well under 64 chars.
constexpr int kMaxFileBytes = (cfg::kHighScoreCount + 1) * 64;

const char* FilePath()
{
    // Static buffer: raylib's TextFormat also returns a static, so copy it out once.
    static char path[512] = {0};
    if (path[0] == '\0') {
        const int n = std::snprintf(path, sizeof(path), "%s%s", GetApplicationDirectory(), cfg::kHighScoreFile);
        assert(n > 0 && n < static_cast<int>(sizeof(path)));
        (void)n;
    }
    return path;
}

// Copies at most kNameMax visible characters into 'dst', always NUL-terminating.
void CopyName(std::array<char, cfg::kNameMax + 1>& dst, const char* src, int srcLen)
{
    assert(src != nullptr && srcLen >= 0);
    int n = 0;
    for (; n < srcLen && n < cfg::kNameMax; ++n) {
        const char c = src[n];
        dst[static_cast<size_t>(n)] = (c >= 32 && c <= 126) ? c : '?';
    }
    dst[static_cast<size_t>(n)] = '\0';
    assert(std::strlen(dst.data()) <= static_cast<size_t>(cfg::kNameMax));
}

} // namespace

bool HighScores::ParseLine(const char* line, int lineLen, Entry& out)
{
    assert(line != nullptr && lineLen >= 0);
    // "<digits>\t<name>"
    int i = 0;
    long score = 0;
    for (; i < lineLen && line[i] >= '0' && line[i] <= '9' && i < 10; ++i) {
        score = score * 10 + (line[i] - '0');
    }
    if (i == 0 || i >= lineLen || line[i] != '\t') { return false; }
    ++i;   // skip the tab
    if (i >= lineLen) { return false; }
    out.score = static_cast<int>(score);
    CopyName(out.name, line + i, lineLen - i);
    return out.name[0] != '\0';
}

void HighScores::Load()
{
    count_ = 0;
    lastName_[0] = '\0';
    if (!FileExists(FilePath())) { return; }

    int   size = 0;
    unsigned char* data = LoadFileData(FilePath(), &size);
    if (data == nullptr) { return; }
    if (size > kMaxFileBytes) { size = kMaxFileBytes; }   // ignore anything absurd

    const char* text  = reinterpret_cast<const char*>(data);
    int         start = 0;
    for (int i = 0; i <= size && count_ < cfg::kHighScoreCount; ++i) {
        if (i < size && text[i] != '\n') { continue; }
        int len = i - start;
        if (len > 0 && text[start + len - 1] == '\r') { --len; }
        if (len > 0) {
            if (len > 6 && std::strncmp(text + start, "#last\t", 6) == 0) {
                CopyName(lastName_, text + start + 6, len - 6);
            } else {
                Entry e {};
                if (ParseLine(text + start, len, e)) { entries_[static_cast<size_t>(count_)] = e; ++count_; }
            }
        }
        start = i + 1;
    }
    UnloadFileData(data);

    // Defensive: keep the table sorted even if the file was hand-edited.
    for (int a = 1; a < count_; ++a) {
        for (int b = a; b > 0 && entries_[static_cast<size_t>(b)].score > entries_[static_cast<size_t>(b - 1)].score; --b) {
            const Entry tmp = entries_[static_cast<size_t>(b)];
            entries_[static_cast<size_t>(b)]     = entries_[static_cast<size_t>(b - 1)];
            entries_[static_cast<size_t>(b - 1)] = tmp;
        }
    }
    assert(count_ >= 0 && count_ <= cfg::kHighScoreCount);
}

void HighScores::Save() const
{
    assert(count_ >= 0 && count_ <= cfg::kHighScoreCount);
    char buf[kMaxFileBytes] = {0};
    int  used = 0;
    if (lastName_[0] != '\0') {
        used += std::snprintf(buf + used, sizeof(buf) - static_cast<size_t>(used), "#last\t%s\n", lastName_.data());
    }
    for (int i = 0; i < count_; ++i) {
        const Entry& e = entries_[static_cast<size_t>(i)];
        used += std::snprintf(buf + used, sizeof(buf) - static_cast<size_t>(used), "%d\t%s\n", e.score, e.name.data());
        assert(used < static_cast<int>(sizeof(buf)));
    }
    if (!SaveFileText(FilePath(), buf)) {
        TraceLog(LOG_WARNING, "HIGHSCORES: could not write %s", FilePath());
    }
}

const HighScores::Entry& HighScores::At(int i) const
{
    assert(i >= 0 && i < count_);
    return entries_[static_cast<size_t>(i)];
}

int HighScores::BestFor(const char* name) const
{
    assert(name != nullptr);
    for (int i = 0; i < count_; ++i) {           // sorted high to low: first match is the best
        const char* e = entries_[static_cast<size_t>(i)].name.data();
        bool same = true;
        for (int k = 0; k <= cfg::kNameMax && same; ++k) {
            char a = e[k], b = name[k];
            if (a >= 'a' && a <= 'z') { a = static_cast<char>(a - 'a' + 'A'); }
            if (b >= 'a' && b <= 'z') { b = static_cast<char>(b - 'a' + 'A'); }
            if (a != b) { same = false; }
            if (a == '\0') { break; }
        }
        if (same) { return entries_[static_cast<size_t>(i)].score; }
    }
    return 0;
}

bool HighScores::Qualifies(int score) const
{
    assert(score >= 0);
    if (score <= 0) { return false; }
    if (count_ < cfg::kHighScoreCount) { return true; }
    return score > entries_[static_cast<size_t>(count_ - 1)].score;
}

int HighScores::Add(const char* name, int score)
{
    assert(name != nullptr && name[0] != '\0');
    assert(Qualifies(score));

    // Find the row: after every entry with score >= ours.
    int row = 0;
    while (row < count_ && entries_[static_cast<size_t>(row)].score >= score) { ++row; }
    assert(row < cfg::kHighScoreCount);

    // Shift the rest down, dropping the last if the table is full.
    const int last = (count_ < cfg::kHighScoreCount) ? count_ : cfg::kHighScoreCount - 1;
    for (int i = last; i > row; --i) {
        entries_[static_cast<size_t>(i)] = entries_[static_cast<size_t>(i - 1)];
    }
    Entry& e = entries_[static_cast<size_t>(row)];
    CopyName(e.name, name, static_cast<int>(std::strlen(name)));
    e.score = score;
    if (count_ < cfg::kHighScoreCount) { ++count_; }
    CopyName(lastName_, name, static_cast<int>(std::strlen(name)));

    Save();
    assert(row >= 0 && row < count_);
    return row;
}
