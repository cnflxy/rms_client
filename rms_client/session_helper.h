#pragma once

#include <string>
#include <vector>
#include <Windows.h>
#include <WtsApi32.h>
#include <UserEnv.h>

#pragma comment(lib, "Wtsapi32")
#pragma comment(lib, "Userenv")

class session_helper {
public:
	struct session_info {
		DWORD id;
		WTS_CONNECTSTATE_CLASS state;
		std::wstring sessionName;
		std::wstring domainName;
		std::wstring userName;

		session_info(const DWORD& _id = -1, const WTS_CONNECTSTATE_CLASS& _state = WTSDown,
			const std::wstring& _sessionName = L"", const std::wstring& _domainName = L"", const std::wstring& _userName = L""
		) : id(_id), state(_state), sessionName(_sessionName), domainName(_domainName), userName(_userName) {}
	};

	static bool get_all_session(std::vector<session_info>& sessions)
	{
		DWORD dwLevel = 1;
		PWTS_SESSION_INFO_1 pSessionInfo = nullptr;
		DWORD cCount;

		sessions.clear();

		if (!WTSEnumerateSessionsExW(WTS_CURRENT_SERVER_HANDLE, &dwLevel, 0, &pSessionInfo, &cCount)) {
			//auto err = GetLastError();
			//ShowErrMsg(err);
			return false;
		}

		for (DWORD i = 0; i < cCount; ++i) {
			//std::wcout << pSessionInfo[i].SessionId << " " << pSessionInfo[i].pSessionName << std::endl;
			//if (pSessionInfo[i].State == WTSActive) {
			sessions.emplace_back(pSessionInfo[i].SessionId, pSessionInfo[i].State,
				pSessionInfo[i].pSessionName ? pSessionInfo[i].pSessionName : L"",
				pSessionInfo[i].pDomainName ? pSessionInfo[i].pDomainName : L"",
				pSessionInfo[i].pUserName ? pSessionInfo[i].pUserName : L""
			);
			//std::wcout << pSessionInfo[i].pDomainName << "\\" << pSessionInfo[i].pUserName << std::endl;
		//}
		}

		WTSFreeMemoryExW(WTSTypeSessionInfoLevel1, pSessionInfo, cCount);

		return !sessions.empty();
	}

	struct station_info {
		std::wstring name;
		bool visible;

		station_info(const std::wstring& _name = L"", const bool& _visible = false) : name(_name), visible(_visible) {}
	};

	static bool get_all_station(std::vector<station_info>& stations)
	{
		stations.clear();

		if (!EnumWindowStationsW(enumWinstationCallback, reinterpret_cast<LPARAM>(&stations))) {
			utility::print_debug_msg(L"session_helper", { L"get_all_station", L"EnumWindowStationsW" }, GetLastError());
			stations.clear();
		}

		return !stations.empty();
	}

	struct desktop_info {
		std::wstring name;
		bool user_connected;

		desktop_info(const std::wstring& _name = L"", const bool& _user_connected = false) : name(_name), user_connected(_user_connected) {}
	};

	static bool get_all_desktop(const std::wstring& stationName, std::vector<desktop_info>& desktops)
	{
		desktops.clear();

		auto hWinSta = OpenWindowStationW(stationName.c_str(), FALSE, WINSTA_ENUMDESKTOPS);
		if (!hWinSta) {
			return false;
		}

		if (!EnumDesktopsW(hWinSta, enumDesktopCallback, reinterpret_cast<LPARAM>(&desktops))) {
			desktops.clear();
		}

		CloseWindowStation(hWinSta);

		return !desktops.empty();
	}

