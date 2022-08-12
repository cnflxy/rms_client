#pragma once

#include <sstream>
#include <random>
#include <string>
#include <locale>
#include <codecvt>
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include <netfw.h>

#define NT_SUCCESS(Status)          (((NTSTATUS)(Status)) >= 0)

#pragma comment(lib, "Iphlpapi")
#pragma comment(lib, "Bcrypt")

const CLSID CLSID_NetFwPolicy2 = __uuidof(NetFwPolicy2);
const CLSID CLSID_NetFwRule = __uuidof(NetFwRule);

class utility {
public:
	static void generate_uuid(GUID& guid)
	{
		if (FAILED(CoCreateGuid(&guid))) {
			std::random_device rd;
			std::mt19937 gen(rd());
			std::uniform_int_distribution<uint16_t> dis(0, 255);
			auto guid_ptr = reinterpret_cast<decltype(dis)::result_type*>(&guid);

			for (int i = 0; i < sizeof(guid); ++i) {
				guid_ptr[i] = dis(gen);
			}
		}
	}

	static uint32_t crc32(const void* in, size_t len)
	{
		static const uint32_t crc32_table[] = {
			0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA,
	 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
	 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
	 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
	 0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE,
	 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
	 0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC,
	 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
	 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
	 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
	 0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940,
	 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
	 0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116,
	 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
	 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
	 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
	 0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A,
	 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
	 0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818,
	 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
	 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
	 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
	 0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C,
	 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
	 0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2,
	 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
	 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
	 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
	 0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086,
	 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
	 0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4,
	 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
	 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
	 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
	 0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8,
	 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
	 0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE,
	 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
	 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
	 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
	 0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252,
	 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
	 0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60,
	 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
	 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
	 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
	 0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04,
	 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
	 0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A,
	 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
	 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
	 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
	 0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E,
	 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
	 0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C,
	 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
	 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
	 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
	 0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0,
	 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
	 0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6,
	 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
	 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
	 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
		};
		uint32_t crc = UINT32_MAX;
		for (size_t i = 0; i < len; ++i) {
			crc = crc32_table[crc & 0xff ^ ((uint8_t*)in)[i]] ^ crc >> 8;
		}
		return crc ^ UINT32_MAX;
	};

