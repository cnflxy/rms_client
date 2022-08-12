#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <functional>
#include <windows.h>
#include <gdiplus.h>
#include <mfapi.h>
#include <mfidl.h>
#include <wmcodecdsp.h>
//#include <ksuuids.h>
//#include <dshow.h>
//#include <dvdmedia.h>
//#include <strmif.h>
//#include "image_viewer.h"
#include "console_helper.h"
#include "com_helper.h"

#pragma comment(lib, "Gdiplus")
#pragma comment(lib, "wmcodecdspuuid")
#pragma comment(lib, "propsys")

#if 1
LPCWSTR mf_guid_to_string(GUID mfGuid)
{
#define MAKE_MF_GUID_MAP_ITEM(_guid, _desc) {std::_Hash_array_representation<char>((char*)&(_guid), sizeof(_guid)), _desc}

	const std::unordered_map<size_t, LPCWSTR> guid_map = {
		MAKE_MF_GUID_MAP_ITEM(MFMediaType_Audio, L"Audio "),
		MAKE_MF_GUID_MAP_ITEM(MFMediaType_Video, L"Video "),
		MAKE_MF_GUID_MAP_ITEM(MFAudioFormat_AAC, L"AAC "),
		MAKE_MF_GUID_MAP_ITEM(MFAudioFormat_WMAudioV8, L"WMAStd "),
		MAKE_MF_GUID_MAP_ITEM(MFAudioFormat_WMAudioV9, L"WMAPro "),
		MAKE_MF_GUID_MAP_ITEM(MFAudioFormat_WMAudio_Lossless, L"WMALossless "),
		MAKE_MF_GUID_MAP_ITEM(MFAudioFormat_MSP1, L"WMAVoice "),
		MAKE_MF_GUID_MAP_ITEM(MFAudioFormat_PCM, L"PCM "),
		MAKE_MF_GUID_MAP_ITEM(MFAudioFormat_Float, L"Float "),
		MAKE_MF_GUID_MAP_ITEM(MFAudioFormat_MP3, L"MP3 "),
		//MAKE_MF_GUID_MAP_ITEM(MFAudioFormat_FLAC, L"FLAC "),
		MAKE_MF_GUID_MAP_ITEM(MFAudioFormat_ADTS, L"ADTS "),
		MAKE_MF_GUID_MAP_ITEM(MFAudioFormat_Dolby_AC3, L"Dolby_AC3 "),
		MAKE_MF_GUID_MAP_ITEM(MFVideoFormat_NV12, L"NV12 "),
		MAKE_MF_GUID_MAP_ITEM(MFVideoFormat_YUY2, L"YUY2 "),
		MAKE_MF_GUID_MAP_ITEM(MFVideoFormat_RGB32, L"RGB32 "),
		MAKE_MF_GUID_MAP_ITEM(MFVideoFormat_RGB24, L"RGB24 "),
		MAKE_MF_GUID_MAP_ITEM(MFVideoFormat_ARGB32, L"ARGB "),
		MAKE_MF_GUID_MAP_ITEM(MFVideoFormat_MJPG, L"MJPG "),
		MAKE_MF_GUID_MAP_ITEM(MFVideoFormat_H264, L"H264/AVC1 "),
		MAKE_MF_GUID_MAP_ITEM(MFVideoFormat_H265, L"H265/HEVC "),
		MAKE_MF_GUID_MAP_ITEM(MFVideoFormat_HEVC, L"H265/HEVC "),
		MAKE_MF_GUID_MAP_ITEM(MFVideoFormat_M4S2, L"M4S2 "),
		MAKE_MF_GUID_MAP_ITEM(MFVideoFormat_MP4V, L"MP4V "),
		MAKE_MF_GUID_MAP_ITEM(MFVideoFormat_H264_ES, L"H264_ES "),
		MAKE_MF_GUID_MAP_ITEM(MEDIASUBTYPE_RAW_AAC1, L"rawAAC "),	// WAVE_FORMAT_RAW_AAC1
	};

	auto hash = std::_Hash_array_representation<char>((char*)&mfGuid, sizeof(mfGuid));

	auto item = guid_map.find(hash);
	if (item == guid_map.cend()) return nullptr;

	return item->second;
}

