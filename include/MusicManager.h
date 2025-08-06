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
        void readSD();
        int getSongCount() {
            return songList.size();
        }
        String getSongByIndex(int index) {
            if (index >= 0 && index < songList.size()) {
                return songList[index];
            }
            return "";
        }

    private:
        std::vector<String> songList;
};

#endif