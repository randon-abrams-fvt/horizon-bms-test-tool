#pragma once

#include "can/CanBus.h"

#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

class DbcModel;

/// Panel for plotting decoded signal values over time using ImPlot.
/// The left pane lists all available signals (filterable checkboxes);
/// the right pane shows a scrolling time-series graph of selected signals.
class PlotPanel
{
  public:
    void setDbc(const DbcModel *dbc);
    void pushFrame(const DecodedFrame &frame);
    void render();

  private:
    static constexpr size_t kMaxPoints = 10'000;

    struct SignalTrace
    {
        std::string label;        ///< "MessageName.SignalName"
        std::deque<double> times; ///< seconds from app start
        std::deque<double> values;
    };

    const DbcModel *dbc_ = nullptr;
    std::vector<std::string> availableSignals_;
    std::unordered_map<std::string, SignalTrace> traces_;
    char filter_[128] = {};
};
