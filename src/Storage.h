#pragma once

// Small text records that survive between runs: the high-score table and the
// pilot names. On the desktop each record is a file next to the exe; in the
// browser build it is a localStorage entry, because the page has no writable
// folder. Both sides are bounded: a record longer than the caller's buffer is
// truncated, never grown.
namespace storage {

// Reads the record into 'buf' (capacity 'cap' bytes, always NUL-terminated).
// Returns the number of bytes stored, 0 when the record does not exist.
int  Read(const char* key, char* buf, int cap);

// Replaces the record with 'text'. Returns false if it could not be written.
bool Write(const char* key, const char* text);

} // namespace storage
