#pragma once

#include <chrono>

namespace lve {

class FrameLimiter {
public:
    explicit FrameLimiter(int targetFps)
        : targetFrameTime{std::chrono::duration<double>{1.0 / static_cast<double>(targetFps)}},
          nextFrameTime{std::chrono::steady_clock::now()}, targetFps_{targetFps} {}

    void wait();
    void settargetFps(int targetFps);

private:
    int targetFps_{0};
    std::chrono::duration<double> targetFrameTime;
    std::chrono::steady_clock::time_point nextFrameTime;
};


}
