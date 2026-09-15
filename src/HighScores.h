#pragma once

#include <array>

#include "Config.h"

// Top-N score table with player names, kept sorted high to low and saved as
// a small text file next to the exe. Fixed storage; no allocation.
//
// File format, one entry per line: "<score><TAB><name>". An optional first
// line "#last<TAB><name>" remembers the most recent entrant so the name box
// can be pre-filled next game.
class HighScores {
public:
    struct Entry {
        std::array<char, cfg::kNameMax + 1> name;   // NUL-terminated
        int  score;
    };

    void Load();          // missing or unreadable file leaves the table empty
    void Save() const;

    int          Count() const { return count_; }
    const Entry& At(int i) const;
    int          Best() const { return (count_ > 0) ? entries_[0].score : 0; }

    // True if 'score' would enter the table (ties go in below existing entries).
    bool Qualifies(int score) const;

    // Inserts and saves; returns the row index it landed on.
    int Add(const char* name, int score);

    const char* LastName() const { return lastName_.data(); }

private:
    static bool ParseLine(const char* line, int lineLen, Entry& out);

    std::array<Entry, cfg::kHighScoreCount> entries_ {};
    int                                     count_ {0};
    std::array<char, cfg::kNameMax + 1>     lastName_ {};
};
