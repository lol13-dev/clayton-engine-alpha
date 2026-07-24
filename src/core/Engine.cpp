// UPGRADED VERSION (ALMOST DONE).
// BETTER REAL-TIME PERFORMANCE WITH RING BUFFER AND FFT SUPPORT. SEE src/core/Engine_v1.cpp for the original version without these features.
// Engine.cpp
#include "Engine.h"
#include "../audio/AudioPlayer.h"
#include "../audio/FFT.h"
#include "../renderer/Window.h"
#include "../renderer/SpectrumRenderer.h"
#include "../trumfaster/TrumFaster.h"
#include "../../third_party/imgui/imgui.h"
#include "../../third_party/imgui/backends/imgui_impl_glfw.h"
#include "../../third_party/imgui/backends/imgui_impl_opengl3.h"

// SEEN IN: src/audio/AudioPlayer.h
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <cmath>
#include <filesystem>
#include <atomic> // <- For thread-safe operations.
#include <algorithm> // <- For standard algorithms like std::sort.
// FOR REPEAT, REPEAT ALL, SHUFFLE the music.
#include <cstdlib> // FOR rand(); math
#include <ctime>
#include <GLFW/glfw3.h> // REQUIRED for dynamic Framebuffer RESIZING.

// "eXperimental" Touch Bar and Apple Bridges.
#ifdef __APPLE__
#include "MacTouchBar.h"
#include "MacMediaCenter.h" // <- THIS IS FOR CONNECT TO Control Center.
#include "MacMenuBar.h"
#endif

namespace fs = std::filesystem; // <- Create a namespace for filesystem operations.

// =====================================
// DRAG & DROP GLOBALS.
// =====================================
static std::vector<std::string> asyncDroppedPaths;
static std::atomic<bool> hasDroppedPaths = false;

// =====================================
// NATIVE OS NOTIFICATION HELPER (BOOST MAX WARNING)
// =====================================
void ShowBoostWarningNotification() {
    // SPIN up a BACKGROUND thread so the visualizer doesn't shutter.
    std::thread([] () {
        #ifdef _WIN32
            std::string cmd = "powershell -WindowStyle Hidden -Command \"Add-Type -AssemblyName System.Windows.Forms; $n = New-Object System.Windows.Forms.NotifyIcon; $n.Icon = [System.Drawing.SystemIcons]::Warning; $n.BalloonTipTitle = '⚠️ BoostMax 250% Mode Activated'; $n.BalloonTipText = 'WARNING: Listening at extreme volumes may damage your device speakers or hearing, especially when using headphones or headsets!'; $n.Visible = $true; $n.ShowBalloonTip(5000); Start-Sleep -s 6; $n.Dispose()\"";
            system(cmd.c_str());
        #elif __APPLE__
            // macOS Native Notification (MATCHES the styling in our SCREENSHOT).
            std::string cmd = "osascript -e 'display notification \"WARNING: Listening at extreme volumes may damage your device speakers or hearing, especially when using headphones or headsets.\" with title \"⚠️ BoostMax 250% Mode Activated\"'";
            system(cmd.c_str());
        #elif __linux__
            std::string cmd = "notify-send '⚠️ BoostMax 250% Mode Activated' 'WARNING: Listening at extreme volumes may damage your device speakers or hearing, especially when using headphones or headsets!'";
            system(cmd.c_str());
        #endif
    }).detach();
}

// This FUNCTION is TRIGGERED by the OS the exact millisecond a file is dropped on the WINDOW.
void DropCallback(GLFWwindow* window, int count, const char** paths) {
    asyncDroppedPaths.clear();
    for (int i = 0; i < count; i++) {
        asyncDroppedPaths.push_back(paths[i]);
    }
    hasDroppedPaths = true;
}

// =====================================
// NATIVE OS NOTIFICATION HELPER.
// =====================================
void ShowOSNotification(const std::string& trackName) {
    // SPIN up a background thread so the visualizer doesn't freeze for even a millisecond.
    std::thread([trackName] () {
        std::string safeName = trackName;
        // SANITIZE the string: REPLACE double quotes with single quotes so I don't break terminal commands.
        std::replace(safeName.begin(), safeName.end(), '"', '\'');

        #ifdef _WIN32
            // Windows 10/11 Native Toast (Using PowerShell and System.Windows.Forms)
            std::string cmd = "powershell -WindowStyle Hidden -Command \"Add-Type -AssemblyName System.Windows.Forms; $n = New-Object System.Windows.Forms.NotifyIcon; $n.Icon = [System.Drawing.SystemIcons]::Information; $n.BalloonTipTitle = '🎵 Now Playing'; $n.BalloonTipText = '" + safeName + "'; $n.Visible = $true; $n.ShowBalloonTip(3000); Start-Sleep -s 4; $n.Dispose()\"";
            system(cmd.c_str());
        #elif __APPLE__
            // macOS Native Notification (Using AppleScript)
            std::string cmd = "osascript -e 'display notification \"" + safeName + "\" with title \"Now Playing\"'";
            system(cmd.c_str());
        #elif __linux__
            // Linux Native Notification
            std::string cmd = "notify-send '🎵 Now Playing' '" + safeName + "'";
            system(cmd.c_str());
        #endif
    }).detach();
}

// =====================================
// CONSTRUCTOR
// =====================================
Engine::Engine()
{
    std::cout << "[CONSTRUCTOR] Engine Object Created.\n";
}

// =====================================
// DESTRUCTOR
// =====================================
Engine::~Engine()
{
    std::cout << "[DESTRUCTOR] Engine Object Destroyed.\n";
}

// =====================================
// INITIALIZE ENGINE
// =====================================
void Engine::Initialize()
{
    std::cout << "Clayton Engine Initialized.\n";
}

