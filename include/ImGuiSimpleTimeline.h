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
    bool item_selected_this_frame = false; // Renamed to avoid conflict if used elsewhere
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    const ImVec2 canvas_pos = window->DC.CursorPos;
    const ImVec2 canvas_size = GetContentRegionAvail();
    const float timeline_ruler_height = 20.0f; // Height for the ruler part
    const float track_height = 20.0f; // Height for each track
    const float total_timeline_height = timeline_ruler_height + num_tracks * track_height;

    // Ensure canvas_size.x is not zero to prevent division by zero
    if (canvas_size.x <= 0) return false;
    float actual_canvas_width = (canvas_size.x > 0) ? canvas_size.x : 1.0f; // Avoid 0 width

    const ImRect timeline_bb(canvas_pos, ImVec2(canvas_pos.x + actual_canvas_width, canvas_pos.y + total_timeline_height));
    ItemSize(timeline_bb);
    if (!ItemAdd(timeline_bb, 0))
        return false;

    PushID(label);

    // --- DRAW BACKGROUND AND RULER ---
    window->DrawList->AddRectFilled(timeline_bb.Min, timeline_bb.Max, 0xFF3D3837, 4.0f); // Dark background

    const float time_range = sequence_end - sequence_start;
    if (time_range <= 0) { PopID(); return false; } // Avoid division by zero if time_range is invalid

    const float pixels_per_second = actual_canvas_width / time_range;

    // Draw ruler ticks
    const float ruler_y_start = canvas_pos.y;
    const float ruler_y_end = canvas_pos.y + timeline_ruler_height - 5; // End a bit before track area
    for (float t_val = 0; t_val <= time_range; t_val += 1.0f) { // Iterate actual time values
        float x = canvas_pos.x + (t_val) * pixels_per_second; // t_val is offset from sequence_start
        if (x >= canvas_pos.x && x <= canvas_pos.x + actual_canvas_width) {
             window->DrawList->AddLine(ImVec2(x, ruler_y_start), ImVec2(x, ruler_y_end), 0xFF888888);
            char time_str[16];
            snprintf(time_str, sizeof(time_str), "%.0fs", t_val + sequence_start);
            window->DrawList->AddText(ImVec2(x + 3, ruler_y_start), 0xFFBBBBBB, time_str);
        }
    }

    // --- DRAW ITEMS ---
    for (int i = 0; i < (int)items.size(); ++i) {
        TimelineItem& item = items[i];

        if (!item.startTime || !item.endTime || !item.track) continue; // Safety check

        // Ensure track is valid
        if (*item.track < 0) *item.track = 0;
        if (*item.track >= num_tracks) *item.track = num_tracks - 1;

        // Clamp item times to be within sequence bounds for drawing
        float item_s_clamped = ImMax(*item.startTime, sequence_start);
        float item_e_clamped = ImMin(*item.endTime, sequence_end);
        if (item_e_clamped <= item_s_clamped) continue; // Skip if not visible or invalid range

        const float item_start_x = canvas_pos.x + (item_s_clamped - sequence_start) * pixels_per_second;
        const float item_end_x = canvas_pos.x + (item_e_clamped - sequence_start) * pixels_per_second;
        const float item_y = canvas_pos.y + timeline_ruler_height + (*item.track) * track_height;

        const ImRect item_bb(ImVec2(item_start_x, item_y), ImVec2(item_end_x, item_y + track_height - 2)); // -2 for some padding

        const bool is_selected = (selected_item_index && *selected_item_index == i);
        const ImU32 item_color = is_selected ? 0xFFFFAA77 : 0xFFFF7755; // Orange-ish colors

        window->DrawList->AddRectFilled(item_bb.Min, item_bb.Max, item_color, 2.0f);

        // Draw item label (clipped within item_bb)
        PushClipRect(item_bb.Min, item_bb.Max, true);
        ImVec2 text_size = CalcTextSize(item.name.c_str());
        // Corrected ImVec2 addition:
        ImVec2 text_pos(item_bb.Min.x + 4, item_bb.Min.y + (item_bb.GetHeight() - text_size.y) * 0.5f);
        window->DrawList->AddText(text_pos, 0xFFFFFFFF, item.name.c_str());
        PopClipRect();

        // --- INTERACTION ---
        const ImGuiID item_id_interaction = window->GetID((void*)&items[i]); // Unique ID for interaction

        // Allow interaction slightly outside the visual rect for easier grabbing
        ImRect whole_item_bb_interact = item_bb;
        // whole_item_bb_interact.Expand(ImVec2(2,2)); // Small expansion for easier click

        // Corrected hover check:
        if (IsMouseHoveringRect(whole_item_bb_interact.Min, whole_item_bb_interact.Max) && IsMouseClicked(0)) {
            if (selected_item_index) *selected_item_index = i;
            item_selected_this_frame = true;
            SetActiveID(item_id_interaction, window);
            FocusWindow(window);
        }

        if (GetActiveID() == item_id_interaction) {
            if (io.MouseDown[0]) {
                // Using mouse_delta directly, assuming it's valid.
                // The check for pixels_per_second > 0 already guards against division by zero for delta_seconds.
                if (pixels_per_second != 0.0f) { // Additional safety for pixels_per_second
                    float delta_seconds = io.MouseDelta.x / pixels_per_second;
                    *item.startTime += delta_seconds;
                    *item.endTime += delta_seconds;
                }
            } else {
                ClearActiveID();
            }
        }
    }

    // --- DRAW CURRENT TIME CURSOR ---
    if (current_time && *current_time >= sequence_start && *current_time <= sequence_end) {
        const float cursor_x = canvas_pos.x + (*current_time - sequence_start) * pixels_per_second;
        if (cursor_x >= timeline_bb.Min.x && cursor_x <= timeline_bb.Max.x) {
            window->DrawList->AddLine(ImVec2(cursor_x, canvas_pos.y), ImVec2(cursor_x, timeline_bb.Max.y), 0xFFFFFFFF, 1.5f);
        }
    }

    PopID();
    return item_selected_this_frame;
}

} // namespace ImGui
