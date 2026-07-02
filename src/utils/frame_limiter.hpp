#pragma once

#include <chrono>

namespace lve {

class FrameLimiter {
public:
    explicit FrameLimiter(double targetFps)
        : targetFrameTime{std::chrono::duration<double>{1.0 / targetFps}},
          nextFrameTime{std::chrono::steady_clock::now()} {}

    void wait();

private:
    std::chrono::duration<double> targetFrameTime;
    std::chrono::steady_clock::time_point nextFrameTime;
};


}