	static bool get_desktop_info(const HDESK& desk, desktop_info& info)
	{
		DWORD nameLen;

		info.name.clear();
		if (!GetUserObjectInformationW(desk, UOI_NAME, nullptr, 0, &nameLen)) {
			std::wstring name;
			name.resize(nameLen / sizeof(std::wstring::value_type));
			if (GetUserObjectInformationW(desk, UOI_NAME, const_cast<wchar_t*>(name.data()), nameLen, &nameLen)) {
				info.name = std::move(name);
			}
		}

		info.user_connected = false;
		BOOL bIO = FALSE;
		if (GetUserObjectInformationW(desk, UOI_IO, &bIO, sizeof(bIO), nullptr) && bIO) {
			info.user_connected = true;
		}/* else {
			auto err = GetLastError();
			utility::print_debug_msg(L"get_desktop_info", {}, err);
		}*/

		return true;
	}

	static bool get_desktop_info(const std::wstring& name, desktop_info& info)
	{
		auto hDesk = OpenDesktopW(name.c_str(), 0, FALSE, DESKTOP_READOBJECTS);
		if (!hDesk) {
			return false;
		}

		auto bRet = get_desktop_info(hDesk, info);
		CloseDesktop(hDesk);

		return bRet;
	}

	static bool get_input_desktop_info(const std::wstring& stationName, desktop_info& info)
	{
		auto hWinSta = OpenWindowStationW(stationName.c_str(), FALSE, WINSTA_ENUMDESKTOPS);
		if (!hWinSta) {
			utility::print_debug_msg(L"session_helper", { L"get_input_desktop_info", L"OpenWindowStationW" }, GetLastError());
			return false;
		}

		auto hWinStaOld = GetProcessWindowStation();

		auto bRet = SetProcessWindowStation(hWinSta);

		CloseWindowStation(hWinSta);
		if (!bRet) {
			utility::print_debug_msg(L"session_helper", { L"get_input_desktop_info", L"SetProcessWindowStation" }, GetLastError());
			return false;
		}

		auto hDesk = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);

		SetProcessWindowStation(hWinStaOld);
		if (hDesk) {
			bRet = get_desktop_info(hDesk, info);
			CloseDesktop(hDesk);
		} else {
			utility::print_debug_msg(L"session_helper", { L"get_input_desktop_info", L"OpenInputDesktop" }, GetLastError());
		}

