#pragma once

#include <mutex>
#include <Windows.h>
#include "image_transformer.h"

class monitor {
public:
	monitor(const DISPLAY_DEVICE& device) : m_deviceName(device.DeviceName), m_deviceDescription(device.DeviceString), m_state(device.StateFlags)
	{}

	const std::wstring& get_name() const {
		return m_deviceName;
	}

	const std::wstring& get_desciption() const {
		return m_deviceDescription;
	}

	bool is_active() const {
		return m_state & DISPLAY_DEVICE_ACTIVE;
	}

	bool is_primary() const {
		return m_state & DISPLAY_DEVICE_PRIMARY_DEVICE;
	}

private:
	std::wstring m_deviceName;
	std::wstring m_deviceDescription;
	DWORD m_state;
};

class monitor_capturer : public image_transformer {
public:
	monitor_capturer() = default;

	~monitor_capturer() = default;

	const std::vector<monitor>& get_monitors(const bool& force_update = false) {
		if (m_monitors.empty() || force_update) {
			std::lock_guard<std::mutex> lg(m_mutex);
			update_monitors_list();
		}
		return m_monitors;
	}

	bool capture(const monitor& mntr, std::vector<uint8_t>& image_buffer) {
		HDC hdc;
		HBITMAP hbm;

		if (!capture(mntr, hdc, hbm)) return false;

		auto ret = image_transformer::set_input(hbm, nullptr);

		if (ret) {
			ret = image_transformer::output(image_buffer);
		}

		DeleteDC(hdc);
		DeleteObject(hbm);

		return ret;
	}

private:
	bool capture(const monitor& mntr, HDC& hdc, HBITMAP& hbm)
	{
		DEVMODEW dm;
		dm.dmSize = sizeof(DEVMODEW);
		dm.dmDriverExtra = 0;
		auto ret = EnumDisplaySettingsW(mntr.get_name().c_str(), ENUM_CURRENT_SETTINGS, &dm);
		if (!ret) return false;
		//std::cout << dm.dmPelsWidth << "x" << dm.dmPelsHeight << "x" << dm.dmBitsPerPel << std::endl;

		HDC hdcDisplay = nullptr, hdcMem = nullptr;
		HBITMAP hbmMem = nullptr, hbmOld = nullptr;

		hdcDisplay = CreateDCW(mntr.get_name().c_str(), nullptr, nullptr, nullptr);
		if (!hdcDisplay) goto fail;
		hdcMem = CreateCompatibleDC(hdcDisplay);
		if (!hdcMem) goto fail;

		hbmMem = CreateCompatibleBitmap(hdcDisplay, dm.dmPelsWidth, dm.dmPelsHeight);
		if (!hbmMem) goto fail;

		hbmOld = static_cast<HBITMAP>(SelectObject(hdcMem, hbmMem));
		DeleteObject(hbmOld);
		ret = BitBlt(hdcMem, 0, 0, dm.dmPelsWidth, dm.dmPelsHeight, hdcDisplay, 0, 0, SRCCOPY);
		if (!ret) goto fail;
		DeleteDC(hdcDisplay);

		hdc = hdcMem;
		hbm = hbmMem;

		return true;

	fail:
		if (hdcDisplay) {
			DeleteDC(hdcDisplay);
		}
		if (hdcMem) {
			DeleteDC(hdcMem);
		}
		if (hbmMem) {
			DeleteObject(hbmMem);
		}

		return false;
	}

	void update_monitors_list() {
		DISPLAY_DEVICEW displayDevice;

		displayDevice.cb = sizeof(DISPLAY_DEVICEW);
		m_monitors.clear();

		for (DWORD i = 0; EnumDisplayDevicesW(nullptr, i, &displayDevice, EDD_GET_DEVICE_INTERFACE_NAME); ++i) {
			m_monitors.emplace_back(displayDevice);
		}
	}

	bool set_input() { return false; }
	bool output() { return false; }

private:
	std::vector<monitor> m_monitors;
	std::mutex m_mutex;
};
