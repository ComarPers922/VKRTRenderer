#include <iostream>
#include "VKRTWindow.h"

int main(int argc, char* argv[])
{
    std::cout << glslang::GetEsslVersionString() << std::endl;
    std::cout << glslang::GetGlslVersionString() << std::endl;
    // ��ʼ��GLSL JIT������
    glslang::InitializeProcess();

    // һ������RTXOn�Ĵ���
	VKRTWindow window;

    window.run();
    // �ͷ�GLSL JIT�༭��
    glslang::FinalizeProcess();
	return 0;
}