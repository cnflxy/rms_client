#pragma once

#include <thread>
#include <mutex>
#include <fstream>
#include <string>
#include <vector>
#include <Windows.h>
#include <windowsx.h>
#include <gdiplus.h>
#include "image_helper.h"

#pragma comment(lib, "Gdiplus")

class image_viewer {
public:
	static image_viewer* Create()
	{
		auto img_viewer = new (std::nothrow) image_viewer();
		if (!img_viewer) return nullptr;

		WNDCLASSEXW wcx{};
		wcx.cbSize = sizeof(wcx);
		wcx.style = CS_DBLCLKS;// | CS_HREDRAW | CS_VREDRAW;
		wcx.hInstance = GetModuleHandleW(nullptr);
		wcx.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
		/*
		* LoadImage(hinstance, // small class icon
		MAKEINTRESOURCE(5),
		IMAGE_ICON,
		GetSystemMetrics(SM_CXSMICON),
		GetSystemMetrics(SM_CYSMICON),
		LR_DEFAULTCOLOR);
		*/
		wcx.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);
		wcx.hCursor = LoadCursorW(nullptr, IDC_ARROW);
		wcx.hbrBackground = GetStockBrush(BLACK_BRUSH);
		wcx.lpszClassName = L"ImageViewer_WNDCLASS";
		wcx.lpfnWndProc = viewerWndMsgProc;
		//img_viewer->m_wndClass = RegisterClassExW(&wcx);
		if (RegisterClassExW(&wcx)) {
			Gdiplus::GdiplusStartupInput gdiplusStartupInput;
			//Gdiplus::GdiplusStartupOutput gdiplusStartupOutput;
			auto status = Gdiplus::GdiplusStartup(&img_viewer->m_gdiplusToken, &gdiplusStartupInput, nullptr);
			if (status != Gdiplus::Ok) {
				UnregisterClassW(wcx.lpszClassName, wcx.hInstance);
				delete img_viewer;
				img_viewer = nullptr;
			} else {
				std::thread(wnd_msg_thread, img_viewer/*, mutex*/).detach();
			}
		}

		return img_viewer;
	}

	bool show(const std::string& image_path) const {
		if (can_show()) {
			std::wstring ws(image_path.cbegin(), image_path.cend());
			return show(ws);
			//SendMessage(m_hwnd, WM_USER + 1, reinterpret_cast<WPARAM>(image_path), 0);
			return true;
		} else {
			return false;
		}
	}

	bool show(const std::wstring& image_path) const {
		if (can_show()) {
			auto image = Gdiplus::Bitmap::FromFile(image_path.c_str());
			SendMessage(m_hwnd, WM_USER + 1, reinterpret_cast<WPARAM>(image), 0);
			return true;
		} else {
			return false;
		}
	}

	bool show(const void* data, size_t size) const {
		if (can_show()) {
			LPSTREAM pStream;
			auto hr = CreateStreamOnHGlobal(nullptr, true, &pStream);
			ULONG cbWriten;
			hr = pStream->Write(data, size, &cbWriten);
			auto ret = show(pStream);
			pStream->Release();
			return ret;
		} else {
			return false;
		}
	}

	bool show(IStream* stream) const {
		if (can_show()) {
			auto image = Gdiplus::Bitmap::FromStream(stream);
			SendMessage(m_hwnd, WM_USER + 1, reinterpret_cast<WPARAM>(image), 0);
			return true;
		} else {
			return false;
		}
	}

	bool can_show() const {
		return IsWindow(m_hwnd);
	}

	~image_viewer() {
		if (IsWindow(m_hwnd)) {
			SendMessage(m_hwnd, WM_COMMAND, WM_QUIT, 0);
		}

		//if (m_gdiplusToken) {
		//	Gdiplus::GdiplusShutdown(m_gdiplusToken);
		//}
	}
