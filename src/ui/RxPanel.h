#pragma once

#include "can/CanBus.h"

#include <deque>
#include <string>

/// Panel that displays decoded incoming CAN messages in a scrollable table.
class RxPanel
{
  public:
    void pushFrame(const DecodedFrame &frame);
    void render();

  private:
    static constexpr size_t kMaxRows = 2000;

    struct RxRow
    {
        double timeMs;
        uint32_t id;
        std::string msgName;
        std::string sigName;
        double value;
    };

    std::deque<RxRow> rows_;
    bool autoScroll_ = true;
    char filter_[128] = {};
};