		return bRet;
	}

	static HANDLE get_primary_token(const HANDLE& token)
	{
		HANDLE duplicateToken;

		if (!DuplicateTokenEx(token, TOKEN_ALL_ACCESS, nullptr, SecurityIdentification, TokenPrimary, &duplicateToken)) {
			//auto err = GetLastError();
			//ShowErrMsg(err);
			duplicateToken = nullptr;
		}

		return duplicateToken;
	}

	static HANDLE get_user_primary_token(DWORD sessionId = DWORD(0xFFFFFFFF))
	{
		if (sessionId == DWORD(0xFFFFFFFF)) {
			sessionId = WTSGetActiveConsoleSessionId();
			if (sessionId == DWORD(0xFFFFFFFF)) {
				return nullptr;
			}
		}

		HANDLE userToken, primaryToken;
		if (!WTSQueryUserToken(sessionId, &userToken)) {
			return nullptr;
		}

		primaryToken = get_primary_token(userToken);
		CloseHandle(userToken);

		return primaryToken;
	}

	static HANDLE get_process_primary_token(const HANDLE& process)
	{
		HANDLE processToken, primaryToken;
		//TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY | TOKEN_ADJUST_SESSIONID
		if (!OpenProcessToken(process, TOKEN_ALL_ACCESS, &processToken)) {
			//auto err = GetLastError();
			//ShowErrMsg(err);
			return nullptr;
		}

		primaryToken = get_primary_token(processToken);
		CloseHandle(processToken);

		return primaryToken;
	}

	static bool enable_process_privilge(const HANDLE& process, const std::wstring& name)
	{
		TOKEN_PRIVILEGES newPrivilege;
		if (!LookupPrivilegeValueW(nullptr, name.c_str(), &newPrivilege.Privileges[0].Luid)) {
			//auto err = GetLastError();
			//ShowErrMsg(err);
			return false;
		}

		HANDLE hToken;
		if (!OpenProcessToken(process, TOKEN_ADJUST_PRIVILEGES/* | TOKEN_QUERY*/, &hToken)) {
			//auto err = GetLastError();
			//ShowErrMsg(err);
			return false;
		}

		newPrivilege.PrivilegeCount = 1;
		newPrivilege.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		auto ret = false;
		if (AdjustTokenPrivileges(hToken, false, &newPrivilege, sizeof(TOKEN_PRIVILEGES), nullptr, nullptr) && GetLastError() == ERROR_SUCCESS) {
			ret = true;
		}
		//	//auto err = GetLastError();
		//	//ShowErrMsg(err);
		//} else {
		//	auto err = GetLastError();
		//	// ERROR_NOT_ALL_ASSIGNED
		//	if (err != ERROR_SUCCESS) {
		//		//ShowErrMsg(err);
		//	} else {
		//		ret = true;
		//	}
		//}

		CloseHandle(hToken);

		return ret;
	}

	static bool create_process_advanced(const std::wstring& exePath, const std::wstring& cmdLine, const std::wstring& desktopPath, const HANDLE& primaryToken, DWORD sessionId = DWORD(0xFFFFFFFF))
	{
		//std::vector<DWORD> activeSessionsId;

		//get_all_active_session_id(activeSessionsId);

		if (sessionId == DWORD(0xFFFFFFFF)) {
			sessionId = WTSGetActiveConsoleSessionId();
			if (sessionId == DWORD(0xFFFFFFFF)) {
				return false;
			}
		}

		//if (!primaryToken) {
		//	primaryToken = get_process_primary_token(GetCurrentProcess());
		//	if (!primaryToken) {
		//		primaryToken = get_user_primary_token(sessionId);
		//		if (!primaryToken) {
		//			return false;
		//		}
		//	}
		//}

		if (!SetTokenInformation(primaryToken, TokenSessionId, &sessionId, sizeof(sessionId))) {
			return false;
		}

		LPVOID lpEnvironment = nullptr;

		if (!CreateEnvironmentBlock(&lpEnvironment, primaryToken, TRUE)) {
			return false;
		}

		STARTUPINFO si{};
		PROCESS_INFORMATION pi;

		si.cb = sizeof(si);
		si.lpDesktop = const_cast<LPWSTR>(desktopPath.data());
		si.wShowWindow = SW_SHOW;
		si.dwFlags = STARTF_USESHOWWINDOW;

		enable_process_privilge(GetCurrentProcess(), SE_INCREASE_QUOTA_NAME);
		enable_process_privilge(GetCurrentProcess(), SE_ASSIGNPRIMARYTOKEN_NAME);

		// To run a process in the session specified in the token, use the CreateProcessAsUser function.
		// https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createprocesswithtokenw
		auto bRet = CreateProcessAsUserW(
			primaryToken,
			exePath.c_str(),
			const_cast<LPWSTR>(cmdLine.data()),
			nullptr,
			nullptr,
			FALSE,
			NORMAL_PRIORITY_CLASS | CREATE_NEW_CONSOLE | CREATE_UNICODE_ENVIRONMENT,
			lpEnvironment,
			L"C:\\",
			&si,
			&pi
		);
		//GetWindowsDirectoryW();

		if (bRet) {
			DWORD exitCode;
			bRet = GetExitCodeThread(pi.hThread, &exitCode);
			if (bRet) {
				bRet = exitCode == STILL_ACTIVE;
			}
			if (bRet) {
				bRet = GetExitCodeProcess(pi.hProcess, &exitCode);
			}
			if (bRet) {
				bRet = exitCode == STILL_ACTIVE;
			}
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		}

		DestroyEnvironmentBlock(lpEnvironment);

		return bRet;

		//enable_process_privilge(SE_IMPERSONATE_NAME);
		//SE_INCREASE_QUOTA_NAME
		//SE_ASSIGNPRIMARYTOKEN_NAME

		//HANDLE hPipe = CreateNamedPipeW(
		//	L"\\\\.\\pipe\\{2104E01A-D35D-4B01-AE40-A17FC672BA74}",
		//	PIPE_ACCESS_INBOUND,
		//	PIPE_TYPE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
		//	2,
		//	0,
		//	0,
		//	0,
		//	nullptr
		//);
		//if (hPipe == INVALID_HANDLE_VALUE) {
		//	auto err = GetLastError();
		//	ShowErrMsg(err);
		//} else {
		//	if (!ConnectNamedPipe(hPipe, nullptr)) {
		//		auto err = GetLastError();
		//		ShowErrMsg(err);
		//	} else {
		//		char bytes[1];
		//		DWORD dwread;
		//		if (!ReadFile(hPipe, bytes, 1, &dwread, NULL)) {
		//			auto err = GetLastError();
		//			ShowErrMsg(err);
		//		} else {
		//			if (!ImpersonateNamedPipeClient(hPipe)) {
		//				auto err = GetLastError();
		//				ShowErrMsg(err);
		//			} else {
		//				HANDLE hThreadToken, hTokenDup;
		//				OpenThreadToken(GetCurrentThread(), TOKEN_ALL_ACCESS, FALSE, &hThreadToken);
		//				DuplicateToken(hThreadToken, SecurityImpersonation, &hTokenDup);
		//				CloseHandle(hThreadToken);
		//				RevertToSelf();
		//				DisconnectNamedPipe(hPipe);
		//				CloseHandle(hPipe);
		//				SetThreadToken(nullptr, hTokenDup);
		//				for (auto session_id : activeSessionsId) {
		//					if (!SetTokenInformation(hPrimaryToken, TokenSessionId, &session_id, sizeof(DWORD))) {
		//						auto err = GetLastError();
		//						ShowErrMsg(err);
		//						continue;
		//					}
		//				}
		//			}
		//		}
		//	}
		//}

		//CloseHandle(hPrimaryToken);


		//auto dwConsoleSessionId = WTSGetActiveConsoleSessionId();

	//WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, dwConsoleSessionId, )


		//return true;
	}

