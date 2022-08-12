#pragma once

#include <string>
#include <Windows.h>

class pipe_client {
public:
	pipe_client() = default;

	pipe_client(pipe_client&& rval) noexcept
	{
		m_pipe = rval.m_pipe;
		rval.m_pipe = nullptr;
	}

	pipe_client(const pipe_client&) = delete;

	~pipe_client()
	{
		disconnect();
	}

	pipe_client& operator=(const pipe_client&) = delete;
	pipe_client& operator=(pipe_client&& rval) noexcept
	{
		m_pipe = rval.m_pipe;
		rval.m_pipe = nullptr;
		return *this;
	}

	bool connect(const std::wstring& name)
	{
		if (m_pipe) return true;

		m_pipe = CreateFileW(
			(L"\\\\.\\pipe\\" + name).c_str(),
			GENERIC_READ | GENERIC_WRITE,
			0,
			nullptr,
			OPEN_EXISTING,
			0,
			nullptr
		);
		if (m_pipe == INVALID_HANDLE_VALUE) {
			m_pipe = nullptr;
			return false;
		}

		DWORD mode = PIPE_READMODE_MESSAGE;
		if (!SetNamedPipeHandleState(
			m_pipe,
			&mode,
			nullptr,
			nullptr
		)) {
			disconnect();
			return false;
		}

		m_name = L"\\\\.\\pipe\\" + name;

		return true;
	}

	void disconnect()
	{
		if (m_pipe) {
			FlushFileBuffers(m_pipe);
			DisconnectNamedPipe(m_pipe);
			CloseHandle(m_pipe);
			m_pipe = nullptr;
		}
	}

	bool is_connected()
	{
		if (!WaitNamedPipeW(m_name.c_str(), 100) && GetLastError() != ERROR_SEM_TIMEOUT) {
			disconnect();
		}

		return m_pipe;
	}

	bool send_msg(const void* data, const uint32_t& size)
	{
		if (!m_pipe) return false;

		DWORD cRet;
		auto bRet = WriteFile(m_pipe, &size, sizeof(size), &cRet, nullptr);
		if (bRet && cRet == sizeof(size)) {
			FlushFileBuffers(m_pipe);
			if (!data || !size) return true;
			if ((bRet = WriteFile(m_pipe, data, size, &cRet, nullptr)) && cRet == size) {
				FlushFileBuffers(m_pipe);
				return true;
			}
		}

		disconnect();
		return false;
	}

	bool wait_for_signal()
	{
		if (!m_pipe) return false;

		uint32_t magic;
		DWORD cRet;
		auto bRet = ReadFile(m_pipe, &magic, sizeof(magic), &cRet, nullptr);
		if (!bRet || cRet != sizeof(magic) || magic != 0x55AA) {
			disconnect();
			return false;
		}

		return true;
	}

private:
	HANDLE m_pipe = nullptr;
	std::wstring m_name;
};