private:

	image_viewer() = default;

	static void wnd_msg_thread(image_viewer* img_viewer) {
		MSG msg;

		img_viewer->m_hwnd = CreateWindowExW(
			WS_EX_OVERLAPPEDWINDOW,
			L"ImageViewer_WNDCLASS",
			L"Image Viewer",
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			800,
			600,
			HWND_DESKTOP,
			nullptr,
			nullptr,
			nullptr
		);

		if (!img_viewer->m_hwnd) {
			auto err = GetLastError();
			std::cout << err << std::endl;
		} else {
			//img_viewer->m_hwndDlg = CreateDialog(nullptr, MAKEINTRESOURCE(IDD_DIALOG_IMAGE_VIEWER), nullptr, dialogProc); // Modeless

		//SetWindowPos(img_viewer->m_hwndDlg, nullptr, 0, 0, 800, 600, SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);

			while (GetMessage(&msg, img_viewer->m_hwnd, 0, 0) > 0) {
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
				//IsDialogMessage(img_viewer->m_hwnd, &msg);
			}
		}
	}

	static LRESULT CALLBACK viewerWndMsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		static std::wstring dialogTitle;
		static Gdiplus::Image* pImage;
		static Gdiplus::Color fillColor = Gdiplus::Color::Green;

		//static int sizedEdge;
		static WPARAM lastSizedEvent;
		static RECT lastClientRect, currentClientRect;
		static Gdiplus::Rect lastDrawRect;
		static bool zoom_in, last_zoom_in;
		static int xPos, yPos;

		//std::cout << message << std::endl;

		switch (msg) {
			//case WM_INITDIALOG:
			//{

			//	//std::cout << "init dialog: " << status << std::endl;
			//	return FALSE;
			//}
		case WM_CLOSE:
			DestroyWindow(hwnd);
			break;
		case WM_DESTROY:
			if (pImage) {
				delete pImage;
				pImage = nullptr;
			}
			PostQuitMessage(0);
			break;
		//case WM_COMMAND:
		//{
		//	switch (LOWORD(wParam)) {
		//	case IDOK:
		//	case IDCANCEL:
		//		if (pImage) {
		//			delete pImage;
		//			pImage = nullptr;
		//		}
		//		//Gdiplus::GdiplusShutdown(gdiplusToken);
		//		//DestroyWindow(hwndDlg);
		//		ShowWindow(hwnd, SW_HIDE);
		//		//	break;
		//		//default:
		//		//	std::cout << "WM_COMMAND: " << std::hex << message << std::dec << std::endl;
		//		break;
		//	case WM_QUIT:
		//		if (pImage) {
		//			delete pImage;
		//			pImage = nullptr;
		//		}
		//		DestroyWindow(hwnd);
		//	}
		//	return 0;
		//}
		case WM_SIZE:
		{
			//CopyRect(&lastClientRect, &currentClientRect);
			RECT out;
			SetRect(&out, 0, 0, LOWORD(lParam), HIWORD(lParam));
			if (wParam == SIZE_RESTORED) {
				if (lastSizedEvent != SIZE_MINIMIZED) {
					if (SubtractRect(&out, &currentClientRect, &out)/* && !zoom_in*/) {
						RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE);
					}
				}
				//InvalidateRect(hwndDlg, &currentClientRect, FALSE);
				//UpdateWindow(hwndDlg);
			}
			lastSizedEvent = wParam;
			break;
		}
		//case WM_SIZING:
		//{
		//	sizedEdge = wParam;
		//	return TRUE;
		//}
		case WM_PAINT:
		{
			if (!pImage) {
				break;
			}

			Gdiplus::SolidBrush fillBrush(fillColor);

			RECT compaUpdateRect;
			CopyRect(&lastClientRect, &currentClientRect);
			GetClientRect(hwnd, &currentClientRect);
			GetUpdateRect(hwnd, &compaUpdateRect, FALSE);

			Gdiplus::Rect updateRect(compaUpdateRect.left, compaUpdateRect.top, compaUpdateRect.right - compaUpdateRect.left, compaUpdateRect.bottom - compaUpdateRect.top);
			Gdiplus::Graphics g(hwnd);

			auto dst_w = currentClientRect.right - currentClientRect.left;
			auto dst_h = currentClientRect.bottom - currentClientRect.top;
			auto src_w = pImage->GetWidth();
			auto src_h = pImage->GetHeight();
			auto scale = 1.0;

			if (dst_w < src_w || dst_h < src_h) {
				scale = min(static_cast<double>(dst_w) / src_w, static_cast<double>(dst_h) / src_h);
			}

			if (zoom_in) {
				if (lastDrawRect.Contains(xPos, yPos)) {
					//auto last_dst_x = static_cast<INT>((dst_w - src_w * scale) / 2), last_dst_y = static_cast<INT>((dst_h - src_h * scale) / 2);
					//if (xPos >= last_dst_x && yPos >= last_dst_y) {
					auto src_center_x = static_cast<double>(xPos - lastDrawRect.X) * src_w / lastDrawRect.Width;
					auto src_center_y = static_cast<double>(yPos - lastDrawRect.Y) * src_h / lastDrawRect.Height;
					auto dst_half_w = dst_w / 2.0, dst_half_h = dst_h / 2.0;
					auto src_x = 0, src_y = 0, dst_x = 0, dst_y = 0;
					if (src_center_x > dst_half_w) {
						src_x = round(src_center_x - dst_half_w);
					} else if (src_center_x < dst_half_w) {
						dst_x = round(dst_half_w - src_center_x);
					}
					if (src_center_y > dst_half_h) {
						src_y = round(src_center_y - dst_half_h);
					} else if (src_center_y < dst_half_h) {
						dst_y = round(dst_half_h - src_center_y);
					}
					if (src_w > dst_w - dst_x) {
						src_w = dst_w - dst_x;
					}
					if (src_h > dst_h - dst_y) {
						src_h = dst_h - dst_y;
					}

					g.Clear(fillColor);
					lastDrawRect = Gdiplus::Rect(dst_x, dst_y, src_w, src_h);
					g.DrawImage(pImage, lastDrawRect, src_x, src_y, src_w, src_h, Gdiplus::Unit::UnitPixel);
				} else {
					zoom_in = false;
				}
			} else if (!lastDrawRect.IsEmptyArea() && EqualRect(&currentClientRect, &lastClientRect) && !updateRect.IsEmptyArea()) {
				Gdiplus::Rect intersectRect;
				if (!Gdiplus::Rect::Intersect(intersectRect, lastDrawRect, updateRect)) {
					g.FillRectangle(&fillBrush, updateRect);
				} else {
					if (intersectRect.Equals(lastDrawRect)) {
						g.DrawImage(pImage, lastDrawRect);
					} else {
						auto src_x = round(static_cast<double>(intersectRect.X - lastDrawRect.X) * src_w / lastDrawRect.Width),
							src_y = round(static_cast<double>(intersectRect.Y - lastDrawRect.Y) * src_h / lastDrawRect.Height);
						src_w *= static_cast<double>(intersectRect.Width) / lastDrawRect.Width;
						src_h *= static_cast<double>(intersectRect.Height) / lastDrawRect.Height;
						g.DrawImage(pImage, intersectRect, src_x, src_y, src_w, src_h, Gdiplus::Unit::UnitPixel);
						//std::cout << "need update rect\n";
					}
				}
			} else {
				src_w *= scale;
				src_h *= scale;

				//
				//lastDrawRect = Gdiplus::Rect((dst_w - src_w) / 2, (dst_h - src_h) / 2, src_w, src_h);
				auto currentDrawRect = Gdiplus::Rect((dst_w - src_w) / 2, (dst_h - src_h) / 2, src_w, src_h);
				if (!lastDrawRect.IsEmptyArea() && !currentDrawRect.Contains(lastDrawRect)) {
					Gdiplus::Rect intersectRect;
					if (!Gdiplus::Rect::Intersect(intersectRect, lastDrawRect, currentDrawRect)) {
						g.Clear(fillColor);
					} else {
						if (lastDrawRect.Y < intersectRect.Y) {
							g.FillRectangle(&fillBrush, lastDrawRect.X, lastDrawRect.Y, lastDrawRect.Width, intersectRect.Y - lastDrawRect.Y);
						}
						if (lastDrawRect.GetBottom() > intersectRect.GetBottom()) {
							auto h = lastDrawRect.GetBottom() - intersectRect.GetBottom();
							g.FillRectangle(&fillBrush, lastDrawRect.X, intersectRect.GetBottom(), lastDrawRect.Width, h);
						}
						if (lastDrawRect.X < intersectRect.X) {
							g.FillRectangle(&fillBrush, lastDrawRect.X, intersectRect.Y, intersectRect.X - lastDrawRect.X, intersectRect.Height);
						}
						if (lastDrawRect.GetRight() > intersectRect.GetRight()) {
							g.FillRectangle(&fillBrush, intersectRect.GetRight(), intersectRect.Y, lastDrawRect.GetRight() - intersectRect.GetRight(), intersectRect.Height);
						}
					}
				} else {
					//g.Clear(Gdiplus::Color::Black);
				}
				lastDrawRect = currentDrawRect;
				g.DrawImage(pImage, currentDrawRect);
			}
			//EndPaint(hwndDlg, &ps);
			break;
		}
		case WM_LBUTTONDBLCLK:
		{
			xPos = GET_X_LPARAM(lParam);
			yPos = GET_Y_LPARAM(lParam);
			zoom_in = !(last_zoom_in = zoom_in);
			//RedrawWindow(hwnd, nullptr, nullptr, RDW_INTERNALPAINT);
			//RedrawWindow(hwnd, nullptr, nullptr, RDW_INTERNALPAINT);
			UpdateWindow(hwnd);
			break;
		}
		case WM_MOUSEMOVE:
		{
			SetWindowTextW(hwnd, (dialogTitle + std::wstring(L" - ").append(std::to_wstring(LOWORD(lParam))).append(L"x").append(std::to_wstring(HIWORD(lParam)))).c_str());
			break;
		}
		case WM_USER + 1:
		{
			if (pImage) {
				delete pImage;
				lastDrawRect = Gdiplus::Rect();
				zoom_in = false;
			} else {
				ShowWindow(hwnd, SW_SHOW);
			}

			pImage = reinterpret_cast<Gdiplus::Bitmap*>(wParam);
			//std::ifstream ifs;
			//const char* image_path;
			//const wchar_t 
			//if (lParam == 0) ifs.open(reinterpret_cast<char*>(wParam), std::ios_base::binary); //image_path = reinterpret_cast<char*>(wParam);
			//else if (lParam == 1) ifs.open(reinterpret_cast<wchar_t*>(wParam), std::ios_base::binary);
			//auto hImageFile = CreateFileA(image_path->c_str(), FILE_READ_DATA, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
			//if (hImageFile == INVALID_HANDLE_VALUE) {
				//std::cerr << GetLastError() << std::endl;
			//}
			//auto len = GetFileSize(hImageFile, nullptr);

			//ifs.open(image_path, std::ios_base::binary);
			//ifs.seekg(0, std::ios_base::end);
			//auto len = ifs.tellg();
			////std::cout << "image_data: " << *image_path << ", len: " << len << std::endl;
			//ifs.seekg(0, std::ios_base::beg);
			//std::vector<std::string::value_type> image_data;
			//image_data.resize(len);
			////ReadFile(hImageFile, image_data.data(), len, nullptr, nullptr);
			//ifs.read(image_data.data(), image_data.size());
			//ifs.close();
			//CloseHandle(hImageFile);

			//LPSTREAM pStream;
			//auto hr = CreateStreamOnHGlobal(nullptr, true, &pStream);
			//ULONG cbWriten;
			//hr = pStream->Write(image_data.data(), image_data.size(), &cbWriten);
			////hr = pStream->Seek({}, STREAM_SEEK_SET, nullptr);

			//if (pImage) {
			//	delete pImage;
			//	lastDrawRect = Gdiplus::Rect();
			//	zoom_in = false;
			//} else {
			//	ShowWindow(hwnd, SW_SHOW);
			//	SetForegroundWindow(hwnd);
			//}
			//pImage = Gdiplus::Bitmap::FromStream(pStream);
			//pStream->Release();

			//print_image_info(pImage);

			if (lastDrawRect.IsEmptyArea()) {
				Gdiplus::Graphics g(hwnd);
				g.Clear(fillColor);
			}
			RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE);

			dialogTitle = std::wstring(L"Image Viewer - ").append(std::to_wstring(pImage->GetWidth())).append(L"x").append(std::to_wstring(pImage->GetHeight()));
			SetWindowTextW(hwnd, dialogTitle.c_str());
			break;
		}
		//break;
		default:
			//std::cout << std::hex << message << std::dec << std::endl;
			return DefWindowProcW(hwnd, msg, wParam, lParam);
		}

		return 0;
		//return TRUE;
	};

	ATOM m_wndClass = 0;
	HWND m_hwnd = nullptr;
	ULONG_PTR m_gdiplusToken = 0;
};
