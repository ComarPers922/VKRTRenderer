#include <iostream>
#include "VKRTWindow.h"

int main(int argc, char* argv[])
{
    std::cout << glslang::GetEsslVersionString() << std::endl;
    std::cout << glslang::GetGlslVersionString() << std::endl;
    // 初始化GLSL JIT编译器
    glslang::InitializeProcess();

    // 一个带有RTXOn的窗口
	VKRTWindow window;

    window.run();
    // 释放GLSL JIT编辑器
    glslang::FinalizeProcess();
	return 0;
}