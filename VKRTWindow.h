#pragma once
#include <GLFW/glfw3.h>
#include <cassert>
#include <memory>
#include <Volk/volk.h>
#include "VKRTApp.h"


class VKRTWindow
{
public:
	// ���ڿ����߶�
	const static int DEFAULT_WIDTH = 1280;
	const static int DEFAULT_HEIGHT = 768;

	VKRTWindow(const int& width = DEFAULT_WIDTH, const int& height = DEFAULT_HEIGHT);
	virtual ~VKRTWindow();

	void run();
private:
	// GLFW�Ĵ���
	GLFWwindow* window;
	// ��������������е�RT����
	std::unique_ptr<VKRTApp> vkRTApp;

	// ���ڵ�һЩ�¼��Ļص�
	static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void mouseCallback(GLFWwindow* window, double x, double y);
	static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

	bool mIsImGUIWindowHovered = false;
};