// =====================================
// MAIN ENGINE LOOP 
// =====================================
void Engine::Run()
{
    // -----------------------------------
    // DYNAMIC TRACK SELECTION MENU (CLEAN BOOT STATE).
    // -----------------------------------

    // UX FIX Step 1: START completely EMPTY. No hardcoded "assets" folder.
    std::vector<std::string> playlist;
    int currentTrackIndex = 0;
    std::string selectedTrackPath = "";
    std::string cleanTrackName = "";

    // NEW: MOVED out of ImGui loop so both UI and Drag/Drop can see it.
    char folderPathBuffer[256] = "";

    // -----------------------------------
    // 1. Create modules.
    // -----------------------------------
    AudioPlayer player;
    FFT fft;

    // -----------------------------------
    // 2. CREATE a Window.
    // -----------------------------------
    Window window(1280, 720, "Spevio (former WaveformVisual Online) v0.9.19.x (Alpha) - Powered by Clayton Engine.");
    if (!window.Initialize())
    {
        std::cout << "[ENGINE] Failed to initialize window. Exiting...\n";
        return;
    }

    // -----------------------------------
    // UX FIX: MINIMUM WINDOW SIZE (Anti-Squish).
    // -----------------------------------
    // LOCK the minimum dimensions to 900x600.
    // This Guarantees the 830px UI PIll NEVER GETS squished.
    // And the visualizer always has ENOUGH room to OVERRIDE.
    glfwSetWindowSizeLimits(window.GetGLFWWindowPointer(), 900, 600, GLFW_DONT_CARE, GLFW_DONT_CARE);

    // REGISTER DRAG & DROP CALLBACK
    glfwSetDropCallback(window.GetGLFWWindowPointer(), DropCallback);

    // Touch Bar
    bool isTouchBarActive = false;
    #ifdef __APPLE__
        isTouchBarActive = InitTouchBar(window.GetGLFWWindowPointer());
        InitMediaCenter();  // I ADD THIS to BOOT the Control Center connection.
    #endif

    // -----------------------------------
    // 3. CREATE a Spectrum Renderer.
    // -----------------------------------
    SpectrumRenderer spectrumRenderer;
    spectrumRenderer.Initialize();

    // ==========================================
    // IMGUI Phase 1: INITIALIZATION.
    // ==========================================
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    
    // Connect ImGui to your Mac Window and OpenGL
    ImGui_ImplGlfw_InitForOpenGL(window.GetGLFWWindowPointer(), true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    const int TARGET_FPS = 60;
    const int FRAME_TIME_MS = 1000 / TARGET_FPS;
    const size_t FFT_WINDOW_SIZE = 1024;

    // NEW: Remembers if the user intentionally paused the music
    bool isUserPaused = false;

    // DEFAULTING to 1.0f is 100% volume. 2.0f allows the user to overdrive the audio to 200%!
    float currentVolume = 1.0f;
    // ASK the MP3 for its length ONCE before the starts.
    float trackDuration = player.GetDuration();

    // ==========================================
    // NEW: PLAYBACK STATE MACHINE (THEN UPDATE 1: Window-Controlled Loop)
    // ==========================================
    // 0 = Normal, 1 = Repeat All, 2 = Repeat 1, 3 = Shuffle
    int playbackMode = 0;   // DEFAULTING to 1 (REPEAT ALL) since that's what I built.
    srand(time(NULL));      // "Seed" the randomizer using my Mac's internal lock.

    // DECLARE the VISUALIZER bars.
    std::vector<float> frozenFrequencies(1024, 0.0f);

    // ==========================================
    // NEW: TrumFaster, Adaptive Rendering & Frame Pacer.
    // ==========================================
    TrumFaster trumFaster(60); 

    // ==========================================
    // NEW: GPU Shader Init (Audio-Reactive Plasma) (FIXED)
    // ==========================================
    // FIXED, DUE TO Black Screen of Death.
    const char* vertexShaderSource = "#version 330 core\n"
        "layout (location = 0) in vec2 aPos;\n"
        "void main() {\n"
        "   gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);\n"
        "}\n";

    // FIXED, DUE TO Black Screen of Death.
    const char* fragmentShaderSource = "#version 330 core\n"
        "out vec4 FragColor;\n"
        "uniform vec2 u_resolution;\n"
        "uniform float u_time;\n"
        "uniform float u_audioEnergy;\n"
        "void main() {\n"
        "   vec2 uv = gl_FragCoord.xy / u_resolution.xy;\n"
        "   uv = uv * 2.0 - 1.0;\n" 
        "   uv.x *= u_resolution.x / u_resolution.y;\n" 
        "   float d = length(uv);\n"
        "   float radius = 0.3 + (u_audioEnergy * 0.4);\n"
        "   float wave = sin(u_time * 3.0 + uv.x * 5.0) * 0.1;\n"
        "   float glow = 0.03 / max(abs(d - radius + wave), 0.001);\n"
        "   vec3 col = vec3(0.0, 0.6, 0.8) * glow;\n" 
        "   col += vec3(0.8, 0.1, 0.8) * (u_audioEnergy * glow * 1.5);\n"
        "   col *= max(1.0 - (d * 0.6), 0.0);\n"
        "   FragColor = vec4(col, 1.0);\n"
        "}\n";

    int success;
    char infoLog[512];

    // OpenGL Step 1: COMPILE Vertex Shader.
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "[GPU_ERROR] Vertex Shader Failed:\n" << infoLog << "\n";
    }

    // OpenGL Step 2: COMPILE Fragment Shader.
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        // IT WILL APPEAR ON TERMINAL IF ERROR.
        std::cout << "[GPU_ERROR] Fragment Shader Failed:\n" << infoLog << "\n";
    }

    // OpenGL Step 3: LINK into a GPU Program.
    unsigned int backgroundShaderProgram = glCreateProgram();
    glAttachShader(backgroundShaderProgram, vertexShader);
    glAttachShader(backgroundShaderProgram, fragmentShader);
    glLinkProgram(backgroundShaderProgram);
    glGetProgramiv(backgroundShaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(backgroundShaderProgram, 512, NULL, infoLog);
        // IT WILL APPEAR ON TERMINAL IF ERROR.
        std::cout << "[GPU_ERROR] Shader Linking Failed:\n" << infoLog << "\n";
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // OpenGL Step 4: Create a Full-Screen Quad (Canvas).
    float quadVertices[] = {
        -1.0f,  1.0f, // Top Left
        -1.0f, -1.0f, // Bottom Left
         1.0f,  1.0f, // Top Right
         1.0f, -1.0f  // Bottom Right
    };
    unsigned int quadVAO, quadVBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    // NEW: USER TOGGLE for TrumFaster.
    bool isTrumFasterEnabled = true;

    // GPU Performance & Quality States.
    bool isBloomEnabled = true;
    bool isVSyncEnabled = true;
    glfwSwapInterval(1);        // <- FORCE V-Sync ON by DEFAULT.

    // DECLARED globally for the frame so the GPU Shader can access it.
    static float avgEnergy = 0.1f;

    // NEW: BOOT UP the Native Mac Menu Bar.
    #ifdef __APPLE__
        bool isMenuBarActive = InitMenuBar();
    #endif

    // ==========================================
    // NEW: THE GPU Profiler.
    // ==========================================
    unsigned int gpuTimeQuery;
    glGenQueries(1, &gpuTimeQuery);
    
    while (window.IsOpen())
    {
        trumFaster.StartFrame(); // START the STOPWATCH.

        // NEW: ASK the GPU HARDWARE to START computing nanosec.
        glBeginQuery(GL_TIME_ELAPSED, gpuTimeQuery);

        // ==========================================
        // NEW: FRAME-RATE INDEPENDENT PHYSICS (DELTA TIME)
        // ==========================================
        // [C++ LEARNING] "Delta time" = how many SECONDS actually passed since the last frame.
        // Without this, gravityStrength and lerpAttackSpeed get applied once PER FRAME,
        // which means the bars fall/rise at different SPEEDS depending on your FPS.
        // This measures the real elapsed time so the physics feels the same at 30fps, 60fps, or 144fps.
        static auto lastFrameTime = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        float deltaTime = std::chrono::duration<float>(now - lastFrameTime).count();
        lastFrameTime = now;
        if (deltaTime > 0.1f) deltaTime = 0.1f;  // SAFETY: clamp huge stalls (window drag, breakpoint) so bars don't teleport

        // ==========================================
        // PROCESS NATIVE DRAG & DROP
        // ==========================================
        if (hasDroppedPaths) {
            hasDroppedPaths = false; 
            if (!asyncDroppedPaths.empty()) {
                std::cout << "[ENGINE] Processing " << asyncDroppedPaths.size() << " dropped item(s)...\n";
                
                std::vector<std::string> tempPlaylist;
                std::string parentDir = "";

                // Iterate through EVERY single item dropped simultaneously
                for (const auto& droppedPath : asyncDroppedPaths) {
                    if (fs::is_directory(droppedPath)) {
                        // It's a folder: scan it
                        if (parentDir.empty()) parentDir = droppedPath; 
                        for (const auto &entry : fs::directory_iterator(droppedPath)) {
                            std::string ext = entry.path().extension().string();
                            if (ext == ".mp3" || ext == ".MP3" || ext == ".wav" || ext == ".WAV" || ext == ".flac" || ext == ".FLAC") {
                                tempPlaylist.push_back(entry.path().string());
                            }
                        }
                    } else {
                        // It's an individual file: just add it directly
                        std::string ext = fs::path(droppedPath).extension().string();
                        if (ext == ".mp3" || ext == ".MP3" || ext == ".wav" || ext == ".WAV" || ext == ".flac" || ext == ".FLAC") {
                            tempPlaylist.push_back(droppedPath);
                            // Steal the parent directory name for the UI Text box
                            if (parentDir.empty()) parentDir = fs::path(droppedPath).parent_path().string();
                        }
                    }
                }

                // Sort the massive combined playlist
                std::sort(tempPlaylist.begin(), tempPlaylist.end());

                if (!tempPlaylist.empty()) {
                        // UPDATE UI Box.
                        strncpy(folderPathBuffer, parentDir.c_str(), sizeof(folderPathBuffer) - 1);
                        folderPathBuffer[sizeof(folderPathBuffer) - 1] = '\0';

                        playlist = tempPlaylist;
                        player.Stop();
                        currentTrackIndex = 0;
                        selectedTrackPath = playlist[currentTrackIndex];
                        cleanTrackName = fs::path(selectedTrackPath).filename().stem().string();

                        player.Load(selectedTrackPath);
                        player.SetVolume(currentVolume);
                        trackDuration = player.GetDuration();
                        player.Play(); 
                        isUserPaused = false;

                        // TRIGGER OS Notification.
                        ShowOSNotification(cleanTrackName);

                        for (size_t i = 0; i < frozenFrequencies.size(); i++) frozenFrequencies[i] = 0.0f;
                } else {
                    std::cout << "[ENGINE WARNING] No Audio files found in the dropped items!\n";
                }
            }
        }

        // ==========================================
        // NEW FEATURE: CONTINUOUS PLAYBACK (AUTO-NEXT)
        // ==========================================
        // If the music stopped naturally (the user didn't click pause)... the track is over!
        if (!isUserPaused && trackDuration > 0.0f && !playlist.empty() && player.GetCurrentPosition() >= (trackDuration - 0.1f)){
            
            player.Stop(); // ENSURE the hardware is FULLY STOPPED.
            bool shouldPlayNext = true;
            std::cout << "[ENGINE] TRACK FINISHED. Auto-playing next track...\n";

            // CHECK the STATE MACHINE.
            if (playbackMode == 2) {
                // REPEAT: The index stays exactly the same. DO NOTHING WITH IT.
            } else if (playbackMode == 3) {
                // SHUFFLE: PICK a COMPLETELY random track index.
                currentTrackIndex = rand() % playlist.size();
            } else if (playbackMode == 0 && currentTrackIndex == playlist.size() - 1) {
                // NORMAL MODE: If we are on the last track, stop the music completely.
                isUserPaused = true;
                shouldPlayNext = false;
            } else {
                // REPEAT ALL (or Normal mode not at the end): Move forward 1 track
                currentTrackIndex = (currentTrackIndex + 1) % playlist.size();
            }

            // ONLY load the next track if Normal Mode didn't halt the player.
            if (shouldPlayNext) {
                // 1. LOAD and PLAY the next track.
                selectedTrackPath = playlist[currentTrackIndex];
                cleanTrackName = fs::path(selectedTrackPath).filename().stem().string();

                // 2. LOAD the NEW TRACK AND APPLY USER SETTINGS.
                player.Load(selectedTrackPath);
                player.SetVolume(currentVolume);
                trackDuration = player.GetDuration();

                // 3. START the MUSIC.
                player.Play();

                // 4. TRIGGER the OS Notification.
                ShowOSNotification(cleanTrackName);
            }
            // RESET the VISUALIZER bars.
            for (size_t i = 0; i < frozenFrequencies.size(); i++) frozenFrequencies[i] = 0.0f;
        }

        // ==========================================
        // NEW: RESPONSIVE FIX for PREVENT UI and Canvas Stretching.
        // ==========================================
        int fbWidth, fbHeight;
        glfwGetFramebufferSize(window.GetGLFWWindowPointer(), &fbWidth, &fbHeight);
        glViewport(0, 0, fbWidth, fbHeight);

        window.Clear(0.0f, 0.0f, 0.0f, 1.0f);
        
        // ==========================================
        // IMGUI Phase 2: START NEW UI FRAME.
        // ==========================================
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // NOTE:QUERY the actual dynamic size of the WINDOW!
        ImVec2 viewportSize = ImGui::GetMainViewport()->Size; 

        // ==========================================
        // PLAY/PAUSE FREEZE LOGIC
        // ==========================================
        // 'static' means this variable remembers its data even when the frame restarts.
        // It holds the "frozen" shape of the bars when you hit pause!
        static std::vector<float> frozenFrequencies(1024, 0.0f);

        // -----------------------------------
        // ONLY crunch the heavy FFT math if the music is actually playing
        // -----------------------------------
        if (player.IsPlaying()){
            std::vector<float> samples = player.GetLatestSamples(FFT_WINDOW_SIZE);

            for (size_t i = 0; i < samples.size(); i++)
            {
                float windowValue = 0.5f * (1.0f - std::cos((2.0f * M_PI * i) / (FFT_WINDOW_SIZE - 1)));
                samples[i] *= windowValue;
            }

            frozenFrequencies = fft.Process(samples);
            frozenFrequencies[0] = 0.0f;
            frozenFrequencies[1] = 0.0f;
        }

        // ==========================================
        // NEW: Zen Mode (Auto-hide UI)
        // ==========================================
        static double lastInteractionTime = glfwGetTime();

        // WAKE up UI on mouse movement, clicks, or keyboard arrow keys.
        if (std::abs(io.MouseDelta.x) > 0.0f || std::abs(io.MouseDelta.y) > 0.0f || ImGui::IsAnyMouseDown() || 
            ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false) || ImGui::IsKeyPressed(ImGuiKey_RightArrow, false) ||
            ImGui::IsKeyPressed(ImGuiKey_Minus, false) || ImGui::IsKeyPressed(ImGuiKey_Equal, false)) {
            lastInteractionTime = glfwGetTime();
        }

        double idleTime = glfwGetTime() - lastInteractionTime;
        float uiAlpha = 1.0f;
        const double IDLE_TIMEOUT = 3.0; // WAIT 3 Sec before HIDING.

        if (idleTime > IDLE_TIMEOUT) {
            uiAlpha = 1.0f - static_cast<float>(idleTime - IDLE_TIMEOUT); // 1 Sec smooth fade out.
            if (uiAlpha < 0.0f) uiAlpha = 0.0f;
        }

        // ==========================================
        // FPS & TrumFaster OVERLAY (Perfectly centered).
        // ==========================================
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, uiAlpha);      // START Top UI FADE.
        std::string fpsText = "FPS: " + std::to_string((int)trumFaster.GetActualFPS());
        // NEW: FORMAT the raw GPU millisec cleanly.
        char gpuBuffer[32];
        snprintf(gpuBuffer, sizeof(gpuBuffer), " | GPU: %.2fms", trumFaster.GetGPUFrameTime());
        std::string gpuText = std::string(gpuBuffer);
        std::string tfStatusText = isTrumFasterEnabled ? " | TrumFaster: ON" : " | TrumFaster: OFF";
        std::string bloomStatusText = isBloomEnabled ? " | GPU Bloom: ON" : " | GPU Bloom: OFF";
        std::string vsyncStatusText = isVSyncEnabled ? " | V-Sync: ON" : " | V-Sync: OFF";
        std::string tbStatusText = isTouchBarActive ? " | Touch Bar Display: ON" : " | Touch Bar Display: OFF"; 

        // SPLIT into TWO neat lines.
        std::string telemetryText = fpsText + gpuText + tfStatusText + bloomStatusText + vsyncStatusText + tbStatusText;

        ImVec2 telemetrySize = ImGui::CalcTextSize(telemetryText.c_str());

        // POSITION it to 10 pixels from the top edge of THE SCREEN.
        ImGui::SetNextWindowPos(ImVec2(0.0f, 10.0f));
        ImGui::SetNextWindowSize(ImVec2(viewportSize.x, 30.0f));
        ImGui::Begin("Telemetry_Overlay", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoScrollbar);

        // FIXED: I must move the cursor to the CALCULATED center.
        // DRAW Line.
        ImGui::SetCursorPosX((viewportSize.x - telemetrySize.x) * 0.5f);
        // LET the user know if optimization is ACTIVE.
        if (isTrumFasterEnabled) {
            // GLOWS Green when optimized.
            ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.2f, 1.0f), "%s", telemetryText.c_str());
        } else {
            // TURNS Red when running unoptimized.
            ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "%s", telemetryText.c_str());
        }
        ImGui::End();
        ImGui::PopStyleVar(); // END Top UI FADE (to protect the Spectrum)

        // ==========================================
        // IMGUI Phase 3: RESPONSIVE "PILL" INTERFACE
        // ==========================================
        // I Wide the pill slightly to 880.0f so the toggle button FITS PERFECLY.
        float pillWidth = 880.0f;
        float pillHeight = 190.0f;
        // RESPONSIVE MATH: Center X, and lock Y to 60 pixels above the BOTTOM edge.
        float pillPosX = (viewportSize.x - pillWidth) * 0.5f;
        float pillPosY = (viewportSize.y - pillHeight) - 40.0f;

        // ==========================================
        // New: TRUE RESPONSIVE MATH (PERCENTAGES) AND VISUALIZER STATE MACHINE
        // ==========================================
        static int visualMode = 0; // 0 = CLASSIC BOTTOM, 1 = CENTER WAVEFORM, 2 = Neon Polyline, 3 = Drake (Special), 4 = Mortal Kombat (Brutal).

        // TrumFaster: LOD Override (FIXED).
        int targetBars = (visualMode == 0) ? 16 : (visualMode == 4 ? 80 : 64);
        TrumFasterProfile tfProfile;

        if (isTrumFasterEnabled) {
            tfProfile = trumFaster.GetOptimizedProfile(targetBars, visualMode);
        } else {
            // MANUAL OVERRIDE: IF TrumFaster is OFF, FORCE maximum QUALITY.
            tfProfile.activeBars = targetBars;
            tfProfile.enableShadows = true;
            tfProfile.lerpAttackSpeed = 0.92f;
        }

        const size_t DISPLAY_BARS = tfProfile.activeBars;

        // ==========================================
        // New: ZEN MODE, FLUID WIDTH EXPANSION.
        // ==========================================
        float zenFactor = 1.0f - uiAlpha; // 0.0 = UI Visible, 1.0 = UI Hidden (Zen).
        
        // Dynamic Width: THE VISUALIZER takes up 80% of the window width
        float maxAvailableWidth = viewportSize.x * (0.8f + (0.15f * zenFactor)); // EXPANDS from 80% to 95%
        float maxWidthCap = 1200.0f + (5000.0f * zenFactor); // Smoothly removes the 1200px limit
        // PUT a cap on it so it doesn't scretch too far on ultra-wide monitors
        if (maxAvailableWidth > maxWidthCap) maxAvailableWidth = maxWidthCap;
        // Divide the available space evenly
        float barSpacing = maxAvailableWidth / DISPLAY_BARS;

        // THE BAR takes up 55% of its given slot, leaving a 45% empty cap
        float barWidth = (visualMode == 0) ? barSpacing * 0.55f : barSpacing * 0.45f;

        float totalBarsWidth = (DISPLAY_BARS - 1) * barSpacing + barWidth;
        float startPosX = (viewportSize.x - totalBarsWidth) * 0.5f;

        // ==========================================
        // NEW: Physics State & AUTO-GAIN COMPRESSOR.
        // ==========================================
        // 'static' means this array survives between frames so it remembers the heights!
        static std::vector<float> barVelocities(128, 0.0f);     // TRACKS failing speed.
        static float avgEnergy = 0.1f;                          // MEMORY for Auto-Gain.

        // ==========================================
        // NEW: GPU Shader BACKGROUND EXECUTION. 
        // ==========================================
        // FIXED: Force a clean GL State so nothing hides the background
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Step1: ACTIVATE the custom graphics program on the GPU.
        glUseProgram(backgroundShaderProgram);

        // Step2: INJECT real-time C++ variables into the GPU memory.
        glUniform2f(glGetUniformLocation(backgroundShaderProgram, "u_resolution"), (float)fbWidth, (float)fbHeight);
        glUniform1f(glGetUniformLocation(backgroundShaderProgram, "u_time"), (float)glfwGetTime());
        glUniform1f(glGetUniformLocation(backgroundShaderProgram, "u_audioEnergy"), avgEnergy); 

        // Step3: COMMAND the GPU to DRAW the pixels.
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // FinalStep: RESET state so ImGui doesn't break.
        glUseProgram(0); 
        glBindVertexArray(0);

        // LIVE Tweak VARIABLES.
        static float baseSensitivity = 12.0f;
        static float gravityStrength = 0.005f;
        // [C++ LEARNING] gravityStrength/lerpAttackSpeed were originally TUNED assuming 60 frames per second.
        // dtScale converts real elapsed time into "how many 60fps-frames' worth of time just passed"
        // so the OLD tuned numbers still feel exactly the same, they just no longer depend on FPS. 
        float dtScale = deltaTime * 60.0f;

        // STEP 1: CALCULATE Total frame Energy from AGC.
        float currentEnergy = 0.0f;
        for (float val : frozenFrequencies) currentEnergy += val;

        // STEP 2: SMOOTH Rolling average (98% old, 2% new).
        // FRAME-RATE INDEPENDENT AUTO-GAIN SMOOTHING:
        // Same issue as the bars: blending 98%/2% every FRAME means the loudness-normalization
        // reacts faster at high FPS and slower at low FPS. Scale the "2% new" rate by dtScale too.
        float agcRate = 1.0f - std::pow(1.0f - 0.02f, dtScale);
        avgEnergy = avgEnergy + (currentEnergy - avgEnergy) * agcRate;

        // STEP 3: THE Magic Inverse CURVE AGC (Tames Dubstep, Boosts Acoustic)
        float agcMultiplier = 15.0f / std::max(avgEnergy, 1.0f);

        // MODE 2: (Polyline) REQUIRES keeping track on points.
        static std::vector<float> smoothHeights(128, 0.0f);
        std::vector<ImVec2> mainLinePoints;
        std::vector<ImVec2> shadowLinePoints;
        std::vector<ImVec2> rawLinePoints; // NEW: Holds raw peaks for GPU Spline application.

        // ==================== SCREEN SHAKE (KOMBAT MODE & FALLBACK) ====================
        static int shakeFrames = 0;
        float shakeOffsetX = 0.0f; // Declared here so it's always defined!
        float shakeOffsetY = 0.0f;

        if (visualMode == 4 && smoothHeights[0] > 0.95f) {
            shakeFrames = 3;
        }

        if (shakeFrames > 0) {
            shakeOffsetX = ((rand() % 11) - 5.0f);
            shakeOffsetY = ((rand() % 11) - 5.0f);
            shakeFrames--;
        }

        // v0.9.3 RESPONSIVE FIX: Anchor relative to the screen height, not the UI Pill!
        // This stops the polyline from flying off the top of the window when shrunk.
        startPosX += shakeOffsetX;

        // ==========================================
        // Zen Mode: FLUID VERTICAL CENTERING
        // ==========================================
        float normalCenterY = viewportSize.y * 0.45f;
        float zenCenterY = viewportSize.y * 0.50f; // Perfect dead center
        float centerY = normalCenterY + ((zenCenterY - normalCenterY) * zenFactor) + shakeOffsetY;

        if (visualMode >= 2){
            rawLinePoints.push_back(ImVec2(startPosX - 40.0f, centerY));
        }

        float minLog = std::log10(2.0f);
        float maxLog = std::log10(256.0f);

        // ONE UNIFIED LOOP FOR ALL MATH.
        for (size_t b = 0; b < DISPLAY_BARS; b++)
        {
            float startLog = minLog + (maxLog - minLog) * ((float)b / DISPLAY_BARS);
            float endLog = minLog + (maxLog - minLog) * ((float)(b + 1) / DISPLAY_BARS);

            size_t startIndex = (size_t)std::pow(10.0f, startLog);
            size_t endIndex = (size_t)std::pow(10.0f, endLog);
            if (endIndex <= startIndex) endIndex = startIndex + 1;

            float binAverage = 0.0f;
            int count = 0;
            for (size_t j = startIndex; j < endIndex; j++)
            {
                // size_t index = 2 + (b * binsPerBar) + j;
                if (j < frozenFrequencies.size()) {
                    binAverage += frozenFrequencies[j];
                    count++;
                }
            }
            if (count > 0) binAverage /= count;

            // ==========================================
            // NEW: Physics State & AUTO-GAIN COMPRESSOR
            // ==========================================
            // STEP 1: A-Weighting (Pink Noise EQ Slope)
            float percentage = (float)b / DISPLAY_BARS;
            // Mid-range gets a TINY boost, extreme highs get massive exponential boost (up to 4.0x)
            float eqBoost = 1.0f + std::pow(percentage, 2.0f) * 3.0f;
            float boostedAverage = binAverage * eqBoost;

            // STEP 2: APPLY Auto-Gain Compressor.
            float logValue = std::log10(boostedAverage * baseSensitivity * agcMultiplier + 1.0f);
            const float MAX_LOG_VALUE = 2.8f;
            
            float targetHeight = logValue / MAX_LOG_VALUE;
            if (targetHeight > 1.0f) targetHeight = 1.0f;
            if (targetHeight < 0.0f) targetHeight = 0.0f;

            // ==========================================
            // NEW: GRAVITY-BASED KINEMATICS (PUNCH)
            // ==========================================
            if (targetHeight > smoothHeights[b]) {
                // FRAME-RATE INDEPENDENT LERP:
                // Old version did (target - current) * lerpAttackSpeed EVERY FRAME, so it moved further per second at higher FPS.
                // This version bends the SAME lerp math so it converges at the SAME real-world speed no matter the frame rate.
                smoothHeights[b] += (targetHeight - smoothHeights[b]) * (1.0f - std::pow(1.0f - tfProfile.lerpAttackSpeed, dtScale));
                barVelocities[b] = 0.0f; 
            } else {
                // GRAVITY, SCALED TO REAL TIME:
                // Both the acceleration AND the distance fallen now scale with dtScale,
                // which is what real free-fall physics does (distance grows with time SQUARED).
                barVelocities[b] += gravityStrength * dtScale;   // Velocity accelerates downwards, scaled to real time
                smoothHeights[b] -= barVelocities[b] * dtScale;  // Apply falling speed to height, scaled to real time

                // Failsafe: Don't fall through the floor!
                if (smoothHeights[b] < 0.0f) {
                    smoothHeights[b] = 0.0f;
                    barVelocities[b] = 0.0f;
                }
            }

            // ==========================================
            // NEW: DYNAMIC BAR HEIGHTS & ANCHORING
            // ==========================================
            // DYNAMIC HEIGHT: Bars can grow up to 45% of the window's total height
            float maxBarHeight = viewportSize.y * 0.45f;
            float actualHeight = smoothHeights[b] * maxBarHeight;

            // UNIVERSAL VARIABLES.
            float xPixelPos = startPosX + (b * barSpacing);
            float topY, bottomY, cornerRadius;
            ImU32 dynamicColor;

            if (visualMode == 0) {
                // MODE 0: CLASSIC BOTTOM ANCHOR (Colorful & Round, Zen Expansion)
                // 1. SHORTER HEIGHT (20% of screen max) so it isn't OVERWHELMING.
                float targetHeightScale = viewportSize.y * (0.55f + (0.25f * zenFactor)); // GROWS from 55% to 80% height.
                float mode0Height = smoothHeights[b] * targetHeightScale;
                if (mode0Height < viewportSize.y * 0.02f) mode0Height = viewportSize.y * 0.02f;

                float normalBottomY = pillPosY - 30.0f;
                float zenBottomY = viewportSize.y - 40.0f; // Anchor closer to the very bottom in Zen mode
                bottomY = normalBottomY + ((zenBottomY - normalBottomY) * zenFactor);
                topY = bottomY - mode0Height;

                // 2. DYNAMIC EDM COLOR (Heatmap Effect):
                // Starts at 0.6 (Cool Blue/Cyan). As the bar grows, it pushes towards 0.0 (Bright Red).
                float hue = 0.6f - (smoothHeights[b] * 0.6f);
                if (hue < 0.0f) hue = 0.0f; // FAILSAFE to PREVENT NEGATIVE colors.

                // 100% Brightness and High Saturation for that neon club look
                ImU32 dynamicColor = ImColor::HSV(hue, 0.9f, 1.0f);

                ImGui::GetBackgroundDrawList()->AddRectFilled(
                    ImVec2(xPixelPos, topY),
                    ImVec2(xPixelPos + (barWidth * 0.8f), bottomY),
                    dynamicColor, 6.0f // Modern, subtle rounded corners
                );
                

            } else if (visualMode == 1) {
                // ==========================================
                // MODE 1: CENTER WAVEFORM (The "SKRILLEX FIX") and also Zen Expansion
                // ==========================================
                // 1. THE DAMPENER: Multiply by 0.6f to COMPRESS loud DUBSTEP peaks
                float zenMode1Multiplier = 0.6f + (0.4f * zenFactor); // Expand height by 40%
                float mode1Height = actualHeight * zenMode1Multiplier;
                // 2. HARD CEILING: A failsafe so it NEVER grows taller than 75% of your screen.
                float maxSafeHeight = viewportSize.y * 0.90f; // I ALLOW it to get bigger safely.
                if (mode1Height > maxSafeHeight) mode1Height = maxSafeHeight;
                // 3. CENTER ANCHOR: Pin the bars precisely 45% up the screen
                float normalM1Center = viewportSize.y * 0.45f;    // Anchored to middle of the screen.
                float zenM1Center = viewportSize.y * 0.50f;
                float localCenterY = normalM1Center + ((zenM1Center - normalM1Center) * zenFactor);
                topY = localCenterY - (mode1Height * 0.5f);           // Grow UP from center.
                bottomY = localCenterY + (mode1Height * 0.5f);       // Grow DOWN from center.
                // 4. DYNAMIC COLOR GRADIENT (Left to Right)
                // We divide the current bar 'b' by the total bars (64) to get a percentage.
                float barPercentage = static_cast<float>(b) / DISPLAY_BARS;
                // Start at 0.5 (Cyan) and smoothly shift towards 0.85 (Purple/Pink).
                float hue = 0.5f + (barPercentage * 0.35f);
                // 5. Reduce EYE STRAIN.
                // Brightness pulses with the music beat, but never goes fully dark
                float brightness = 0.5f + (smoothHeights[b] * 0.5f);

                // GPU Bloom Pass (Mode 1).
                if (isBloomEnabled) {
                    ImGui::GetBackgroundDrawList()->AddRectFilled(
                        ImVec2(xPixelPos - 4.0f, topY - 4.0f), ImVec2(xPixelPos + barWidth + 4.0f, bottomY + 4.0f),
                        ImColor::HSV(hue, 0.8f, brightness, 0.15f), 6.0f
                    );
                    ImGui::GetBackgroundDrawList()->AddRectFilled(
                        ImVec2(xPixelPos - 8.0f, topY - 8.0f), ImVec2(xPixelPos + barWidth + 8.0f, bottomY + 8.0f),
                        ImColor::HSV(hue, 0.8f, brightness, 0.05f), 12.0f
                    );
                }

                // The final 0.85f drops the opacity to 85%, killing the blinding glare!
                ImGui::GetBackgroundDrawList()->AddRectFilled(
                    ImVec2(xPixelPos, topY), ImVec2(xPixelPos + barWidth, bottomY),
                    ImColor::HSV(hue, 0.8f, brightness, 0.85f), 2.0f
                );

            } else if (visualMode >= 2 && visualMode <= 4) {
                // MODE 2, 3 & 4: Gather raw PEAKS for SPLINE (Zen Expansion).
                float targetHeightScale = viewportSize.y * (0.35f + (0.12f * zenFactor)); // EXPANDS from 35% to 47%
                float mode2Height = smoothHeights[b] * targetHeightScale;
                float peakY = centerY - mode2Height;
                float centerOfBarX = xPixelPos + (barWidth * 0.5f);

                rawLinePoints.push_back(ImVec2(centerOfBarX, peakY));
            } else if (visualMode == 5) {
                // ==========================================
                // Mode 5: Radial / Circular (Arc Reactor)
                // ==========================================
                // NEW, ZEN EXPANSION: BOTH the core ring and the bars grow MASSIVELY when UI hides.
                float innerRadius = viewportSize.y * (0.15f + (0.10f * zenFactor));    // THE empty circle in the center.
                float radialBarLen = actualHeight * (0.4f + (0.2f * zenFactor));       // Grows outward.

                // MIRRORED Math: DRAW two bars per loop (one left, one right).
                // M_PI is 180 DEGREES. Subtracting M_PI/2 OFFSETS it so index 0 (bass) is PERFECTLY at 12 o'clock.
                float angleRight = ((float)b / DISPLAY_BARS) * M_PI - (M_PI / 2.0f);
                float angleLeft  = -((float)b / DISPLAY_BARS) * M_PI - (M_PI / 2.0f);
                
                // SCREEN CENTERS.
                float cx = viewportSize.x * 0.5f;

                // TRIGONOMETRY: X = Center + Cos(Angle)*Radius | Y = Center + Sin(Angle)*Radius
                // INNER POINTS (Where the bar starts)
                ImVec2 innerR = ImVec2(cx + std::cos(angleRight) * innerRadius, centerY + std::sin(angleRight) * innerRadius);
                ImVec2 innerL = ImVec2(cx + std::cos(angleLeft) * innerRadius, centerY + std::sin(angleLeft) * innerRadius);

                // OUTER POINTS (Where the bar ends)
                ImVec2 outerR = ImVec2(cx + std::cos(angleRight) * (innerRadius + radialBarLen), centerY + std::sin(angleRight) * (innerRadius + radialBarLen));
                ImVec2 outerL = ImVec2(cx + std::cos(angleLeft) * (innerRadius + radialBarLen), centerY + std::sin(angleLeft) * (innerRadius + radialBarLen));

                // COLOR GRADIENT (Cyan -> Pink/Purple) matching the CONCEPT IMAGE.
                float hue = 0.5f + ((float)b / DISPLAY_BARS) * 0.4f;
                float brightness = 0.5f + (smoothHeights[b] * 0.5f);
                ImU32 color = ImColor::HSV(hue, 0.9f, brightness);

                // SCALE thickness DYNAMICALLY based on SCREEN WIDTH
                float radThickness = (maxAvailableWidth / 150.0f);

                if (isBloomEnabled) {
                    ImU32 glow = ImColor::HSV(hue, 0.9f, brightness, 0.25f);
                    ImGui::GetBackgroundDrawList()->AddLine(innerR, outerR, glow, radThickness * 3.0f);
                    ImGui::GetBackgroundDrawList()->AddLine(innerL, outerL, glow, radThickness * 3.0f);
                }

                // DRAW Solid Core.
                ImGui::GetBackgroundDrawList()->AddLine(innerR, outerR, color, radThickness);
                ImGui::GetBackgroundDrawList()->AddLine(innerL, outerL, color, radThickness);
            }
        } // END OF BAR DRAWING LOOP.

        // Touch Bar Features On Engine.cpp.
        #ifdef __APPLE__
            // SEND THE SMOOTHED HEIGHTS TO THE MAC TOUCH BAR AT 60FPS!
            if (isTouchBarActive) {
                std::vector<float> touchBarData(smoothHeights.begin(), smoothHeights.begin() + DISPLAY_BARS);
                UpdateTouchBar(touchBarData);
            }

            // NEW: SEND the same smoothed heights to THE Mac MENU BAR SECTION.
            std::vector<float> menuBarData(smoothHeights.begin(), smoothHeights.begin() + DISPLAY_BARS);
            UpdateMenuBar(menuBarData);
        #endif

        // EXECUTE Polyline DRAW OUTSIDE the LOOP.
        if (visualMode >= 2 && visualMode <= 4) {
            // ==========================================
            // HIGH-FIDELITY GPU SPLINE RASTERIZATION
            // ==========================================
            // Increase the vertex count by 10x to force the GPU to render a liquid-smooth curve!
            // This forces the GPU's rasterizer to actually wake up and do some heavy lifting.
            const int SPLINE_RESOLUTION = (visualMode == 3) ? 5 : 10;
            rawLinePoints.push_back(ImVec2(startPosX + totalBarsWidth + 40.0f, centerY));

            if (rawLinePoints.size() >= 2) {
                // Catmull-Rom Spline Math Lambda.
                auto CatmullRom = [](const ImVec2& p0, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, float t) {
                    float t2 = t * t;
                    float t3 = t2 * t;
                    return ImVec2(
                        0.5f * ((2.0f * p1.x) + (-p0.x + p2.x) * t + (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 + (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3),
                        0.5f * ((2.0f * p1.y) + (-p0.y + p2.y) * t + (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 + (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3)
                    );
                };

                // INTERPOLATE a smooth curve between every SINGLE audio peak.
                for (size_t i = 0; i < rawLinePoints.size() - 1; i++) {
                    ImVec2 p1 = rawLinePoints[i];
                    ImVec2 p2 = rawLinePoints[i + 1];
                    ImVec2 p0 = (i == 0) ? ImVec2(p1.x - (p2.x - p1.x), p1.y) : rawLinePoints[i - 1];
                    ImVec2 p3 = (i + 2 < rawLinePoints.size()) ? rawLinePoints[i + 2] : ImVec2(p2.x + (p2.x - p1.x), p2.y);

                    // Generate high-density vertices for the GPU
                    for (int j = 0; j < SPLINE_RESOLUTION; j++) {
                        float t = static_cast<float>(j) / static_cast<float>(SPLINE_RESOLUTION);
                        ImVec2 interpolated = CatmullRom(p0, p1, p2, p3, t);
                        mainLinePoints.push_back(interpolated);
                        shadowLinePoints.push_back(ImVec2(interpolated.x, interpolated.y + 6.0f));
                    }
                }
                // PUSH the final point perfectly.
                mainLinePoints.push_back(rawLinePoints.back());
                shadowLinePoints.push_back(ImVec2(rawLinePoints.back().x, rawLinePoints.back().y + 6.0f));
            }

            // TRUE GPU BLOOM (Multi-Pass Spline Rendering)
            if (visualMode == 2) {
                if (isBloomEnabled) {
                    // Massive Outer Glow (Bleeds light into background)
                    ImGui::GetBackgroundDrawList()->AddPolyline(shadowLinePoints.data(), shadowLinePoints.size(), IM_COL32(230, 70, 230, 25), ImDrawFlags_None, 24.0f);
                    // Tighter Inner Glow
                    ImGui::GetBackgroundDrawList()->AddPolyline(shadowLinePoints.data(), shadowLinePoints.size(), IM_COL32(230, 70, 230, 60), ImDrawFlags_None, 12.0f);
                } else {
                    // Standard Shadow
                    ImGui::GetBackgroundDrawList()->AddPolyline(shadowLinePoints.data(), shadowLinePoints.size(), IM_COL32(230, 70, 230, 255), ImDrawFlags_None, 6.0f);
                }
                // Core Hot Neon Line (Turns white-hot when blooming)
                ImU32 coreColor = isBloomEnabled ? IM_COL32(220, 220, 255, 255) : IM_COL32(70, 70, 230, 255);
                float coreThickness = isBloomEnabled ? 3.0f : 6.0f;
                ImGui::GetBackgroundDrawList()->AddPolyline(mainLinePoints.data(), mainLinePoints.size(), coreColor, ImDrawFlags_None, coreThickness);

            } else if (visualMode == 3) {
                // MODE 3: DRAKE (OVO GOLD MIRRORED)
                std::vector<ImVec2> bottomLinePoints;
                for (const auto& p : mainLinePoints) {
                    bottomLinePoints.push_back(ImVec2(p.x, centerY + (centerY - p.y)));
                }   

                if (isBloomEnabled) {
                    // Optimized thickness to 24px and explicit int casts
                    ImGui::GetBackgroundDrawList()->AddPolyline(mainLinePoints.data(), (int)mainLinePoints.size(), IM_COL32(212, 175, 55, 30), 0, 24.0f);
                    ImGui::GetBackgroundDrawList()->AddPolyline(bottomLinePoints.data(), (int)bottomLinePoints.size(), IM_COL32(212, 175, 55, 30), 0, 24.0f);
                    
                    ImGui::GetBackgroundDrawList()->AddPolyline(mainLinePoints.data(), (int)mainLinePoints.size(), IM_COL32(255, 230, 150, 80), 0, 10.0f);
                    ImGui::GetBackgroundDrawList()->AddPolyline(bottomLinePoints.data(), (int)bottomLinePoints.size(), IM_COL32(255, 230, 150, 80), 0, 10.0f);
                }

                ImGui::GetBackgroundDrawList()->AddPolyline(mainLinePoints.data(), (int)mainLinePoints.size(), IM_COL32(255, 255, 255, 255), 0, 3.0f);
                ImGui::GetBackgroundDrawList()->AddPolyline(bottomLinePoints.data(), (int)bottomLinePoints.size(), IM_COL32(255, 255, 255, 255), 0, 3.0f);

                for (size_t i = 0; i < mainLinePoints.size(); i += SPLINE_RESOLUTION) {
                    float opacity = isBloomEnabled ? 100.0f : 40.0f;
                    ImGui::GetBackgroundDrawList()->AddLine(mainLinePoints[i], bottomLinePoints[i], IM_COL32(212, 175, 55, (int)opacity), 1.0f);
                }
            } else if (visualMode == 4) {
                // MODE 4: MORTAL KOMBAT (Brutal Jagged Spikes)
                // I DO NOT use Catmull-Rom splines here. We draw sharp, violent lines directly between peaks.

                // TRACK 1 & 2 REPRESENTS extreme sub-bass ENERGY.
                float energy = smoothHeights[1] + smoothHeights[2];
                
                ImU32 fillColor;
                ImU32 outlineColor;
                if (energy > 1.3f) { // IF LOUD: Shao Kahn's Blood.
                    fillColor = IM_COL32(200, 0, 0, 100);
                    outlineColor = IM_COL32(255, 20, 20, 255);
                } else if (energy > 0.6) { // IF MEDIUM: Scorpion's Hellfire.
                    fillColor = IM_COL32(200, 100, 0, 100);
                    outlineColor = IM_COL32(255, 150, 0, 255);
                } else { // IF QUIET: Sub-Zero's Ice
                    fillColor = IM_COL32(0, 150, 200, 100); 
                    outlineColor = IM_COL32(0, 220, 255, 255);
                }

                if (rawLinePoints.size() >= 2) {
                    // DRAW segment by segment to SAFELY fill a complex, concave jagged shape.
                    for (size_t i = 0; i < rawLinePoints.size() - 1; i++) {
                        ImVec2 t1 = rawLinePoints[i];
                        ImVec2 t2 = rawLinePoints[i + 1];
                        ImVec2 b1 = ImVec2(t1.x, centerY + (centerY - t1.y)); // Mirror bottom
                        ImVec2 b2 = ImVec2(t2.x, centerY + (centerY - t2.y)); // Mirror bottom

                        // FILL Center connecting top points and bottom mirrored points.
                        ImGui::GetBackgroundDrawList()->AddQuadFilled(t1, t2, b2, b1, fillColor);
                    }

                    // Draw the sharp, violent outer jagged lines
                    std::vector<ImVec2> bottomPoints;
                    for (const auto& p : rawLinePoints) {
                        bottomPoints.push_back(ImVec2(p.x, centerY + (centerY - p.y)));
                    }
                    
                    if (isBloomEnabled) { // Adds a glowing aura
                        ImGui::GetBackgroundDrawList()->AddPolyline(rawLinePoints.data(), rawLinePoints.size(), fillColor, 0, 15.0f);
                        ImGui::GetBackgroundDrawList()->AddPolyline(bottomPoints.data(), bottomPoints.size(), fillColor, 0, 15.0f);
                    }
                    
                    ImGui::GetBackgroundDrawList()->AddPolyline(rawLinePoints.data(), rawLinePoints.size(), outlineColor, 0, 3.0f);
                    ImGui::GetBackgroundDrawList()->AddPolyline(bottomPoints.data(), bottomPoints.size(), outlineColor, 0, 3.0f);
                }
            }
        }

        // ==========================================
        // IMGUI Phase 3: METADATA & UP NEXT QUEUE, IT'S NOW SEAMLESSLY. (UX FIX Step 1)
        // ==========================================
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, uiAlpha); // START BOTTOM UI Fade.
        std::string nowPlayingText;
        if (cleanTrackName.empty()) {
            nowPlayingText = "Now Playing: No Music Detected. DRAG & DROP A FOLDER OR FILE";
        } else {
            nowPlayingText = "Now Playing: " + cleanTrackName;
        }

        // SAFE "UP NEXT" MATH (Prevents Modulo-by-Zero CRASHES).
        std::string upNextText = "Up Next: None";

        std::string nextTrackName = "None";
        if (playlist.size() > 0) {
            if (playbackMode == 3) {
                // MODE 3: SHUFFLE
                upNextText = "Up Next: Songs will be selected randomly (Shuffle Mode)";

            } else if (playbackMode == 2) {
                // MODE 2: REPEAT 1
                upNextText = "Up Next: Repeating Current Track";

            } else {
                // MODE 0 & 1: NORMAL & REPEAT ALL
                size_t nextIndex = (currentTrackIndex + 1) % playlist.size();
                std::string nextTrackName = fs::path(playlist[nextIndex]).filename().stem().string();
                upNextText = "Up Next: " + nextTrackName;
            }
        }

        // CENTERING math for the main title (NOW PLAYING).
        ImVec2 mainTextSize = ImGui::CalcTextSize(nowPlayingText.c_str());
        float mainTextPosX = (viewportSize.x - mainTextSize.x) * 0.5f;

        // CENTERING math for the the subtitle (UP NEXT).
        ImVec2 subTextSize = ImGui::CalcTextSize(upNextText.c_str());
        float subTextPosX = (viewportSize.x - subTextSize.x) * 0.5f;

        ImGui::SetNextWindowPos(ImVec2(0.0f, 45.0f));
        ImGui::SetNextWindowSize(ImVec2(viewportSize.x, 80.0f)); // Give the window enough height for two lines

        ImGui::Begin("Metadata", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs);

        // 1. DRAW the "Now Playing" text (Bright/White Gray)
        ImGui::SetCursorPosX(mainTextPosX);
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", nowPlayingText.c_str());

        // 2. DRAW the "Up Next" text (Dimmer, slightly transparent green/gray)
        ImGui::SetCursorPosX(subTextPosX);
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.7f, 0.8f), "%s", upNextText.c_str());

        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(pillPosX, pillPosY)); 
        ImGui::SetNextWindowSize(ImVec2(pillWidth, pillHeight));

        // Create a dark, rounded window container
        ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);

        // Center the buttons inside the pill
        ImGui::SetCursorPos(ImVec2(70.0f, 15.0f)); 
        // [C++ LEARNING] 'PushStyleVar' changes the internal ImGui drawing rules.
        // 'FrameRounding, 10.0f' curves the corners of every button drawn after this line!
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);

        // ==================== PREV BUTTON (FIXED) ====================
        // Triggers if clicked OR if '-' (main keyboard or numpad Subtract) is pressed.
        // NEW: MINUS KEY OR NUMPAD MINUS
        bool pressedPrev = ImGui::IsKeyPressed(ImGuiKey_Minus) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract);
        if (ImGui::Button("Prev", ImVec2(100, 50)) || (!io.WantCaptureKeyboard && pressedPrev)) {
            // THE 3 SECOND RULE: check how far into the song we are.
            // UX FIX Step 3: Crash prevention! Only seek/skip if a playlist actually exists.
            if (!playlist.empty()) {
                if (player.GetCurrentPosition() > 3.0f) {
                    player.Stop();
                    player.SeekToPosition(0.0f);
                    if (!isUserPaused) player.Play();
                } else {
                    // IF we are at the beginning, go to ACTUAL previous track.
                    player.Stop();
                    // Loops BACKWARDS cleanly EVEN if you are on Track 1.
                    currentTrackIndex = (currentTrackIndex - 1 + playlist.size()) % playlist.size();
                    selectedTrackPath = playlist[currentTrackIndex];
                    cleanTrackName = fs::path(selectedTrackPath).filename().stem().string();

                    player.Load(selectedTrackPath);
                    player.SetVolume(currentVolume);
                    trackDuration = player.GetDuration();
                    player.Play();
                    isUserPaused = false;

                    // TRIGGER OS Notification.
                    ShowOSNotification(cleanTrackName);
                }
            }
        }

        // [C++ LEARNING] Forces exactly 10 pixels of space between buttons
        ImGui::SameLine(0.0f, 10.0f);

        // ==================== STOP BUTTON (FIXED) ====================
        if (ImGui::Button("STOP", ImVec2(100, 50)) || (!io.WantCaptureKeyboard && ImGui::IsKeyPressed((ImGuiKey_S)))) {
            if (!playlist.empty()) {
                player.Stop();
                // The bars will now gracefully drop to the bottom and wait!
                // [C++ LEARNING] Loop through the array and set every frequency back to 0.0!
                // Because we set it to 0, the Lerp math will gracefully animate the bars falling down.

                // [C++ LEARNING] Reloading the path forces the audio buffer back to 0:00!
                // Now when you hit PLAY next, it will start from the very beginning.
                player.Load(selectedTrackPath); 
                // [C++ LEARNING] Re-apply the volume state immediately so the new track doesn't blast at 100%!
                player.SetVolume(currentVolume);
                isUserPaused = true;

                for (size_t i = 0; i < frozenFrequencies.size(); i++) {
                    frozenFrequencies[i] = 0.0f;
                }
            }
        }

        ImGui::SameLine(0.0f, 10.0f); // C++ LEARNING WHILE CODE: Forces the next button to be on the SAME horizontal row!
        
        // ==================== Dynamic Play/Stop (Stop FIXED) Button ====================
        if (player.IsPlaying()) {
            if (ImGui::Button("PAUSE", ImVec2(100, 50)) || (!io.WantCaptureKeyboard && ImGui::IsKeyPressed((ImGuiKey_Space)))) {
                player.Stop();
                // [C++ LEARNING] Re-apply the volume state immediately so the new track doesn't blast at 100%!
                player.SetVolume(currentVolume);
                isUserPaused = true; // Tell the engine this was intentional!
            }
        } else {
            if (ImGui::Button("PLAY", ImVec2(100, 50)) || (!io.WantCaptureKeyboard && ImGui::IsKeyPressed((ImGuiKey_Space)))) {
                if (!playlist.empty()) {
                    player.Play();
                    // [C++ LEARNING] Re-apply the volume state immediately so the new track doesn't blast at 100%!
                    player.SetVolume(currentVolume);
                    isUserPaused = false; // Music is running naturally again
                }
            }
        }
        
        ImGui::SameLine(0.0f, 10.0f);

        // ==================== NEXT BUTTON ====================
        // Triggers if clicked OR if '+' (Equal key on main keyboard or Numpad Add) is pressed.
        // NEW: EQUAL/PLUS KEY OR NUMPAD PLUS
        bool pressedNext = ImGui::IsKeyPressed(ImGuiKey_Equal) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd);
        if (ImGui::Button("Next", ImVec2(100, 50)) || (!io.WantCaptureKeyboard && pressedNext)) {
            // UX FIX Step 3: Crash prevention! Only skip if a playlist actually exists
            if (!playlist.empty()) {
                player.Stop();
                // Loops back to track 1 if you hit Next on the final track
                currentTrackIndex = (currentTrackIndex + 1) % playlist.size();
                selectedTrackPath = playlist[currentTrackIndex];
                cleanTrackName = fs::path(selectedTrackPath).filename().stem().string();
                player.Load(selectedTrackPath);
                // [C++ LEARNING] Re-apply the volume state immediately so the new track doesn't blast at 100%!
                player.SetVolume(currentVolume);
                trackDuration = player.GetDuration();
                player.Play();
                isUserPaused = false;
            }
        }

        ImGui::SameLine(0.0f, 10.0f);

        // ==================== DYNAMIC PLAYBACK MODE BUTTON ====================
        // THIS ARRAY holds the text for our 4 states.
        const char* modeLabels[] = { "Normal", "Repeat All", "Repeat 1", "Shuffle" };

        // THE BUTTON physically CHANGES its text based on the current playbackMode integer.
        if (ImGui::Button(modeLabels[playbackMode], ImVec2(100, 50))) {

            // [C++ NOTE] This smoothly cycles the number: 0 -> 1 -> 2 -> 3 -> 0 -> 1...
            playbackMode = (playbackMode + 1) % 4;
        }

        // ==================== SEEK BAR (NEW) ====================
        static float uiSliderPos = 0.0f;
        static bool isDraggingSeek = false;
        
        // ONLY update the UI from the real audio if I not DRAGGING the MOUSE
        if (!isDraggingSeek){
            uiSliderPos = player.GetCurrentPosition();
        }
        
        // MATH to CONVERT RAW seconds into minutes and SECONDS.
        int curM = (int)uiSliderPos / 60;
        int curS = (int)uiSliderPos % 60;
        int totM = (int)trackDuration / 60;
        int totS = (int)trackDuration % 60;

        // FORMAT the text to look like "1:23 / 3:45"
        char timeLabel[64];
        snprintf(timeLabel, sizeof(timeLabel), "%d:%02d / %d:%02d", curM, curS, totM, totS);

        ImGui::SetCursorPos(ImVec2(70.0f, 75.0f)); // PUT SEEK BAR at Y: 75
        ImGui::PushItemWidth(540.0f);

        // DRAW the SLIDER, but I REMOVED the IMMEDIATE SEEK command.
        ImGui::SliderFloat("##SeekBar", &uiSliderPos, 0.0f, trackDuration, timeLabel);

        if (ImGui::IsItemActive()) {
            isDraggingSeek = true;
        }

        // SAFELY seek ONLY when the user LET'S GO of THE MOUSE.
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            player.Stop(); // HALT the AUDIO THREAD.
            player.SeekToPosition(uiSliderPos); // MOVE the MP3 playhead.
            if (!isUserPaused) player.Play(); // RESUME playing seamlessly.
            isDraggingSeek = false;
        } else if (ImGui::IsItemDeactivated()) {
            isDraggingSeek = false; // MOUSE released without changing anything.
        }
        ImGui::PopItemWidth();

        // ================ Keyboard Seeking (+, - after 5 seconds) ====================
        if (!io.WantCaptureKeyboard && trackDuration > 0.0f) {
            // SKIP FOWARD 5 Seconds.
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
                float seekPos = player.GetCurrentPosition() + 5.0f;
                if (seekPos > trackDuration) seekPos = trackDuration - 0.5f; // FAILSAFE: Don't seek past the end

                player.Stop();
                player.SeekToPosition(seekPos);
                if (!isUserPaused) player.Play();
            }
            // SKIP BACKWARD 5 Seconds.
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
                float seekPos = player.GetCurrentPosition() - 5.0f;
                if (seekPos < 0.0f) seekPos = 0.0f; // FAILSAFE: Don't seek into negative time
                
                player.Stop();
                player.SeekToPosition(seekPos);
                if (!isUserPaused) player.Play();
            }
        }

        // ============= VOLUME OVERDRIVE SLIDER ================
        // Move the "cursor" down to Y: 75 so it sits nicely under the buttons.
        // We keep X: 70 so its left edge aligns perfectly with the 'Prev' button.
        ImGui::SetCursorPos(ImVec2(70.0f, 110.0f));

        // PushItemWidth locks the slider's length to exactly 460 pixels 
        // so it perfectly matches the width of the 4 buttons above it!
        ImGui::PushItemWidth(220.0f);
        bool isBoosted = currentVolume > 1.0f;  

        // UX UPGRADE: Turn the slider RED if BoostMax is Active.
        if (isBoosted) {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.5f, 0.1f, 0.1f, 1.0f));        // Dark Red Track
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.7f, 0.2f, 0.2f, 1.0f)); // Lighter Red Hover
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));  // Bright Red Drag
            ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));     // Bright Red Handle
        }

        // DYNAMICALLY change the text based on VOLUME LEVEL.
        const char* volFormat = isBoosted ? "BoostMax: %.2fx" : "Volume: %.2fx";

        // SliderFloat min is 0.0f (mute), max is 2.0f (200% overdrive).
        if (ImGui::SliderFloat("##Volume", &currentVolume, 0.0f, 2.5f, volFormat)){
            // When the USER drags the slider, this BLOCKS TRIGGERS!
            player.SetVolume(currentVolume);
        }

        if (isBoosted) {
            ImGui::PopStyleColor(4);    // REMOVE the RED styling so it DOESN'T affect other buttons.
        }
        ImGui::PopItemWidth();

        // ==========================================
        // BoostMax Notification State Machine.
        // ==========================================
        static bool hasWarnedBoost = false;

        if (currentVolume > 1.0f) {
            if (!hasWarnedBoost) {
                ShowBoostWarningNotification();
                hasWarnedBoost = true;  // LOCK it so it DOESN'T spam the OS.
            }
        } else {
            // RESET the LOCK if THEY turn the volume BACK DOWN to NORMAL LEVELS.
            hasWarnedBoost = false;
        }

        ImGui::SameLine(0.0f, 10.0f);

        // The 100px Theme Switcher Button (430 + 10 + 100 = 540px grid perfectly maintained!).
        const char* themeLabels[] = { "Vis: Classic", "Vis: Real Waveform", "Vis: Neon Polyline", "Vis: Drake (Special)", "Vis: Kombat (Brutal)", "Vis: Arc Reactor"};
        if (ImGui::Button(themeLabels[visualMode], ImVec2(150, 24))) {
            visualMode = (visualMode + 1) % 6; // TOGGLES BETWEEN 0 and 5
        }

        ImGui::SameLine(0.0f, 10.0f);

        // TrumFaster Toggle Button (Dynamic Colors).
        if (isTrumFasterEnabled) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.5f, 0.2f, 1.0f)); // GREEN WHEN ACTIVE.
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.6f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.4f, 0.1f, 1.0f));

            if (ImGui::Button("TrumFaster: ON", ImVec2(140, 24))) isTrumFasterEnabled = false; // TURN OFF.

            ImGui::PopStyleColor(3);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f)); // RED when TURN OFF.
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.3f, 0.3f, 1.0f)); 
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.1f, 0.1f, 1.0f));

            if (ImGui::Button("TrumFaster: OFF", ImVec2(140, 24))) isTrumFasterEnabled = true; // TURN ON.

            ImGui::PopStyleColor(3);
        }

        ImGui::SameLine(0.0f, 10.0f);

        // GPU BLOOM & V-SYNC UI CONTROLS
        if (ImGui::Button(isBloomEnabled ? "GPU Bloom: ON" : "GPU Bloom: OFF", ImVec2(120, 24))) {
            isBloomEnabled = !isBloomEnabled;
        }
        ImGui::SameLine(0.0f, 10.0f);
        if (ImGui::Button(isVSyncEnabled ? "V-Sync: ON" : "V-Sync: OFF", ImVec2(100, 24))) {
            isVSyncEnabled = !isVSyncEnabled;
            glfwSwapInterval(isVSyncEnabled ? 1 : 0); // Directly control GPU Monitor Sync
        }

        // ==================== LIVE FOLDER LOADER (UPGRADED) ====================
        ImGui::SetCursorPos(ImVec2(70.0f, 145.0f)); // POSITION IT BELOW THE VOLUME SLIDER

        // [C++ Coding-while-Learning] A static char array holds the text the user types into ImGui.
        // UI FIX Step 4: START with an EMPTY buffer.
        static char folderPathBuffer[256] = "";

        // ---------------------------------------------------------
        // BACKGROUND THREAD VARIABLES (Survives frame resets)
        // ---------------------------------------------------------
        static std::atomic<bool> isBrowsing = false;
        static std::string asyncSelectedPath = "";

        // If the background thread finished finding a folder, copy it to the UI safely.
        if (!isBrowsing && !asyncSelectedPath.empty()) {
            strncpy(folderPathBuffer, asyncSelectedPath.c_str(), sizeof(folderPathBuffer) - 1);
            folderPathBuffer[sizeof(folderPathBuffer) - 1] = '\0'; // SAFETY NULL-TERMINATION.
            asyncSelectedPath.clear(); // CLEAR it so it only UPDATES once.
        }

        // ---------------------------------------------------------
        // NEW FEATURE: Native Mac "Browse" Button
        // ---------------------------------------------------------
        if (isBrowsing) {
            // [UI TRICK] DISABLED the button while the window is open so they can't spam 50 windows.
            ImGui::BeginDisabled();
            ImGui::Button("Browsing...", ImVec2(100, 24));
            ImGui::EndDisabled();
        } else {
            if (ImGui::Button("Browse...", ImVec2(100, 24))) {
                isBrowsing = true; // LOCK the BUTTON.
                asyncSelectedPath = ""; // CLEAR old paths

                // [NOTE] SPAWN a background worker to open the Mac Finder window.
                std::thread([]() {

                    // ------ CROSS-PLATFORM DIALOG GENERATOR ------
                    std::string command;
                    
                    #ifdef _WIN32
                        // Windows uses PowerShell to OPEN the File Explorer dialog.
                        command = "powershell -NoProfile -Command \"(New-Object -ComObject Shell.Application).BrowseForFolder(0, 'Select Music Folder', 0).Self.Path\"";
                        #define POPEN _popen
                        #define PCLOSE _pclose
                    #elif __APPLE__
                        // macOS uses AppleScript.
                        command = "osascript -e 'POSIX path of (choose folder with prompt \"Select Music Folder\")'";
                        #define POPEN popen
                        #define PCLOSE pclose
                    #elif __linux__
                        // Linux uses Zenity
                        command = "zenity --file-selection --directory --title=\"Select Music Folder\"";
                        #define POPEN popen
                        #define PCLOSE pclose
                    #endif

                    FILE* pipe = POPEN(command.c_str(), "r");
                    if (pipe) {
                        char pathBuffer[512];
                        if (fgets(pathBuffer, sizeof(pathBuffer), pipe) != nullptr) {
                            // STRIP BOTH standard UNIX newlines and Windows carriage returns (\r\n)
                            pathBuffer[strcspn(pathBuffer, "\r\n")] = 0;
                            asyncSelectedPath = pathBuffer; // SAVE the result.
                        }
                        pclose(pipe);
                    }
                    isBrowsing = false; // UNLOCK the button when the Mac window closes
                }).detach(); // .detach() tells the main engine: "Don't wait for me, keep running!"
            }
        }

        ImGui::SameLine(0.0f, 10.0f);

        ImGui::PushItemWidth(300.0f);
        // THIS creates a text box where I can type any Mac folder path!
        // UX FIX Step 5: Beautiful placeholder text guiding the user!
        ImGui::InputTextWithHint("##FolderPath", "Select a music directory...", folderPathBuffer, sizeof(folderPathBuffer));
        ImGui::PopItemWidth();

        ImGui::SameLine(0.0f, 10.0f);
        
        // When the user clicks the button, the engine re-scans the COMPUTER.
        if (ImGui::Button("Load Folder", ImVec2(120, 0))) {
            std::string newPath = folderPathBuffer;

            // UX FIX Step 6: Smart Error Handling!
            if (newPath.empty()) {
                std::cout << "[ENGINE] Please select or type a folder path first!\n";
            }
            else if (fs::exists(newPath) && fs::is_directory(newPath)) {
                std::cout << "[ENGINE] Scanning new folder: " << newPath << "\n";
                std::vector<std::string> tempPlaylist;

                for (const auto &entry : fs::directory_iterator(newPath)) {
                    if (entry.path().extension() == ".mp3" || entry.path().extension() == ".MP3" || entry.path().extension() == ".wav" || entry.path().extension() == ".WAV" || entry.path().extension() == ".flac" || entry.path().extension() == ".FLAC"){
                        tempPlaylist.push_back(entry.path().string());
                    }
                }

                std::sort(tempPlaylist.begin(), tempPlaylist.end());

                if (!tempPlaylist.empty()) {
                    playlist = tempPlaylist;

                    player.Stop();
                    currentTrackIndex = 0;
                    selectedTrackPath = playlist[currentTrackIndex];
                    cleanTrackName = fs::path(selectedTrackPath).filename().stem().string();

                    player.Load(selectedTrackPath);
                    player.SetVolume(currentVolume);
                    trackDuration = player.GetDuration();
                    player.Play(); 

                    isUserPaused = false;

                    // TRIGGER OS Notification.
                    ShowOSNotification(cleanTrackName);

                    for (size_t i = 0; i < frozenFrequencies.size(); i++) {
                        frozenFrequencies[i] = 0.0f;
                    }
                    avgEnergy = 0.1f; // NEW: I don't carry loudness memory from the previous track into a new one.
                } else {
                    std::cout << "[ENGINE WARNING!] No Audio files found in the FOLDER!\n";
                }
            } else {
                // Now it tells you EXACTLY what path failed!
                std::cout << "[ENGINE WARNING!] The folder path is INVALID: '" << newPath << "'\n";
            }
        }

        ImGui::PopStyleVar();
        ImGui::End();

        ImGui::PopStyleVar();

        // ==========================================
        // Mac CONTROL CENTER Synchronization.
        // ==========================================
        #ifdef __APPLE__
            // ONLY update the OS every 30 FRAMES (twice a second) to save CPU.
            static int syncCounter = 0;
            if (syncCounter++ > 30) {
                // UX FIX: Pass 'currentVolume' to the Mac Media Center!
                UpdateMediaCenter(cleanTrackName.empty() ? "Clayton Engine" : cleanTrackName, trackDuration, player.GetCurrentPosition(), !isUserPaused, currentVolume);
                syncCounter = 0;
            }

            // Listen for OS Control Center Clicks!
            if (g_mediaPlayPauseToggle) {
                g_mediaPlayPauseToggle = false;
                if (player.IsPlaying()) {
                    player.Stop();
                    player.SetVolume(currentVolume);
                    isUserPaused = true;
                } else if (!playlist.empty()) {
                    player.Play();
                    player.SetVolume(currentVolume);
                    isUserPaused = false;
                }
            }
            if (g_mediaNextTrack) {
                g_mediaNextTrack = false;
                if (!playlist.empty()) {
                    player.Stop();
                    currentTrackIndex = (currentTrackIndex + 1) % playlist.size();
                    selectedTrackPath = playlist[currentTrackIndex];
                    cleanTrackName = fs::path(selectedTrackPath).filename().stem().string();
                    player.Load(selectedTrackPath);
                    player.SetVolume(currentVolume);
                    trackDuration = player.GetDuration();
                    player.Play();
                    isUserPaused = false;
                    ShowOSNotification(cleanTrackName);
                }
            }
            if (g_mediaPrevTrack) {
                g_mediaPrevTrack = false;
                if (!playlist.empty()) {
                    player.Stop();
                    currentTrackIndex = (currentTrackIndex - 1 + playlist.size()) % playlist.size();
                    selectedTrackPath = playlist[currentTrackIndex];
                    cleanTrackName = fs::path(selectedTrackPath).filename().stem().string();
                    player.Load(selectedTrackPath);
                    player.SetVolume(currentVolume);
                    trackDuration = player.GetDuration();
                    player.Play();
                    isUserPaused = false;
                    ShowOSNotification(cleanTrackName);
                }
            }
        #endif // __APPLE__

        // ==========================================
        // IMGUI Phase 4: RENDER TO SCREEN
        // ==========================================
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // NEW: STOP the GPU Timer BEFORE V-Sync forces the thread to sleep!
        glBeginQuery(GL_TIME_ELAPSED, gpuTimeQuery);
        GLuint64 gpuTimeNs = 0;
        glGetQueryObjectui64v(gpuTimeQuery, GL_QUERY_RESULT, &gpuTimeNs);
        float gpuTimeMs = (float)gpuTimeNs / 1000000.0f;

        // Swap the video buffers and push the pixel data to your monitor
        window.Update();

        // Let TrumFaster calculate the exact sleep math instead of hardcoding it.
        trumFaster.EndFrame();
    }

    // ==========================================
    // IMGUI Phase 5: CLEAN SHUTDOWN
    // ==========================================
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    player.Stop();
}

// =====================================
// SHUTDOWN ENGINE
// =====================================
void Engine::Shutdown()
{
    std::cout << "Engine Shutdown.\n";
}