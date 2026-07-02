#include "frame_limiter.hpp"


#include <thread>


namespace lve {

void FrameLimiter::wait() {
    using Clock = std::chrono::steady_clock;

    nextFrameTime += std::chrono::duration_cast<Clock::duration>(targetFrameTime);

    const auto now = Clock::now();

    if (nextFrameTime > now) {
        std::this_thread::sleep_until(nextFrameTime);
    } else {
        nextFrameTime = now;
    }
}


}
