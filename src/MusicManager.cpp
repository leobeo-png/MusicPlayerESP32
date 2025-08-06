#include <MusicManager.h>



MusicManager::MusicManager() {
    // Constructor
}

void MusicManager::readSD() {
    File root = SD.open("/music");
    songList.clear();

    while (true) {
        File entry = root.openNextFile();
        if (!entry) {
            break; // No more files
        }
        if(!entry.isDirectory()) {
            String filename = entry.name();
            if (filename.endsWith(".mp3") || filename.endsWith(".wav")) {
                String fullpath = String("/music/") + filename;
                songList.push_back(fullpath);
                Serial.print("Found file: " + fullpath + "\n");
            }
        }
        entry.close();
    }
    Serial.print("Total songs found: " + String(songList.size()) + "\n");
    root.close();
    
}