	static bool sha256(const uint8_t* in, const size_t& len, std::vector<uint8_t>& sha) {
		BCRYPT_ALG_HANDLE hAlg = nullptr;
		BCRYPT_HASH_HANDLE hHash = nullptr;
		DWORD cbData, cbHash, cbHashObject;
		PBYTE pbHashObject = nullptr/*, pbHash = nullptr*/;
		//std::vector<uint8_t> sha;
		NTSTATUS Status;

		Status = BCryptOpenAlgorithmProvider(
			&hAlg,
			BCRYPT_SHA256_ALGORITHM,
			nullptr,
			0
		);
		if (!NT_SUCCESS(Status)) {
			//std::cerr << "BCryptOpenAlgorithmProvider failed: " << std::hex << std::showbase << Status << std::noshowbase << std::dec << std::endl;
			goto exit;
		}

		Status = BCryptGetProperty(
			hAlg,
			BCRYPT_OBJECT_LENGTH,
			reinterpret_cast<PUCHAR>(&cbHashObject),
			sizeof(DWORD),
			&cbData,
			0
		);
		if (!NT_SUCCESS(Status)) {
			//std::cerr << "BCryptGetProperty failed: " << std::hex << std::showbase << Status << std::noshowbase << std::dec << std::endl;
			goto exit;
		}

		pbHashObject = new (std::nothrow) BYTE[cbHashObject];
		if (!pbHashObject) {
			//std::cerr << "alloc HashObject failed!" << std::endl;
			goto exit;
		}

		Status = BCryptGetProperty(
			hAlg,
			BCRYPT_HASH_LENGTH,
			reinterpret_cast<PUCHAR>(&cbHash),
			sizeof(DWORD),
			&cbData,
			0
		);
		if (!NT_SUCCESS(Status)) {
			//std::cerr << "BCryptGetProperty failed: " << std::hex << std::showbase << Status << std::noshowbase << std::dec << std::endl;
			goto exit;
		}

		sha.resize(cbHash);
		//pbHash = new (std::nothrow) BYTE[cbHash];
		//if (!pbHash) {
			//std::cerr << "alloc HashBuffer failed!" << std::endl;
			//goto exit;
		//}

		Status = BCryptCreateHash(
			hAlg,
			&hHash,
			pbHashObject,
			cbHashObject,
			nullptr,
			0,
			0
		);
		if (!NT_SUCCESS(Status)) {
			//std::cerr << "BCryptCreateHash failed: " << std::hex << std::showbase << Status << std::noshowbase << std::dec << std::endl;
			goto exit;
		}

		Status = BCryptHashData(
			hHash,
			const_cast<PUCHAR>(in),
			len,
			0
		);
		if (!NT_SUCCESS(Status)) {
			//std::cerr << "BCryptHashData failed: " << std::hex << std::showbase << Status << std::noshowbase << std::dec << std::endl;
			goto exit;
		}

		Status = BCryptFinishHash(
			hHash,
			sha.data(),
			cbHash,
			0
		);
		if (!NT_SUCCESS(Status)) {
			//std::cerr << "BCryptFinishHash failed: " << std::hex << std::showbase << Status << std::noshowbase << std::dec << std::endl;
			goto exit;
		}

		//sha.assign(pbHash, pbHash + cbHash);

	exit:
		if (hAlg) {
			BCryptCloseAlgorithmProvider(hAlg, 0);
		}

		if (hHash) {
			BCryptDestroyHash(hHash);
		}

		if (pbHashObject) {
			delete[] pbHashObject;
		}

		if (!NT_SUCCESS(Status)) {
			sha.clear();
		}
		//if (pbHash) {
			//delete[] pbHash;
		//}

		return !sha.empty();
	};

	static bool allow_inbound_on_port(const uint16_t& port, const std::wstring& rule_name, const bool& tcp = false)
	{
		HRESULT hr;
		INetFwPolicy2* pNetFwPolicy2 = nullptr;
		INetFwRules* pFwRules = nullptr;
		INetFwRule* pFwRule = nullptr;
		//long CurrentProfilesBitMask;

		std::wstring selfPath;
		if (!get_self_image_name(selfPath)) {
			//std::cerr << "GetModuleFileName failed: " << GetLastError() << std::endl;
			return false;
		}

		hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
		if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
			//std::cerr << "CoInitializeEx failed: " << hr << std::endl;
			return false;
		}

		BSTR bstrRuleName = SysAllocString(rule_name.c_str());
		//BSTR bstrRuleDescription = SysAllocString(L"");
		BSTR bstrRuleGroup = SysAllocString(L"RMS");
		BSTR bstrRuleApplication = SysAllocString(selfPath.c_str());
		BSTR bstrRuleLPorts = SysAllocString(std::to_wstring(port).c_str());