void print_media_type(IMFMediaType* pType)
{
	if (!pType) return;

	using std::wcout;
	using std::endl;

	GUID guidMajorType = GUID_NULL;
	auto hr = pType->GetGUID(MF_MT_MAJOR_TYPE, &guidMajorType);
	auto majorType = mf_guid_to_string(guidMajorType);
	do {
		if (FAILED(hr) || !majorType) {
			wcout << L"Unknown";
			break;
		}
		wcout << majorType;

		GUID guidSubType = GUID_NULL;
		hr = pType->GetGUID(MF_MT_SUBTYPE, &guidSubType);
		auto subType = mf_guid_to_string(guidSubType);
		if (FAILED(hr) || !subType) {
			wcout << L"Unknown";
			break;
		}
		wcout << subType;

		PROPVARIANT var;
		PropVariantInit(&var);
		if (guidMajorType == MFMediaType_Audio) {
			pType->GetItem(MF_MT_AUDIO_SAMPLES_PER_SECOND, &var);
			wcout << var.uintVal << " ";
			PropVariantClear(&var);

			if (guidSubType == MFAudioFormat_AAC) {
				pType->GetItem(MF_MT_AAC_PAYLOAD_TYPE, &var);
				wcout << "(" << var.uintVal << " ";
				PropVariantClear(&var);

				pType->GetItem(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, &var);
				wcout << "" << var.uintVal << ") ";
				PropVariantClear(&var);
			}

			pType->GetItem(MF_MT_AUDIO_NUM_CHANNELS, &var);
			wcout << (var.uintVal == 1 ? "mono" :
				(var.uintVal == 2 ? "stereo" :
					(var.uintVal == 6 ? "5.1" : std::to_string(var.uintVal).c_str()
						)
					)
				) << " ";
			PropVariantClear(&var);

			pType->GetItem(MF_MT_AUDIO_BITS_PER_SAMPLE, &var);
			wcout << var.uintVal << " ";
			PropVariantClear(&var);

			pType->GetItem(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &var);
			wcout << var.uintVal * 8 / 1000 << "kbps ";
			PropVariantClear(&var);

			pType->GetItem(MF_MT_AUDIO_BLOCK_ALIGNMENT, &var);
			wcout << var.uintVal;
			PropVariantClear(&var);
		} else if (guidMajorType == MFMediaType_Video) {
			if (guidSubType == MFVideoFormat_H264) {
				pType->GetItem(MF_MT_MPEG2_PROFILE, &var);
				wcout << "(" << var.uintVal << " ";
				PropVariantClear(&var);

				pType->GetItem(MF_MT_MPEG2_LEVEL, &var);
				wcout << var.uintVal << ") ";
				PropVariantClear(&var);
			}

			pType->GetItem(MF_MT_ORIGINAL_4CC, &var);
			wcout << "(" << std::showbase << std::hex << var.uintVal << std::dec << std::noshowbase << ")";
			PropVariantClear(&var);

			pType->GetItem(MF_MT_INTERLACE_MODE, &var);
			wcout << ", (" << var.uintVal << ") ";
			PropVariantClear(&var);

			UINT32 width = 0, height = 0;
			MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, &width, &height);
			wcout << width << "x" << height << " ";

			UINT32 num = 1, den = 1;
			MFGetAttributeRatio(pType, MF_MT_PIXEL_ASPECT_RATIO, &num, &den);
			wcout << "[SAR " << num << ":" << den << " ";

			//FORMAT_MPEG2Video;
			//FORMAT_MPEG2Video;
			//MF_PROGRESSIVE_CODING_CONTENT;
			//MF_NALU_LENGTH_SET;
			MFVideoArea dar{};
			hr = pType->GetBlob(MF_MT_GEOMETRIC_APERTURE, (UINT8*)&dar, sizeof(MFVideoArea), nullptr);
			{
				auto bcd = [](auto a, auto b) {
					auto t = a % b;
					while (t) {
						a = b;
						b = t;
						t = a % b;
					}

					return b;
				};

				UINT32 a, b, d;
				if (SUCCEEDED(hr)) {
					// https://docs.microsoft.com/en-us/windows/win32/medfound/picture-aspect-ratio
					// 720¡Á480 10:11 -> (704/480) ¡Á (10/11) = 4:3
					// (704*10)/(480*11)=7040/5280
					// (7040, 5280)=1760
					// 7040/1760=4, 5280/1760=3
					d = bcd(dar.Area.cx, dar.Area.cy);
					a = dar.Area.cx / d * num;
					b = dar.Area.cy / d * den;
				} else {
					d = bcd(width, height);
					a = width / d * num;
					b = height / d * den;
				}

				d = bcd(a, b);
				wcout << "DAR " << a / d << ":" << b / d << "] ";
			}

			//UINT32 bitrate = 0;
			pType->GetItem(MF_MT_AVG_BITRATE, &var);
			wcout << var.uintVal / 1000 << "kbps ";
			PropVariantClear(&var);

			num = 0, den = 1;
			MFGetAttributeRatio(pType, MF_MT_FRAME_RATE, &num, &den);
			auto old_flags = wcout.flags();
			wcout.flags(std::ios_base::fixed | std::ios_base::showpoint);
			wcout.precision(2);
			wcout << num * 1.0 / den << "fps ";
			wcout.flags(old_flags);
		}
	} while (false);

	wcout << endl;
}
//
//struct my_hash {
//	_NODISCARD size_t operator()(const PROPERTYKEY& _Keyval) const noexcept {
//		return std::_Hash_array_representation((char*)&_Keyval, sizeof(PROPERTYKEY)); // map -0 to 0
//	}
//};
//
//struct my_equal_to {
//	_NODISCARD constexpr bool operator()(const PROPERTYKEY& _Left, const PROPERTYKEY& _Right) const {
//		return _Left == _Right;
//	}
//};

