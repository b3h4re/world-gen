#pragma once

#include "window/window_surface.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <string>


namespace lve {

    class LveWindow : public WindowSurface {
        public:
            LveWindow(int w, int h, std::string name);
            ~LveWindow() override;

            LveWindow(const LveWindow&) = delete;
            LveWindow &operator=(const LveWindow&) = delete;

            bool shouldClose() const override;
            void requestClose() override;
            VkExtent2D getExtent() const override;
            bool wasWindowResized() const override;
            void resetWindowResizedFlag() override;
            void waitEvents() override;
            GLFWwindow* getGLFWwindow() const { return window; }

            void createWindowSurface(VkInstance instance, VkSurfaceKHR *surface) override;
            std::vector<const char*> getRequiredInstanceExtensions() const override;

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
