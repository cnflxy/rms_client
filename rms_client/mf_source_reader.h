#pragma once

//#include <iostream>
#include <queue>
#include <mutex>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <propkey.h>
#include <wmsdk.h>
#include "mf_helper.h"
#include "com_helper.h"

#pragma comment(lib, "Mfplat")
#pragma comment(lib, "Mfreadwrite")
#pragma comment(lib, "Mfuuid")

class mf_source_reader : private IMFSourceReaderCallback {
public:
	mf_source_reader(MFTIME bufferedMaxDuration = 500) : m_bufferedMaxDuration(bufferedMaxDuration * 1000 * 10) {}

	bool init(IMFMediaSource* source)
	{
		auto attrs = create_reader_attributes();
		if (!attrs) return false;

		auto hr = MFCreateSourceReaderFromMediaSource(source, attrs, &m_reader);
		SafeRelease(attrs);

		if (SUCCEEDED(hr)) {
			m_source = source;
			source->AddRef();
			return true;
		}

		SafeRelease(m_reader);

		return false;
	}

	bool init(LPCWSTR url)
	{
		IMFSourceResolver* resolver;
		auto hr = MFCreateSourceResolver(&resolver);
		if (FAILED(hr)) return false;

		//auto hr = MFCreateSourceReaderFromURL(url, attrs, &m_reader);
		MF_OBJECT_TYPE type = MF_OBJECT_INVALID;
		IUnknown* unk;
		hr = resolver->CreateObjectFromURL(
			url,
			MF_RESOLUTION_MEDIASOURCE | MF_RESOLUTION_CONTENT_DOES_NOT_HAVE_TO_MATCH_EXTENSION_OR_MIME_TYPE | MF_RESOLUTION_READ,
			nullptr,
			&type,
			&unk
		);
		SafeRelease(resolver);

		IMFMediaSource* source = nullptr;
		if (SUCCEEDED(hr)) {
			hr = unk->QueryInterface(&source);
			SafeRelease(unk);
		}

		if (SUCCEEDED(hr)) {
			auto ret = init(source);
			SafeRelease(source);
			return ret;
		}

		return false;
	}

	bool init(IMFByteStream* stream)
	{
		IMFSourceResolver* resolver;
		auto hr = MFCreateSourceResolver(&resolver);
		if (FAILED(hr)) return false;

		//auto hr = MFCreateSourceReaderFromByteStream(stream, attrs, &m_reader);
		MF_OBJECT_TYPE type = MF_OBJECT_INVALID;
		IUnknown* unk;
		hr = resolver->CreateObjectFromByteStream(
			stream,
			nullptr,
			MF_RESOLUTION_MEDIASOURCE | MF_RESOLUTION_CONTENT_DOES_NOT_HAVE_TO_MATCH_EXTENSION_OR_MIME_TYPE | MF_RESOLUTION_READ,
			nullptr,
			&type,
			&unk
		);
		SafeRelease(resolver);

		IMFMediaSource* source = nullptr;
		if (SUCCEEDED(hr)) {
			hr = unk->QueryInterface(&source);
			SafeRelease(unk);
		}

		if (SUCCEEDED(hr)) {
			auto ret = mf_source_reader::init(source);
			SafeRelease(source);
			return ret;
		}

		return false;
	}

	void shutdown()
	{
		stop();
		SafeRelease(m_reader);
		if (m_source) m_source->Shutdown();
		SafeRelease(m_source);
		SafeRelease(m_sourceType);

		while (!m_sampleQueue.empty()) {
			auto drop = m_sampleQueue.front();
			m_sampleQueue.pop();
			SafeRelease(drop);
		}
	}

	bool set_stream(DWORD streamIdx)
	{
		auto hr = m_reader->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
		if (SUCCEEDED(hr)) {
			hr = m_reader->SetStreamSelection(streamIdx, TRUE);
		}

		if (SUCCEEDED(hr)) {
			hr = m_reader->GetCurrentMediaType(streamIdx, &m_sourceType);
			//GetNativeMediaType
		}

		/*GUID sourceSubType;
		if (SUCCEEDED(hr)) {
			hr = m_sourceType->GetGUID(MF_MT_SUBTYPE, &sourceSubType);
		}

		if (SUCCEEDED(hr) && sourceSubType == MEDIASUBTYPE_RAW_AAC1) {
			hr = m_sourceType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
			if (SUCCEEDED(hr)) {
				hr = m_reader->SetCurrentMediaType(streamIdx, nullptr, m_sourceType);
			}

			hr = S_OK;
		}*/

		if (SUCCEEDED(hr)) {
			m_streamIdx = streamIdx;
			print_source_info();
		}

		return SUCCEEDED(hr);
	}

