#include "ui/TxPanel.h"

#include "can/CanBus.h"
#include "dbc/DbcModel.h"
#include "imgui.h"

#include <algorithm>
#include <chrono>
#include <cstdio>

static double nowMs()
{
    using namespace std::chrono;
    return duration<double, std::milli>(steady_clock::now().time_since_epoch())
        .count();
}

void TxPanel::setDbc(const DbcModel *dbc)
{
    dbc_ = dbc;
    rebuildFromDbc();
}

void TxPanel::setCanBus(CanBus *bus)
{
    bus_ = bus;
}

void TxPanel::rebuildFromDbc()
{
    messages_.clear();
    selected_ = -1;
    if (!dbc_)
        return;

    for (const auto &m : dbc_->messages())
    {
        MsgState ms;
        ms.id = m.id;
        ms.name = m.name;
        ms.dlc = m.dlc;

        for (const auto &s : m.signals)
        {
            SigState ss;
            ss.name = s.name;
            ss.unit = s.unit;
            ss.min = static_cast<float>(s.min);
            ss.max = static_cast<float>((s.max != s.min) ? s.max : s.min + 1.0);
            ss.value = ss.min;
            ms.signals.push_back(std::move(ss));
        }
        messages_.push_back(std::move(ms));
    }
}

void TxPanel::transmit(size_t idx)
{
    if (!bus_ || !dbc_)
        return;

    auto &ms = messages_[idx];
    std::vector<std::pair<std::string, double>> vals;
    vals.reserve(ms.signals.size());
    for (const auto &ss : ms.signals)
        vals.emplace_back(ss.name, static_cast<double>(ss.value));

    CanFrame frame;
    if (dbc_->encode(ms.id, vals, frame))
        bus_->send(frame);
}

void TxPanel::render()
{
    ImGui::Begin("TX Messages");

    if (!dbc_ || !dbc_->isLoaded())
    {
        ImGui::TextDisabled("No DBC loaded. Use File > Open DBC.");
        ImGui::End();
        return;
    }

    // ── Cyclic transmission — check all messages every frame ────────────
    const double now = nowMs();
    for (size_t i = 0; i < messages_.size(); ++i)
    {
        auto &ms = messages_[i];
        if (ms.cyclicEnabled &&
            (now - ms.lastSentMs) >= static_cast<double>(ms.cycleMs))
        {
            transmit(i);
            ms.lastSentMs = now;
        }
    }

    // ── Message list ────────────────────────────────────────────────────
    const float listWidth = 230.0f;
    ImGui::BeginChild("##MsgList", ImVec2(listWidth, 0.0f), true);

    for (int i = 0; i < static_cast<int>(messages_.size()); ++i)
    {
        const auto &ms = messages_[i];
        char label[128];
        std::snprintf(
            label, sizeof(label), "0x%03X  %s", ms.id, ms.name.c_str());
        if (ImGui::Selectable(label, selected_ == i))
            selected_ = i;
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // ── Signal editor ────────────────────────────────────────────────────
    ImGui::BeginChild("##SigEditor", ImVec2(0.0f, 0.0f), true);

    if (selected_ >= 0 && selected_ < static_cast<int>(messages_.size()))
    {
        auto &ms = messages_[static_cast<size_t>(selected_)];

        ImGui::Text(
            "Message: %s   (0x%03X,  %d bytes)",
            ms.name.c_str(),
            ms.id,
            ms.dlc);
        ImGui::Separator();

        ImGui::PushItemWidth(220.0f);
        for (auto &ss : ms.signals)
        {
            // Label
            ImGui::Text("%-28s", ss.name.c_str());
            ImGui::SameLine();

            // Slider
            char widgetId[128];
            std::snprintf(widgetId, sizeof(widgetId), "##%s", ss.name.c_str());
            ImGui::SliderFloat(widgetId, &ss.value, ss.min, ss.max, "%.4g");

            // Unit
            if (!ss.unit.empty())
            {
                ImGui::SameLine();
                ImGui::TextDisabled("%s", ss.unit.c_str());
            }
        }
        ImGui::PopItemWidth();

        ImGui::Separator();

        // Cyclic controls
        ImGui::Checkbox("Cyclic", &ms.cyclicEnabled);
        if (ms.cyclicEnabled)
        {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(90.0f);
            ImGui::InputFloat("ms##cyc", &ms.cycleMs, 10.0f, 100.0f, "%.0f");
            ms.cycleMs = std::max(1.0f, ms.cycleMs);
        }

        if (ImGui::Button("Send Once"))
            transmit(static_cast<size_t>(selected_));

        if (!bus_)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("(CAN not connected)");
        }
    }
    else
    {
        ImGui::TextDisabled("Select a message from the list.");
    }

    ImGui::EndChild();
    ImGui::End();
}
