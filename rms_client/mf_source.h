#pragma once

#include <vector>
#include <string>
#include <mfapi.h>
#include <mfidl.h>
#include <mmdeviceapi.h>
#include <devicetopology.h>
#include <uuids.h>
#include "com_helper.h"

#pragma comment(lib, "Mf")
#pragma comment(lib, "mfuuid")
#pragma comment(lib, "Strmiids")

class mf_source {
public:
	enum class SOURCE_TYPE {
		SOURCE_AUDIO,
		SOURCE_VIDEO
	};

	static bool get_all_source(std::vector<mf_source>& source_list, SOURCE_TYPE type = SOURCE_TYPE::SOURCE_AUDIO)
	{
		IMFAttributes* attrs;

		source_list.clear();

		auto hr = MFCreateAttributes(&attrs, 2);
		if (FAILED(hr)) return false;

		hr = attrs->SetGUID(
			MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
			type == SOURCE_TYPE::SOURCE_AUDIO ? MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_GUID : MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
		);

		if (type == SOURCE_TYPE::SOURCE_AUDIO) {
			hr = attrs->SetUINT32(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_ROLE, eConsole);
		} else {
			hr = attrs->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_CATEGORY, CLSID_VideoInputDeviceCategory);
		}

		if (SUCCEEDED(hr)) {
			IMFActivate** sources;
			UINT32 count;
			hr = MFEnumDeviceSources(attrs, &sources, &count);
			if (SUCCEEDED(hr) && count) {
				source_list.assign(&sources[0], &sources[count]);
				SafeFree(sources);
			}
			SafeRelease(attrs);
		}

		return SUCCEEDED(hr);
	}

