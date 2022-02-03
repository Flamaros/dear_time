#include "ui.h"

#include "application.h"
#include "time.h"

#include <imgui/imgui.h>
#include <implot/implot.h>

#include <algorithm>
#include <vector>
#include <iterator>
#include <string>
#include <ctime>
#include <format>

#undef min
#undef max

constexpr float stats_panel_width = 240.0f;
constexpr const char* groups_popup_name = "Groups edition";
constexpr size_t group_name_maximum_length = 32;
constexpr size_t processes_string_maximum_length = 4096;

// Forward declaration of helpers
static void     insert_merge_entry(std::vector<RunningEntry>& merged_entries, const RunningEntry& entry);
static void     merge_durations(const std::vector<double>& durations, uint64_t period_duration, std::vector<double>& result);
static uint64_t get_optimal_period_duration(double range_start_s, double range_end_s);
static void     generate_time_data(const std::string& group_name);
static uint64_t WindowsTickToUnixSeconds(uint64_t windowsTicks);
static void     draw_graph(Group* group);
static void     duration_formmatter(double value, char* buff, int size, void* user_data);
static void     ui_groups_dialog();

inline float maximum_group_name_ui_width()
{
    constexpr char wider_chars[group_name_maximum_length] = "wwwwwwwwwwwwwwwwwwwwwwwwwwwwwww";
    return ImGui::CalcTextSize(wider_chars).x + ImGui::GetStyle().FramePadding.x * 2.0f;
}

inline double floor_at_unit(double value, uint64_t unit)
{
    return value - ((uint64_t)value % unit);
}

void initialize_ui()
{
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    ImGui::GetStyle().WindowPadding = ImVec2(6.0f, 6.0f);
    ImGui::GetStyle().FrameBorderSize = 1.0f;
    ImGui::GetStyle().WindowRounding = 0.0f;

    ImPlot::GetStyle().UseLocalTime = true;
    ImPlot::GetStyle().UseISO8601 = true;
    ImPlot::GetStyle().Use24HourClock = true;
    ImPlot::GetStyle().FitPadding = ImVec2(0.0f, 0.1f);
}

