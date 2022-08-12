#pragma once

#include <thread>
#include <mutex>
#include <string>
#include <vector>
#include <unordered_set>
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
//#include <Windows.h>
#include <iphlpapi.h>
#include "utility.h"

#pragma comment(lib, "Ws2_32")
#pragma comment(lib, "Iphlpapi")

#pragma pack(push, 1)
struct host_probe_msg {
public:
	static host_probe_msg* Create()
	{
		auto msg = new (std::nothrow) host_probe_msg();
		if (!msg) return nullptr;

		msg->crc = utility::crc32(reinterpret_cast<uint8_t*>(msg), FIELD_OFFSET(host_probe_msg, crc));

		return msg;
	}

	static bool verify_msg(const char* raw_msg)
	{
		auto msg = reinterpret_cast<const host_probe_msg*>(raw_msg);
		//auto _magic = *reinterpret_cast<const decltype(magic)*>(msg);
		//auto _size = *reinterpret_cast<const decltype(size)*>(msg + FIELD_OFFSET(host_probe_msg, size));
		//auto _crc = *reinterpret_cast<const decltype(crc)*>(msg + FIELD_OFFSET(host_probe_msg, crc));
		auto _real_crc = utility::crc32(reinterpret_cast<const uint8_t*>(msg), FIELD_OFFSET(host_probe_msg, crc));

		if (msg->magic != 0xAA55 || msg->size != sizeof(guid) + sizeof(crc) || msg->crc != _real_crc) {
			return false;
		}

		//if (_magic != 0xAA55 || _size != sizeof(crc) + sizeof(guid) || _crc != _real_crc) {
		//	return false;
		//}

		return true;
	}

	//bool operator != (const host_probe_msg* other) {
	//	return magic != other->magic || size != other->size || guid != other->guid;
	//}

	static constexpr uint32_t MSG_SIZE = 2 + 1 + 4 + 16;

private:
	host_probe_msg() = default;

	uint16_t magic = 0xAA55;
	uint8_t size = sizeof(guid) + sizeof(crc);
	// {3B3CD10D-C506-4DF8-A257-3F09C9339485}
	GUID guid = { 0x3b3cd10d, 0xc506, 0x4df8, { 0xa2, 0x57, 0x3f, 0x9, 0xc9, 0x33, 0x94, 0x85 } };
	uint32_t crc = 0;
};
#pragma pack(pop)

template <>
struct std::hash<in_addr> {
	_NODISCARD size_t operator()(const in_addr& _Keyval) const noexcept {
		return _Hash_representation(_Keyval.s_addr);
	}
};

bool operator== (const in_addr& l, const in_addr& r) {
	return l.s_addr == r.s_addr;
}

class host_probe_client {
public:
	~host_probe_client() {
		closesocket(m_sock);

		WSACleanup();
	}

	static host_probe_client* Create(const USHORT& port) {
		WSADATA wsaData;

		auto ret = WSAStartup(WINSOCK_VERSION, &wsaData);
		if (ret) {
			return nullptr;
		}

		auto client = new (std::nothrow) host_probe_client();
		if (!client) return nullptr;

		client->m_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (client->m_sock == INVALID_SOCKET) {
			return nullptr;
		}

		BOOL en = TRUE;
		ret = setsockopt(client->m_sock, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<PCHAR>(&en), sizeof(en));
		if (ret == SOCKET_ERROR) {
			closesocket(client->m_sock);
			delete client;
			return nullptr;
		}

		client->m_port = port;

		return client;
	}

	void send_probe_msg(const host_probe_msg* msg, bool force_update = false) {
		if (m_AdaptersBroadcastAddress.empty() || force_update) {
			if (!update_adapters_broadcast_address()) return;
		}

		sockaddr_in to;
		to.sin_family = AF_INET;
		to.sin_port = htons(m_port);
		for (const auto& bcast : m_AdaptersBroadcastAddress) {
			to.sin_addr.s_addr = bcast;
			int ret = sendto(m_sock, reinterpret_cast<const char*>(msg), sizeof(*msg), 0, reinterpret_cast<sockaddr*>(&to), sizeof(to));
			WCHAR ipString[3 + 1 + 3 + 1 + 3 + 1 + 3 + 1];
			utility::print_debug_msg(L"send_probe_msg", { InetNtopW(AF_INET, &to.sin_addr, ipString, 16), (ret == sizeof(*msg) ? L"ok" : std::to_wstring(ret)) });
		}
	}

private:
	host_probe_client() = default;

