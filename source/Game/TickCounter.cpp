#include "TickCounter.h"

#include "DoomDefines.h"
#include <algorithm>
#include <chrono>

BEGIN_NAMESPACE(TickCounter)

// Only allow this many ticks to be simulated at once. If we exceed this amount then the
// game will physically begin to slow down with respect to real world time.
static constexpr int64_t MAX_TICKS_TO_SIMULATE = 10;

// This is the number of nanoseconds per tick
static constexpr int64_t NS_PER_TICK = 1000000000 / TICKSPERSEC;

// If are short of a full tick but this far along then allow a simulation frame to proceed.
// This anticipates that draw will be very expensive so it's best to simulate the tick ahead of time.
static constexpr int64_t ADVANCE_SIMULATE_TICK_THRESHOLD = (NS_PER_TICK * 85) / 100;

typedef std::chrono::high_resolution_clock::time_point  TimePoint;
typedef std::chrono::nanoseconds                        NSDuration;

static TimePoint    gLastTime;
static bool         gDidFirstUpdate;
static int64_t      gUnsimulatedNanoSeconds;

static void clearTimeValues() noexcept {
    gLastTime = {};
    gDidFirstUpdate = false;
    gUnsimulatedNanoSeconds = 0;
}

void init() noexcept {
    clearTimeValues();
}

void shutdown() noexcept {
    clearTimeValues();
}

uint32_t update() noexcept {
    const TimePoint now = std::chrono::high_resolution_clock::now();

    // Note: the first update always requests to simulate 1 tick.
    // Otherwise we see how much time has elapsed between calls.
    if (gDidFirstUpdate) {
        const NSDuration timeElapsed = now - gLastTime;
        gLastTime = now;
        gUnsimulatedNanoSeconds += timeElapsed.count();

        if (gUnsimulatedNanoSeconds > 0) {
            const int64_t ticksToSimulate = gUnsimulatedNanoSeconds / NS_PER_TICK;

            if (ticksToSimulate > 0) {
                gUnsimulatedNanoSeconds -= ticksToSimulate * NS_PER_TICK;
                return (uint32_t) std::min(ticksToSimulate, MAX_TICKS_TO_SIMULATE);
            } else {
                if (gUnsimulatedNanoSeconds >= ADVANCE_SIMULATE_TICK_THRESHOLD) {
                    gUnsimulatedNanoSeconds -= NS_PER_TICK;
                    return 1;
                }
            }
        }

        return 0;
    }
    else {
        gLastTime = now;
        gDidFirstUpdate = true;
        return 1;
    }
}

END_NAMESPACE(TickCounter)
