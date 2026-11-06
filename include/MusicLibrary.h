#pragma once
// Song index on SD + compact RAM tables:
//   offsets_ : byte offset of each path line in INDEX_FILE (4 B/song)  -> path(i) is one seek
//   az_      : song indices in A–Z order by display name (2 B/song), cached in AZ_FILE
//   folders_ : contiguous [first, first+count) ranges per directory (4 B/folder)
// The index is written folder by folder, each directory's entries sorted case-insensitively,
// so every folder is one contiguous range.
#include <Arduino.h>
#include <FS.h>
#include <vector>

struct Folder { uint16_t first, count; };

class MusicLibrary {
public:
    static constexpr size_t MAX_SONGS = 3000;  // keeps the boot-time A–Z sort under ~45 KB

    bool begin(bool rescan);

    size_t count() const { return offsets_.size(); }
    bool path(size_t i, char* buf, size_t len) const;   // thread-safe
    int find(const char* p) const;                      // index of path or -1

    uint16_t az(size_t k) const { return az_[k]; }      // k-th song in A–Z order
    int azPos(size_t song) const;                       // inverse of az(), -1 if not found

    size_t folderCount() const { return folders_.size(); }
    const Folder& folder(size_t f) const { return folders_[f]; }
    int folderOf(size_t song) const;
    bool folderName(size_t f, char* buf, size_t len) const;

    static void displayName(const char* path, char* out, size_t len);  // "Song" from ".../Song.mp3"

private:
    bool build();
    bool load();
    bool buildAZ();
    bool loadAZ();
    void scanDir(const String& dir, File& out, uint8_t depth);
    static bool isAudioFile(const char* name);

    std::vector<uint32_t> offsets_;
    std::vector<uint16_t> az_;
    std::vector<Folder> folders_;
    size_t scanned_ = 0;
};
