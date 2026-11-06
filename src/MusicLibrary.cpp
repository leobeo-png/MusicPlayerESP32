#include "MusicLibrary.h"
#include "Config.h"
#include <SD.h>
#include <algorithm>

static constexpr size_t MAX_PATH = 256;
static constexpr const char* AZ_FILE = "/song_az.bin";

static bool lessNoCase(const String& a, const String& b) { return strcasecmp(a.c_str(), b.c_str()) < 0; }

bool MusicLibrary::isAudioFile(const char* name) {
    if (name[0] == '.') return false;  // skip macOS "._x.mp3" and hidden files
    const char* dot = strrchr(name, '.');
    if (!dot) return false;
    // FLAC is excluded: classic ESP32 has no PSRAM for its block buffer.
    static const char* const exts[] = {".mp3", ".wav", ".m4a", ".aac"};
    for (const char* e : exts)
        if (strcasecmp(dot, e) == 0) return true;
    return false;
}

// ------------------------------------------------------------------ scanning

void MusicLibrary::scanDir(const String& dir, File& out, uint8_t depth) {
    if (depth > 8 || scanned_ >= MAX_SONGS) return;
    File d = SD.open(dir);
    if (!d || !d.isDirectory()) return;

    std::vector<String> files, dirs;
    for (File e = d.openNextFile(); e; e = d.openNextFile()) {
        const char* name = e.name();  // core 2.x: name only
        if (e.isDirectory()) { if (name[0] != '.') dirs.emplace_back(name); }
        else if (isAudioFile(name)) files.emplace_back(name);
        e.close();
    }
    d.close();
    std::sort(files.begin(), files.end(), lessNoCase);
    std::sort(dirs.begin(), dirs.end(), lessNoCase);

    // This directory's files first (one contiguous folder range), then subdirectories.
    for (const String& f : files) {
        if (scanned_ >= MAX_SONGS) break;
        String p = dir + "/" + f;
        if (p.length() >= MAX_PATH) continue;
        out.print(p);
        out.write('\n');
        ++scanned_;
    }
    files.clear();
    for (const String& sub : dirs) scanDir(dir + "/" + sub, out, depth + 1);
}

bool MusicLibrary::build() {
    File out = SD.open(cfg::INDEX_FILE, FILE_WRITE);
    if (!out) return false;
    uint32_t t0 = millis();
    scanned_ = 0;
    scanDir(cfg::MUSIC_DIR, out, 0);
    out.close();
    SD.remove(AZ_FILE);
    bool ok = load() && buildAZ();
    Serial.printf("[lib] scanned %u songs, %u folders in %lu ms\n",
                  (unsigned)count(), (unsigned)folderCount(), millis() - t0);
    return ok;
}

// Reads the index once: line offsets + folder ranges.
bool MusicLibrary::load() {
    offsets_.clear();
    folders_.clear();
    File f = SD.open(cfg::INDEX_FILE, FILE_READ);
    if (!f) return false;

    char line[MAX_PATH], prevDir[MAX_PATH] = "";
    size_t len = 0;
    uint32_t pos = 0, lineStart = 0;
    uint8_t buf[512];
    auto endLine = [&]() {
        while (len && line[len - 1] == '\r') --len;
        if (len && offsets_.size() < MAX_SONGS) {
            line[len] = '\0';
            char* slash = strrchr(line, '/');
            if (slash) *slash = '\0';
            if (folders_.empty() || strcmp(line, prevDir) != 0) {
                folders_.push_back({(uint16_t)offsets_.size(), 0});
                strlcpy(prevDir, line, sizeof prevDir);
            }
            folders_.back().count++;
            offsets_.push_back(lineStart);
        }
        len = 0;
    };
    while (int n = f.read(buf, sizeof buf)) {
        for (int i = 0; i < n; ++i, ++pos) {
            if (buf[i] == '\n') { endLine(); lineStart = pos + 1; }
            else if (len < MAX_PATH - 1) line[len++] = (char)buf[i];
        }
    }
    endLine();
    f.close();
    return true;
}

// ------------------------------------------------------------------ A–Z order

namespace {
struct AzKey { char k[12]; uint16_t idx; };
}

