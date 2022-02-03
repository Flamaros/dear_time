#include <Windows.h> // @TODO remove that, only needed for FILETIME type, ULARGE_INTEGER requiere winnt.h

inline uint64_t to_ms(const FILETIME& file_time)
{
	ULARGE_INTEGER	lv_Large;

	lv_Large.LowPart = file_time.dwLowDateTime;
	lv_Large.HighPart = file_time.dwHighDateTime;

	return lv_Large.QuadPart / 10'000;
}
