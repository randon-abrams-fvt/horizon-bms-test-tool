#pragma once

#include "can/CanBus.h"

#include <deque>
#include <string>
#include <unordered_map>

/// Tab 1 — shows all CAN traffic and per-message bus statistics.
/// Does not create its own top-level window; renders inline into the current
/// tab item.
class BusMonitorPanel
{
  public:
    void pushFrame(const DecodedFrame &frame);
    void render();

  private:
    struct IdStats
    {
        uint32_t id = 0;
        std::string name;
        bool isExtended = false;
        uint32_t count = 0;
        double rateHz = 0.0;
        double lastTimeMs = 0.0;

        // For rolling rate estimation
        uint32_t countSnapshot = 0;
        double snapshotTimeMs = 0.0;
    };

    struct LogEntry
    {
        double timeMs;
        uint32_t id;
        bool isExtended;
        std::string name;
        uint8_t dlc;
        uint8_t data[8] = {};
    };

    static constexpr size_t kMaxLog = 2000;
    static constexpr double kRateUpdateIntervalMs = 500.0;

    std::unordered_map<uint32_t, IdStats> stats_;
    std::deque<LogEntry> log_;
    uint32_t totalFrames_ = 0;
    double startTimeMs_ = -1.0;
    bool autoScroll_ = true;
    char logFilter_[128] = {};
};
