#pragma once

#include <string>
#include "rms_user.h"

class rms_client {
public:
	static rms_client* Create(const std::wstring& device_path)
	{
		auto clnt = new (std::nothrow) rms_client();
		if (clnt) {
			clnt->m_rmsDevice = CreateFile(device_path.c_str(), FILE_ANY_ACCESS, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (clnt->m_rmsDevice == INVALID_HANDLE_VALUE) {
				clnt->m_rmsDevice = nullptr;
				delete clnt;
				clnt = nullptr;
			}
		}

		return clnt;
	}

	~rms_client()
	{
		if (m_connected) {
			disconnect();
		}
		if (m_rmsDevice) {
			CloseHandle(m_rmsDevice);
		}
	}

	bool connect(sockaddr_in* remote)
	{
		if (m_connected) return false;

		DWORD dummyLen;
		m_connected = DeviceIoControl(m_rmsDevice, RMS_IOCTL_CONNECT, remote, sizeof(*remote), nullptr, 0, &dummyLen, nullptr);

		return m_connected;
	}

	void disconnect()
	{
		if (!m_connected) return;

		DWORD dummyLen;
		DeviceIoControl(m_rmsDevice, RMS_IOCTL_DISCONNECT, nullptr, 0, nullptr, 0, &dummyLen, nullptr);
		m_connected = false;
	}

	size_t get(void* buf, const size_t& size)
	{
		if (!m_connected) return 0;

		DWORD readLen;
		auto bRet = DeviceIoControl(m_rmsDevice, RMS_IOCTL_RECV, nullptr, 0, buf, size, &readLen, nullptr);
		if (!bRet && GetLastError() == ERROR_CONNECTION_INVALID) {
			m_connected = false;
		}
		return readLen;
	}

	bool send(const void* data, const size_t& size)
	{
		if (!m_connected) return false;

		DWORD dummyLen;
		auto bRet = DeviceIoControl(m_rmsDevice, RMS_IOCTL_SEND, nullptr, 0, const_cast<void*>(data), size, &dummyLen, nullptr);
		if (!bRet && GetLastError() == ERROR_CONNECTION_INVALID) {
			m_connected = false;
		}
		return bRet;
	}

	bool is_connected()
	{
		if (m_connected) {
			DWORD dummyLen;
			m_connected = DeviceIoControl(m_rmsDevice, RMS_IOCTL_CONNECTED, nullptr, 0, nullptr, 0, &dummyLen, nullptr);
		}

		return m_connected;
	}

	uint64_t received_size()
	{
		if (!m_connected) return 0;

		DWORD dummyLen;
		uint64_t received;
		auto bRet = DeviceIoControl(m_rmsDevice, RMS_IOCTL_RECEIVED, nullptr, 0, &received, sizeof(received), &dummyLen, nullptr);
		if (!bRet) {
			if (GetLastError() == ERROR_CONNECTION_INVALID) {
				m_connected = false;
			}
			received = 0;
		}

		return received;
	}

private:
	rms_client() : m_rmsDevice(), m_connected() {}

	HANDLE m_rmsDevice;
	bool m_connected;
};
