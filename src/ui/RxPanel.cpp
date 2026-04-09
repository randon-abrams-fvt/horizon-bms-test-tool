#include "ui/RxPanel.h"

#include "imgui.h"

void RxPanel::pushFrame(const DecodedFrame &frame)
{
    if (!frame.signals.empty())
    {
        for (const auto &[sigName, sigVal] : frame.signals)
        {
            RxRow row;
            row.timeMs = frame.raw.timestampMs;
            row.id = frame.raw.id;
            row.msgName = frame.messageName;
            row.sigName = sigName;
            row.value = sigVal;
            rows_.push_back(std::move(row));
        }
    }
    else
    {
        // Unknown message ID — show the raw frame with no signal info
        RxRow row;
        row.timeMs = frame.raw.timestampMs;
        row.id = frame.raw.id;
        row.msgName = "(unknown)";
        row.value = 0.0;
        rows_.push_back(std::move(row));
    }

    while (rows_.size() > kMaxRows)
        rows_.pop_front();
}

void RxPanel::render()
{
    ImGui::Begin("RX Messages");

    // Filter + controls bar
    ImGui::SetNextItemWidth(180.0f);
    ImGui::InputText("Filter", filter_, sizeof(filter_));
    ImGui::SameLine();
    if (ImGui::Button("Clear"))
        rows_.clear();
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &autoScroll_);

    ImGui::Separator();

    constexpr ImGuiTableFlags kTableFlags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;

    if (ImGui::BeginTable("##RxTable", 5, kTableFlags))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn(
            "Time (ms)", ImGuiTableColumnFlags_WidthFixed, 95.0f);
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 65.0f);
        ImGui::TableSetupColumn(
            "Message", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn("Signal", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(
            "Value", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableHeadersRow();

        const bool hasFilter = filter_[0] != '\0';

        for (const auto &row : rows_)
        {
            if (hasFilter)
            {
                if (row.msgName.find(filter_) == std::string::npos &&
                    row.sigName.find(filter_) == std::string::npos)
                    continue;
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%.1f", row.timeMs);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("0x%03X", row.id);
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(row.msgName.c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(row.sigName.c_str());
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%.4g", row.value);
        }

        if (autoScroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndTable();
    }

    ImGui::End();
}
