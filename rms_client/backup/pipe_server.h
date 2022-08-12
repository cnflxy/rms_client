#pragma once

#include <vector>
#include <string>
#include <Windows.h>

class pipe_server {
public:
	pipe_server() = default;

	pipe_server(pipe_server&& rval) noexcept
	{
		m_pipe = rval.m_pipe;
		rval.m_pipe = nullptr;
	}

	pipe_server(const pipe_server&) = delete;

	~pipe_server()
	{
		stop();
	}

	pipe_server& operator=(const pipe_server&) = delete;
	pipe_server& operator=(pipe_server&& rval) noexcept
	{
		m_pipe = rval.m_pipe;
		rval.m_pipe = nullptr;
		return *this;
	}

	bool start(const std::wstring& name)
	{
		if (!m_pipe) {
			m_pipe = CreateNamedPipeW(
				(L"\\\\.\\pipe\\" + name).c_str(),
				PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,	// sync
				PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
				PIPE_UNLIMITED_INSTANCES,
				4096,
				4,
				3000,
				nullptr
			);
			if (m_pipe == INVALID_HANDLE_VALUE) {
				m_pipe = nullptr;
				return false;
			}
		}

		return true;
	}

	void stop()
	{
		disconnect();

		if (m_pipe) {
			CloseHandle(m_pipe);
			m_pipe = nullptr;
		}
	}

	void disconnect()
	{
		if (m_connected) {
			FlushFileBuffers(m_pipe);
			DisconnectNamedPipe(m_pipe);
			//CloseHandle(m_pipe);
			m_connected = false;
		}
	}

	bool wait_for_connect()
	{
		if (!m_pipe) return false;
		if (!m_connected) {
			m_connected = ConnectNamedPipe(m_pipe, nullptr) ? true : (GetLastError() == ERROR_PIPE_CONNECTED);
		}

		return m_connected;
	}

	bool is_connected() const
	{
		return m_connected;
	}

	bool signal_client()
	{
		if (!m_pipe || !m_connected) return false;

		uint32_t magic = 0x55AA;
		DWORD cRet;
		auto bRet = WriteFile(m_pipe, &magic, sizeof(magic), &cRet, nullptr);
		if (!bRet || cRet != sizeof(magic)) {
			disconnect();
			return false;
		}

		FlushFileBuffers(m_pipe);

		return true;
	}

	bool recv_msg(std::vector<uint8_t>& buf)
	{
		buf.clear();

		uint32_t size;
		DWORD cRet;
		auto bRet = ReadFile(m_pipe, &size, sizeof(size), &cRet, nullptr);
		if (!bRet || cRet != sizeof(size)) {
			disconnect();
			return false;
		}

		if (size) {
			buf.resize(size);

			bRet = ReadFile(m_pipe, buf.data(), size, &cRet, nullptr);
			if (!bRet || cRet != size) {
				disconnect();
				buf.clear();
				return false;
			}
		}

		return true;
	}

private:
	HANDLE m_pipe = nullptr;
	bool m_connected = false;
};
