#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <Windows.h>

#pragma comment(lib, "Winmm")

constexpr uint32_t PIPE_MESSAGE_MAGIC = 0x55AA;

struct pipe_msg {
	uint32_t magic = PIPE_MESSAGE_MAGIC;
	uint32_t size = 0;
};

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

		CloseHandle(m_event);
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
			m_event = nullptr;
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

void do_server_process()
{
	pipe_server server;

	if (!server.init()) {
		std::cerr << "server::init\n";
		return;
	}

	if (!server.start(L"test_pipe")) {
		std::cerr << "server::start\n";
		return;
	}

	if (!server.accept()) {
		std::cerr << "server::accept\n";
		return;
	}

	std::ofstream ofs;
	ofs.open("out.png", std::ios_base::binary);

	while (server.is_connected()) {
		if (!server.signal_client()) {
			std::cerr << "server::signal_client\n";
			break;
		}

		auto start = timeGetTime();// GetTickCount64(); //time(nullptr);
		std::vector<uint8_t> msg;
		if (!server.recv_msg_with_timeout(msg)) {
			std::cerr << "server::recv_msg_with_timeout\n";
			break;
		}
		auto stop = timeGetTime();//  GetTickCount64(); // time(nullptr);

		std::cout << "start: " << start
			<< " stop: " << stop << std::endl;

		ofs.write((char*)msg.data(), msg.size());
		break;
	}

	//server.disconnect();
	//server.stop();
}

void do_client_process()
{
	pipe_client client;

	if (!client.init()) {
		std::cerr << "client::init\n";
		return;
	}

	if (!client.connect(L"test_pipe")) {
		std::cerr << "client::connect\n";
		return;
	}

	if (!client.is_connected()) {
		std::cerr << "client::is_connected\n";
		return;
	}

	std::ifstream ifs;
	ifs.open("in.png", std::ios_base::binary);

	std::vector<char> msg_buf;
	msg_buf.resize(ifs.seekg(0, std::ios_base::end).tellg());
	ifs.seekg(0);
	ifs.read(msg_buf.data(), msg_buf.size());
	ifs.close();

	if (!client.wait_for_signal()) {
		std::cerr << "client::wait_for_signal\n";
		return;
	}

	auto start = timeGetTime();//  GetTickCount64(); //time(nullptr);
	if (!client.send_msg_with_timeout(msg_buf.data(), msg_buf.size())) {
		std::cerr << "client::send_msg_with_timeout\n";
		return;
	}
	auto stop = timeGetTime();//  GetTickCount64(); // time(nullptr);

	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	std::cout << "start: " << start
		<< " stop: " << stop << std::endl;

	//std::cin.get();


	//client.disconnect();
}

int main()
{
	//do_server_process();

	std::thread(do_server_process).detach();

	std::this_thread::sleep_for(std::chrono::seconds(1));

	do_client_process();

	//std::this_thread::sleep_for(std::chrono::seconds(1));

	//std::cin.get();
}