void ui_frame()
{
    Group* group = g_dear_time.empty_group;
    
    EnterCriticalSection(&g_dear_time.editing_groups_critical_section);

    if (g_dear_time.current_group_name.size())
        group = get_tracking_group(g_dear_time.current_group_name);

    const auto& io = ImGui::GetIO();

    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::Begin("Graphs", NULL,
        ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration);

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            //if (ImGui::MenuItem("Open..", "Ctrl+O")) { /* Do stuff */ }
            //if (ImGui::MenuItem("Save", "Ctrl+S")) { /* Do stuff */ }
            if (ImGui::MenuItem("Quit", "Ctrl+W")) { g_dear_time.done = true; }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Settings"))
        {
            if (ImGui::MenuItem("Groups", "Ctrl+G")) { g_dear_time.groups_dialog = true; }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    ui_groups_dialog();

    // @Warning Graph seems to be smaller than its real size, so I increased the x spacing from 8 to 12.
    // But it is certainly a bad idea that can breaks other things.
    // @TODO investigate before sending a bug report to ImPlot
    // Or the issue can be related to the computation I do for the graph width
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12.0f, 4.0f));

    float graph_width = io.DisplaySize.x - stats_panel_width - ImGui::GetStyle().ItemSpacing.x - ImGui::GetStyle().FrameBorderSize;

    if (graph_width >= 1.0f)
    {
        ImGui::BeginChild("Graph frame", ImVec2(graph_width, -1.0f));
        draw_graph(group);
        ImGui::EndChild();

        ImGui::SameLine(std::max(io.DisplaySize.x - stats_panel_width, ImGui::GetStyle().ItemSpacing.x));
    }

    ImGui::BeginGroup();
    {
        ImGui::SetNextItemWidth(maximum_group_name_ui_width());
        if (ImGui::BeginCombo("###Group", g_dear_time.current_group_name.c_str()))
        {
            for (auto it : g_dear_time.groups)
            {
                bool is_selected = (g_dear_time.current_group_name == it.second->name);

                if (ImGui::Selectable(it.second->name.c_str(), is_selected))
                    g_dear_time.current_group_name = it.second->name;
                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::NewLine();

        // @Warning I add a little epsilon to be able to get 1.0 year and 1.0 month (instead of 4.3 weeks (which is ugly))
        // @TODO Investigate to find the source of this issue, I don't know if it is related to double vs uint64_t conversions or
        // an ImPlot issue
        ImGui::Text("Visible period : %s", format_duration(group->plot_range_duration + 1).c_str());
        ImGui::Text("Bar period : %s", format_duration((uint64_t)group->bar_width + 1).c_str());
        ImGui::NewLine();
        ImGui::Text("Nb executions : %lld", group->nb_executions);
        ImGui::Text("Total execution time : %s", format_duration(group->total_execution_time).c_str());
        ImGui::Text("Maximum duration : %s", format_duration(group->maximum_duration).c_str());
        ImGui::Text("Average execution time : %s", format_duration((uint64_t)group->average_executions_time).c_str());
    }
    ImGui::EndGroup();

    ImGui::PopStyleVar(1);

    ImGui::End();

    LeaveCriticalSection(&g_dear_time.editing_groups_critical_section);
}

void test_ui_insert_merge_entry()
{
    std::vector<RunningEntry>   initial_entries;

    insert_merge_entry(initial_entries, { 5, 6 });
    assert(initial_entries == std::vector<RunningEntry>({ {5, 6} }));

    insert_merge_entry(initial_entries, { 7, 8 });
    assert(initial_entries == std::vector<RunningEntry>({ {5, 6}, {7, 8} }));

    insert_merge_entry(initial_entries, { 1, 3 });
    assert(initial_entries == std::vector<RunningEntry>({ {1, 3}, {5, 6}, {7, 8} }));

    insert_merge_entry(initial_entries, { 12, 13 });
    assert(initial_entries == std::vector<RunningEntry>({ {1, 3}, {5, 6}, {7, 8}, {12, 13} }));

    insert_merge_entry(initial_entries, { 13, 15 });
    assert(initial_entries == std::vector<RunningEntry>({ {1, 3}, {5, 6}, {7, 8}, {12, 15} }));

    insert_merge_entry(initial_entries, { 5, 9 });
    assert(initial_entries == std::vector<RunningEntry>({ {1, 3}, { 5, 9}, { 12, 15} }));

    insert_merge_entry(initial_entries, { 2, 4 });
    assert(initial_entries == std::vector<RunningEntry>({ {1, 4}, {5, 9}, { 12, 15} }));

    insert_merge_entry(initial_entries, { 10, 13 });
    assert(initial_entries == std::vector<RunningEntry>({ {1, 4}, {5, 9}, { 10, 15} }));

    // Merge on edges of ranges
    initial_entries = { {1, 3}, { 5, 9}, { 12, 15} };

    insert_merge_entry(initial_entries, { 2, 5 });
    assert(initial_entries == std::vector<RunningEntry>({ {1, 9}, { 12, 15} }));

    insert_merge_entry(initial_entries, { 9, 13 });
    assert(initial_entries == std::vector<RunningEntry>({ {1, 15} }));

    // Multiple merges
    initial_entries = { {1, 3}, {5, 6}, {7, 8}, {12, 15} };
    insert_merge_entry(initial_entries, { 1, 12 });
    assert(initial_entries == std::vector<RunningEntry>({ {1, 15} }));

    initial_entries = { {1, 3}, {5, 6}, {7, 8}, {12, 15} };
    insert_merge_entry(initial_entries, { 0, 16 });
    assert(initial_entries == std::vector<RunningEntry>({ {0, 16} }));
}

// =============================================================================

constexpr uint64_t WINDOWS_TICK = 10'000'000;
constexpr uint64_t SEC_TO_UNIX_EPOCH = 11'644'473'600LL;

inline uint64_t WindowsTickToUnixSeconds(uint64_t windowsTicks)
{
    return windowsTicks / WINDOWS_TICK - SEC_TO_UNIX_EPOCH;
}

inline uint64_t UnixSecondsToWindowsTick(uint64_t unixSeconds)
{
    return (unixSeconds + SEC_TO_UNIX_EPOCH) * WINDOWS_TICK;
}

/** @brief expect an already sorted vector of entries
*/
void insert_merge_entry(std::vector<RunningEntry>& merged_entries, const RunningEntry& entry)
{
    bool start_overlap = false;
    bool end_overlap = false;
    size_t start_pos = merged_entries.size();
    size_t end_pos = start_pos + 1;

    // Search the position of start (@Speed reverse iteration)
    for (size_t i = merged_entries.size() - 1; i != (size_t)-1; i--)
    {
        if (entry.start_time > merged_entries[i].end_time) // start_time of entry is just after the current one
        {
            start_pos = i + 1;
            break;
        }
        else if (entry.start_time >= merged_entries[i].start_time) // start_time of entry is in the range of the current one
        {
            start_pos = i;
            start_overlap = true;
            break;
        }
        start_pos = i;
    }

    // Search the position of end (@speed from start_pos)
    for (end_pos = start_pos + 1; end_pos < merged_entries.size(); end_pos++)
    {
        if (entry.end_time < merged_entries[end_pos].start_time) // end_time of entry is just before the next one
        {
            break;
        }
        else if (entry.end_time <= merged_entries[end_pos].end_time) // end_time of entry is in the range of the next one
        {
            end_pos++;
            end_overlap = true;
            break;
        }
    }
    // Fix end_overlap with latest range in merged_entries
    if (end_pos - 1 < merged_entries.size())
        end_overlap = entry.end_time >= merged_entries[end_pos - 1].start_time;

    // Insertion/Merge
    if (start_overlap == false && end_overlap == false && start_pos == end_pos - 1)
    {
        merged_entries.insert(std::next(merged_entries.begin(), start_pos), entry);
    }
    else
    {
        merged_entries[start_pos].start_time = std::min(merged_entries[start_pos].start_time, entry.start_time);
        merged_entries[start_pos].end_time = std::max(merged_entries[end_pos - 1].end_time, entry.end_time);
        if (start_pos - end_pos > 0)
            merged_entries.erase(std::next(merged_entries.begin(), start_pos + 1), std::next(merged_entries.begin(), end_pos));
    }
}

void merge_durations(const std::vector<double>& durations, uint64_t period_duration, std::vector<double>& result)
{
    if (durations.empty())
        return;

    size_t insertion_index = -2;

    result.reserve(durations.size());
    for (size_t i = 0; i < durations.size(); i += 2)
    {
        const double& start_time = durations[i];
        const double& duration = durations[i + 1];

        double start_time_on_period = floor_at_unit(start_time, period_duration);

        if (result.empty() || start_time_on_period != result[insertion_index])
        {
            result.push_back(start_time_on_period);
            result.push_back(0.0);
            insertion_index += 2;
        }

        result[insertion_index + 1] += duration;
    }
}

uint64_t get_optimal_period_duration(double range_start_s, double range_end_s)
{
    uint64_t range_size = (uint64_t)(range_end_s - range_start_s);
    constexpr size_t min_nb_periods = 10; // @TODO may I need to adapt this to the actual plot width (in pixels)?

    if (range_size > 10 * year_duration)
        return (uint64_t)year_duration;
    if (range_size > 4 * year_duration)
        return 6 * (uint64_t)month_duration;
    if (range_size > 2 * year_duration)
        return 3 * (uint64_t)month_duration;
    else if (range_size > 10 * (uint64_t)month_duration)
        return (uint64_t)month_duration;
    else if (range_size > 6 * (uint64_t)month_duration)
        return 2 * week_duration;
    else if (range_size > 2 * (uint64_t)month_duration)
        return week_duration;
    else if (range_size > (uint64_t)month_duration)
        return 3 * day_duration;
    else if (range_size > 2 * week_duration)
        return day_duration;
    else if (range_size > 6 * day_duration)
        return 12 * hour_duration;
    else if (range_size > 2 * day_duration)
        return 6 * hour_duration;
    else if (range_size > day_duration)
        return 2 * hour_duration;
    else if (range_size > 12 * hour_duration)
        return 30 * minute_duration;
    else if (range_size > 4 * hour_duration)
        return 15 * minute_duration;
    else if (range_size > hour_duration)
        return 10 * minute_duration;
    else if (range_size > 5 * minute_duration)
        return minute_duration;
    else if (range_size > minute_duration)
        return 15 * second_duration;

    return second_duration;
}

void generate_time_data(Group* group)
{
    EnterCriticalSection(&group->executions_critical_section);
    if (group->executions.size())
    {
        // Merge entries
        group->merged_executions.reserve(group->merged_executions.size() + group->executions.size());
        for (size_t i = 0; i < group->executions.size(); i++)
        {
            insert_merge_entry(group->merged_executions, group->executions[i]);
        }
        group->executions.clear();
    }
    LeaveCriticalSection(&group->executions_critical_section);
}

void generate_view_data(Group* group, double start, double end, double merging_range)
{
    // Clear data
    group->plot_durations.clear();
    group->plot_durations.reserve(group->merged_executions.size() * 2);
    group->plot_merged_durations.clear();
    group->bar_width = 1.0;

    group->plot_range_duration = (uint64_t)end - (uint64_t)start;
    group->nb_executions = 0;
    group->total_execution_time = 0;
    group->maximum_duration = 0;
    group->average_executions_time = 0;

    // @TODO compute the optimal period of bars depending on the timeline scale
    uint64_t period_duration = get_optimal_period_duration(start, end);

    // @TODO get group->merged_execution range out-side the main loop to ease the cumulation of durations
    // @SpeedUp, we should be able to do a fast search of the sub-range because merged_executions are sorted

    for (size_t i = 0; i < group->merged_executions.size(); i++)
    {
        double starting_date = (double)WindowsTickToUnixSeconds(group->merged_executions[i].start_time);

        // As bars are offseted by half there width we should have to enlarge the "visible" range
        if (starting_date < floor_at_unit(start + 0.5 * period_duration, period_duration) || starting_date > floor_at_unit(end + 1.5 * period_duration, period_duration))
            continue;
        
        uint64_t duration = (group->merged_executions[i].end_time - group->merged_executions[i].start_time) / WINDOWS_TICK;

        group->plot_durations.push_back(starting_date);
        group->plot_durations.push_back((double)duration);

        group->nb_executions++;
        group->total_execution_time += duration;
        group->maximum_duration = std::max(duration, group->maximum_duration);
        group->average_executions_time = group->total_execution_time / (double)group->nb_executions;

        // Stick bars on minutes, hours,... and compute a bar_width
    }

    // Merge durations for bars
    merge_durations(group->plot_durations, period_duration, group->plot_merged_durations);
    group->bar_width = (double)period_duration;

    // Scale durations depending on the ideal time unit
    {
        double max_merged_duration = 0.0;
        for (size_t i = 1; i < group->plot_merged_durations.size(); i += 2)
            max_merged_duration = std::max(max_merged_duration, group->plot_merged_durations[i]);
        group->duration_unit = get_time_unit((uint64_t)max_merged_duration);

        for (size_t i = 1; i < group->plot_merged_durations.size(); i += 2)
            group->plot_merged_durations[i] /= get_time_unit_divisor(group->duration_unit);
    }
}

void draw_graph(Group* group)
{
    generate_time_data(group);

    if (ImPlot::BeginPlot("##Time", ImVec2(-1, -1))) {
        double now_date = (double)time(0);

        ImPlot::SetupAxis(ImAxis_X1, "date", ImPlotAxisFlags_Time);
        ImPlot::SetupAxis(ImAxis_Y1, "duration", /*ImPlotAxisFlags_Time |*/ ImPlotAxisFlags_LockMin | ImPlotAxisFlags_AutoFit);
        ImPlot::SetupAxisLimits(ImAxis_X1, now_date - (24.0 * 60.0 * 60.0), now_date + (24.0 * 60.0 * 60.0)); // 2 days around current date
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, 10 * 60.0); // from 0.0 to 10min

        ImPlot::SetupAxisFormat(ImAxis_Y1, &duration_formmatter, group);

        ImPlotRect plot_rect = ImPlot::GetPlotLimits(ImAxis_X1, ImAxis_Y2);
        generate_view_data(group, plot_rect.Min().x, plot_rect.Max().x, 1.0);

        ImPlot::PlotBars(group->name.c_str(),
            &group->plot_merged_durations.data()[0], &group->plot_merged_durations.data()[1],
            (int)group->plot_merged_durations.size() / 2, group->bar_width * 0.8, 0,
            2 * sizeof(double));
        ImPlot::EndPlot();
    }
}

