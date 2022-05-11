#pragma once

#include "time.h"

#include <unordered_set>
#include <unordered_map>
#include <string>
#include <string_view>
#include <vector>

#include <Windows.h> // For CRITICAL_SECTION

constexpr uint32_t min_nb_redraws = 3;

constexpr uint64_t sleeping_duration_ms = 4;
// 4 ms should reduce CPU usage by a lot and it is small enough to keep latency of UI low.
// Notice that the maximum frequency will in every cases be limited by the rendering
// performances and v-sync.

struct RunningEntry
{
	uint64_t start_time; // Windows ticks (100 nanoseconds) since midnight on January 1, 1601 at Greenwich, England
	uint64_t end_time; // Windows ticks (100 nanoseconds) since midnight on January 1, 1601 at Greenwich, England

#if defined(_DEBUG)
	// For tests (assert checks)
	inline bool operator==(const RunningEntry& other) const
	{
		return this->start_time == other.start_time && this->end_time == other.end_time;
	}
#endif
};

struct Group
{
	std::string							name;
	std::unordered_set<std::wstring>	proccess_names; // @Warning Should be lower case

	CRITICAL_SECTION					executions_critical_section;
	std::vector<RunningEntry>			executions;

	std::vector<RunningEntry>			merged_executions;

	// Only used by ui module
	// Recomputed each "frame"

	std::vector<double> plot_durations; // Interlaced starting dates and durations
	std::vector<double> plot_merged_durations; // Interlaced starting dates and durations
	double				bar_width;
	Time_Unit			duration_unit;

	uint64_t plot_range_duration;
	uint64_t nb_executions;
	uint64_t total_execution_time;
	uint64_t maximum_duration;
	double average_executions_time;
};

struct DearTime
{
	bool				ready_to_draw = false;
	bool				done = false;

	// Flag to know if the user session is ending, in which case we shorten the shutdown path
	// We don't have to destroy DX context and Window in this case. We just have to ensure we do
	// the backup as fast as possible
	bool				done_by_end_session = false;

	// Flag to know if we already took the shortest path
	bool				has_shutdown = false;

	bool				is_quitting = false;
	CRITICAL_SECTION	is_quitting_critical_section;
	CRITICAL_SECTION	editing_groups_critical_section;

	bool				groups_dialog = false;

	std::wstring app_data_folder_path;
	std::wstring record_file_path;
	volatile LONG* nb_requested_redraws = nullptr;

	std::unordered_map<std::string, Group*> groups;
	Group* empty_group;

	// ui
	std::string current_group_name;
};

extern DearTime g_dear_time;

void initialize_application();
void shutdown_application();
void safe_backup();

//==============================================================================

// Helper functions
Group*		get_tracking_group(const std::string& name);
Group*		get_tracking_group_by_process(const std::wstring& process_name); // @TODO should return a list
void		request_redraw();

inline void request_redraw()
{
	// @Warning
	// Doing a Or bitwise operation always give a value at least equals to min_nb_redraws
	// So if nb_requested_redraws = 0xffffffff it stay at 0xffffffff (permanent redraw value)
	InterlockedOr(g_dear_time.nb_requested_redraws, min_nb_redraws);
}

inline void start_permanent_redraw()
{
	InterlockedExchange(g_dear_time.nb_requested_redraws, 0xffffffff);
}

inline void stop_permanent_redraw()
{
	// @TODO Check if it is safe to put 0 instead of min_nb_redraws
	// I think it can leads to issues if a redraw was requested just after the stop
	InterlockedExchange(g_dear_time.nb_requested_redraws, min_nb_redraws);
}

// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// @Warning Following methods should only be called from the main thread

enum class Rename_Errors
{
	no_error,
	error_empty,
	error_conflict,
	error_not_found,
};

enum class Update_Processes_Errors
{
	no_error,
	group_not_found,
	parse_error,
	too_many_processes,
};

std::string				create_new_group(); // Return generated group name
bool					create_group(const std::string& name);
std::string				delete_group(const std::string& name); // Name of following group if any, the previous if any or an empty string
Rename_Errors			rename_group(const std::string& name, const std::string& new_name);
void					get_processes_string(const std::string& group_name, char* buffer, size_t buffer_size);
Update_Processes_Errors udpate_processes(const std::string& group_name, const std::string& processes_string, size_t processes_string_maximum_length);
