#pragma once
#include <GLFW/glfw3.h>
#include <cassert>
#include <memory>
#include <Volk/volk.h>
#include "VKRTApp.h"


class VKRTWindow
{
public:
	// 窗口宽度与高度
	const static int DEFAULT_WIDTH = 1280;
	const static int DEFAULT_HEIGHT = 768;

	VKRTWindow(const int& width = DEFAULT_WIDTH, const int& height = DEFAULT_HEIGHT);
	virtual ~VKRTWindow();

	void run();
private:
	// GLFW的窗口
	GLFWwindow* window;
	// 这里面包含了所有的RT绘制
	std::unique_ptr<VKRTApp> vkRTApp;

	// 窗口的一些事件的回调
	static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void mouseCallback(GLFWwindow* window, double x, double y);
	static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

	bool mIsImGUIWindowHovered = false;
};