bool MusicLibrary::buildAZ() {
    const size_t n = count();
    az_.clear();
    std::vector<AzKey> keys;
    keys.reserve(n);

    File f = SD.open(cfg::INDEX_FILE, FILE_READ);
    if (!f) return false;
    char path[MAX_PATH], name[128];
    for (size_t i = 0; i < n; ++i) {
        size_t len = f.readBytesUntil('\n', path, sizeof path - 1);
        while (len && path[len - 1] == '\r') --len;
        path[len] = '\0';
        displayName(path, name, sizeof name);
        AzKey key;
        memset(key.k, 0, sizeof key.k);
        for (size_t c = 0; c < sizeof key.k && name[c]; ++c)
            key.k[c] = (name[c] >= 'A' && name[c] <= 'Z') ? name[c] + 32 : name[c];  // ASCII fold; UTF-8 sorts by code point
        key.idx = (uint16_t)i;
        keys.push_back(key);
    }
    f.close();

    std::stable_sort(keys.begin(), keys.end(), [](const AzKey& a, const AzKey& b) {
        return memcmp(a.k, b.k, sizeof a.k) < 0;
    });
    az_.reserve(n);
    for (const AzKey& k : keys) az_.push_back(k.idx);

    File o = SD.open(AZ_FILE, FILE_WRITE);
    if (o) {
        uint16_t cnt = (uint16_t)n;
        o.write((const uint8_t*)&cnt, 2);
        o.write((const uint8_t*)az_.data(), n * 2);
        o.close();
    }
    return true;
}

bool MusicLibrary::loadAZ() {
    File f = SD.open(AZ_FILE, FILE_READ);
    if (!f) return false;
    uint16_t cnt = 0;
    bool ok = f.read((uint8_t*)&cnt, 2) == 2 && cnt == count();
    if (ok) {
        az_.resize(cnt);
        ok = f.read((uint8_t*)az_.data(), cnt * 2) == (int)(cnt * 2);
    }
    f.close();
    return ok;
}

bool MusicLibrary::begin(bool rescan) {
    // No A–Z cache = index from an older firmware (unsorted folders) -> full rescan once.
    if (!rescan && load() && count() && loadAZ()) {
        Serial.printf("[lib] loaded %u songs, %u folders\n", (unsigned)count(), (unsigned)folderCount());
        return true;
    }
    return build();
}

// ------------------------------------------------------------------ lookups

bool MusicLibrary::path(size_t i, char* buf, size_t len) const {
    if (i >= offsets_.size() || len == 0) return false;
    File f = SD.open(cfg::INDEX_FILE, FILE_READ);
    if (!f || !f.seek(offsets_[i])) return false;
    size_t n = f.readBytesUntil('\n', buf, len - 1);
    f.close();
    while (n && (buf[n - 1] == '\r' || buf[n - 1] == ' ')) --n;
    buf[n] = '\0';
    return n > 0;
}

int MusicLibrary::find(const char* p) const {
    if (!p || !*p) return -1;
    File f = SD.open(cfg::INDEX_FILE, FILE_READ);
    if (!f) return -1;
    char line[MAX_PATH];
    for (size_t i = 0; i < offsets_.size(); ++i) {
        size_t n = f.readBytesUntil('\n', line, sizeof line - 1);
        while (n && line[n - 1] == '\r') --n;
        line[n] = '\0';
        if (strcmp(line, p) == 0) { f.close(); return (int)i; }
    }
    f.close();
    return -1;
}

int MusicLibrary::azPos(size_t song) const {
    for (size_t k = 0; k < az_.size(); ++k)
        if (az_[k] == song) return (int)k;
    return -1;
}

int MusicLibrary::folderOf(size_t song) const {
    // folders_ is sorted by first -> binary search for the last folder with first <= song
    auto it = std::upper_bound(folders_.begin(), folders_.end(), song,
                               [](size_t s, const Folder& f) { return s < f.first; });
    if (it == folders_.begin()) return -1;
    return (int)(it - folders_.begin()) - 1;
}

bool MusicLibrary::folderName(size_t f, char* buf, size_t len) const {
    char p[MAX_PATH];
    if (f >= folders_.size() || !path(folders_[f].first, p, sizeof p)) return false;
    char* slash = strrchr(p, '/');
    if (slash) *slash = '\0';               // directory
    const char* base = strrchr(p, '/');
    base = base ? base + 1 : p;
    if (strcmp(p, cfg::MUSIC_DIR) == 0) base = "(Loose songs)";
    strlcpy(buf, base, len);
    return true;
}

void MusicLibrary::displayName(const char* path, char* out, size_t len) {
    const char* s = strrchr(path, '/');
    s = s ? s + 1 : path;
    const char* dot = strrchr(s, '.');
    size_t n = dot ? (size_t)(dot - s) : strlen(s);
    if (n >= len) n = len - 1;
    memcpy(out, s, n);
    out[n] = '\0';
}