	/*
	* MFMediaType_Audio
	* MFMediaType_Video
	*
	* MFAudioFormat_PCM
	* MFAudioFormat_Float
	*/
	bool set_format(IMFMediaType** finalType, const GUID& majorType = MFMediaType_Audio, const GUID& subType = MFAudioFormat_PCM)
	{
		IMFMediaType* type;

		auto hr = MFCreateMediaType(&type);
		if (FAILED(hr)) return false;

		if (m_streamIdx == MF_SOURCE_READER_INVALID_STREAM_INDEX) {
			m_streamIdx = majorType == MFMediaType_Audio ? MF_SOURCE_READER_FIRST_AUDIO_STREAM : MF_SOURCE_READER_FIRST_VIDEO_STREAM;
			if (!set_stream(m_streamIdx)) hr = E_FAIL;
		}

		//if (SUCCEEDED(hr)) {
		//	hr = m_sourceType->CopyAllItems(type);
		//}

		if (SUCCEEDED(hr)) {
			hr = type->SetGUID(MF_MT_MAJOR_TYPE, majorType);
		}

		if (SUCCEEDED(hr)) {
			hr = type->SetGUID(MF_MT_SUBTYPE, subType);
		}

		if (SUCCEEDED(hr)) {
			if (majorType == MFMediaType_Audio) {
				UINT32 samplingRate;
				hr = m_sourceType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &samplingRate);
				//MFGetAttributeUINT32(m_sourceType, MF_MT_AUDIO_SAMPLES_PER_SECOND, 0);
				if (SUCCEEDED(hr)) {
					hr = type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, samplingRate);
				}
			}/* else {
				UINT64 frameSize;
				hr = m_sourceType->GetUINT64(MF_MT_FRAME_SIZE, &frameSize);
				if (SUCCEEDED(hr)) {
					hr = type->SetUINT64(MF_MT_FRAME_SIZE, frameSize);
				}
			}*/

			hr = S_OK;
		}

		if (SUCCEEDED(hr)) {
			/*
			* m_sourceType->GetGUID(MF_MT_SUBTYPE, &sourceSubType);
			* will failed in case sourceSubType == MEDIASUBTYPE_RAW_AAC1
			*/
			hr = m_reader->SetCurrentMediaType(m_streamIdx, nullptr, type);
		}

		SafeRelease(type);

		if (SUCCEEDED(hr)) {
			hr = m_reader->GetCurrentMediaType(m_streamIdx, finalType);
		}

		return SUCCEEDED(hr);
	}

	bool start()
	{
		auto hr = m_reader->ReadSample(m_streamIdx, 0, nullptr, nullptr, nullptr, nullptr);

		return m_running = SUCCEEDED(hr);
	}

	void stop()
	{
		m_running = false;
		//auto hr = m_reader->Flush(m_streamIdx);
	}

	bool seek(MFTIME offset)
	{
		if (!m_running || m_seeking) return false;

		m_updateMutex.lock();
		m_currentPosition = m_currentPosition + offset <= 0 ? 0 : m_currentPosition + offset;
		m_seeking = true;
		m_updateMutex.unlock();

		return true;
	}

	IMFSample* read()
	{
		if (m_seeking) {
			if (m_running) {
				PROPVARIANT varPosition;
				PropVariantInit(&varPosition);
				varPosition.vt = VT_I8;
				varPosition.hVal.QuadPart = m_currentPosition;
				auto hr = m_reader->SetCurrentPosition(GUID_NULL, varPosition);
				if (SUCCEEDED(hr)) {
					while (!m_sampleQueue.empty()) {
						auto drop = m_sampleQueue.front();
						m_sampleQueue.pop();
						SafeRelease(drop);
					}

					//m_currentPosition = -1;
					m_bufferedDuration = 0;
					m_paused = m_seeking = false;
					start();
				}
				PropVariantClear(&varPosition);
			}
			m_seeking = false;
		}

		if (m_sampleQueue.empty()) return nullptr;

		IMFSample* sample = nullptr;
		m_updateMutex.lock();
		//if (!m_sampleQueue.empty()) {
		sample = m_sampleQueue.front();
		m_sampleQueue.pop();
		MFTIME duration;
		sample->GetSampleDuration(&duration);
		m_bufferedDuration -= duration;
		m_currentPosition += duration;
		m_updateMutex.unlock();

		if (m_bufferedDuration < m_bufferedMaxDuration && m_paused && m_running && !m_seeking) m_paused = !start();

		return sample;
	}

	bool running() const
	{
		return m_running;
	}

	MFTIME get_total_duration() const
	{
		return m_bufferedTotalDuration;
	}

	MFTIME get_duration() const
	{
		return m_bufferedDuration;
	}

	MFTIME get_max_duration() const
	{
		return m_bufferedMaxDuration;
	}

	MFTIME get_position() const
	{
		return m_currentPosition;
	}

	IMFMediaType* get_source_type()
	{
		m_sourceType->AddRef();
		return m_sourceType;
	}

