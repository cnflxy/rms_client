#pragma once

#include "pipe_message.h"
#include <string>
#include <vector>
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

		if (m_event) {
			CloseHandle(m_event);
		}
	}

	pipe_server& operator=(const pipe_server&) = delete;
	pipe_server& operator=(pipe_server&& rval) noexcept
	{
		m_pipe = rval.m_pipe;
		rval.m_pipe = nullptr;
		return *this;
	}

	bool init()
	{
		if (!m_event) {
			m_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
		}

		m_overlap.hEvent = m_event;

		return m_event;
	}

	bool start(const std::wstring& name)
	{
		if (m_pipe) {
			if (name == m_pipeName) return true;
			stop();
		}

		m_pipe = CreateNamedPipeW(
			(L"\\\\.\\pipe\\" + name).c_str(),
			PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED,
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
			1,
			0,
			0,
			3000,
			nullptr
		);
		if (m_pipe == INVALID_HANDLE_VALUE) {
			m_pipe = nullptr;
			return false;
		}

		m_pipeName = name;

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
			DisconnectNamedPipe(m_pipe);
			m_connected = false;
		}
	}

	bool accept(DWORD timeoutMillseconds = 3000)
	{
		if (!m_pipe) return false;
		if (m_connected) return true;

		auto bRet = ConnectNamedPipe(m_pipe, &m_overlap);
		if (bRet) return false;

		auto err = GetLastError();
		if (err == ERROR_PIPE_CONNECTED) {
			return m_connected = true;
		} else if (err != ERROR_IO_PENDING) {
			return false;
		}

		auto ret = WaitForSingleObject(m_event, timeoutMillseconds);
		if (ret != WAIT_OBJECT_0 && !HasOverlappedIoCompleted(&m_overlap)) {
			CancelIo(m_pipe);
			return false;
		}

		//HasOverlappedIoCompleted(&overlap);

		DWORD dummy;
		bRet = GetOverlappedResult(m_pipe, &m_overlap, &dummy, FALSE);
		if (!bRet) return false;

		return m_connected = true;
	}

	bool is_connected()
	{
		if (m_connected) {
			const auto pipeFullPath = L"\\\\.\\pipe\\" + m_pipeName;

			if (WaitNamedPipeW(pipeFullPath.c_str(), 1) || GetLastError() != ERROR_SEM_TIMEOUT) {
				disconnect();
			}

			//ULONG clntProcessId;
			//if (!GetNamedPipeClientProcessId(m_pipe, &clntProcessId) || !clntProcessId) {
			//	disconnect();
			//}
		}

		return m_connected;
	}

	bool signal_client()
	{
		if (!m_pipe || !m_connected) return false;

		pipe_msg msg_hdr;
		OVERLAPPED overlap{};
		overlap.hEvent = m_event;
		DWORD cRet;
		auto bRet = WriteFile(m_pipe, &msg_hdr, sizeof(msg_hdr), &cRet, &overlap);
		if (!bRet) {
			auto err = GetLastError();
			if (err == ERROR_IO_PENDING) {
				bRet = GetOverlappedResult(m_pipe, &overlap, &cRet, TRUE);
			}
		}

		if (bRet && cRet == sizeof(msg_hdr)) {
			return true;
		}

		disconnect();
		return false;
	}

	bool recv_msg_with_timeout(std::vector<uint8_t>& buf, DWORD timeout = 3000)
	{
		pipe_msg msg_hdr;

		auto bRet = recv_with_timeout(&msg_hdr, sizeof(msg_hdr), timeout);

		if (!bRet || msg_hdr.magic != PIPE_MESSAGE_MAGIC) {
			goto fail;
		}

		buf.resize(msg_hdr.size);

		if (msg_hdr.size) {
			bRet = recv_with_timeout(buf.data(), msg_hdr.size, timeout);
			if (!bRet) {
				goto fail;
			}
		}

		return true;

	fail:
		buf.clear();
		disconnect();
		return false;
	}

private:

	bool recv_with_timeout(void* buf, const uint32_t& size, DWORD timeout = 3000)
	{
		DWORD cRet;
		auto bRet = ReadFile(m_pipe, buf, size, &cRet, &m_overlap);
		if (!bRet) {
			auto err = GetLastError();
			if (err != ERROR_IO_PENDING) {
				return false;
			}

			auto ret = WaitForSingleObject(m_event, timeout);
			if (ret != WAIT_OBJECT_0 && !HasOverlappedIoCompleted(&m_overlap)) {
				CancelIo(m_pipe);
				return false;
			}

			bRet = GetOverlappedResult(m_pipe, &m_overlap, &cRet, FALSE);
		}

		if (bRet && cRet == size) {
			return true;
		}

		return false;
	}

private:
	HANDLE m_pipe = nullptr;
	HANDLE m_event = nullptr;
	OVERLAPPED m_overlap{};
	std::wstring m_pipeName;
	bool m_connected = false;
};