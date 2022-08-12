#pragma once

#include <cassert>
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include <nlohmann/json.hpp>
#include "utility.h"

using json = nlohmann::json;

class file_explorer {
	typedef BOOL(WINAPI* PFN_Wow64DisableWow64FsRedirection)(_Out_ PVOID* OldValue);
	typedef BOOL(WINAPI* PFN_Wow64RevertWow64FsRedirection)(_In_ PVOID OlValue);

public:
	static void init() {
		SYSTEM_INFO si;

		// PROCESSOR_ARCHITECTURE_AMD64
		// PROCESSOR_ARCHITECTURE_INTEL
		GetSystemInfo(&si);
		GetNativeSystemInfo(&si);
		auto hKernel32 = GetModuleHandle(L"kernel32");
		assert(hKernel32 != nullptr);

		typedef BOOL(WINAPI* PFN_IsWow64Process) (_In_ HANDLE hProcess, _Out_ PBOOL Wow64Process);

		auto IsWow64Process = reinterpret_cast<PFN_IsWow64Process>(GetProcAddress(hKernel32, "IsWow64Process"));
		m_DisableWow64FsRedirection = reinterpret_cast<PFN_Wow64DisableWow64FsRedirection>(GetProcAddress(hKernel32, "Wow64DisableWow64FsRedirection"));
		m_RevertWow64FsRedirection = reinterpret_cast<PFN_Wow64RevertWow64FsRedirection>(GetProcAddress(hKernel32, "Wow64RevertWow64FsRedirection"));
		if (IsWow64Process && m_DisableWow64FsRedirection && m_RevertWow64FsRedirection) {
			BOOL bIsWow64;
			if (IsWow64Process(GetCurrentProcess(), &bIsWow64) && bIsWow64) {
				m_IsWow64 = true;
			}
		}
	}

	static bool get_all_drives(std::vector<std::wstring>& drives)
	{
		drives.clear();
		std::wstring drive_name = L"A";
		auto LogicalDrivesMap = GetLogicalDrives();

		while (LogicalDrivesMap) {
			if (LogicalDrivesMap & 0x1) {
				drives.push_back(drive_name);
			}
			LogicalDrivesMap >>= 1;
			++drive_name[0];
		}

		return !drives.empty();
	}

	static bool get_drive_info(json& j, const std::wstring& drive)
	{
		j.clear();

		const auto& drive_root_path = drive + L":\\";
		auto drive_name = utility::cvt_ws_to_utf8(drive);
		auto type = GetDriveTypeW(drive_root_path.c_str());

		if (drive_name.empty()) return false;

		j["type"] = 1;
		j["data"] = { {"drive", drive_name}, {"type", type} };

		ULARGE_INTEGER driveTotalSize, driveFreeSize;
		if (GetDiskFreeSpaceExW(drive_root_path.c_str(), nullptr, &driveTotalSize, &driveFreeSize)) {
			j["data"]["size"] = { {"free", driveFreeSize.QuadPart}, {"total", driveTotalSize.QuadPart} };
		}

		WCHAR VolumeName[MAX_PATH + 1];
		WCHAR FileSystemName[MAX_PATH + 1];
		DWORD VolumeSerialNumber;
		if (GetVolumeInformationW(drive_root_path.c_str(), VolumeName, MAX_PATH, &VolumeSerialNumber, nullptr, nullptr, FileSystemName, MAX_PATH)) {
			j["data"]["name"] = utility::cvt_ws_to_utf8(VolumeName);
			j["data"]["serial"] = VolumeSerialNumber;
			j["data"]["fs"] = utility::cvt_ws_to_utf8(FileSystemName);
		}

		return true;
	}

	static bool list_files(json& j, const std::wstring& search_path, const std::wstring& filter = L"*")
	{
		j.clear();

		if (search_path.empty()) return false;
		std::wstring find_paten = search_path;
		if (filter.empty()) find_paten += L"*";
		else find_paten += filter;

		j["type"] = 2;

		WIN32_FIND_DATAW fileInfo;
		set_fs_redirection(false);
		auto hFindFileHandle = FindFirstFileW(find_paten.c_str(), &fileInfo);
		if (hFindFileHandle != INVALID_HANDLE_VALUE) {
			do {
				ULARGE_INTEGER fileSize;
				fileSize.HighPart = fileInfo.nFileSizeHigh;
				fileSize.LowPart = fileInfo.nFileSizeLow;
				ULARGE_INTEGER fileLastWriteTime;
				fileLastWriteTime.LowPart = fileInfo.ftLastWriteTime.dwLowDateTime;
				fileLastWriteTime.HighPart = fileInfo.ftLastWriteTime.dwHighDateTime;
				j["data"]["files"].push_back(json::object({ {"name", utility::cvt_ws_to_utf8(fileInfo.cFileName)}, {"attr", fileInfo.dwFileAttributes} }));
				if (!(fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
					j["data"]["files"].back()["size"] = fileSize.QuadPart;
				}
				j["data"]["files"].back()["last"] = fileLastWriteTime.QuadPart;
			} while (FindNextFileW(hFindFileHandle, &fileInfo));
			FindClose(hFindFileHandle);
		}

		set_fs_redirection();

		if (j.size() != 2) j.clear();
		return !j.empty();
	}

	static bool set_fs_redirection(const bool& enable = true)
	{
		if (!m_IsWow64) {
			return true;
		}

		if (enable) {
			if (!m_IsDisabledFsRedirection) return true;
			m_IsDisabledFsRedirection = !m_RevertWow64FsRedirection(m_FsRedirectionOldValue);
			return !m_IsDisabledFsRedirection;
		} else {
			if (m_IsDisabledFsRedirection) return true;
			m_IsDisabledFsRedirection = m_DisableWow64FsRedirection(&m_FsRedirectionOldValue);
			return m_IsDisabledFsRedirection;
		}
	}

private:
	static PFN_Wow64DisableWow64FsRedirection m_DisableWow64FsRedirection;
	static PFN_Wow64RevertWow64FsRedirection m_RevertWow64FsRedirection;
	static PVOID m_FsRedirectionOldValue;
	static bool m_IsDisabledFsRedirection;
	static bool m_IsWow64;
};

file_explorer::PFN_Wow64DisableWow64FsRedirection file_explorer::m_DisableWow64FsRedirection = nullptr;
file_explorer::PFN_Wow64RevertWow64FsRedirection file_explorer::m_RevertWow64FsRedirection = nullptr;
PVOID file_explorer::m_FsRedirectionOldValue;
bool file_explorer::m_IsDisabledFsRedirection = false;
bool file_explorer::m_IsWow64 = false;