public:
	STDMETHOD_(ULONG, AddRef)()
	{
		return InterlockedIncrement(&m_RefCount);
	}

	STDMETHOD_(ULONG, Release)()
	{
		ULONG returnValue = InterlockedDecrement(&m_RefCount);
		if (!returnValue) {
			delete this;
		}
		return returnValue;
	}

protected:
	virtual ~mf_source_reader() {}

private:

	IMFAttributes* create_reader_attributes()
	{
		IMFAttributes* attrs = nullptr;

		auto hr = MFCreateAttributes(&attrs, 4);
		if (SUCCEEDED(hr)) {
			hr = attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
		}

		if (SUCCEEDED(hr)) {
			hr = attrs->SetUINT32(MF_SOURCE_READER_DISCONNECT_MEDIASOURCE_ON_SHUTDOWN, TRUE);
		}

		if (SUCCEEDED(hr)) {
			hr = attrs->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
		}

		//if (SUCCEEDED(hr)) {
		//	hr = attrs->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, TRUE);
		//}

		if (SUCCEEDED(hr)) {
			hr = attrs->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, this);
		} else if (attrs) {
			attrs->Release();
			attrs = nullptr;
		}

		return attrs;
	}

	void print_source_info()
	{
		PROPVARIANT var;

		PropVariantInit(&var);
		auto hr = m_reader->GetPresentationAttribute((DWORD)MF_SOURCE_READER_MEDIASOURCE, MF_PD_MIME_TYPE, &var);
		if (SUCCEEDED(hr)) {
			std::wcout << "mime_type: " << var.pwszVal << std::endl;
			PropVariantClear(&var);
		}

		hr = m_reader->GetPresentationAttribute((DWORD)MF_SOURCE_READER_MEDIASOURCE, MF_PD_TOTAL_FILE_SIZE, &var);
		if (SUCCEEDED(hr)) {
			std::cout << "file_size: " << var.uhVal.QuadPart << std::endl;
			//PropVariantClear(&var);
		}

		hr = m_reader->GetPresentationAttribute((DWORD)MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &var);
		if (SUCCEEDED(hr)) {
			std::cout << "duration: " << var.uhVal.QuadPart / 10.0 / 1000 / 1000 << std::endl;
			//PropVariantClear(&var);
		}

		hr = m_reader->GetPresentationAttribute((DWORD)MF_SOURCE_READER_MEDIASOURCE, MF_PD_AUDIO_ENCODING_BITRATE, &var);
		if (SUCCEEDED(hr)) {
			std::cout << "bitrate: " << var.ulVal / 1000.0 << std::endl;
			//PropVariantClear(&var);
		}

		hr = m_reader->GetPresentationAttribute((DWORD)MF_SOURCE_READER_MEDIASOURCE, MF_PD_AUDIO_ISVARIABLEBITRATE, &var);
		if (SUCCEEDED(hr)) {
			std::cout << "vbr: " << (var.bVal ? "true" : "false") << std::endl;
			//PropVariantClear(&var);
		}

		//MFMEDIASOURCE_CHARACTERISTICS
		hr = m_reader->GetPresentationAttribute((DWORD)MF_SOURCE_READER_MEDIASOURCE, MF_SOURCE_READER_MEDIASOURCE_CHARACTERISTICS, &var);
		if (SUCCEEDED(hr)) {
			auto old_flags = std::cout.flags();
			std::cout.flags(std::ios_base::showbase | std::ios_base::hex);
			std::cout << "characteristics:" << var.ulVal;
			std::cout.flags(old_flags);
			if (var.ulVal & MFMEDIASOURCE_IS_LIVE) std::cout << " living";
			if (var.ulVal & MFMEDIASOURCE_CAN_SEEK) std::cout << " seekable";
			if (var.ulVal & MFMEDIASOURCE_CAN_PAUSE) std::cout << " pauseable";
			if (var.ulVal & MFMEDIASOURCE_DOES_NOT_USE_NETWORK) std::cout << " local";
			std::cout << std::endl;
			//PropVariantClear(&var);
		}
		std::cout << std::endl;

		::print_source_info(m_source);
		std::cout << std::endl;

		IMFPresentationDescriptor* PD;
		hr = m_source->CreatePresentationDescriptor(&PD);
		if (FAILED(hr)) {
			return;
		}

		IMFMetadataProvider* metadataProvider;
		hr = MFGetService(
			m_source,
			MF_METADATA_PROVIDER_SERVICE,
			IID_PPV_ARGS(&metadataProvider)
		);

		if (SUCCEEDED(hr)) {
			IMFMetadata* metadata;
			hr = metadataProvider->GetMFMetadata(PD, 0, 0, &metadata);

			if (SUCCEEDED(hr)) {
				PROPVARIANT varLanguages;
				hr = metadata->GetAllLanguages(&varLanguages);
				if (SUCCEEDED(hr)) {
					if (varLanguages.vt == (VT_VECTOR | VT_LPWSTR) && varLanguages.calpwstr.cElems) {
						for (DWORD i = 0; i < varLanguages.calpwstr.cElems; ++i) {
							std::wcout << i << ": " << varLanguages.calpwstr.pElems[i] << std::endl;
						}
					}
					PropVariantClear(&varLanguages);
				}

				PROPVARIANT varPropNames;
				hr = metadata->GetAllPropertyNames(&varPropNames);
				if (SUCCEEDED(hr)) {
					if (varPropNames.vt == (VT_VECTOR | VT_LPWSTR)) {
						for (DWORD i = 0; i < varPropNames.calpwstr.cElems; ++i) {
							std::wcout << i << ": " << varPropNames.calpwstr.pElems[i];
							hr = metadata->GetProperty(varPropNames.calpwstr.pElems[i], &var);
							if (SUCCEEDED(hr)) {
								if (var.vt == VT_LPWSTR) {
									dynamic_cast<my_printf&>(my_cout << ", " << var.pwszVal).print();
									//std::wcout << ", " << var.pwszVal;
								} else if (var.vt == (VT_VECTOR | VT_BLOB)) {
									for (ULONG j = 0; j < var.cabstrblob.cElems; ++j) {
										WM_USER_TEXT wmText;
										wmText.pwszDescription = (LPWSTR)var.cabstrblob.pElems[j].pData;
										wmText.pwszText = (LPWSTR)var.cabstrblob.pElems[j].pData + lstrlenW(wmText.pwszDescription) + 1;
										//WM_USER_TEXT* wmText = reinterpret_cast<WM_USER_TEXT*>(var.cabstrblob.pElems[i].pData);
										std::wcout << ", " << wmText.pwszDescription << ": " << wmText.pwszText;
									}
								} else {
									std::wcout << " " << var.vt;
								}
								PropVariantClear(&var);
							}
							std::cout << std::endl;
						}
					}
					PropVariantClear(&varPropNames);
				}
				std::cout << std::endl;
				SafeRelease(metadata);
			}

			SafeRelease(metadataProvider);
		}

		DWORD streamCnt;
		hr = PD->GetStreamDescriptorCount(&streamCnt);
		if (SUCCEEDED(hr) && streamCnt) {
			for (DWORD i = 0; i < streamCnt; ++i) {
				BOOL selected;
				IMFStreamDescriptor* SD;
				hr = PD->GetStreamDescriptorByIndex(i, &selected, &SD);
				if (FAILED(hr)) continue;

				IMFMediaTypeHandler* typeHandler;
				hr = SD->GetMediaTypeHandler(&typeHandler);
				SafeRelease(SD);
				if (FAILED(hr)) {
					continue;
				}

				GUID guidMajorType;
				hr = typeHandler->GetMajorType(&guidMajorType);
				if (SUCCEEDED(hr)) {
					if (guidMajorType == MFMediaType_Audio) {
						std::cout << "input " << i << "_" << selected << ": ";
					} else if (guidMajorType == MFMediaType_Video) {
						std::cout << "input " << i << "_" << selected << ": ";
						//if (selected) {
						//	hr = PD->DeselectStream(i);
						//}
					} else {
						OLECHAR guid[GUIDSTRING_MAX];
						if (StringFromGUID2(guidMajorType, guid, GUIDSTRING_MAX)) {
							std::cout << "input " << i << "_" << selected << ": " << guid;
						}
						//std::cout << guid << 
					}
				}
				IMFMediaType* mediaType;
				hr = typeHandler->GetCurrentMediaType(&mediaType);
				if (SUCCEEDED(hr)) {
					print_media_type(mediaType);
					SafeRelease(mediaType);
				} else {
					std::cout << std::endl;
				}
				SafeRelease(typeHandler);
			}
		}
		SafeRelease(PD);
	}