LPCWSTR pkey_guid_to_string(PROPERTYKEY pkey)
{
	//OLECHAR id[GUIDSTRING_MAX];

	//if (!StringFromGUID2(pkey.fmtid, id, GUIDSTRING_MAX)) {
	//	return nullptr;
	//}

	//std::wstring ws(id);
	//ws.append(std::to_wstring(pkey.pid));

#define MAKE_PKEY_MAP_ITEM(_pkey) {std::_Hash_array_representation<char>((char*)&(_pkey), sizeof(_pkey)), L#_pkey}

	/*
	* {46802C11-ADA9-41B7-8EBE-65BA6699358B}, 100
	*/
	const std::unordered_map<size_t, LPCWSTR> pkey_map = {
		MAKE_PKEY_MAP_ITEM(PKEY_Media_Duration),
		MAKE_PKEY_MAP_ITEM(PKEY_Media_Year),
		MAKE_PKEY_MAP_ITEM(PKEY_Media_EncodingSettings),
		MAKE_PKEY_MAP_ITEM(PKEY_Media_DateEncoded),
		MAKE_PKEY_MAP_ITEM(PKEY_Media_Publisher),
		MAKE_PKEY_MAP_ITEM(PKEY_Media_EncodedBy),
		MAKE_PKEY_MAP_ITEM(PKEY_Media_DateReleased),
		MAKE_PKEY_MAP_ITEM(PKEY_Media_DlnaProfileID),
		MAKE_PKEY_MAP_ITEM(MFPKEY_Content_DLNA_Profile_ID),
		MAKE_PKEY_MAP_ITEM(PKEY_Audio_EncodingBitrate),
		MAKE_PKEY_MAP_ITEM(PKEY_Audio_ChannelCount),
		MAKE_PKEY_MAP_ITEM(PKEY_Audio_Format),
		MAKE_PKEY_MAP_ITEM(PKEY_Audio_SampleRate),
		MAKE_PKEY_MAP_ITEM(PKEY_Audio_SampleSize),
		MAKE_PKEY_MAP_ITEM(PKEY_Audio_StreamNumber),
		MAKE_PKEY_MAP_ITEM(PKEY_Audio_IsVariableBitRate),
		MAKE_PKEY_MAP_ITEM(PKEY_Video_EncodingBitrate),
		MAKE_PKEY_MAP_ITEM(PKEY_Video_FrameWidth),
		MAKE_PKEY_MAP_ITEM(PKEY_Video_FrameHeight),
		MAKE_PKEY_MAP_ITEM(PKEY_Video_FrameRate),
		MAKE_PKEY_MAP_ITEM(PKEY_Video_Compression),
		MAKE_PKEY_MAP_ITEM(PKEY_Video_IsSpherical),
		MAKE_PKEY_MAP_ITEM(PKEY_Video_IsStereo),
		MAKE_PKEY_MAP_ITEM(PKEY_Video_FourCC),
		MAKE_PKEY_MAP_ITEM(PKEY_Video_HorizontalAspectRatio),
		MAKE_PKEY_MAP_ITEM(PKEY_Video_VerticalAspectRatio),
		MAKE_PKEY_MAP_ITEM(PKEY_Video_Orientation),
		MAKE_PKEY_MAP_ITEM(PKEY_Video_StreamNumber),
		MAKE_PKEY_MAP_ITEM(PKEY_Video_TotalBitrate),
		MAKE_PKEY_MAP_ITEM(PKEY_Music_AlbumTitle),
		MAKE_PKEY_MAP_ITEM(PKEY_Music_Artist),
		MAKE_PKEY_MAP_ITEM(PKEY_Music_Genre),
		MAKE_PKEY_MAP_ITEM(PKEY_Music_TrackNumber),
		MAKE_PKEY_MAP_ITEM(PKEY_Music_AlbumArtist),
		MAKE_PKEY_MAP_ITEM(PKEY_Music_PartOfSet),
		MAKE_PKEY_MAP_ITEM(PKEY_Music_Composer),
		MAKE_PKEY_MAP_ITEM(PKEY_Music_BeatsPerMinute),
		MAKE_PKEY_MAP_ITEM(PKEY_Music_InitialKey),
		MAKE_PKEY_MAP_ITEM(PKEY_RecordedTV_RecordingTime),
		MAKE_PKEY_MAP_ITEM(PKEY_MIMEType),
		MAKE_PKEY_MAP_ITEM(PKEY_DRM_IsProtected),
		MAKE_PKEY_MAP_ITEM(PKEY_Author),
		MAKE_PKEY_MAP_ITEM(PKEY_Comment),
		MAKE_PKEY_MAP_ITEM(PKEY_Title),
		MAKE_PKEY_MAP_ITEM(PKEY_ThumbnailStream),
		MAKE_PKEY_MAP_ITEM(PKEY_SoftwareUsed),
		MAKE_PKEY_MAP_ITEM(PKEY_Keywords),
		MAKE_PKEY_MAP_ITEM(PKEY_Copyright),
		MAKE_PKEY_MAP_ITEM(PKEY_FileDescription),
		MAKE_PKEY_MAP_ITEM(PKEY_Language),
	};

	auto hash = std::_Hash_array_representation<char>((char*)&pkey, sizeof(pkey));

	auto item = pkey_map.find(hash);
	if (item == pkey_map.cend()) return nullptr;

	return item->second;
	//if(pkey_map.find())
	//switch (hash) {
	//case constexpr std::_Hash_array_representation<char>((char*)&PKEY_Media_Duration, sizeof(PKEY_Media_Duration)):

	//}
}

