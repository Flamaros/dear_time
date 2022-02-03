#include "application.h"

#include "utils.h"

#include <algorithm>
#include <string>
#include <format>
#include <codecvt>
#include <string_view>

#include <cstring>
#include <cassert>

#include <Shlobj.h>

constexpr uint32_t record_format_version = 0;

void initialize_application()
{
	InitializeCriticalSection(&g_dear_time.is_quitting_critical_section);
	InitializeCriticalSection(&g_dear_time.editing_groups_critical_section);

	// @TODO check errors

	// Load record
	WCHAR* known_path = nullptr;
	SHGetKnownFolderPath(FOLDERID_ProgramData, KF_FLAG_DEFAULT, NULL, &known_path);

	g_dear_time.app_data_folder_path = known_path + std::wstring(L"\\Dear Time");
	CoTaskMemFree(known_path);
	SHCreateDirectoryEx(NULL, g_dear_time.app_data_folder_path.c_str(), NULL);

	g_dear_time.record_file_path = g_dear_time.app_data_folder_path + std::wstring(L"\\records.dat");

	{
		HANDLE hFile = CreateFileW(g_dear_time.record_file_path.c_str(), // name of the write
			GENERIC_READ,           // open for reading
			0,                      // do not share
			NULL,                   // default security
			OPEN_EXISTING,          // open only existing
			FILE_ATTRIBUTE_NORMAL,  // normal file
			NULL);                  // no attr. template

		if (hFile != INVALID_HANDLE_VALUE)
		{
			DWORD dwBytesRead;

			char magic_number[5];
			ReadFile(hFile, &magic_number[0], sizeof(magic_number), &dwBytesRead, NULL);

			assert(strncmp(magic_number, "DTIME", 5) == 0);

			uint32_t file_format_version;

			ReadFile(hFile, &file_format_version, sizeof(file_format_version), &dwBytesRead, NULL);

			uint32_t nb_groups;
			ReadFile(hFile, &nb_groups, sizeof(nb_groups), &dwBytesRead, NULL);
			for (uint32_t group_index = 0; group_index < nb_groups; group_index++)
			{
				Group* group = new Group();

				uint32_t group_name_size;
				ReadFile(hFile, &group_name_size, sizeof(group_name_size), &dwBytesRead, NULL);

				group->name.resize(group_name_size);
				ReadFile(hFile, group->name.data(), group_name_size * sizeof(*group->name.data()), &dwBytesRead, NULL);

				uint32_t nb_process_names;
				ReadFile(hFile, &nb_process_names, sizeof(nb_process_names), &dwBytesRead, NULL);
				for (uint32_t process_name_index = 0; process_name_index < nb_process_names; process_name_index++)
				{
					uint32_t process_name_size;
					ReadFile(hFile, &process_name_size, sizeof(process_name_size), &dwBytesRead, NULL);
					std::wstring process_name;

					process_name.resize(process_name_size);
					ReadFile(hFile, process_name.data(), process_name_size * sizeof(*process_name.data()), &dwBytesRead, NULL);
					group->proccess_names.insert(process_name);
				}

				uint32_t nb_merged_executions;

				ReadFile(hFile, &nb_merged_executions, sizeof(nb_merged_executions), &dwBytesRead, NULL);
				group->merged_executions.resize(nb_merged_executions);
				ReadFile(hFile, group->merged_executions.data(), nb_merged_executions * sizeof(*group->merged_executions.data()), &dwBytesRead, NULL);

				InitializeCriticalSection(&group->executions_critical_section);
				g_dear_time.groups.insert(std::make_pair(group->name, group));
			}

			// Selected group name
			{
				uint32_t group_name_size;
				ReadFile(hFile, &group_name_size, sizeof(group_name_size), &dwBytesRead, NULL);

				g_dear_time.current_group_name.resize(group_name_size);
				ReadFile(hFile, g_dear_time.current_group_name.data(), group_name_size * sizeof(*g_dear_time.current_group_name.data()), &dwBytesRead, NULL);
			}
		}

		CloseHandle(hFile);
	}

	// Create the empty group
	{
		g_dear_time.empty_group = new Group();
		g_dear_time.empty_group->name = "";
		InitializeCriticalSection(&g_dear_time.empty_group->executions_critical_section);
	}

	// Initialize atomic variables
	g_dear_time.nb_requested_redraws = (LONG*)_aligned_malloc(4, 32);
	InterlockedExchange(g_dear_time.nb_requested_redraws, min_nb_redraws);
}