private:
	STDMETHOD(QueryInterface)(REFIID iid, void** pvObject)
	{
		if (!pvObject) {
			return E_POINTER;
		}

		*pvObject = nullptr;

		if (iid == IID_IUnknown) {
			*pvObject = static_cast<IUnknown*>(static_cast<IMFSourceReaderCallback*>(this));
			AddRef();
		} else if (iid == __uuidof(IMFSourceReaderCallback)) {
			*pvObject = static_cast<IMFSourceReaderCallback*>(this);
			AddRef();
		} else {
			return E_NOINTERFACE;
		}
		return S_OK;
	}

	STDMETHOD(OnReadSample)(
		_In_ HRESULT hrStatus,
		_In_ DWORD /*dwStreamIndex*/,
		_In_ DWORD dwStreamFlags,
		_In_ LONGLONG llTimestamp,
		_In_opt_ IMFSample* pSample
		)
	{
		//static LONGLONG sampleTimeBase = 0;

		do {
			if (!m_running || m_seeking) break;

			if (FAILED(hrStatus)) {
				m_running = false;
				break;
			}

			if (dwStreamFlags & (MF_SOURCE_READERF_ERROR | MF_SOURCE_READERF_ENDOFSTREAM)) {
				m_running = false;
				break;
			}

			if (dwStreamFlags & MF_SOURCE_READERF_STREAMTICK) break;

			if (!pSample) break;

			//if (m_currentPosition < 0) {
			if (m_sampleQueue.empty()) {
				//sampleTimeBase = llTimestamp;
				m_currentPosition = llTimestamp;
				//m_bufferedDuration = 0;
			}

			//if (m_bufferedDuration >= m_bufferedMaxDuration) {
			//	m_updateMutex.lock();
			//	auto drop = m_sampleQueue.front();
			//	m_sampleQueue.pop();
			//	m_updateMutex.unlock();

			//	drop->GetSampleTime(&sampleTimeBase);
			//	LONGLONG duration;
			//	drop->GetSampleDuration(&duration);
			//	m_bufferedDuration -= duration;
			//	drop->Release();
			//}

			LONGLONG duration;
			auto hr = pSample->GetSampleDuration(&duration);
			if (FAILED(hr)) {
				break;
			}
			pSample->AddRef();

			m_updateMutex.lock();
			m_bufferedDuration += duration;
			m_bufferedTotalDuration += duration;
			m_sampleQueue.push(pSample);
			m_updateMutex.unlock();
		} while (false);

		if (m_bufferedDuration >= m_bufferedMaxDuration) m_paused = true;

		if (m_running && !m_paused && !m_seeking) {
			m_running = start();
		}

		return S_OK;
	}

	STDMETHOD(OnFlush)(_In_ DWORD /*dwStreamIndex*/)
	{
		return S_OK;
	}

	STDMETHOD(OnEvent) (_In_ DWORD /*dwStreamIndex*/, _In_ IMFMediaEvent* /*pEvent*/)
	{
		return S_OK;
	}

private:
	IMFMediaSource* m_source = nullptr;
	IMFSourceReader* m_reader = nullptr;
	IMFMediaType* m_sourceType = nullptr;
	DWORD m_streamIdx = (DWORD)MF_SOURCE_READER_INVALID_STREAM_INDEX;
	std::queue<IMFSample*> m_sampleQueue;
	MFTIME m_bufferedDuration = 0, m_bufferedMaxDuration, m_bufferedTotalDuration = 0, m_currentPosition = -1;
	std::mutex m_updateMutex;
	bool m_running = false;
	bool m_paused = false;
	bool m_seeking = false;

	LONG m_RefCount = 1;
};
