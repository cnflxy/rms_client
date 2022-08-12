#include <iostream>
#include <locale>
#include <ctime>
#include "mf_source.h"
#include "mf_source_reader.h"
#include "image_transformer.h"

ULONG_PTR g_gdiplusToken;

int main()
{
	std::vector<mf_source> sources;

	auto hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);

	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	// Initialize GDI+.
	auto status = GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, nullptr);
	//if (status != Gdiplus::Ok) {
	//	delete cap;
	//	return nullptr;
	//}

	std::locale::global(std::locale("", std::locale::ctype));

	SetConsoleOutputCP(65001);

	if (mf_source::get_all_source(sources)) {
		for (const auto& source : sources) {
			std::wstring ws;
			if (source.get_name(ws)) {
				std::wcout << ws << " ";
			}

			if (source.get_endpoint_id(ws)) {
				std::wcout << ws << " ";
			}

			GUID majorType, subType;
			if (source.get_media_type(majorType, subType)) {
				ws.resize(GUIDSTRING_MAX);
				MFMediaType_Video;
				MFVideoFormat_RGB32;
				StringFromGUID2(majorType, (LPOLESTR)ws.data(), GUIDSTRING_MAX);
				std::wcout << ws << "-";
				StringFromGUID2(subType, (LPOLESTR)ws.data(), GUIDSTRING_MAX);
				std::wcout << ws;
			}

			std::cout << std::endl;
		}
	}

	if (mf_source::get_all_source(sources, mf_source::SOURCE_TYPE::SOURCE_VIDEO)) {
		for (const auto& source : sources) {
			std::wstring ws;
			if (source.get_name(ws)) {
				std::wcout << ws << " ";
			}

			if (source.get_symblic_link(ws)) {
				std::wcout << ws << " ";
			}

			std::cout << std::endl;
			GUID majorType, subType;
			if (source.get_media_type(majorType, subType)) {
				ws.resize(GUIDSTRING_MAX);
				StringFromGUID2(majorType, (LPOLESTR)ws.data(), GUIDSTRING_MAX);
				std::wcout << ws << "-";
				StringFromGUID2(subType, (LPOLESTR)ws.data(), GUIDSTRING_MAX);
				std::wcout << ws;
			}

			std::cout << std::endl;

			IMFMediaSource* mediaSource;
			if (!source.get_media_source(&mediaSource)) continue;

			auto reader = new (std::nothrow) mf_source_reader();

			reader->init(mediaSource);
			SafeRelease(mediaSource);

			reader->set_stream((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM);

			auto mediaType = reader->get_source_type();
			std::cout << "input: ";
			print_media_type(mediaType);
			SafeRelease(mediaType);
			//std::cout << std::endl;
			if (reader->set_format(&mediaType, MFMediaType_Video, MFVideoFormat_RGB32)) {
				//MFVIDEOFORMAT* format;
				//hr = mediaType->GetRepresentation(FORMAT_MFVideoFormat, (LPVOID*)&format);
				std::cout << "output: ";
				print_media_type(mediaType);
				//std::cout << std::endl;
				UINT32 width, height, stride;
				hr = MFGetAttributeSize(mediaType, MF_MT_FRAME_SIZE, &width, &height);

				if (SUCCEEDED(hr)) {
					hr = mediaType->GetUINT32(MF_MT_DEFAULT_STRIDE, &stride);
				}

				SafeRelease(mediaType);

				if (SUCCEEDED(hr)) {
					reader->start();
					auto sample = reader->read();
					while (!sample) {
						sample = reader->read();
					}

					IMFMediaBuffer* buffer;
					hr = sample->ConvertToContiguousBuffer(&buffer);
					BYTE* data;
					hr = buffer->Lock(&data, nullptr, nullptr);
					image_transformer encoder;

					encoder.set_input(data, width, height, stride, PixelFormat32bppRGB);
					encoder.set_encoder(L"image/bmp");
					encoder.output(std::to_wstring(time(nullptr)).append(L".bmp"));
					encoder.set_encoder(L"image/png");
					encoder.output(std::to_wstring(time(nullptr)).append(L".png"));
					encoder.set_encoder(L"image/jpeg");
					encoder.output(std::to_wstring(time(nullptr)).append(L".jpeg"));
					hr = buffer->Unlock();

					SafeRelease(buffer);
					SafeRelease(sample);
				}
			}

			reader->stop();
			reader->shutdown();
			SafeRelease(reader);

			//if (!reader->set_format(&mediaType, MFMediaType_Video, MFVideoFormat_NV12)) {
			//    if (!reader->set_format(&mediaType, MFMediaType_Video, MFVideoFormat_YUY2)) {

			//    }
			//}

			//
		}
	}

	Gdiplus::GdiplusShutdown(g_gdiplusToken);
	CoUninitialize();
}
