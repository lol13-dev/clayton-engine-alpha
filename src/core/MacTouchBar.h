#pragma once
#include <vector>

#ifdef __APPLE__
// FOWARD declare the GLFW window so I don't have to include all of OpenGL core.
struct GLFWwindow;

// THE two bridge functions that connect C++ to the Mac OLED Touch Bar.
void InitTouchBar(GLFWwindow* window);
void UpdateTouchBar(const std::vector<float>& frequencies);
#endif