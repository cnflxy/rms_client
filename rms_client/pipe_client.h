#pragma once

#include "pipe_message.h"
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

		if (m_event) {
			CloseHandle(m_event);
		}
	}

	pipe_client& operator=(const pipe_client&) = delete;
	pipe_client& operator=(pipe_client&& rval) noexcept
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

	bool connect(const std::wstring& name)
	{
		if (m_pipe) {
			if (name == m_pipeName) return true;
			disconnect();
		}

		const auto pipeFullPath = L"\\\\.\\pipe\\" + name;

		m_pipe = CreateFileW(
			pipeFullPath.c_str(),
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			nullptr,
			OPEN_EXISTING,
			FILE_FLAG_OVERLAPPED,
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

		m_pipeName = name;

		return true;
	}

	void disconnect()
	{
		if (m_pipe) {
			DisconnectNamedPipe(m_pipe);
			CloseHandle(m_pipe);
			m_pipe = nullptr;
		}
	}

	bool is_connected()
	{
		if (m_pipe) {
			const auto pipeFullPath = L"\\\\.\\pipe\\" + m_pipeName;

			if (WaitNamedPipeW(pipeFullPath.c_str(), 1) || GetLastError() != ERROR_SEM_TIMEOUT) {
				disconnect();
			}
		}

		return m_pipe;
	}

	bool send_msg_with_timeout(const void* data, const uint32_t& size, DWORD timeout = 3000)
	{
		if (!m_pipe) return false;

		pipe_msg msg_hdr;
		msg_hdr.size = size;

		auto bRet = send_with_timeout(&msg_hdr, sizeof(msg_hdr), timeout);
		if (!bRet) goto fail;

		if (!data || !size) return true;

		bRet = send_with_timeout(data, size, timeout);
		if (!bRet) goto fail;

		return true;

	fail:
		disconnect();
		return false;
	}

	bool wait_for_signal()
	{
		if (!m_pipe) return false;

		pipe_msg msg_hdr;
		DWORD cRet;
		auto bRet = ReadFile(m_pipe, &msg_hdr, sizeof(msg_hdr), &cRet, &m_overlap);
		if (!bRet) {
			auto err = GetLastError();
			if (err == ERROR_IO_PENDING) {
				bRet = GetOverlappedResult(m_pipe, &m_overlap, &cRet, TRUE);
			}
		}

		if (bRet && cRet == sizeof(msg_hdr) && msg_hdr.magic == PIPE_MESSAGE_MAGIC && !msg_hdr.size) {
			return true;
		}

		disconnect();
		return false;
	}

private:
	bool send_with_timeout(const void* data, const uint32_t& size, DWORD timeout = 3000)
	{
		DWORD cRet;
		auto bRet = WriteFile(m_pipe, data, size, &cRet, &m_overlap);
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
};