void shutdown_application()
{
	// We don't call destructors, we let Windows do the dirty job (much faster than us)

	// @TODO check errors

	// Backup records
	HANDLE hFile = CreateFileW(g_dear_time.record_file_path.c_str(), // name of the write
		GENERIC_WRITE,          // open for writing
		0,                      // do not share
		NULL,                   // default security
		CREATE_ALWAYS,			// create or open and truncate
		FILE_ATTRIBUTE_NORMAL,  // normal file
		NULL);                  // no attr. template

	DWORD dwBytesWritten;

	WriteFile(hFile, "DTIME", 5, &dwBytesWritten, NULL);
	WriteFile(hFile, &record_format_version, sizeof(record_format_version), &dwBytesWritten, NULL);

	uint32_t nb_groups = (uint32_t)g_dear_time.groups.size();
	WriteFile(hFile, &nb_groups, sizeof(nb_groups), &dwBytesWritten, NULL);
	for (auto group_pair : g_dear_time.groups)
	{
		uint32_t group_name_size = (uint32_t)group_pair.first.size();

		WriteFile(hFile, &group_name_size, sizeof(group_name_size), &dwBytesWritten, NULL);
		WriteFile(hFile, group_pair.first.data(), group_name_size * sizeof(*group_pair.first.data()), &dwBytesWritten, NULL);

		uint32_t nb_process_names = (uint32_t)group_pair.second->proccess_names.size();

		WriteFile(hFile, &nb_process_names, sizeof(nb_process_names), &dwBytesWritten, NULL);
		for (const auto& process_name : group_pair.second->proccess_names)
		{
			uint32_t process_name_size = (uint32_t)process_name.size();

			WriteFile(hFile, &process_name_size, sizeof(process_name_size), &dwBytesWritten, NULL);
			WriteFile(hFile, process_name.data(), process_name_size * sizeof(*process_name.data()), &dwBytesWritten, NULL);
		}

		uint32_t nb_merged_executions = (uint32_t)group_pair.second->merged_executions.size();

		WriteFile(hFile, &nb_merged_executions, sizeof(nb_merged_executions), &dwBytesWritten, NULL);
		WriteFile(hFile, group_pair.second->merged_executions.data(), nb_merged_executions * sizeof(*group_pair.second->merged_executions.data()), &dwBytesWritten, NULL);
	}

	// Selected group name
	{
		uint32_t group_name_size = (uint32_t)g_dear_time.current_group_name.size();

		WriteFile(hFile, &group_name_size, sizeof(group_name_size), &dwBytesWritten, NULL);
		WriteFile(hFile, g_dear_time.current_group_name.data(), group_name_size * sizeof(*g_dear_time.current_group_name.data()), &dwBytesWritten, NULL);
	}

	CloseHandle(hFile);
}

//==============================================================================

Group* get_tracking_group(const std::string& name)
{
	auto it = g_dear_time.groups.find(name);

	if (it == g_dear_time.groups.end())
		return nullptr;
	return it->second;
}

Group* get_tracking_group_by_process(const std::wstring& process_name)
{
	// @TODO @SpeedUp should be able to take the parameter as string_view, but we actually can't mix key
	// types even when they are compatibles for comparison and hashing.

	std::wstring lower_name = process_name;

	std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
	for (auto it_group : g_dear_time.groups)
	{
		if (it_group.second->proccess_names.find(lower_name) != it_group.second->proccess_names.end())
			return it_group.second;
	}

	return nullptr;
}

std::string	create_new_group()
{
	uint32_t	new_group_id = 0;
	std::string name;

	do
	{
		name = std::format("NEW GROUP {:02}", new_group_id++);
	} while (create_group(name) == false);

	return name;
}

bool create_group(const std::string& name)
{
	// @Warning As group editing methods should be called only from the main thread it is safe to read groups here
	if (g_dear_time.groups.find(name.c_str()) != g_dear_time.groups.end())
		return false;

	EnterCriticalSection(&g_dear_time.editing_groups_critical_section);
	{
		Group* new_group = new Group();

		new_group->name = name;
		InitializeCriticalSection(&new_group->executions_critical_section);
		g_dear_time.groups.insert(std::make_pair(name, new_group));
	}
	LeaveCriticalSection(&g_dear_time.editing_groups_critical_section);

	return true;
}