void duration_formmatter(double value, char* buff, int size, void* user_data)
{
    Group* group = (Group*)user_data;

    std::format_to_n_result<char*> result;

    result = std::format_to_n(buff, size, "{:.1f} {}", value, get_time_unit_short_label(group->duration_unit));

    *result.out = '\0';
}

void ui_groups_dialog()
{
    static std::string current_group_name;
    static Rename_Errors rename_result = Rename_Errors::no_error;
    static Update_Processes_Errors update_processes_result = Update_Processes_Errors::no_error;
    static ImVec2 popup_size = ImVec2(500, 170);

    char new_group_name_buffer[group_name_maximum_length];
    char process_buffer[processes_string_maximum_length];

    new_group_name_buffer[0] = '\0';
    if (g_dear_time.groups_dialog)
        ImGui::OpenPopup(groups_popup_name);

    ImGui::SetNextWindowSize(popup_size);
    if (ImGui::BeginPopupModal(groups_popup_name, &g_dear_time.groups_dialog))
    {
        popup_size = ImGui::GetWindowSize();

        // On Same Line
        {
            ImGui::SetNextItemWidth(maximum_group_name_ui_width());
            if (ImGui::BeginCombo("###Group", current_group_name.c_str()))
            {
                for (auto it : g_dear_time.groups)
                {
                    bool is_selected = (current_group_name == it.second->name);

                    if (ImGui::Selectable(it.second->name.c_str(), is_selected)) {
                        current_group_name = it.second->name;
                        rename_result = Rename_Errors::no_error;
                    }
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::SameLine();
            if (ImGui::Button("+"))
            {
                current_group_name = create_new_group();
                rename_result = Rename_Errors::no_error;
            }

            ImGui::SameLine();
            if (ImGui::Button("-"))
            {
                current_group_name = delete_group(current_group_name);
                rename_result = Rename_Errors::no_error;
            }
        }

        ImGuiInputTextFlags rename_flags = ImGuiInputTextFlags_EnterReturnsTrue;
        const char* rename_hint = "new name";

        if (current_group_name.empty()) {
            rename_flags |= ImGuiInputTextFlags_ReadOnly;
            rename_hint = "";
        }
        ImGui::SetNextItemWidth(maximum_group_name_ui_width());
        if (ImGui::InputTextWithHint("###Name", rename_hint, new_group_name_buffer, sizeof(new_group_name_buffer), rename_flags))
        {
            // @TODO request rendering at cursor frequency
            rename_result = rename_group(current_group_name, new_group_name_buffer);
            if (rename_result == Rename_Errors::no_error)
                current_group_name = new_group_name_buffer;
        }

        if (rename_result == Rename_Errors::no_error)
            ImGui::NewLine();
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
            if (rename_result == Rename_Errors::error_empty)
                ImGui::Text("Group name can't be empty!");
            else if (rename_result == Rename_Errors::error_conflict)
                ImGui::Text("Group name already exist!");
            ImGui::PopStyleColor();
        }
        ImGui::NewLine();
        
        ImGuiInputTextFlags update_processes_flags = ImGuiInputTextFlags_EnterReturnsTrue;

        if (current_group_name.empty()) {
            update_processes_flags |= ImGuiInputTextFlags_ReadOnly;
        }

        get_processes_string(current_group_name, process_buffer, sizeof(process_buffer));
        ImGui::Text("Executable names (separated by semicolons):");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("###Executables", process_buffer, sizeof(process_buffer), update_processes_flags))
        {
            update_processes_result = udpate_processes(current_group_name, process_buffer, processes_string_maximum_length);
        }

        if (update_processes_result == Update_Processes_Errors::no_error)
            ImGui::NewLine();
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
            if (update_processes_result == Update_Processes_Errors::too_many_processes)
                ImGui::Text("There is too many process to be able to display the list!?");
            ImGui::PopStyleColor();
        }

        ImGui::EndPopup();
    }
}
