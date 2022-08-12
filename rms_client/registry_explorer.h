#pragma once

#include <Windows.h>
#include <nlohmann/json.hpp>
#include "utility.h"

class registry_explorer {
	using json = nlohmann::json;

public:
	static void init()
	{
		SYSTEM_INFO si;

		// PROCESSOR_ARCHITECTURE_AMD64
		// PROCESSOR_ARCHITECTURE_INTEL
		//GetSystemInfo(&si);
		GetNativeSystemInfo(&si);
		if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) {
			auto hKernel32 = GetModuleHandle(L"kernel32");
			assert(hKernel32 != nullptr);

			if (GetProcAddress(hKernel32, "IsWow64Process")) m_is64BitsPlt = true;
		}
	}

	static bool list_keys_values(json& j, const HKEY& rootKey, std::wstring path = L"")
	{
		HKEY searchKey;

		j.clear();

		j["type"] = 2;

		auto status = RegOpenKeyExW(rootKey, path.c_str(), 0, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE | KEY_WOW64_64KEY, &searchKey);
		if (status == ERROR_SUCCESS) {
			j["data"]["keys"] = json::array();
			j["data"]["values"] = json::array();

			if (!enum_keys(searchKey, j["data"]["keys"])) {
				j["data"].erase("keys");
			}
			if (!enum_values(searchKey, j["data"]["values"])) {
				j["data"].erase("values");
			}
			RegCloseKey(searchKey);
		}

		//if (m_is64BitsPlt && std::find_if(j["data"]["keys"].cbegin(), j["data"]["keys"].cend(), [](const auto& item) {return item.contains("WOW6432Node"); }) != j["data"]["keys"].cend()) {
		//	auto status = RegOpenKeyExW(rootKey, path.c_str(), 0, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE | KEY_WOW64_32KEY, &searchKey);
		//	if (status == ERROR_SUCCESS) {
		//		j["data"]["keys"].push_back({ "WOW6432Node", json::array() });
		//		if (!enum_keys(searchKey, j["data"]["keys"].front())) {
		//			j["data"]["keys"].erase("WOW6432Node");
		//			j["data"]["keys"].clear();
		//		}
		//		j["data"]["values"].push_back({ "WOW6432Node", json::array() });
		//		if (!enum_values(searchKey, j["data"]["values"].front())) {
		//			j["data"]["values"].erase("WOW6432Node");
		//			j["data"]["values"].clear();
		//		}
		//		RegCloseKey(searchKey);
		//	}
		//}

		//if (j["data"]["keys"].empty()) {
		//	j["data"].erase("keys");
		//}

		//if (j["data"]["values"].empty()) {
		//	j["data"].erase("values");
		//}

		return j.size() > 1;
	}

private:
	static bool enum_keys(const HKEY& key, json& j)
	{
		//std::wstring::value_type keyName[256];	// max 255 char
		//keyName.resize(256);

		DWORD countKeys, maxKeyNameLen;
		auto status = RegQueryInfoKeyW(
			key,
			nullptr,
			nullptr,
			nullptr,
			&countKeys,
			&maxKeyNameLen,
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			nullptr
		);
		if (status != ERROR_SUCCESS || !countKeys) return false;

		//std::vector<wchar_t> keyName;
		LPWSTR keyName = new (std::nothrow) wchar_t[maxKeyNameLen + 1];
		if (!keyName) return false;

		for (DWORD i = 0, keyNameLen;; ++i) {
			keyNameLen = maxKeyNameLen + 1;
			status = RegEnumKeyExW(
				key,
				i,
				keyName,
				&keyNameLen,
				nullptr,
				nullptr,
				nullptr,
				nullptr
			);

			if (status == ERROR_SUCCESS) {
				j.push_back({ { "name", utility::cvt_ws_to_utf8(keyNameLen ? keyName : L"(default)")} });
			} else if (status == ERROR_NO_MORE_ITEMS) {
				break;
			}
			/* {
				Status = ERROR_SUCCESS;
				break;
			}*/
		}

		delete[]keyName;

		return !j.empty();
	}

	static bool enum_values(const HKEY& key, json& j)
	{
		DWORD countVals, maxValNameLen;
		auto status = RegQueryInfoKeyW(
			key,
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			&countVals,
			&maxValNameLen,
			nullptr,
			nullptr,
			nullptr
		);
		if (status != ERROR_SUCCESS || !countVals) return false;

		//if (!maxValNameLen) maxValNameLen = 1;

		//std::vector<wchar_t> valueName;

		LPWSTR valueName = new (std::nothrow) wchar_t[maxValNameLen + 1];
		if (!valueName) return false;

		for (DWORD i = 0, valNameLen, valType;; ++i) {
			//valueName.resize(valNameLen = maxValNameLen + 1);
			valNameLen = maxValNameLen + 1;

			status = RegEnumValueW(
				key,
				i,
				valueName,
				&valNameLen,
				nullptr,
				&valType,
				nullptr,
				nullptr
			);
			if (status == ERROR_SUCCESS) {
				j.push_back({ {"name", utility::cvt_ws_to_utf8(valNameLen ? valueName : L"(default)")}, {"type", valType} });
			} else if (status == ERROR_NO_MORE_ITEMS) {
				break;
			}
		}

		delete[] valueName;

		return !j.empty();
	}

	static bool m_is64BitsPlt;
};

bool registry_explorer::m_is64BitsPlt = false;
