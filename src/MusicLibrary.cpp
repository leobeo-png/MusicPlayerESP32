#include "MusicLibrary.h"
#include "Config.h"
#include <SD.h>

static constexpr size_t MAX_PATH = 256;

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

void MusicLibrary::scanDir(const String& dir, File& out, uint32_t& pos, uint8_t depth) {
    if (depth > 8) return;
    File d = SD.open(dir);
    if (!d || !d.isDirectory()) return;

    for (File e = d.openNextFile(); e; e = d.openNextFile()) {
        const char* name = e.name();  // core 2.x: name only, no path
        if (e.isDirectory()) {
            if (name[0] != '.') scanDir(dir + "/" + name, out, pos, depth + 1);
        } else if (isAudioFile(name)) {
            String p = dir + "/" + name;
            if (p.length() < MAX_PATH) {
                offsets_.push_back(pos);
                out.print(p);
                out.write('\n');
                pos += p.length() + 1;
            }
        }
        e.close();
    }
    d.close();
}

bool MusicLibrary::build() {
    offsets_.clear();
    File out = SD.open(cfg::INDEX_FILE, FILE_WRITE);
    if (!out) return false;
    uint32_t pos = 0;
    uint32_t t0 = millis();
    scanDir(cfg::MUSIC_DIR, out, pos, 0);
    out.close();
    Serial.printf("[lib] scanned %u songs in %lu ms\n", (unsigned)offsets_.size(), millis() - t0);
    return true;
}

bool MusicLibrary::load() {
    offsets_.clear();
    File f = SD.open(cfg::INDEX_FILE, FILE_READ);
    if (!f) return false;
    uint8_t buf[512];
    uint32_t pos = 0;
    bool lineStart = true;
    while (int n = f.read(buf, sizeof buf)) {
        for (int i = 0; i < n; ++i, ++pos) {
            if (lineStart && buf[i] != '\n' && buf[i] != '\r') {
                offsets_.push_back(pos);
                lineStart = false;
            }
            if (buf[i] == '\n') lineStart = true;
        }
    }
    f.close();
    return true;
}

bool MusicLibrary::begin(bool rescan) {
    if (!rescan && load() && !offsets_.empty()) return true;
    return build();
}

bool MusicLibrary::path(size_t i, char* buf, size_t len) const {
    if (i >= offsets_.size() || len == 0) return false;
    File f = SD.open(cfg::INDEX_FILE, FILE_READ);
    if (!f || !f.seek(offsets_[i])) return false;
    size_t n = f.readBytesUntil('\n', buf, len - 1);
    f.close();
    while (n && (buf[n - 1] == '\r' || buf[n - 1] == ' ')) --n;  // tolerate old CRLF index files
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

void MusicLibrary::displayName(const char* path, char* out, size_t len) {
    const char* s = strrchr(path, '/');
    s = s ? s + 1 : path;
    const char* dot = strrchr(s, '.');
    size_t n = dot ? (size_t)(dot - s) : strlen(s);
    if (n >= len) n = len - 1;
    memcpy(out, s, n);
    out[n] = '\0';
}