		hr = CoCreateInstance(
			CLSID_NetFwPolicy2,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&pNetFwPolicy2)
		);
		if (FAILED(hr)) {
			//std::cerr << "CoCreateInstance CLSID_NetFwPolicy2 failed: " << hr << std::endl;
			goto Cleanup;
		}

		hr = pNetFwPolicy2->get_Rules(&pFwRules);
		if (FAILED(hr)) {
			//std::cerr << "get_Rules failed: " << hr << std::endl;
			goto Cleanup;
		}

		hr = pFwRules->Item(bstrRuleName, &pFwRule);
		while (S_OK == hr) {
			//goto Cleanup;
			pFwRule->Release();
			hr = pFwRules->Remove(bstrRuleName);
			if (hr == S_OK) {
				hr = pFwRules->Item(bstrRuleName, &pFwRule);
			}
		}

		//hr = pNetFwPolicy2->get_CurrentProfileTypes(&CurrentProfilesBitMask);

		// When possible we avoid adding firewall rules to the Public profile.
		// If Public is currently active and it is not the only active profile, we remove it from the bitmask
		//if ((CurrentProfilesBitMask & NET_FW_PROFILE2_PUBLIC) &&
			//(CurrentProfilesBitMask != NET_FW_PROFILE2_PUBLIC))
		//{
			//CurrentProfilesBitMask ^= NET_FW_PROFILE2_PUBLIC;
		//}

		// Create a new Firewall Rule object.
		hr = CoCreateInstance(
			CLSID_NetFwRule,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&pFwRule)
		);
		if (FAILED(hr)) {
			//std::cerr << "CoCreateInstance CLSID_NetFwRule failed: " << hr << std::endl;
			goto Cleanup;
		}

		// Populate the Firewall Rule object
		pFwRule->put_Name(bstrRuleName);
		//pFwRule->put_Description(bstrRuleDescription);
		pFwRule->put_ApplicationName(bstrRuleApplication);
		pFwRule->put_Protocol(tcp ? NET_FW_IP_PROTOCOL_TCP : NET_FW_IP_PROTOCOL_UDP);
		pFwRule->put_LocalPorts(bstrRuleLPorts);
		pFwRule->put_Direction(NET_FW_RULE_DIR_IN);
		pFwRule->put_Grouping(bstrRuleGroup);
		//pFwRule->put_Profiles(CurrentProfilesBitMask);
		pFwRule->put_Profiles(NET_FW_PROFILE2_ALL);
		pFwRule->put_Action(NET_FW_ACTION_ALLOW);
		pFwRule->put_EdgeTraversal(VARIANT_TRUE);
		pFwRule->put_Enabled(VARIANT_TRUE);

		// Add the Firewall Rule
		hr = pFwRules->Add(pFwRule);
		if (FAILED(hr)) {
			//std::cerr << "Add failed: " << hr << std::endl;

			//printf("Firewall Rule Add failed: 0x%08lx\n", hr);
			//goto Cleanup;
		}

	Cleanup:

		if (pFwRule) {
			pFwRule->Release();
		}

		if (pFwRules) {
			pFwRules->Release();
		}

		if (pNetFwPolicy2) {
			pNetFwPolicy2->Release();
		}

		SysFreeString(bstrRuleName);
		//SysFreeString(bstrRuleDescription);
		SysFreeString(bstrRuleGroup);
		SysFreeString(bstrRuleLPorts);
		SysFreeString(bstrRuleApplication);

		CoUninitialize();

		return SUCCEEDED(hr);
	}

	static std::string cvt_ws_to_utf8(const std::wstring& ws) {
		std::wstring_convert<std::codecvt_utf8<std::wstring::value_type>> converter;
		return converter.to_bytes(ws);
	};

	static std::wstring cvt_ws_from_utf8(const std::string& s) {
		std::wstring_convert<std::codecvt_utf8<std::wstring::value_type>> converter;
		return converter.from_bytes(s);
	}

	static std::string cvt_ws_to_native(const std::wstring& ws) {
		using converter_type = std::codecvt<std::wstring::value_type, std::string::value_type, std::mbstate_t>;
		static const std::locale locale("");
		static const converter_type& converter = std::use_facet<converter_type>(locale);
		std::vector<std::string::value_type> to(ws.length() * converter.max_length());
		std::mbstate_t state;
		const std::wstring::value_type* from_next;
		std::string::value_type* to_next;
		const converter_type::result result = converter.out(state, ws.data(), ws.data() + ws.length(), from_next, &to[0], &to[0] + to.size(), to_next);
		if (result == converter_type::ok or result == converter_type::noconv) {
			const std::string s(&to[0], to_next);
			return s;
		}
		return std::string();
	};

	static std::wstring cvt_ws_from_native(const std::string& s) {
		using converter_type = std::codecvt<std::string::value_type, std::wstring::value_type, std::mbstate_t>;
		static const std::locale locale("");
		static const converter_type& converter = std::use_facet<converter_type>(locale);
		std::vector<std::wstring::value_type> to(s.length() * converter.max_length());
		std::mbstate_t state;
		const std::string::value_type* from_next;
		std::wstring::value_type* to_next;
		const converter_type::result result = converter.out(state, s.data(), s.data() + s.length(), from_next, &to[0], &to[0] + to.size(), to_next);
		if (result == converter_type::ok or result == converter_type::noconv) {
			const std::wstring ws(&to[0], to_next);
			return ws;
		}
		return std::wstring();
	};

	static std::string bytes_to_hex_string(const std::vector<uint8_t>& bytes) {
		std::ostringstream oss;

		oss.flags(std::ios_base::hex);
		oss.fill('0');
		for (const auto& b : bytes) {
			oss.width(2);
			oss << static_cast<uint16_t>(b);
		}

		return std::move(oss.str());
	};

	static bool hex_string_to_bytes(const std::string& s, std::vector<uint8_t>& bytes) {
		if (s.size() & 1) return false;

		for (size_t i = 0; i < s.size(); i += 2) {
			//strtol(s.c_str() + i, (char**) &(s.c_str() + i + 2), 16);
			size_t idx;
			auto b = std::stoul(s.substr(i, 2), &idx, 16);
			if (idx != 2) return false;
			bytes.emplace_back((uint8_t)b);
		}

		return true;
	};

	static bool have_admin_power()
	{
		BOOL b;
		SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
		PSID AdministratorsGroup;
		b = AllocateAndInitializeSid(
			&NtAuthority,
			2,
			SECURITY_BUILTIN_DOMAIN_RID,
			DOMAIN_ALIAS_RID_ADMINS,
			0, 0, 0, 0, 0, 0,
			&AdministratorsGroup
		);

		if (b) {
			if (!CheckTokenMembership(nullptr, AdministratorsGroup, &b)) {
				b = FALSE;
			}
			FreeSid(AdministratorsGroup);
		}

		return(b);
	}

	static bool get_self_image_name(std::wstring& imageName)
	{
		//QueryFullProcessImageNameW();

		imageName.resize(MAX_PATH);
		auto ret = GetModuleFileNameW(nullptr, (LPWSTR)imageName.data(), imageName.size());
		while (ret == imageName.size() && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
			imageName.resize(imageName.size() << 1);
			ret = GetModuleFileNameW(nullptr, (LPWSTR)imageName.data(), imageName.size());
		}
		imageName.resize(ret);
		return imageName.size();
	}

	static int get_platform_type()
	{
		SYSTEM_INFO si;
		GetNativeSystemInfo(&si);

		if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) {
			return 1;
		} else if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL) {
			return 2;
		} else {
			return 0;
		}
	}

	static std::wstring get_temp_file_path(const std::wstring& suffix)
	{
		std::wstring temp_path;
		temp_path.resize(GetTempPathW(0, nullptr));
		temp_path.resize(GetTempPathW(temp_path.size(), (LPWSTR)temp_path.data()));

		return std::move(temp_path.append(std::to_wstring(time(nullptr))).append(suffix));
	}

	static void print_debug_msg(const std::wstring& tag, const std::initializer_list<std::wstring>& msg_list, const DWORD& err = 0)
	{
		std::wstring errMsg;

		if (err) {
			LPWSTR lpErrMsg;
			if (FormatMessageW(
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
				nullptr,
				err,
				0,
				/*MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),*/
				reinterpret_cast<LPWSTR>(&lpErrMsg),
				0,
				nullptr
			)) {
				errMsg = lpErrMsg;
				LocalFree(lpErrMsg);
			}
		}

		std::wostringstream oss;

		oss << tag;
		for (const auto& msg : msg_list) {
			oss << L" " << msg;
		}
		if (err) {
			oss << ", ";
			if (!errMsg.empty()) {
				oss << errMsg << " ";
			}
			oss << std::hex << std::showbase << err;
		}
		oss << std::endl;

		OutputDebugStringW(oss.str().c_str());
	}
};
