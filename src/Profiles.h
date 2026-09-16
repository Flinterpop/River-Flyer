#pragma once

#include <array>

#include "Config.h"

// Pilot names that appear on the title screen, saved as profiles.txt next
// to the exe (one name per line). Fixed capacity; new names push out the
// oldest when full.
class Profiles {
public:
    void Load();      // missing file -> the default names
    void Save() const;

    int         Count() const { return count_; }
    const char* At(int i) const;

    // Index of a name (case-insensitive), or -1.
    int Find(const char* name) const;

    // Adds (or moves to the front) a name; returns its index.
    int Add(const char* name);

private:
    std::array<std::array<char, cfg::kNameMax + 1>, cfg::kMaxProfiles> names_ {};
    int count_ {0};
};
