#include "ui/BusMonitorPanel.h"

#include "imgui.h"

#include <chrono>
#include <cstdio>

static double bmpNowMs()
{
    using namespace std::chrono;
    static const auto kStart = steady_clock::now();
    return duration<double, std::milli>(steady_clock::now() - kStart).count();
}

void BusMonitorPanel::pushFrame(const DecodedFrame &frame)
{
    const double now =
        frame.raw.timestampMs > 0.0 ? frame.raw.timestampMs : bmpNowMs();

    if (startTimeMs_ < 0.0)
        startTimeMs_ = now;
    ++totalFrames_;

    // Update per-ID stats
    auto &st = stats_[frame.raw.id];
    st.id = frame.raw.id;
    st.isExtended = frame.raw.isExtended;
    st.lastTimeMs = now;
    if (st.name.empty() && !frame.messageName.empty())
        st.name = frame.messageName;
    ++st.count;

    // Rolling rate update every kRateUpdateIntervalMs
    if (st.snapshotTimeMs == 0.0)
    {
        st.snapshotTimeMs = now;
        st.countSnapshot = st.count;
    }
    else if (now - st.snapshotTimeMs >= kRateUpdateIntervalMs)
    {
        const double dt = (now - st.snapshotTimeMs) / 1000.0;
        if (dt > 0.0)
            st.rateHz = static_cast<double>(st.count - st.countSnapshot) / dt;
        st.snapshotTimeMs = now;
        st.countSnapshot = st.count;
    }

    // Append to log
    LogEntry entry;
    entry.timeMs = now;
    entry.id = frame.raw.id;
    entry.isExtended = frame.raw.isExtended;
    entry.name = frame.messageName.empty() ? "unknown" : frame.messageName;
    entry.dlc = frame.raw.dlc;
    const uint8_t copyLen = frame.raw.dlc < 8 ? frame.raw.dlc : 8;
    for (uint8_t i = 0; i < copyLen; ++i)
        entry.data[i] = frame.raw.data[i];

    log_.push_back(std::move(entry));
    while (log_.size() > kMaxLog)
        log_.pop_front();
}

void BusMonitorPanel::render()
{
    const double now = bmpNowMs();
    const double elapsedSec =
        (startTimeMs_ >= 0.0) ? (now - startTimeMs_) / 1000.0 : 0.0;
    const double globalRate =
        (elapsedSec > 0.5) ? totalFrames_ / elapsedSec : 0.0;

    // ── Top stats bar ────────────────────────────────────────────────────
    ImGui::Text(
        "Total frames: %u   |   Rate: %.1f msg/s   |   Unique IDs: %zu",
        totalFrames_,
        globalRate,
        stats_.size());
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 120.0f);
    ImGui::Checkbox("Auto-scroll", &autoScroll_);
    ImGui::Separator();

    const float statsWidth = 370.0f;

    // ── Left: per-ID statistics table ───────────────────────────────────
    ImGui::BeginChild("##Stats", ImVec2(statsWidth, 0.0f), true);
    ImGui::TextDisabled("Per-ID Statistics");
    ImGui::Separator();

    constexpr ImGuiTableFlags kStatsFlags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("##StatsTbl", 4, kStatsFlags))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 75.0f);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(
            "Count", ImGuiTableColumnFlags_WidthFixed, 55.0f);
        ImGui::TableSetupColumn("Hz", ImGuiTableColumnFlags_WidthFixed, 45.0f);
        ImGui::TableHeadersRow();

        for (auto &[id, st] : stats_)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text(st.isExtended ? "%08X" : "%03X", st.id);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(st.name.empty() ? "?" : st.name.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%u", st.count);
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.1f", st.rateHz);
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // ── Right: scrolling frame log ───────────────────────────────────────
    ImGui::BeginChild("##Log", ImVec2(0.0f, 0.0f), true);
    ImGui::TextDisabled("Frame Log");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputText("Filter##log", logFilter_, sizeof(logFilter_));
    ImGui::SameLine();
    if (ImGui::Button("Clear"))
        log_.clear();
    ImGui::Separator();

    constexpr ImGuiTableFlags kLogFlags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;

    if (ImGui::BeginTable("##LogTbl", 5, kLogFlags))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn(
            "Time (ms)", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn(
            "Name", ImGuiTableColumnFlags_WidthFixed, 170.0f);
        ImGui::TableSetupColumn("DLC", ImGuiTableColumnFlags_WidthFixed, 30.0f);
        ImGui::TableSetupColumn("Data", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        const bool hasFilter = logFilter_[0] != '\0';

        for (const auto &entry : log_)
        {
            if (hasFilter && entry.name.find(logFilter_) == std::string::npos)
                continue;

            char dataBuf[32] = {};
            char *p = dataBuf;
            for (int i = 0; i < entry.dlc && i < 8; ++i)
                p += std::snprintf(
                    p, sizeof(dataBuf) - (p - dataBuf), "%02X ", entry.data[i]);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%.1f", entry.timeMs);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text(entry.isExtended ? "%08X" : "%03X", entry.id);
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(entry.name.c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%u", entry.dlc);
            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(dataBuf);
        }

        if (autoScroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndTable();
    }
    ImGui::EndChild();
}
