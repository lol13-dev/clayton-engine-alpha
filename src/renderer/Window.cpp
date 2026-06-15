#include "Window.h"
#include <iostream>


Window::Window(int width, int height, const std::string& title)
    : m_Width(width), m_Height(height), m_Title(title), m_Window(nullptr) {}

Window::~Window()
{
    Shutdown();
}

bool Window::Initialize()
{
    if (!glfwInit())
    {
        std::cout << "[RENDERER] Failed to initialize GLFW!\n";
        return false;
    }

    // Force Mac to use modern OpenGL 3.3 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Required for macOS

    // Create the physical window
    m_Window = glfwCreateWindow(m_Width, m_Height, m_Title.c_str(), nullptr, nullptr);
    if (!m_Window)
    {
        std::cout << "[RENDERER] Failed to create GLFW window!\n";
        glfwTerminate();
        return false;
    }

    // Lock the GPU context to this window
    glfwMakeContextCurrent(m_Window);
    
    std::cout << "[RENDERER] Hardware-accelerated window created successfully.\n";
    return true;
}

bool Window::IsOpen()
{
    return !glfwWindowShouldClose(m_Window);
}

void Window::Clear(float r, float g, float b, float a)
{
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void Window::Update()
{
    glfwSwapBuffers(m_Window);
    glfwPollEvents();
}

void Window::Shutdown()
{
    if (m_Window)
    {
        glfwDestroyWindow(m_Window);
        m_Window = nullptr;
    }
    glfwTerminate();
}
