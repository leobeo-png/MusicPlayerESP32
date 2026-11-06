#pragma once
// Song index: paths stored one per line in INDEX_FILE, byte offsets of each line kept in RAM
// (4 B/song) so lookups are a single seek + short read instead of a full-file scan.
#include <Arduino.h>
#include <FS.h>
#include <vector>

class MusicLibrary {
public:
    // Loads the index, (re)building it from MUSIC_DIR if rescan is true or the index is missing.
    bool begin(bool rescan);

    size_t count() const { return offsets_.size(); }

    // Full path of song i into buf. Thread-safe (each call opens its own handle).
    bool path(size_t i, char* buf, size_t len) const;

    // Index of a path, or -1.
    int find(const char* p) const;

    // "/music/a/b/Song.mp3" -> "Song"
    static void displayName(const char* path, char* out, size_t len);

private:
    bool build();
    bool load();
    void scanDir(const String& dir, File& out, uint32_t& pos, uint8_t depth);
    static bool isAudioFile(const char* name);

    std::vector<uint32_t> offsets_;
};