private:
	static BOOL CALLBACK enumWinstationCallback(LPWSTR name, LPARAM stations)
	{
		bool visible = false;
		auto hWinSta = OpenWindowStationW(name, FALSE, WINSTA_READATTRIBUTES);
		if (hWinSta) {
			USEROBJECTFLAGS uoFlags;
			if (GetUserObjectInformationW(hWinSta, UOI_FLAGS, &uoFlags, sizeof(uoFlags), nullptr)) {
				visible = uoFlags.dwFlags & WSF_VISIBLE;
			}
			CloseWindowStation(hWinSta);
		}
		reinterpret_cast<std::vector<station_info>*>(stations)->emplace_back(name, visible);
		return TRUE;
	}

	static BOOL CALLBACK enumDesktopCallback(LPWSTR name, LPARAM desktops)
	{
		desktop_info info;
		if (get_desktop_info(name, info)) {
			reinterpret_cast<std::vector<desktop_info>*>(desktops)->emplace_back(std::move(info));
		}
		//bool user_connected = false;
		//auto hDesk = OpenDesktopW(name, 0, FALSE, DESKTOP_READOBJECTS);
		//if (hDesk) {
		//	DWORD nameLen;

		//	auto bRet = GetUserObjectInformationW(hDesk, UOI_IO, nullptr, 0, nullptr);
		//	if (!GetUserObjectInformationW(hDesk, UOI_NAME, nullptr, 0, &nameLen) && nameLen) {
		//		//USEROBJECTFLAGS uoFlags;
		//		//if (GetUserObjectInformationW(hDesk, UOI_USER_SID, nullptr, 0, nullptr)) {
		//		user_connected = true;
		//	}
		//	CloseDesktop(hDesk);
		//}
		//reinterpret_cast<std::vector<desktop_info>*>(desktops)->emplace_back(name, user_connected);
		return TRUE;
	}
};
