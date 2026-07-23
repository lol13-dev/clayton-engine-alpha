#pragma once
#include <vector>

#ifdef __APPLE__
// THE BRIDGE functions that connect my C++ Engine to the macOS Menu Bars.
bool InitMenuBar();
void UpdateMenuBar(const std::vector<float>& frequencies);
#endif