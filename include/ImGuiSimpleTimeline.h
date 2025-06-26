// ImGuiSimpleTimeline.h
#pragma once
#include "imgui.h"
#include "imgui_internal.h" // We need this for GImGui
#include <vector>
#include <string>
#include <cstdio> // For snprintf
#include <cmath>  // For floor, fmod, ceil
#include <algorithm> // For ImMax, ImMin (though ImGui might provide its own)

#ifndef FLT_MAX
#define FLT_MAX __FLT_MAX__
#endif

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
inline bool SimpleTimeline(const char* label, std::vector<TimelineItem>& items, float* current_time,
                           int* selected_item_index, int num_tracks,
                           float sequence_total_start_time_seconds, float sequence_total_end_time_seconds,
                           float& horizontal_scroll_seconds, float& zoom_factor)
{
    bool item_selected_this_frame = false;
    ImGuiIO& io = ImGui::GetIO();
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    const ImVec2 canvas_pos = window->DC.CursorPos;
    ImVec2 canvas_size = GetContentRegionAvail();
    if (canvas_size.x <= 0.0f) canvas_size.x = 1.0f;
    if (canvas_size.y <= 0.0f) canvas_size.y = 1.0f;


    const float timeline_ruler_height = 30.0f;
    const float track_height = 20.0f;
    const float total_timeline_height = timeline_ruler_height + num_tracks * track_height;

    float actual_canvas_width = canvas_size.x;

    const ImRect timeline_bb(canvas_pos, ImVec2(canvas_pos.x + actual_canvas_width, canvas_pos.y + total_timeline_height));
    ItemSize(timeline_bb);
    if (!ItemAdd(timeline_bb, 0))
        return false;

    PushID(label);

    window->DrawList->AddRectFilled(timeline_bb.Min, timeline_bb.Max, 0xFF3D3837, 4.0f);

    zoom_factor = ImMax(0.01f, zoom_factor);
    const float overall_sequence_duration = sequence_total_end_time_seconds - sequence_total_start_time_seconds;
    if (overall_sequence_duration <= 0.00001f) { PopID(); return false; }

    const float pixels_per_second = (actual_canvas_width / overall_sequence_duration) * zoom_factor;
    const float visible_duration_on_ruler = actual_canvas_width / pixels_per_second;

    float max_scroll = sequence_total_end_time_seconds - visible_duration_on_ruler;
    if (max_scroll < sequence_total_start_time_seconds) max_scroll = sequence_total_start_time_seconds;
    horizontal_scroll_seconds = ImClamp(horizontal_scroll_seconds, sequence_total_start_time_seconds, max_scroll);

    float current_view_start_time_on_ruler = horizontal_scroll_seconds;
    float current_view_end_time_on_ruler = horizontal_scroll_seconds + visible_duration_on_ruler;

    const float ruler_y_start = canvas_pos.y;
    const float tick_area_y_end = canvas_pos.y + timeline_ruler_height;
    window->DrawList->AddRectFilled(ImVec2(canvas_pos.x, ruler_y_start), ImVec2(canvas_pos.x + actual_canvas_width, tick_area_y_end), 0xFF222222, 2.0f);

    // --- Dynamic Tick Calculation ---
    const float min_pixels_per_tick_label = 50.0f; // Min horizontal space for a label like "10.0s"
    const float min_pixels_per_minor_tick = 5.0f;

    // Determine suitable minor tick interval based on zoom
    float minor_tick_interval_seconds = 0.1f; // Start with a small interval
    if (pixels_per_second * minor_tick_interval_seconds < min_pixels_per_minor_tick) minor_tick_interval_seconds = 0.2f;
    if (pixels_per_second * minor_tick_interval_seconds < min_pixels_per_minor_tick) minor_tick_interval_seconds = 0.5f;
    if (pixels_per_second * minor_tick_interval_seconds < min_pixels_per_minor_tick) minor_tick_interval_seconds = 1.0f;
    if (pixels_per_second * minor_tick_interval_seconds < min_pixels_per_minor_tick) minor_tick_interval_seconds = 2.0f;
    if (pixels_per_second * minor_tick_interval_seconds < min_pixels_per_minor_tick) minor_tick_interval_seconds = 5.0f;
    if (pixels_per_second * minor_tick_interval_seconds < min_pixels_per_minor_tick) minor_tick_interval_seconds = 10.0f;
    if (pixels_per_second * minor_tick_interval_seconds < min_pixels_per_minor_tick) minor_tick_interval_seconds = 30.0f;
    if (pixels_per_second * minor_tick_interval_seconds < min_pixels_per_minor_tick) minor_tick_interval_seconds = 60.0f; // 1 minute
    if (pixels_per_second * minor_tick_interval_seconds < min_pixels_per_minor_tick) minor_tick_interval_seconds = 5 * 60.0f; // 5 minutes
    if (pixels_per_second * minor_tick_interval_seconds < min_pixels_per_minor_tick) minor_tick_interval_seconds = 10 * 60.0f; // 10 minutes

    // Determine major tick interval (usually a multiple of minor ticks)
    // Major ticks should have labels, so they need more space
    float major_tick_interval_seconds = minor_tick_interval_seconds;
    while (pixels_per_second * major_tick_interval_seconds < min_pixels_per_tick_label) {
        if (major_tick_interval_seconds < 1.0f) major_tick_interval_seconds *= 2.0f; // 0.1 -> 0.2 -> 0.4 (approx 0.5) -> 0.8 (approx 1.0)
        else if (major_tick_interval_seconds < 5.0f) major_tick_interval_seconds = ceil(major_tick_interval_seconds / 1.0f) * 1.0f + 1.0f; // 1->2, 2->3 etc
        else if (major_tick_interval_seconds < 10.0f) major_tick_interval_seconds = 5.0f;
        else if (major_tick_interval_seconds < 30.0f) major_tick_interval_seconds = 10.0f;
        else if (major_tick_interval_seconds < 60.0f) major_tick_interval_seconds = 30.0f;
        else if (major_tick_interval_seconds < 5*60.0f) major_tick_interval_seconds = 60.0f;
        else major_tick_interval_seconds *= 2.0f; // For larger intervals

        // Safety break for extremely zoomed out cases
        if (major_tick_interval_seconds > overall_sequence_duration / 2.0f && overall_sequence_duration > 0) {
             major_tick_interval_seconds = overall_sequence_duration / 2.0f;
             if (pixels_per_second * major_tick_interval_seconds < min_pixels_per_tick_label) break; // Still too small, give up trying to make it larger
        }
        if (major_tick_interval_seconds <= 0.0f) { // Safety break for invalid interval
            major_tick_interval_seconds = overall_sequence_duration > 0 ? overall_sequence_duration : 1.0f; break;
        }
    }
     // Ensure major is at least minor and a sensible multiple if possible
    if (major_tick_interval_seconds < minor_tick_interval_seconds) major_tick_interval_seconds = minor_tick_interval_seconds;
    if (fmod(major_tick_interval_seconds, minor_tick_interval_seconds) > 0.0001f && major_tick_interval_seconds > minor_tick_interval_seconds) {
        major_tick_interval_seconds = floor(major_tick_interval_seconds / minor_tick_interval_seconds) * minor_tick_interval_seconds;
        if (major_tick_interval_seconds < minor_tick_interval_seconds) major_tick_interval_seconds = minor_tick_interval_seconds;
    }
    if (major_tick_interval_seconds <= 0.0f) major_tick_interval_seconds = 1.0f; // Final safety
    if (minor_tick_interval_seconds <= 0.0f) minor_tick_interval_seconds = 1.0f; // Final safety


    float first_visible_time = current_view_start_time_on_ruler;
    float last_visible_time = current_view_end_time_on_ruler;

    float current_tick_time = floor(first_visible_time / minor_tick_interval_seconds) * minor_tick_interval_seconds;
    if (current_tick_time < sequence_total_start_time_seconds) current_tick_time = sequence_total_start_time_seconds;


    float last_label_x_end_pos = -FLT_MAX; // To prevent label overlap

    for (float t_world = current_tick_time; t_world <= last_visible_time + minor_tick_interval_seconds * 0.5f; t_world += minor_tick_interval_seconds) {
        if (t_world > sequence_total_end_time_seconds + minor_tick_interval_seconds * 0.5f) break;
        if (t_world < sequence_total_start_time_seconds - minor_tick_interval_seconds * 0.5f && t_world < first_visible_time - minor_tick_interval_seconds * 0.5f) continue;


        float x_on_canvas = canvas_pos.x + (t_world - first_visible_time) * pixels_per_second;

        if (x_on_canvas >= canvas_pos.x -1.0f && x_on_canvas <= canvas_pos.x + actual_canvas_width + 1.0f) {
            bool is_major_tick = (fmod(t_world + minor_tick_interval_seconds * 0.001f, major_tick_interval_seconds) < minor_tick_interval_seconds * 0.01f || major_tick_interval_seconds <= minor_tick_interval_seconds);

            float tick_height_px = is_major_tick ? 10.0f : 5.0f;
            window->DrawList->AddLine(
                ImVec2(x_on_canvas, tick_area_y_end - tick_height_px),
                ImVec2(x_on_canvas, tick_area_y_end -1.0f), // Draw up to the bottom of the ruler
                0xFF888888);

            if (is_major_tick) {
                char time_str[32];
                if (major_tick_interval_seconds < 1.0f) {
                    snprintf(time_str, sizeof(time_str), "%.1fs", t_world);
                } else if (major_tick_interval_seconds < 10.0f && fmod(t_world, 1.0f) > 0.001f) {
                     snprintf(time_str, sizeof(time_str), "%.1fs", t_world);
                }
                else {
                    snprintf(time_str, sizeof(time_str), "%.0fs", t_world);
                }
                ImVec2 text_size = CalcTextSize(time_str);

                // Attempt to center label on major tick, ensure it doesn't overlap previous label
                float text_x_pos = x_on_canvas - text_size.x / 2.0f;

                if (text_x_pos > last_label_x_end_pos + 5.0f) { // 5px padding between labels
                     // Clamp text position to be within canvas bounds
                    if (text_x_pos < canvas_pos.x + 3.0f) text_x_pos = canvas_pos.x + 3.0f;
                    if (text_x_pos + text_size.x > canvas_pos.x + actual_canvas_width - 3.0f) {
                        text_x_pos = canvas_pos.x + actual_canvas_width - text_size.x - 3.0f;
                    }
                    // Final check to ensure it's still within reasonable bounds after clamping and doesn't overlap
                    if (text_x_pos > last_label_x_end_pos + 5.0f && (text_x_pos + text_size.x < canvas_pos.x + actual_canvas_width)) {
                        window->DrawList->AddText(ImVec2(text_x_pos, ruler_y_start + 2), 0xFFBBBBBB, time_str);
                        last_label_x_end_pos = text_x_pos + text_size.x;
                    }
                }
            }
        }
    }


    for (int i = 0; i < (int)items.size(); ++i) {
        TimelineItem& item = items[i];
        if (!item.startTime || !item.endTime || !item.track) continue;
        if (*item.track < 0) *item.track = 0;
        if (*item.track >= num_tracks) *item.track = num_tracks - 1;

        float item_world_start_time = *item.startTime;
        float item_world_end_time = *item.endTime;

        float item_start_x_on_canvas = canvas_pos.x + (item_world_start_time - current_view_start_time_on_ruler) * pixels_per_second;
        float item_end_x_on_canvas   = canvas_pos.x + (item_world_end_time - current_view_start_time_on_ruler) * pixels_per_second;

        if (item_end_x_on_canvas < canvas_pos.x || item_start_x_on_canvas > canvas_pos.x + actual_canvas_width) continue;

        float item_visual_start_x = ImMax(item_start_x_on_canvas, canvas_pos.x);
        float item_visual_end_x   = ImMin(item_end_x_on_canvas, canvas_pos.x + actual_canvas_width);

        if (item_visual_end_x <= item_visual_start_x) continue;

        const float item_y_on_canvas = canvas_pos.y + timeline_ruler_height + (*item.track) * track_height;
        const ImRect item_visual_bb(ImVec2(item_visual_start_x, item_y_on_canvas), ImVec2(item_visual_end_x, item_y_on_canvas + track_height - 2));

        const bool is_selected = (selected_item_index && *selected_item_index == i);
        const ImU32 item_color = is_selected ? 0xFFFFAA77 : 0xFFFF7755;

        window->DrawList->AddRectFilled(item_visual_bb.Min, item_visual_bb.Max, item_color, 2.0f);

        if (item_end_x_on_canvas - item_start_x_on_canvas > 5.0f) {
            PushClipRect(item_visual_bb.Min, item_visual_bb.Max, true);
            ImVec2 text_size = CalcTextSize(item.name.c_str());
            ImVec2 text_pos(item_start_x_on_canvas + 4, item_visual_bb.Min.y + (item_visual_bb.GetHeight() - text_size.y) * 0.5f);
            if (text_pos.x < item_visual_bb.Min.x + 4) text_pos.x = item_visual_bb.Min.x + 4;

            if (text_pos.x + text_size.x < item_visual_bb.Max.x - 4) {
                 window->DrawList->AddText(text_pos, 0xFFFFFFFF, item.name.c_str());
            }
            PopClipRect();
        }

        // Interaction logic for dragging body and edges
        const float edge_handle_width_px = ImMax(8.0f, pixels_per_second * 0.1f); // Make handles at least 8px or 0.1s wide in screen space
        const float min_item_duration_seconds = 0.1f; // Minimum duration an item can be resized to

        // Define interaction bounding boxes for edges and body
        // Note: item_visual_bb is already clamped to canvas
        ImRect left_edge_bb(item_visual_bb.Min, ImVec2(ImMin(item_visual_bb.Min.x + edge_handle_width_px, item_visual_bb.Max.x), item_visual_bb.Max.y));
        ImRect right_edge_bb(ImVec2(ImMax(item_visual_bb.Max.x - edge_handle_width_px, item_visual_bb.Min.x), item_visual_bb.Min.y), item_visual_bb.Max);
        // Body is between the handles. Ensure body_bb.Min.x <= body_bb.Max.x
        ImRect body_bb(ImVec2(left_edge_bb.Max.x, item_visual_bb.Min.y), ImVec2(right_edge_bb.Min.x, item_visual_bb.Max.y));
        if (body_bb.Min.x > body_bb.Max.x) { // If item is too small, body might be inverted or zero width
            // Prioritize edges, or let body cover the whole item if handles overlap significantly
            body_bb = item_visual_bb;
        }


        const ImGuiID item_id_body = window->GetID((void*)&items[i]);
        const ImGuiID item_id_left_edge = window->GetID((void*)((char*)&items[i] + sizeof(TimelineItem) * 1)); // Offset ID
        const ImGuiID item_id_right_edge = window->GetID((void*)((char*)&items[i] + sizeof(TimelineItem) * 2)); // Further offset ID

        bool mouse_over_left_edge = IsMouseHoveringRect(left_edge_bb.Min, left_edge_bb.Max) && (item_visual_bb.GetWidth() > edge_handle_width_px * 1.5f); // Only active if item is large enough
        bool mouse_over_right_edge = IsMouseHoveringRect(right_edge_bb.Min, right_edge_bb.Max) && (item_visual_bb.GetWidth() > edge_handle_width_px * 1.5f);
        bool mouse_over_body = IsMouseHoveringRect(body_bb.Min, body_bb.Max) && !mouse_over_left_edge && !mouse_over_right_edge;

        if (mouse_over_left_edge || mouse_over_right_edge) {
            SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        } else if (mouse_over_body && IsWindowHovered() && IsMouseHoveringRect(item_visual_bb.Min, item_visual_bb.Max)) {
            // No specific cursor for body drag, but could set to Hand if desired
        }

        if (IsMouseClicked(0)) {
            if (mouse_over_left_edge) {
                SetActiveID(item_id_left_edge, window);
                if (selected_item_index) { *selected_item_index = i; item_selected_this_frame = true; FocusWindow(window); }
            } else if (mouse_over_right_edge) {
                SetActiveID(item_id_right_edge, window);
                if (selected_item_index) { *selected_item_index = i; item_selected_this_frame = true; FocusWindow(window); }
            } else if (mouse_over_body && IsMouseHoveringRect(item_visual_bb.Min, item_visual_bb.Max)) { // Ensure click was on the item
                SetActiveID(item_id_body, window);
                if (selected_item_index) { *selected_item_index = i; item_selected_this_frame = true; FocusWindow(window); }
            }
        }

        ImGuiID current_active_id = GetActiveID();
        if (io.MouseDown[0] && current_active_id != 0) {
            if (pixels_per_second > 0.00001f) { // Check for valid pixels_per_second
                float delta_seconds = io.MouseDelta.x / pixels_per_second;
                if (delta_seconds != 0.0f) {
                    if (current_active_id == item_id_body) {
                        *item.startTime += delta_seconds;
                        *item.endTime += delta_seconds;
                    } else if (current_active_id == item_id_left_edge) {
                        float new_start_time = *item.startTime + delta_seconds;
                        if (new_start_time < (*item.endTime - min_item_duration_seconds)) {
                            *item.startTime = new_start_time;
                        } else { // Snap to maintain min duration
                            *item.startTime = *item.endTime - min_item_duration_seconds;
                        }
                    } else if (current_active_id == item_id_right_edge) {
                        float new_end_time = *item.endTime + delta_seconds;
                        if (new_end_time > (*item.startTime + min_item_duration_seconds)) {
                            *item.endTime = new_end_time;
                        } else { // Snap to maintain min duration
                            *item.endTime = *item.startTime + min_item_duration_seconds;
                        }
                    }
                     // Clamp item times to overall sequence bounds after modification
                    *item.startTime = ImClamp(*item.startTime, sequence_total_start_time_seconds, sequence_total_end_time_seconds - min_item_duration_seconds);
                    *item.endTime = ImClamp(*item.endTime, sequence_total_start_time_seconds + min_item_duration_seconds, sequence_total_end_time_seconds);
                    if (*item.startTime > *item.endTime - min_item_duration_seconds) { // Final check post-clamping
                        if (current_active_id == item_id_left_edge) *item.startTime = *item.endTime - min_item_duration_seconds;
                        else *item.endTime = *item.startTime + min_item_duration_seconds;
                    }
                }
            }
        } else if (current_active_id != 0 && !io.MouseDown[0]) { // Mouse released
             if (current_active_id == item_id_body || current_active_id == item_id_left_edge || current_active_id == item_id_right_edge) {
                ClearActiveID();
            }
        }
    }

    if (current_time && *current_time >= current_view_start_time_on_ruler && *current_time <= current_view_end_time_on_ruler) {
        const float cursor_x_on_canvas = canvas_pos.x + (*current_time - current_view_start_time_on_ruler) * pixels_per_second;
        window->DrawList->AddLine(ImVec2(cursor_x_on_canvas, canvas_pos.y), ImVec2(cursor_x_on_canvas, timeline_bb.Max.y), 0xFFFFFFFF, 1.5f);
    }

    // --- Timeline Scrubbing ---
    const ImGuiID ruler_interaction_id = window->GetID("##TimelineRulerInteraction");
    ImRect ruler_bb(canvas_pos, ImVec2(canvas_pos.x + actual_canvas_width, canvas_pos.y + timeline_ruler_height));

    if (IsMouseHoveringRect(ruler_bb.Min, ruler_bb.Max) && IsMouseClicked(0)) {
        SetActiveID(ruler_interaction_id, window);
        FocusWindow(window);
    }

    if (GetActiveID() == ruler_interaction_id) {
        if (io.MouseDown[0]) {
            if (pixels_per_second > 0.00001f) {
                float mouse_x_relative_to_canvas = io.MousePos.x - canvas_pos.x;
                float time_at_mouse_pos = current_view_start_time_on_ruler + mouse_x_relative_to_canvas / pixels_per_second;
                // Clamp to the total sequence duration, not just the visible part
                *current_time = ImClamp(time_at_mouse_pos, sequence_total_start_time_seconds, sequence_total_end_time_seconds);
            }
        } else {
            ClearActiveID();
        }
    }


    PopID();
    return item_selected_this_frame;
}

} // namespace ImGui
