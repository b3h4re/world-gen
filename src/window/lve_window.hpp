#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <string>


namespace lve {

    class LveWindow {
        public:
            LveWindow(int w, int h, std::string name);
            ~LveWindow();

            LveWindow(const LveWindow&) = delete;
            LveWindow &operator=(const LveWindow&) = delete;

            bool shouldClose();
            VkExtent2D getExtent();
            bool wasWindowResized();
            void resetWindowResizedFlag();
            GLFWwindow* getGLFWwindow() const { return window; }

            void createWindowSurface(VkInstance instance, VkSurfaceKHR *surface);

        private:
            static void frameBufferResizedCallback(GLFWwindow *window, int width, int height);
            void initWindow();

            int width;
            int height;
            bool frameBufferResized = false;

            std::string windowName;
            GLFWwindow *window;
    };
}