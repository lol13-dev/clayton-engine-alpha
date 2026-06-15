#pragma once

#include <GLFW/glfw3.h>
#include <string>

class Window
{
public:
    // Constructor.
    Window(int width, int height, const std::string &title);

    // Destructor.
    ~Window();

    // Initializes the window.
    bool Initialize();

    // Checks if the window is open.
    bool IsOpen();

    // Fills the screen with a solid background color
    void Clear(float r, float g, float b, float a);

    // Swaps the video buffers and checks for Mac events (like clicking the X to close)
    void Update();
    void Shutdown();

    // Add this right under bool IsOpen();
    GLFWwindow *GetGLFWWindowPointer() { 
        return m_Window; 
    }

private:
    GLFWwindow *m_Window;
    int m_Width;
    int m_Height;
    std::string m_Title;
};