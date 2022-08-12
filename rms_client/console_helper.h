#pragma once

#include <sstream>
#include <mutex>
#include <wincon.h>
//#include <processenv.h>
#include <WinBase.h>

class my_printf : public std::wostringstream {
public:
	my_printf(HANDLE outHandle) :m_handleOutput(outHandle) {}

	my_printf& print()
	{
		m_printMutex.lock();
		auto ws = str();
		str(L"");
		clear();
		m_printMutex.unlock();
		if (!ws.empty()) return *this;

		DWORD numWritten = 0;
		if (!WriteConsoleW(m_handleOutput, ws.data(), ws.size(), &numWritten, nullptr) || numWritten != ws.size()) {
			*this << ws.substr(numWritten);
		}

		return *this;
	}

private:
	std::mutex m_printMutex;
	const HANDLE m_handleOutput;
};

my_printf my_cout(GetStdHandle(STD_OUTPUT_HANDLE));
my_printf my_cerr(GetStdHandle(STD_ERROR_HANDLE));

class my_scanf : public std::wistringstream {
public:
	my_scanf() :std::wistringstream() {}

	my_scanf& scan()
	{
		std::lock_guard<std::mutex> lg(m_scanMutex);

		auto ws = str().substr(tellg());
		if (ws.size() == 1 && ws.back() == L'\n') ws.clear();

		CONSOLE_READCONSOLE_CONTROL readControl{ sizeof(CONSOLE_READCONSOLE_CONTROL) };
		//readControl.nLength = sizeof(CONSOLE_READCONSOLE_CONTROL);
		//readControl.dwCtrlWakeupMask = 1 << '\r' | 1 << '\n';

		DWORD mode;
		bool mode_changed = false;
		if (GetConsoleMode(m_handleInput, &mode) && (mode & ENABLE_LINE_INPUT || !(mode & ~ENABLE_PROCESSED_INPUT))) {
			mode_changed = SetConsoleMode(m_handleInput, mode & ~(ENABLE_LINE_INPUT) | ENABLE_PROCESSED_INPUT);
		}

		auto cp = GetConsoleCP();
		bool cp_changed = SetConsoleCP(65001);

		while (ws.empty() || ws.back() != L'\n') {
			DWORD readOut;
			std::wstring::value_type wc[2];	// win7 bug, will corrupted stack so make 2
			bool ret;
			do {
				readOut = 0;
				ret = ReadConsoleW(m_handleInput, &wc, 1, &readOut, &readControl);
				//DebugBreak();
				if (ret && readOut == 1) {
					if (wc[0] == L'\r') {
						ws.push_back(L'\n');
						dynamic_cast<my_printf&>(my_cout << '\n').print();
						break;
					} else if (wc[0] == L'\b') {
						dynamic_cast<my_printf&>(my_cout << "\b \b").print();
						ws.pop_back();
						continue;
					} else /*if (iswprint(wc[0]))*/ {
						dynamic_cast<my_printf&>(my_cout << wc[0]).print();
						ws.push_back(wc[0]);
					}/* else {
						DebugBreak();
					}*/
				} else {
					auto err = GetLastError();
					DebugBreak();
				}
			} while (ret);
		}

		if (cp_changed) {
			SetConsoleCP(cp);
		}
		if (mode_changed) {
			SetConsoleMode(m_handleInput, mode);
		}

		clear();
		str(ws);

		return *this;
	}

private:
	const HANDLE m_handleInput = GetStdHandle(STD_INPUT_HANDLE);
	std::mutex m_scanMutex;
};

my_scanf my_cin;