public:
	mf_source(IMFActivate* activate) : m_sourceActivate(activate) {}

	~mf_source()
	{
		if (m_sourceActivate) {
			m_sourceActivate->ShutdownObject();
			m_sourceActivate->Release();
			//SafeRelease(m_sourceActivate);
		}
	}

	//mf_source& operator=(mf_source& r) = delete;
	//{
	//	r.m_sourceActivate->AddRef();
	//	m_sourceActivate = r.m_sourceActivate;
	//	return *this;
	//}

	//mf_source& operator=(IMFActivate* activate)
	//{
	//	m_sourceActivate = activate;
	//	return *this;
	//}

	bool get_media_source(IMFMediaSource** source) const
	{
		if (!m_sourceActivate) return false;

		auto hr = m_sourceActivate->ActivateObject(IID_PPV_ARGS(source));
		if (SUCCEEDED(hr)) {
			//(*source)->AddRef();
			hr = m_sourceActivate->DetachObject();
			//if (FAILED(hr)) {
			//	hr = 
			//}
			hr = S_OK;
		}

		return SUCCEEDED(hr);
	}

	bool get_name(std::wstring& name) const
	{
		if (m_sourceActivate) {
			UINT32 length;
			auto hr = m_sourceActivate->GetStringLength(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &length);

			if (SUCCEEDED(hr) && length) {
				name.resize(length);

				hr = m_sourceActivate->GetString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, (LPWSTR)name.data(), length + 1, &length);
				if (SUCCEEDED(hr) && length == name.length()) {
					return true;
				}
			}
		}

		name.clear();
		return false;
	}

	bool get_source_type(GUID& type) const
	{
		type = GUID_NULL;

		if (!m_sourceActivate) return false;

		auto hr = m_sourceActivate->GetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, &type);

		return SUCCEEDED(hr);
	}

	bool get_media_type(GUID& majorType, GUID& subType) const
	{
		majorType = subType = GUID_NULL;

		if (!m_sourceActivate) return false;

		MFT_REGISTER_TYPE_INFO typeInfo;
		UINT32 retSize;
		auto hr = m_sourceActivate->GetBlob(MF_DEVSOURCE_ATTRIBUTE_MEDIA_TYPE, (UINT8*)&typeInfo, sizeof(typeInfo), &retSize);
		if (SUCCEEDED(hr) && retSize == sizeof(typeInfo)) {
			majorType = typeInfo.guidMajorType;
			subType = typeInfo.guidSubtype;
			return true;
		}

		return false;
	}

	/*
	* only for audio source
	*/
	bool get_endpoint_id(std::wstring& id) const
	{
		if (m_sourceActivate) {
			UINT32 length;
			auto hr = m_sourceActivate->GetStringLength(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_ENDPOINT_ID, &length);

			if (SUCCEEDED(hr) && length) {
				id.resize(length);

				hr = m_sourceActivate->GetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_ENDPOINT_ID, (LPWSTR)id.data(), length + 1, &length);
				if (SUCCEEDED(hr) && length == id.length()) {
					return true;
				}
			}
		}

		id.clear();
		return false;
	}

	/*
	* only for audio source
	*/
	bool is_active() const
	{
		std::wstring id;

		if (!get_endpoint_id(id)) return false;

		IMMDeviceEnumerator* deviceEnumerator;

		auto hr = CoCreateInstance(
			__uuidof(MMDeviceEnumerator),
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&deviceEnumerator)
		);

		if (FAILED(hr)) return false;

		IMMDevice* device;
		hr = deviceEnumerator->GetDevice(id.c_str(), &device);

		SafeRelease(deviceEnumerator);

		if (SUCCEEDED(hr)) {
			DWORD state;
			hr = device->GetState(&state);
			if (SUCCEEDED(hr) && state != DEVICE_STATE_ACTIVE) {
				hr = E_FAIL;
			}
			SafeRelease(device);
		}

		return SUCCEEDED(hr);
	}

	/*
	* only for audio source
	*/
	bool is_microphone() const
	{
		std::wstring id;

		if (!get_endpoint_id(id)) return false;

		IMMDeviceEnumerator* deviceEnumerator;

		auto hr = CoCreateInstance(
			__uuidof(MMDeviceEnumerator),
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&deviceEnumerator)
		);

		if (FAILED(hr)) return false;

		IMMDevice* device;
		hr = deviceEnumerator->GetDevice(id.c_str(), &device);
		SafeRelease(deviceEnumerator);

		IDeviceTopology* deviceTopology = nullptr;
		if (SUCCEEDED(hr)) {
			hr = device->Activate(__uuidof(IDeviceTopology), CLSCTX_INPROC_SERVER, nullptr, (void**)&deviceTopology);
			SafeRelease(device);
		}

		IConnector* connectorPlug = nullptr;
		if (SUCCEEDED(hr)) {
			hr = deviceTopology->GetConnector(0, &connectorPlug);
			SafeRelease(deviceTopology);
		}

		//connectorPlug->GetDataFlow();

		ConnectorType connectorType = Unknown_Connector;
		if (SUCCEEDED(hr)) {
			hr = connectorPlug->GetType(&connectorType);
		}

		IConnector* connectorJack = nullptr;
		if (SUCCEEDED(hr) && connectorType == Physical_External) {
			hr = connectorPlug->GetConnectedTo(&connectorJack);
		} else {
			hr = E_FAIL;
		}

		SafeRelease(connectorPlug);

		if (SUCCEEDED(hr)) {
			hr = connectorJack->GetType(&connectorType);
		}

		//return SUCCEEDED(hr) && connectorType == Physical_External;

		IPart* jackAsPart = nullptr;
		if (SUCCEEDED(hr) && connectorType != Unknown_Connector) {
			hr = connectorJack->QueryInterface(&jackAsPart);
		} else {
			hr = E_FAIL;
		}

		SafeRelease(connectorJack);

		GUID type = GUID_NULL;
		if (SUCCEEDED(hr)) {
			//PartType partType;
			//hr = jackAsPart->GetPartType(&partType);
			hr = jackAsPart->GetSubType(&type);
			SafeRelease(jackAsPart);
		}

		return SUCCEEDED(hr) && connectorType == Physical_External/* && type == KSNODETYPE_MICROPHONE_ARRAY*/;
	}

	/*
	* only for video source
	*/
	bool get_symblic_link(std::wstring& link) const
	{
		if (m_sourceActivate) {
			UINT32 length;
			auto hr = m_sourceActivate->GetStringLength(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &length);

			if (SUCCEEDED(hr) && length) {
				link.resize(length);

				hr = m_sourceActivate->GetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, (LPWSTR)link.data(), length + 1, &length);
				if (SUCCEEDED(hr) && length == link.length()) {
					return true;
				}
			}
		}

		link.clear();
		return false;
	}

	//void test()
	//{
	//	IMFMediaSource* source;

	//	if (!get_media_source(&source)) return;

	//	source->
	//}

private:
	static HRESULT get_default_source(mf_source* source, IMFAttributes* attrs)
	{
		auto hr = MFCreateDeviceSourceActivate(attrs, &source->m_sourceActivate);

		return hr;
	}

private:

	IMFActivate* m_sourceActivate;
};
