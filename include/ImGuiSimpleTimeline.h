// include/ImGuiSimpleTimeline.h
#pragma once
#include "imgui.h"
#include "imgui_internal.h" // We need this for GImGui
#include <vector>
#include <string>

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
    bool item_selected = false;
    ImGuiIO& io = ImGui::GetIO();
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    const ImVec2 canvas_pos = window->DC.CursorPos;
    const ImVec2 canvas_size = GetContentRegionAvail();
    const float timeline_height = (num_tracks + 1) * 20.0f;
    const ImRect timeline_bb(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + timeline_height));
    ItemSize(timeline_bb);
    if (!ItemAdd(timeline_bb, 0))
        return false;

    PushID(label);
    
    // --- DRAW BACKGROUND AND RULER ---
    window->DrawList->AddRectFilled(timeline_bb.Min, timeline_bb.Max, 0xFF3D3837, 4.0f);
    
    const float time_range = sequence_end - sequence_start;
    const float pixels_per_second = (canvas_size.x > 0) ? canvas_size.x / time_range : 0;

    if (pixels_per_second > 0) {
        for (float t = 0; t <= time_range; t += 1.0f) {
            float x = canvas_pos.x + t * pixels_per_second;
            window->DrawList->AddLine(ImVec2(x, canvas_pos.y), ImVec2(x, canvas_pos.y + 15), 0xFF888888);
            char time_str[16];
            snprintf(time_str, sizeof(time_str), "%.0fs", t + sequence_start);
            window->DrawList->AddText(ImVec2(x + 3, canvas_pos.y), 0xFFBBBBBB, time_str);
        }
    }
    
    // --- DRAW ITEMS ---
    for (int i = 0; i < (int)items.size(); ++i) {
        TimelineItem& item = items[i];
        
        if (*item.track < 0) *item.track = 0;
        if (*item.track >= num_tracks) *item.track = num_tracks - 1;

        const float item_start_x = canvas_pos.x + (*item.startTime - sequence_start) * pixels_per_second;
        const float item_end_x = canvas_pos.x + (*item.endTime - sequence_start) * pixels_per_second;
        const float item_y = canvas_pos.y + (1 + *item.track) * 20.0f;
        
        const ImRect item_bb(ImVec2(item_start_x, item_y), ImVec2(item_end_x, item_y + 18));
        
        const bool is_selected = (selected_item_index && *selected_item_index == i);
        const ImU32 item_color = is_selected ? 0xFFFFAA77 : 0xFFFF7755;
        
        window->DrawList->AddRectFilled(item_bb.Min, item_bb.Max, item_color, 2.0f);
        
        ImVec2 text_size = CalcTextSize(item.name.c_str());
        // CORRECTED: Construct new ImVec2 directly to avoid const issues
        ImVec2 text_pos(item_bb.Min.x + 4, item_bb.Min.y + (18 - text_size.y) * 0.5f);
        if (text_pos.x + text_size.x < item_bb.Max.x) {
             window->DrawList->AddText(text_pos, 0xFFFFFFFF, item.name.c_str());
        }

        const ImGuiID item_id = window->GetID((void*)&items[i]);
        
        // CORRECTED: Use IsMouseHoveringRect for custom hit detection
        if (IsMouseHoveringRect(item_bb.Min, item_bb.Max) && io.MouseClicked[0]) {
            if (selected_item_index) *selected_item_index = i;
            item_selected = true;
            SetActiveID(item_id, window);
        }

        if (GetActiveID() == item_id) {
            if (io.MouseDown[0]) {
                if(pixels_per_second > 0) {
                    *item.startTime += io.MouseDelta.x / pixels_per_second;
                    *item.endTime += io.MouseDelta.x / pixels_per_second;
                }
            } else {
                ClearActiveID();
            }
        }
    }
    
    if(pixels_per_second > 0) {
        const float cursor_x = canvas_pos.x + (*current_time - sequence_start) * pixels_per_second;
        if (cursor_x >= timeline_bb.Min.x && cursor_x <= timeline_bb.Max.x) {
            window->DrawList->AddLine(ImVec2(cursor_x, canvas_pos.y), ImVec2(cursor_x, timeline_bb.Max.y), 0xFFFFFFFF, 2.0f);
        }
    }
    
    PopID();
    return item_selected;
}

} // namespace ImGui
