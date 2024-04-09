#include "VKRTWindow.h"

#include <thread>

#define TARGET_FPS 144

VKRTWindow::VKRTWindow(const int& width, const int& height)
{
    glfwInit(); // 初始化VKRTWindow

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // 窗口不能被缩放，因为窗口缩放涉及Vulkan需要写更多的代码，否则程序会直接崩溃
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); 

    // 创建窗口
	window = glfwCreateWindow(width, height, "VulkanRT", nullptr, nullptr);
	glfwSetWindowUserPointer(window, this);
    // 以下是涉及到键盘鼠标操作的回调
	glfwSetKeyCallback(window, keyCallback);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    // 将新建的窗口用于VKRTApp（主要管理渲染的类）
    vkRTApp = std::make_unique<VKRTApp>(window, width, height);
}

void VKRTWindow::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    auto* rtWindow = reinterpret_cast<VKRTWindow*>(glfwGetWindowUserPointer(window));

    if (key == GLFW_KEY_ESCAPE)
    {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    const float UP_DOWN_SPEED = 500;

    if (key == GLFW_KEY_Q)
    {
        rtWindow->vkRTApp->MoveCameraUpDown(UP_DOWN_SPEED);
    }
    if (key == GLFW_KEY_E)
    {
        rtWindow->vkRTApp->MoveCameraUpDown(-UP_DOWN_SPEED);
    }

    const float SIDE_FORWARD_SPEED = 200;

    vec2 velocity(0, 0);

    if (key == GLFW_KEY_W || key == GLFW_KEY_UP)
    {
        velocity.y += SIDE_FORWARD_SPEED;
    }
    if (key == GLFW_KEY_S || key == GLFW_KEY_DOWN)
    {
        velocity.y -= SIDE_FORWARD_SPEED;
    }
    if (key == GLFW_KEY_A || key == GLFW_KEY_LEFT)
    {
        velocity.x -= SIDE_FORWARD_SPEED;
    }
    if (key == GLFW_KEY_D || key == GLFW_KEY_RIGHT)
    {
        velocity.x += SIDE_FORWARD_SPEED;
    }

    rtWindow->vkRTApp->MoveCamera(velocity.x, velocity.y);
}

void VKRTWindow::mouseCallback(GLFWwindow* window, double x, double y)
{
    auto* rtWindow = reinterpret_cast<VKRTWindow*>(glfwGetWindowUserPointer(window));
    rtWindow->vkRTApp->RotateCamera(static_cast<float>(x), static_cast<float>(y));
}

void VKRTWindow::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    auto* rtWindow = reinterpret_cast<VKRTWindow*>(glfwGetWindowUserPointer(window));
    if (rtWindow->mIsImGUIWindowHovered)
    {
        return;
    }
    if (button == 0 && action == GLFW_PRESS)
    {
        rtWindow->vkRTApp->SetMousePressed(true);
    }
    else if (button == 0 && action == GLFW_RELEASE)
    {
        rtWindow->vkRTApp->SetMousePressed(false);
    }
}

VKRTWindow::~VKRTWindow()
{
	glfwDestroyWindow(window);
}

void VKRTWindow::run()
{
    static double curDeltaTime = 0;
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
#pragma region 与ImGUI有关，暂时可以不管
        auto isIconified = glfwGetWindowAttrib(window, GLFW_ICONIFIED);

        if (isIconified == GLFW_TRUE)
        {
            continue;
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        mIsImGUIWindowHovered = ImGui::IsWindowHovered();
#pragma endregion

        auto timeBeforeDraw = glfwGetTime();

        vkRTApp->ProcessFrame(curDeltaTime);

        auto timeAfterDraw = glfwGetTime();
        curDeltaTime = timeAfterDraw - timeBeforeDraw;
        //if (curDeltaTime < TARGET_FPS)
        //{
        //    auto timeDiff = TARGET_FPS - curDeltaTime;
        //    std::this_thread::sleep_for(std::chrono::nanoseconds((long long)(timeDiff * 1e9)));
        //}
    }
}