std::string delete_group(const std::string& name)
{
	// @Warning As group editing methods should be called only from the main thread it is safe to read groups here
	auto it = g_dear_time.groups.find(name.c_str());
	if (it == g_dear_time.groups.end())
		return "";

	std::unordered_map<std::string, Group*>::iterator next_it;
	EnterCriticalSection(&g_dear_time.editing_groups_critical_section);
	{
		Group* group = it->second;

		DeleteCriticalSection(&group->executions_critical_section);
		delete group;

		next_it = g_dear_time.groups.erase(it);
	}
	LeaveCriticalSection(&g_dear_time.editing_groups_critical_section);
	if (next_it != g_dear_time.groups.end())
		return next_it->second->name;

	auto prev_it = std::prev(next_it);
	if (prev_it != g_dear_time.groups.end())
		return prev_it->second->name;
	return "";
}

Rename_Errors rename_group(const std::string& name, const std::string& new_name)
{
	if (new_name.empty())
		return Rename_Errors::error_empty;
	// May wan't to return an enum for error kind and ui message

	// @Warning As group editing methods should be called only from the main thread it is safe to read groups here
	if (g_dear_time.groups.find(new_name.c_str()) != g_dear_time.groups.end())
		return Rename_Errors::error_conflict;

	auto it = g_dear_time.groups.find(name.c_str());
	if (it == g_dear_time.groups.end()) {
		return Rename_Errors::error_not_found;
	}

	EnterCriticalSection(&g_dear_time.editing_groups_critical_section);
	{
		Group* group = it->second;

		group->name = new_name;
		g_dear_time.groups.erase(it);
		g_dear_time.groups.insert(std::make_pair(new_name, group));
	}
	LeaveCriticalSection(&g_dear_time.editing_groups_critical_section);

	return Rename_Errors::no_error;
}

void get_processes_string(const std::string& group_name, char* buffer, size_t buffer_size)
{
	assert(buffer_size > 0);

	buffer[0] = '\0';

	// @Warning As group editing methods should be called only from the main thread it is safe to read groups here
	auto it = g_dear_time.groups.find(group_name.c_str());
	if (it == g_dear_time.groups.end()) {
		return;
	}

	Group* group = it->second;
	size_t pos = 0;

	using convert_type = std::codecvt_utf8<wchar_t>;
	std::wstring_convert<convert_type, wchar_t> converter;

	for (const auto& proccess_name : group->proccess_names)
	{
		std::string utf8 = converter.to_bytes(proccess_name);

		if (pos + group->proccess_names.size() * 2 - 2 > buffer_size)
			return;

		if (pos  > 1)
			buffer[pos++] = ' ';

		memcpy(&buffer[pos], utf8.c_str(), utf8.size());
		pos += utf8.size();
		buffer[pos++] = ';';
	}
	if (pos > 0)
		buffer[pos - 1] = '\0'; // Erase the last ';'
}

Update_Processes_Errors udpate_processes(const std::string& group_name, const std::string& processes_string, size_t processes_string_maximum_length)
{
	auto it = g_dear_time.groups.find(group_name.c_str());
	if (it == g_dear_time.groups.end()) {
		return Update_Processes_Errors::group_not_found;
	}

	Group* group = it->second;
	auto split = split_string(processes_string, ";");
	size_t reformatted_size = 0;

	using convert_type = std::codecvt_utf8<wchar_t>;
	std::wstring_convert<convert_type, wchar_t> converter;

	Update_Processes_Errors result = Update_Processes_Errors::no_error;
	size_t string_size = 0;

	EnterCriticalSection(&g_dear_time.editing_groups_critical_section);
	{
		group->proccess_names.clear();
		group->proccess_names.reserve(split.size());
		for (const auto& word : split) {
			auto trimmed = trim(word);

			std::wstring utf16 = converter.from_bytes(std::string(trimmed));

			if (utf16.empty())
				continue;

			string_size += utf16.size();
			if (string_size + group->proccess_names.size() * 2 >= processes_string_maximum_length) // processes_string_maximum_length - 1 because of ending '\0'
			{
				result = Update_Processes_Errors::too_many_processes;
				break;
			}

			group->proccess_names.insert(utf16);
		}
	}
	LeaveCriticalSection(&g_dear_time.editing_groups_critical_section);

	return result;
}
