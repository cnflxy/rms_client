#pragma once

#include <string>
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>

class service_helper {
public:
	static bool init()
	{

	}

	static bool install(const std::wstring& name, const std::wstring& path, const bool& auto_start = false, const bool& kernel_driver = false)
	{
		auto scManagerHandle = OpenSCManagerW(nullptr, SERVICES_ACTIVE_DATABASE, SC_MANAGER_CREATE_SERVICE);
		if (!scManagerHandle) {
			return false;
		}

		auto scServiceHandle = CreateServiceW(
			scManagerHandle,
			name.c_str(),
			name.c_str(),
			0,
			kernel_driver ? SERVICE_KERNEL_DRIVER : SERVICE_WIN32_OWN_PROCESS,
			auto_start ? SERVICE_AUTO_START : SERVICE_DEMAND_START,
			SERVICE_ERROR_NORMAL,
			path.c_str(),
			nullptr, nullptr, nullptr, nullptr, nullptr
		);

		CloseServiceHandle(scManagerHandle);
		if (!scServiceHandle) {
			auto err = GetLastError();
			if (err != ERROR_SERVICE_EXISTS) {
				return false;
			}
		} else {
			CloseServiceHandle(scServiceHandle);
		}

		return true;
	}

	static bool remove(const std::wstring& name, const bool& force = false)
	{
		if (!stop(name) && !force) {
			return false;
		}

		auto scManagerHandle = OpenSCManagerW(nullptr, SERVICES_ACTIVE_DATABASE, SC_MANAGER_CONNECT);
		if (!scManagerHandle) {
			return false;
		}

		auto scServiceHandle = OpenServiceW(scManagerHandle, name.c_str(), DELETE);

		CloseServiceHandle(scManagerHandle);
		if (scServiceHandle) {
			auto bRet = DeleteService(scServiceHandle);

			CloseServiceHandle(scServiceHandle);
			if (!bRet && GetLastError() == ERROR_SERVICE_MARKED_FOR_DELETE) {
				bRet = true;
			}

			return bRet;
		} else if (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST) {
			return true;
		}

		return false;
	}

	static bool start(const std::wstring& name)
	{
		auto scManagerHandle = OpenSCManagerW(nullptr, SERVICES_ACTIVE_DATABASE, SC_MANAGER_CONNECT);
		if (!scManagerHandle) {
			return false;
		}

		auto scServiceHandle = OpenServiceW(scManagerHandle, name.c_str(), SERVICE_START);

		CloseServiceHandle(scManagerHandle);
		if (scServiceHandle) {
			auto bRet = StartServiceW(scServiceHandle, 0, nullptr);

			CloseServiceHandle(scServiceHandle);
			if (!bRet && GetLastError() == ERROR_SERVICE_ALREADY_RUNNING) {
				bRet = true;
			}

			return bRet;
		}

		return false;
	}

	static bool stop(const std::wstring& name)
	{
		auto scManagerHandle = OpenSCManagerW(nullptr, SERVICES_ACTIVE_DATABASE, SC_MANAGER_CONNECT);
		if (!scManagerHandle) {
			return false;
		}

		auto scServiceHandle = OpenServiceW(scManagerHandle, name.c_str(), SERVICE_STOP);

		CloseServiceHandle(scManagerHandle);
		if (scServiceHandle) {
			SERVICE_CONTROL_STATUS_REASON_PARAMS controlParams;
			controlParams.dwReason = SERVICE_STOP_REASON_FLAG_PLANNED | SERVICE_STOP_REASON_MAJOR_NONE | SERVICE_STOP_REASON_MINOR_NONE;
			controlParams.pszComment = nullptr;

			auto bRet = ControlServiceExW(scServiceHandle, SERVICE_CONTROL_STOP, SERVICE_CONTROL_STATUS_REASON_INFO, &controlParams);

			CloseServiceHandle(scServiceHandle);
			if (!bRet) {
				auto err = GetLastError();
				auto curState = controlParams.ServiceStatus.dwCurrentState;
				if (err == ERROR_SERVICE_NOT_ACTIVE ||
					err == ERROR_SHUTDOWN_IN_PROGRESS ||
					err == ERROR_SERVICE_CANNOT_ACCEPT_CTRL &&
					(SERVICE_STOPPED == curState || SERVICE_STOP_PENDING == curState)
					) {
					bRet = true;
				}
			}

			return bRet;
		}

		return false;
	}

	//private:
	//	SC_HANDLE hScManager;
};