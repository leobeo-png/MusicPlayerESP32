#include <MusicManager.h>

MusicManager::MusicManager() {
    // Constructor
}

void MusicManager::scanFolderRecursive(const String &folder, File &indexFile) {
    File dir = SD.open(folder);
    if (!dir) {
        return;
    }

    File entry;
    while ((entry = dir.openNextFile())) {
        String name = entry.name();
        if(entry.isDirectory()) {
            scanFolderRecursive(String(folder + "/" + name), indexFile); // go in the sub folder
        } else {
            name.toLowerCase();
            if(name.endsWith(".mp3") || name.endsWith(".wav") || name.endsWith(".flac")) {
                String songPath = folder + "/" + name;
                indexFile.println(songPath);
                Serial.println("Found song: " + String(folder + "/" + name));
            }
        }
        entry.close();
    }
    dir.close();
}

void MusicManager::buildSongIndex(const String &musicFolder, const String &indexFileName) {
    File indexFile = SD.open(indexFileName, FILE_WRITE);
    if (!indexFile) {
        return;
    }
    scanFolderRecursive(musicFolder, indexFile);

    indexFile.close();
    Serial.println("Song index built successfully.");
    
}


int MusicManager::getSongCount(const String &indexFileName) {
    File indexFile = SD.open(indexFileName);
    if(!indexFile) {
        return 0;
    }

    int count = 0;
    while(indexFile.available()) {
        indexFile.readStringUntil('\n'); 
        count++;
    }

    indexFile.close();
    return count;
}

String MusicManager::getSongByIndex(const String &indexFileName, int index) {
    File indexFile = SD.open(indexFileName);
    if(!indexFile) {
        return "";
    }

    int currentIndex = 0;
    String line;
    while(indexFile.available()) {
        line = indexFile.readStringUntil('\n'); 
        if(currentIndex == index) {
            indexFile.close();
            return line; // Return the song path at the specified index
        }
        currentIndex++;
    }

    indexFile.close();
    return "";
}