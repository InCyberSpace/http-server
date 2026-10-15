#include "utils.hpp"

#include <algorithm>
#include <cstring>
#include <ranges>
#include <string>

#ifdef _WIN32
#	include <windows.h>
#else
#	include <cstdlib>
#	include <errno.h>
#endif

#include <spdlog/spdlog.h>

namespace utils_lib
{
	void strip(std::string &s)
	{
		const auto str_start = std::ranges::find_if(
			s.begin(), s.end(), [](const char c) -> bool { return !std::isspace(c); });
		s.erase(s.begin(), str_start);

		const auto str_rend = std::ranges::find_if(
			s.rbegin(), s.rend(), [](const char c) -> bool { return !std::isspace(c); });

		if (str_rend == s.rend())
		{
			return;
		}

		const auto str_end = std::next(str_rend).base();
		s.erase(std::next(str_end), s.end());
	}

	std::wstring get_last_error_as_wstr()
	{
#ifdef _WIN32
		const auto error_message_id = GetLastError();
		if (error_message_id == 0)
		{
			return L"";
		}

		wchar_t *msg_buf = nullptr;
		const int buf_size = static_cast<int>(
			FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
							  FORMAT_MESSAGE_IGNORE_INSERTS,
						  nullptr, error_message_id, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
						  reinterpret_cast<wchar_t *>(&msg_buf), 0, nullptr));
		if (buf_size < 2)
		{
			return L"";
		}

		msg_buf[buf_size - 2] = '\0'; // Remove \r\n
		std::wstring message(msg_buf, buf_size);
		LocalFree(msg_buf);
		return message;
#else
		std::string str(std::strerror(errno));
		std::wstring wstr;
		wstr.reserve(str.size());
		std::mbstowcs(wstr.data(), str.data(), str.size());
		wstr.shrink_to_fit();
		return wstr;
#endif
	}

	std::string wstr_to_str(const std::wstring &wstr)
	{
#ifdef _WIN32
		const int buf_size =
			WideCharToMultiByte(CP_UTF8, WC_NO_BEST_FIT_CHARS, wstr.c_str(),
								static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);

		const auto buf = std::make_unique<char[]>(buf_size + 1);
		WideCharToMultiByte(CP_UTF8, WC_NO_BEST_FIT_CHARS, wstr.c_str(),
							static_cast<int>(wstr.size()), buf.get(), buf_size, nullptr, nullptr);
		buf[buf_size] = '\0';

		const std::string result = buf.get();
#else
		std::string result;
		result.reserve(wstr.size() * 4);
		std::wcstombs(result.data(), wstr.c_str(), wstr.size());
		result.shrink_to_fit();
#endif
		return result;
	}

	std::string get_last_error_as_str()
	{
#ifdef _WIN32
		return wstr_to_str(get_last_error_as_wstr());
#else
		return std::strerror(errno);
#endif
	}
} // namespace utils_lib
