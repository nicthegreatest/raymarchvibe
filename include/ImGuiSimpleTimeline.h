// ImGuiSimpleTimeline.h
#pragma once
#include "imgui.h"
#include "imgui_internal.h" // We need this for GImGui
#include <vector>
#include <string>
#include <cstdio> // For snprintf

namespace ImGui {

// Struct to hold information about a timeline event
struct TimelineItem {
    std::string name;
    float* startTime;
    float* endTime;
    int* track; // Pointer to an integer for the track index
};

// Main function to draw the timeline
// Returns true if an item was selected this frame. The selected_item_index will be updated.
inline bool SimpleTimeline(const char* label, std::vector<TimelineItem>& items, float* current_time, int* selected_item_index, int num_tracks, float sequence_start, float sequence_end)
{
    bool item_selected_this_frame = false;
    ImGuiIO& io = ImGui::GetIO();
    // ImGuiStyle& style = ImGui::GetStyle(); // Not strictly needed for this version
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    const ImVec2 canvas_pos = window->DC.CursorPos;
    const ImVec2 canvas_size = GetContentRegionAvail();

    const float timeline_ruler_height = 20.0f;
    const float track_height = 20.0f;
    const float total_timeline_height = timeline_ruler_height + num_tracks * track_height;

    if (canvas_size.x <= 0) return false;
    float actual_canvas_width = canvas_size.x;

    const ImRect timeline_bb(canvas_pos, ImVec2(canvas_pos.x + actual_canvas_width, canvas_pos.y + total_timeline_height));
    ItemSize(timeline_bb);
    if (!ItemAdd(timeline_bb, 0))
        return false;

    PushID(label);

    window->DrawList->AddRectFilled(timeline_bb.Min, timeline_bb.Max, 0xFF3D3837, 4.0f);

    const float time_range = sequence_end - sequence_start;
    if (time_range <= 0.00001f) { PopID(); return false; }

    const float pixels_per_second = actual_canvas_width / time_range;

    const float ruler_y_start = canvas_pos.y;
    const float ruler_y_end = canvas_pos.y + timeline_ruler_height - 5;
    for (float t_val = 0.0f; t_val <= time_range; t_val += 1.0f) {
        float x = canvas_pos.x + t_val * pixels_per_second;
        if (x >= canvas_pos.x && x <= canvas_pos.x + actual_canvas_width) {
             window->DrawList->AddLine(ImVec2(x, ruler_y_start), ImVec2(x, ruler_y_end), 0xFF888888);
            char time_str[16];
            snprintf(time_str, sizeof(time_str), "%.0fs", t_val + sequence_start);
            window->DrawList->AddText(ImVec2(x + 3, ruler_y_start), 0xFFBBBBBB, time_str);
        }
    }

    for (int i = 0; i < (int)items.size(); ++i) {
        TimelineItem& item = items[i];

        if (!item.startTime || !item.endTime || !item.track) continue;

        if (*item.track < 0) *item.track = 0;
        if (*item.track >= num_tracks) *item.track = num_tracks - 1;

        float item_s_clamped = ImMax(*item.startTime, sequence_start);
        float item_e_clamped = ImMin(*item.endTime, sequence_end);

        // If clamped item is outside the drawable range, or duration is zero/negative, skip
        if (item_e_clamped <= item_s_clamped || item_s_clamped > sequence_end || item_e_clamped < sequence_start) continue;

        const float item_start_x = canvas_pos.x + (item_s_clamped - sequence_start) * pixels_per_second;
        const float item_end_x = canvas_pos.x + (item_e_clamped - sequence_start) * pixels_per_second;
        const float item_y = canvas_pos.y + timeline_ruler_height + (*item.track) * track_height;

        const ImRect item_bb(ImVec2(item_start_x, item_y), ImVec2(item_end_x, item_y + track_height - 2));

        const bool is_selected = (selected_item_index && *selected_item_index == i);
        const ImU32 item_color = is_selected ? 0xFFFFAA77 : 0xFFFF7755;

        window->DrawList->AddRectFilled(item_bb.Min, item_bb.Max, item_color, 2.0f);

        PushClipRect(item_bb.Min, item_bb.Max, true);
        ImVec2 text_size = CalcTextSize(item.name.c_str());
        ImVec2 text_pos(item_bb.Min.x + 4, item_bb.Min.y + (item_bb.GetHeight() - text_size.y) * 0.5f);
        window->DrawList->AddText(text_pos, 0xFFFFFFFF, item.name.c_str());
        PopClipRect();

        const ImGuiID item_id_interaction = window->GetID((void*)&items[i]);

        if (IsMouseHoveringRect(item_bb.Min, item_bb.Max) && IsMouseClicked(0)) {
            if (selected_item_index) *selected_item_index = i;
            item_selected_this_frame = true;
            SetActiveID(item_id_interaction, window);
            FocusWindow(window);
        }

        if (GetActiveID() == item_id_interaction) {
            if (io.MouseDown[0]) {
                if (pixels_per_second != 0.0f) {
                    float delta_seconds = io.MouseDelta.x / pixels_per_second;
                    *item.startTime += delta_seconds;
                    *item.endTime += delta_seconds;
                }
            } else {
                ClearActiveID();
            }
        }
    }

    if (current_time && *current_time >= sequence_start && *current_time <= sequence_end) {
        const float cursor_x = canvas_pos.x + (*current_time - sequence_start) * pixels_per_second;
        if (cursor_x >= timeline_bb.Min.x && cursor_x <= timeline_bb.Max.x) { // Ensure cursor is within bounds
            window->DrawList->AddLine(ImVec2(cursor_x, canvas_pos.y), ImVec2(cursor_x, timeline_bb.Max.y), 0xFFFFFFFF, 1.5f);
        }
    }

    PopID();
    return item_selected_this_frame;
}

} // namespace ImGui