	bool update_adapters_broadcast_address() {
		PIP_ADAPTER_INFO AdaptersInfo;
		ULONG AdatersInfoSize = 0;

		auto result = GetAdaptersInfo(nullptr, &AdatersInfoSize);
		do {
			if (ERROR_BUFFER_OVERFLOW != result) break;
			AdaptersInfo = reinterpret_cast<PIP_ADAPTER_INFO>(new (std::nothrow) char[AdatersInfoSize]);
			if (!AdaptersInfo) break;
			result = GetAdaptersInfo(AdaptersInfo, &AdatersInfoSize);
			if (ERROR_SUCCESS == result) {
				for (auto Adapter = AdaptersInfo; Adapter; Adapter = Adapter->Next) {
					//auto ip = inet_addr(Adapter->IpAddressList.IpAddress.String);
					in_addr ip;
					inet_pton(AF_INET, Adapter->IpAddressList.IpAddress.String, &ip);
					if (ip.s_addr) {
						//auto ip_mask = inet_addr(Adapter->IpAddressList.IpMask.String);
						in_addr ip_mask;
						inet_pton(AF_INET, Adapter->IpAddressList.IpAddress.String, &ip_mask);
						auto bcast = ip.s_addr & ip_mask.s_addr | ~ip_mask.s_addr;
						m_AdaptersBroadcastAddress.push_back(bcast);
					}
				}
			}

			delete[]AdaptersInfo;
		} while (false);

		return !m_AdaptersBroadcastAddress.empty();
	}

	SOCKET m_sock;
	USHORT m_port;
	std::vector<uint32_t> m_AdaptersBroadcastAddress;
};

class host_probe_server {
public:
	~host_probe_server() {
		WSACloseEvent(m_RecvFromEvent);
		closesocket(m_sock);

		WSACleanup();
	}

	static host_probe_server* Create(const USHORT& port) {
		WSADATA wsaData;

		if (!utility::allow_inbound_on_port(port, L"RMS主机发现")) {
			return nullptr;
		}

		auto ret = WSAStartup(WINSOCK_VERSION, &wsaData);
		if (ret) {
			return nullptr;
		}

		auto server = new (std::nothrow) host_probe_server();
		if (!server) return nullptr;

		server->m_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (server->m_sock == INVALID_SOCKET) {
			delete server;
			return nullptr;
		}

		BOOL en = TRUE;
		ret = setsockopt(server->m_sock, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<PCHAR>(&en), sizeof(en));
		if (ret == SOCKET_ERROR) {
			delete server;
			return nullptr;
		}

		sockaddr_in srv_addr;
		srv_addr.sin_family = AF_INET;
		srv_addr.sin_port = htons(port);
		srv_addr.sin_addr.s_addr = INADDR_ANY;
		ret = bind(server->m_sock, reinterpret_cast<sockaddr*>(&srv_addr), sizeof(srv_addr));
		if (ret == SOCKET_ERROR) {
			delete server;
			return nullptr;
		}

		server->m_RecvFromEvent = WSACreateEvent();
		if (server->m_RecvFromEvent == WSA_INVALID_EVENT) {
			delete server;
			return nullptr;
		}

		ret = WSAEventSelect(server->m_sock, server->m_RecvFromEvent, FD_READ);
		if (ret == SOCKET_ERROR) {
			delete server;
			return nullptr;
		}

		std::thread(recvfrom_event_feed_routine, server).detach();

		return server;
	}

	bool get_all_valid_client(std::vector<in_addr>& clients) {
		if (!m_validClient.empty()) {
			std::lock_guard<std::mutex> lg(m_mutex);
			//std::move(m_validClient.cbegin(), m_validClient.cend(), clients.begin());

			clients.assign(m_validClient.cbegin(), m_validClient.cend());
			m_validClient.clear();
			return true;
		} else {
			clients.clear();
			return false;
		}
	}

private:
	host_probe_server() = default;

	static void recvfrom_event_feed_routine(host_probe_server* server)
	{
		for (;;) {
			auto status = WSAWaitForMultipleEvents(1, &server->m_RecvFromEvent, FALSE, 1000, FALSE);
			if (status == WSA_WAIT_TIMEOUT) {
				continue;
			}
			if (status == WSA_WAIT_FAILED) {
				break;
			}

			if (status != WSA_WAIT_EVENT_0) {
				std::cerr << "wrong status: " << status << std::endl;
				break;
			}

			WSANETWORKEVENTS events;
			auto ret = WSAEnumNetworkEvents(server->m_sock, server->m_RecvFromEvent, &events);
			if (ret == SOCKET_ERROR) {
				break;
			}

			if (!(events.lNetworkEvents & FD_READ)) {
				std::cerr << "wrong events returned: " << events.lNetworkEvents << std::endl;
				continue;
			}

			if (events.iErrorCode[FD_READ_BIT]) {
				std::cerr << "error code: " << events.iErrorCode[FD_READ_BIT] << std::endl;
				continue;
			}

			char buf[host_probe_msg::MSG_SIZE];
			sockaddr_in from;
			int addrlen = sizeof(from);
			ret = recvfrom(server->m_sock, buf, host_probe_msg::MSG_SIZE, 0, reinterpret_cast<sockaddr*>(&from), &addrlen);
			if (ret != host_probe_msg::MSG_SIZE) continue;
			if (from.sin_family != AF_INET) continue;
			if (!host_probe_msg::verify_msg(buf)) continue;
			server->m_mutex.lock();
			server->m_validClient.emplace(from.sin_addr);
			server->m_mutex.unlock();
		}

		std::cout << "bp!\n";
	}

	SOCKET m_sock = INVALID_SOCKET;
	HANDLE m_RecvFromEvent = WSA_INVALID_EVENT;
	std::unordered_set<in_addr> m_validClient;
	std::mutex m_mutex;
};
