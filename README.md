🎧 Clayton Engine

Hardware-Accelerated C++ Audio Engine & Visualizer

Clayton Engine is a custom, hardware-accelerated C++ audio engine and real-time FFT (Fast Fourier Transform) spectrum visualizer. Built with a focus on low-level memory management, thread safety, and responsive UI design, it transforms raw audio data into fluid, real-time graphical representations. Created by Clayton the Nerdy Lab.

📑 Table of Contents

Core Features

What's New in v0.8.1

Project Structure

Technical Architecture

Building & Running

Roadmap

✨ Core Features

🎛️ Real-Time FFT Visualizer: Computes audio frequencies on the fly with smooth, asymmetric lerping (fast attack, slow decay) for organic, fluid bar movement.

🎨 Dual Theme Engine: Toggle instantly between "Classic Bars" (bottom-anchored, colorful) and "Real Waveform" (center-anchored, high-density, with EDM height-dampening and cyan-to-pink HSV gradients).

📐 Responsive Floating UI: Built with Dear ImGui, featuring a mathematically centered, perfectly symmetrical 540px grid interface that scales dynamically with the window size.

🔄 Playback State Machine: Fully integrated Normal, Repeat All, Repeat 1, and Shuffle (RNG seeded) playback modes.

🔊 Custom Software Volume Mixer: Bypasses OS-level hardware restrictions by multiplying raw audio streams in the data callback, unlocking up to 200% Overdrive.

🚀 What's New in v0.8.1 (The "Premium UX" Update)

Version 0.8.1 transforms the project from a technical demo into a premium, commercial-grade media player with major UX enhancements and a dedicated developer toolchain.

UX & Polish

"3-Second Rule" Prev Button: Smart previous button logic that restarts the current track if it has been playing for >3 seconds (Spotify/Apple Music style UX).

Anti-Distortion Seek Bar: Intercepts ImGui drag events to temporarily mute the audio thread while seeking, eliminating robotic stuttering and buffer corruption.

Perfect UI Symmetry: Redesigned the main control pill to adhere to a strict pixel grid, ensuring all buttons, text fields, and sliders align flawlessly across rows.

Folder Mixing Fix: Re-engineered the folder scanner using an isolated tempPlaylist bucket and <algorithm> sorting to guarantee C++ arrays match the macOS Finder layout.

Developer Tools Integration

Clayton Dev Tool (ClaytonDevTool): A completely isolated, standalone C++ terminal program in the tools/ directory. Interfaces with yt-dlp and ffmpeg to rip MP3s directly from YouTube into the assets/ folder, keeping the main engine lightweight and free of development bloat.

📂 Project Structure

ClaytonEngine/
├── assets/                 # Audio files and loaded MP3s
├── build/                  # Compiled binary executables
├── src/                    # Main Engine Source
│   ├── audio/              # Miniaudio logic & FFT processors
│   ├── core/               # Engine loop & State machines
│   └── renderer/           # OpenGL, GLFW, and ImGui rendering
├── third_party/            # External libraries (ImGui, GLM, GLFW)
└── tools/                  # Isolated developer scripts (DevDownloader)


🏗️ Technical Architecture

Clayton Engine separates its logic to maintain a flawless 60 FPS visual experience:

The Audio Worker (Background Thread): Powered by the low-level Miniaudio API. Handles raw byte streaming, MP3 decoding, and custom software volume multiplication.

The Graphics Worker (Main Thread): Powered by OpenGL, GLFW, and ImGui. Handles heavy FFT windowing calculations, renders the UI, and captures human input.

The Toolchain (Isolated): Development tools are kept strictly out of the src/ directory to prevent duplicate _main linker errors and keep the production build pristine.

💻 Building & Running

Prerequisites

C++17 Compiler (clang++ or g++)

OpenGL & GLFW3 installed on your system

For the Dev Tool: yt-dlp and ffmpeg binaries placed in the root folder.

1. Compile & Run the Main Engine

Use this command to compile the engine, ImGui backend, and link all necessary Apple Frameworks:

# Compile the main engine
clang++ -std=c++17 src/**/*.cpp third_party/imgui/*.cpp third_party/imgui/backends/imgui_impl_glfw.cpp third_party/imgui/backends/imgui_impl_opengl3.cpp -I third_party/glfw/include -I third_party -I third_party/glm -I third_party/imgui -I third_party/imgui/backends -DGL_SILENCE_DEPRECATION -L third_party/glfw/lib -L /usr/local/lib -lglfw3 -o build/ClaytonEngine_v0.8.1 -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo

# Run the engine
./build/ClaytonEngine_v0.8.1


2. Compile & Run the Dev Downloader Tool

# Compile the isolated dev tool
clang++ -std=c++17 tools/DevDownloader.cpp -o build/ClaytonDevTool

# Run the tool
./build/ClaytonDevTool


🗺️ Roadmap (Upcoming in v0.9 - The "Cross-Platform" Update)

[ ] CMake Integration: Replace terminal compile commands with a universal CMakeLists.txt to automatically generate native Visual Studio .sln files for Windows users.

[ ] Universal File Dialogs: Replace AppleScript (osascript) with a cross-platform C++ Native File Dialog library so the "Browse" button works natively on Windows File Explorer.

[ ] Visual Playlist Manager: A dedicated ImGui window to view the upcoming queue and manually skip to specific tracks.