#pragma once

template<typename T>
void SafeRelease(T*& obj)
{
	if (obj) {
		obj->Release();
		obj = nullptr;
	}
}

template<typename T>
void SafeFree(T*& ptr)
{
	if (ptr) {
		CoTaskMemFree(ptr);
		ptr = nullptr;
	}
}

void SafeClose(void*& h)
{
	if (h) {
		CloseHandle(h);
		h = nullptr;
	}
}
