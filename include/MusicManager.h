#ifndef MusicManager_h
#define MusicManager_h

#include <Arduino.h>
#include "Audio.h"
#include "SD.h"
#include "FS.h"
#include "vector"

extern Audio audio;

class MusicManager {
        
    public:
        MusicManager();
        void scanFolderRecursive(const String &folder, File &indexFile);
        void buildSongIndex(const String &musicFolder, const String &indexFileName);
        int getSongCount(const String &indexFileName);
        String getSongByIndex(const String &indexFileName, int index);

};

#endif