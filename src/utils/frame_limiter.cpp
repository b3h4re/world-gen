#include "frame_limiter.hpp"


#include <thread>


namespace lve {

void FrameLimiter::wait() {
    if (targetFps_ <= 0) {
        return;
    }
    using Clock = std::chrono::steady_clock;

    nextFrameTime += std::chrono::duration_cast<Clock::duration>(targetFrameTime);

    const auto now = Clock::now();

    if (nextFrameTime > now) {
        std::this_thread::sleep_until(nextFrameTime);
    } else {
        nextFrameTime = now;
    }
}

void FrameLimiter::settargetFps(int targetFps) {
    targetFps_ = targetFps;
    targetFrameTime = std::chrono::duration<double>{1.0 / static_cast<double>(targetFps)};
    nextFrameTime = std::chrono::steady_clock::now();
}


}
