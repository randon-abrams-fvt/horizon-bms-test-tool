#include "ui/PlotPanel.h"

#include "dbc/DbcModel.h"
#include "imgui.h"
#include "implot.h"

#include <chrono>
#include <vector>

static double nowSec()
{
    using namespace std::chrono;
    static const auto kStart = steady_clock::now();
    return duration<double>(steady_clock::now() - kStart).count();
}

void PlotPanel::setDbc(const DbcModel *dbc)
{
    dbc_ = dbc;
    availableSignals_.clear();
    if (!dbc_)
        return;

    for (const auto &m : dbc_->messages())
        for (const auto &s : m.signals)
            availableSignals_.push_back(m.name + "." + s.name);
}

void PlotPanel::pushFrame(const DecodedFrame &frame)
{
    if (frame.messageName.empty())
        return;

    const double t = nowSec();
    for (const auto &[sigName, val] : frame.signals)
    {
        const std::string key = frame.messageName + "." + sigName;
        auto it = traces_.find(key);
        if (it == traces_.end())
            continue; // only buffer signals the user selected

        auto &tr = it->second;
        tr.times.push_back(t);
        tr.values.push_back(val);
        while (tr.times.size() > kMaxPoints)
        {
            tr.times.pop_front();
            tr.values.pop_front();
        }
    }
}

void PlotPanel::render()
{
    const float listWidth = 220.0f;

    // ── Signal selector ──────────────────────────────────────────────────
    ImGui::BeginChild("##SigList", ImVec2(listWidth, 0.0f), true);
    ImGui::TextDisabled("Signals");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("##filt", filter_, sizeof(filter_));
    ImGui::Separator();

    const bool hasFilter = filter_[0] != '\0';

    for (const auto &label : availableSignals_)
    {
        if (hasFilter && label.find(filter_) == std::string::npos)
            continue;

        bool active = traces_.count(label) > 0;
        if (ImGui::Checkbox(label.c_str(), &active))
        {
            if (active)
                traces_.emplace(label, SignalTrace{label, {}, {}});
            else
                traces_.erase(label);
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // ── Plot area ────────────────────────────────────────────────────────
    ImGui::BeginChild("##PlotArea", ImVec2(0.0f, 0.0f), false);

    if (traces_.empty())
    {
        ImGui::TextDisabled("Check signals on the left to begin plotting.");
    }
    else if (ImPlot::BeginPlot("##Signals", ImVec2(-1.0f, -1.0f)))
    {
        const double tNow = nowSec();
        ImPlot::SetupAxes("Time (s)", "Value");
        ImPlot::SetupAxisLimits(ImAxis_X1, tNow - 30.0, tNow, ImGuiCond_Always);

        for (const auto &[key, tr] : traces_)
        {
            if (tr.times.empty())
                continue;

            // Copy deques to contiguous arrays (ImPlot requires contiguous
            // data)
            const std::vector<double> xs(tr.times.begin(), tr.times.end());
            const std::vector<double> ys(tr.values.begin(), tr.values.end());

            ImPlot::PlotLine(
                tr.label.c_str(),
                xs.data(),
                ys.data(),
                static_cast<int>(xs.size()));
        }

        ImPlot::EndPlot();
    }

    ImGui::EndChild();
}
