#include <iostream>
#include <string>
#include <cstdlib>

int main() {
    std::cout << "\n==================================================\n";
    std::cout << "      CLAYTON ENGINE - DEV DOWNLOADER TOOL      \n";
    std::cout << "==================================================\n";
    std::cout << "This tool rips MP3s directly into my assets folder.\n";

    std::string url;

    // CREATE the infinite loop so I can download multiple song easily.
    while (true) {
        std::cout << "PASTE YouTube URL (or type 'exit' to QUIT): ";
        std::getline(std::cin, url);

        if (url == "exit" || url == "quit") {
            std::cout << "SHUTTING down Dev Tool...\n";
            break;
        }

        if (url.empty()) continue;

        std::cout << "\n[DOWNLOADING] EXTRACTING MP3 From YouTube...\n";
        
        // EXPLICIT PATH: Tells yt-dlp exactly where our bundled ffmpeg binary is located!
        std::string cmd = "./yt-dlp --ffmpeg-location ./ffmpeg -x --audio-format mp3 --audio-quality 0 -o 'assets/%(title)s.%(ext)s' \"" + url + "\"";

        // RUN the command.
        int result = system(cmd.c_str());

        if (result == 0) {
            std::cout << "\n[SUCCESS] MP3 successfully saved to your assets/ folder!\n";
            std::cout << "--------------------------------------------------\n\n";
        } else {
            std::cout << "\n[ERROR] Download failed. Make sure you pasted a valid URL.\n";
            std::cout << "--------------------------------------------------\n\n";
        }
    }
    return 0;
}