void print_source_info(IMFMediaSource* source)
{
	if (!source) return;

	IPropertyStore* propStore;

	auto hr = MFGetService(source, MF_PROPERTY_HANDLER_SERVICE, IID_PPV_ARGS(&propStore));
	if (FAILED(hr)) return;

	DWORD cProp;
	hr = propStore->GetCount(&cProp);
	if (SUCCEEDED(hr) && cProp) {
		PROPVARIANT var;
		PropVariantInit(&var);

		for (DWORD i = 0; i < cProp; ++i) {
			PROPERTYKEY propKey;
			hr = propStore->GetAt(i, &propKey);
			if (FAILED(hr)) {
				continue;
			}

			std::cout << i << ": ";
			PWSTR propKeyName;
			hr = PSGetNameFromPropertyKey(propKey, &propKeyName);
			if (hr == S_OK) {
				std::wcout << propKeyName;
				SafeFree(propKeyName);
			} else {
				auto pkey_name = pkey_guid_to_string(propKey);
				if (!pkey_name) {
					OLECHAR fmtid[GUIDSTRING_MAX];
					if (StringFromGUID2(propKey.fmtid, fmtid, GUIDSTRING_MAX)) {
						std::wcout << fmtid << ", " << propKey.pid;
					} else {
						std::cout << "unknown";
					}
				} else {
					std::wcout << pkey_name;
				}
			}

			hr = propStore->GetValue(propKey, &var);
			if (SUCCEEDED(hr)) {
				std::cout << ", ";

				if (var.vt == VT_UI4) {
					std::cout << var.ulVal;
				} else if (var.vt == VT_UI8) {
					std::cout << var.uhVal.QuadPart;
				} else if (var.vt == VT_BOOL) {
					std::cout << std::boolalpha << (var.boolVal == VARIANT_TRUE) << std::noboolalpha;
				} else if ((var.vt & VT_ILLEGALMASKED) == VT_LPWSTR) {
					auto lpwstr = &var.pwszVal;
					DWORD count = 1;
					if (var.vt & VT_VECTOR) {
						count = var.calpwstr.cElems;
						lpwstr = var.calpwstr.pElems;
					}
					for (DWORD j = 0; j < count; ++j) {
						GUID mfType;
						if ((propKey == PKEY_Audio_Format || propKey == PKEY_Video_Compression) && SUCCEEDED(IIDFromString(lpwstr[j], &mfType))) {
							auto subType = mf_guid_to_string(mfType);
							if (subType) {
								std::wcout << subType;
							} else {
								std::wcout << "unknown " << lpwstr[j];
							}
						} else {
							dynamic_cast<my_printf&>(my_cout << lpwstr[j]).print();
							//std::wcout << var.pwszVal;
						}
					}
				} else if (var.vt == VT_FILETIME) {
					SYSTEMTIME st;
					if (FileTimeToSystemTime(&var.filetime, &st)) {
						std::cout << st.wYear << "/" << st.wMonth << "/" << st.wDay << " " << st.wHour << ":" << st.wMinute << ":" << st.wSecond;
					} else {
						ULARGE_INTEGER ui;
						ui.LowPart = var.filetime.dwLowDateTime;
						ui.HighPart = var.filetime.dwHighDateTime;
						std::cout << "time: " << std::showbase << std::hex << ui.QuadPart << std::dec << std::noshowbase;
					}
				} else if (var.vt == VT_STREAM) {
					if (propKey == PKEY_ThumbnailStream && var.pStream) {
						//STATSTG stat;
						//hr = var.pStream->Stat(&stat, STATFLAG_NONAME);
						//if (SUCCEEDED(hr) && stat.cbSize.QuadPart) {

						//}
						//auto thumbnail = Image::FromStream(var.pStream);
						//auto imageViewer = image_viewer::Create();
						//while (!imageViewer->can_show()) std::this_thread::sleep_for(std::chrono::milliseconds(10));
						//imageViewer->show(var.pStream);
						//Gdiplus::Image thumbnail(var.pStream);
						//if (thumbnail.GetLastStatus() == Gdiplus::Ok) {
						//	Gdiplus::Graphics g(GetDesktopWindow());
						//	g.DrawImage(&thumbnail, 0, 0);
						//	//thumbnail->Save(std::to_wstring(time(nullptr)).append(L".jpg").c_str(), nullptr);
						//	//delete thumbnail;
						//}
					}
					std::cout << "stream: " << var.pStream;
				//} else if (var.vt == (VT_LPWSTR | VT_VECTOR)) {
				//	for (ULONG i = 0; i < var.calpwstr.cElems; ++i) {
				//		dynamic_cast<my_printf&>(my_cout << var.calpwstr.pElems[i] << (i + 1 < var.calpwstr.cElems ? L", " : L"")).print();
				//		//std::wcout << var.calpwstr.pElems[i] << (i + 1 < var.calpwstr.cElems ? L", " : L"");
				//	}
				} else if (var.vt != VT_EMPTY) {
					std::cout << "unknown_vt: " << var.vt;
				}
				PropVariantClear(&var);
			}

			std::wcout << std::endl;
		}
	}

	//PROPVARIANT varFormat;
	//PropVariantInit(&varFormat);
	//hr = propStore->GetValue(PKEY_Audio_Format, &varFormat);
	//if (SUCCEEDED(hr)) {
	//	std::wcout << varFormat.pwszVal << std::endl;
	//	PropVariantClear(&varFormat);
	//}

	SafeRelease(propStore);
}
#endif
