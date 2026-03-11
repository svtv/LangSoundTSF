#include <windows.h>
#include <msctf.h>
#include <oleauto.h>
#include <stdio.h>

#include <vector>

#include "..\\Globals.h"

static void PrintHr(const wchar_t* label, HRESULT hr)
{
	wprintf(L"%s: 0x%08X", label, (unsigned)hr);

	LPWSTR msg = nullptr;
	DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
	DWORD langId = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
	DWORD rc = FormatMessageW(flags, nullptr, (DWORD)hr, langId, (LPWSTR)&msg, 0, nullptr);
	if (rc && msg)
	{
		while (rc && (msg[rc - 1] == L'\r' || msg[rc - 1] == L'\n' || msg[rc - 1] == L' ' || msg[rc - 1] == L'\t'))
		{
			msg[rc - 1] = L'\0';
			--rc;
		}
		wprintf(L" (%s)", msg);
		LocalFree(msg);
	}

	wprintf(L"\n");
}

static void PrintGuid(const GUID& guid)
{
	wchar_t buf[64] = {};
	if (StringFromGUID2(guid, buf, ARRAYSIZE(buf)))
		wprintf(L"%s", buf);
	else
		wprintf(L"{?}");
}

static void DumpCurrentThreadLayout()
{
	HKL hkl = GetKeyboardLayout(0);
	LANGID lang = LOWORD(reinterpret_cast<DWORD_PTR>(hkl));
	wprintf(L"CurrentThreadHKL=0x%p LANGID=0x%04X\n", (void*)hkl, lang);
}

static LANGID GetDefaultInputLangId()
{
	HKL defaultLayout = nullptr;
	if (SystemParametersInfoW(SPI_GETDEFAULTINPUTLANG, 0, (PVOID)&defaultLayout, 0) && defaultLayout)
		return LOWORD(reinterpret_cast<DWORD_PTR>(defaultLayout));
	return 0;
}

static std::vector<LANGID> GetInstalledLangIds()
{
	int count = GetKeyboardLayoutList(0, nullptr);
	std::vector<HKL> layouts;
	if (count > 0)
	{
		layouts.resize(static_cast<size_t>(count));
		GetKeyboardLayoutList(count, layouts.data());
	}

	std::vector<LANGID> langs;
	langs.reserve(layouts.size());
	for (HKL hkl : layouts)
	{
		LANGID langId = LOWORD(reinterpret_cast<DWORD_PTR>(hkl));
		bool exists = false;
		for (const LANGID x : langs)
		{
			if (x == langId)
			{
				exists = true;
				break;
			}
		}
		if (!exists)
			langs.push_back(langId);
	}

	if (langs.empty())
	{
		HKL defaultLayout = nullptr;
		if (SystemParametersInfoW(SPI_GETDEFAULTINPUTLANG, 0, (PVOID)&defaultLayout, 0) && defaultLayout)
			langs.push_back(LOWORD(reinterpret_cast<DWORD_PTR>(defaultLayout)));
		else
			langs.push_back(GetUserDefaultLangID());
	}

	return langs;
}

static void DumpLangSoundStatus(ITfInputProcessorProfiles* profiles)
{
	LANGID activeLang = 0;
	GUID activeProfile = {};
	HRESULT hr = profiles->GetActiveLanguageProfile(CLSID_LangSound, &activeLang, &activeProfile);
	if (SUCCEEDED(hr))
	{
		wprintf(L"ActiveLanguageProfile: LANGID=0x%04X GUID=", activeLang);
		PrintGuid(activeProfile);
		wprintf(L"\n");
	}
	else
	{
		PrintHr(L"GetActiveLanguageProfile", hr);
	}

	LANGID defaultLang = GetDefaultInputLangId();
	wprintf(L"DefaultInputLang: 0x%04X\n", defaultLang);

	std::vector<LANGID> langs = GetInstalledLangIds();
	if (langs.empty())
	{
		wprintf(L"InstalledLangIds: <empty>\n");
		return;
	}

	for (LANGID langId : langs)
	{
		GUID guid = GetProfileGuidForLang(langId);
		BOOL enabled = FALSE;
		hr = profiles->IsEnabledLanguageProfile(CLSID_LangSound, langId, guid, &enabled);
		if (FAILED(hr))
		{
			wprintf(L"LANGID=0x%04X GUID=", langId);
			PrintGuid(guid);
			wprintf(L" ");
			PrintHr(L"IsEnabledLanguageProfile", hr);
			continue;
		}

		BSTR desc = nullptr;
		HRESULT hrDesc = profiles->GetLanguageProfileDescription(CLSID_LangSound, langId, guid, &desc);

		wprintf(L"LANGID=0x%04X%s enabled=%d GUID=", langId, (langId == defaultLang) ? L" (default)" : L"", enabled ? 1 : 0);
		PrintGuid(guid);
		if (SUCCEEDED(hrDesc) && desc)
		{
			wprintf(L" desc=\"%s\"", desc);
			SysFreeString(desc);
		}
		else
		{
			wprintf(L" desc=<n/a>");
		}
		wprintf(L"\n");
	}
}

int wmain(int argc, wchar_t** argv)
{
	SetConsoleTitleW(L"LangSoundTSF Host");
	wprintf(L"LangSoundTSF Host\n");
	wprintf(L"Optional: pass full path to LangSoundTSF.dll as argv[1] to load without registration\n\n");
	wprintf(L"F1 = Activate default input language profile\n");
	wprintf(L"F2 = Activate non-default input language profile (cycle installed)\n");
	wprintf(L"ESC = Exit\n\n");

	HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	if (FAILED(hr))
	{
		PrintHr(L"CoInitializeEx failed", hr);
		return 1;
	}

	ITfThreadMgr* threadMgr = nullptr;
	hr = CoCreateInstance(
		CLSID_TF_ThreadMgr,
		nullptr,
		CLSCTX_INPROC_SERVER,
		IID_ITfThreadMgr,
		reinterpret_cast<void**>(&threadMgr));

	if (FAILED(hr))
	{
		PrintHr(L"CoCreateInstance(CLSID_TF_ThreadMgr) failed", hr);
		CoUninitialize();
		return 1;
	}

	TfClientId clientId = TF_CLIENTID_NULL;
	hr = threadMgr->Activate(&clientId);
	if (FAILED(hr))
	{
		PrintHr(L"threadMgr->Activate failed", hr);
		threadMgr->Release();
		CoUninitialize();
		return 1;
	}

	ITfTextInputProcessor* tip = nullptr;
	HMODULE tipModule = nullptr;

	if (argc > 1 && argv[1] && argv[1][0])
	{
		tipModule = LoadLibraryW(argv[1]);
		if (!tipModule)
		{
			PrintHr(L"LoadLibraryW failed", HRESULT_FROM_WIN32(GetLastError()));
		}
		else
		{
			auto pDllGetClassObject = reinterpret_cast<HRESULT(STDAPICALLTYPE*)(REFCLSID, REFIID, LPVOID*)>(
				GetProcAddress(tipModule, "DllGetClassObject"));

			if (!pDllGetClassObject)
			{
				PrintHr(L"GetProcAddress(DllGetClassObject) failed", HRESULT_FROM_WIN32(GetLastError()));
			}
			else
			{
				IClassFactory* cf = nullptr;
				hr = pDllGetClassObject(CLSID_LangSound, IID_IClassFactory, reinterpret_cast<void**>(&cf));
				if (SUCCEEDED(hr))
				{
					hr = cf->CreateInstance(nullptr, IID_ITfTextInputProcessor, reinterpret_cast<void**>(&tip));
					cf->Release();
				}
				else
				{
					PrintHr(L"DllGetClassObject failed", hr);
				}
			}
		}
	}

	if (!tip)
	{
		hr = CoCreateInstance(
			CLSID_LangSound,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_ITfTextInputProcessor,
			reinterpret_cast<void**>(&tip));
	}

	if (FAILED(hr))
	{
		PrintHr(L"CoCreateInstance(CLSID_LangSound) failed", hr);
		if (tipModule)
			FreeLibrary(tipModule);
		threadMgr->Deactivate();
		threadMgr->Release();
		CoUninitialize();
		return 1;
	}

	hr = tip->Activate(threadMgr, clientId);
	if (FAILED(hr))
	{
		PrintHr(L"tip->Activate failed", hr);
		tip->Release();
		threadMgr->Deactivate();
		threadMgr->Release();
		CoUninitialize();
		return 1;
	}

	ITfInputProcessorProfiles* profiles = nullptr;
	hr = CoCreateInstance(
		CLSID_TF_InputProcessorProfiles,
		nullptr,
		CLSCTX_INPROC_SERVER,
		IID_ITfInputProcessorProfiles,
		reinterpret_cast<void**>(&profiles));

	if (FAILED(hr))
	{
		PrintHr(L"CoCreateInstance(CLSID_TF_InputProcessorProfiles) failed", hr);
	}
	else
	{
		DumpLangSoundStatus(profiles);
	}

	RegisterHotKey(nullptr, 1, 0, VK_F1);
	RegisterHotKey(nullptr, 2, 0, VK_F2);
	RegisterHotKey(nullptr, 3, 0, VK_ESCAPE);

	LANGID defaultLang = GetDefaultInputLangId();
	if (defaultLang == 0)
		defaultLang = GetUserDefaultLangID();
	std::vector<LANGID> installedLangs = GetInstalledLangIds();
	std::vector<LANGID> nonDefaultLangs;
	for (const LANGID x : installedLangs)
	{
		if (x != defaultLang)
			nonDefaultLangs.push_back(x);
	}
	size_t nonDefaultIndex = 0;

	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (msg.message == WM_HOTKEY)
		{
			if (msg.wParam == 3)
				break;

			if (profiles)
			{
				LANGID langId = defaultLang;
				if (msg.wParam == 2)
				{
					if (!nonDefaultLangs.empty())
					{
						langId = nonDefaultLangs[nonDefaultIndex % nonDefaultLangs.size()];
						nonDefaultIndex++;
					}
				}
				GUID profileGuid = GetProfileGuidForLang(langId);

				wprintf(L"ActivateLanguageProfile: LANGID=0x%04X GUID=", langId);
				PrintGuid(profileGuid);
				wprintf(L"\n");

				DumpCurrentThreadLayout();
				hr = profiles->ChangeCurrentLanguage(langId);
				PrintHr(L"ChangeCurrentLanguage", hr);
				DumpCurrentThreadLayout();

				hr = profiles->ActivateLanguageProfile(CLSID_LangSound, langId, profileGuid);
				PrintHr(L"ActivateLanguageProfile", hr);
				DumpLangSoundStatus(profiles);
			}

			continue;
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	UnregisterHotKey(nullptr, 1);
	UnregisterHotKey(nullptr, 2);
	UnregisterHotKey(nullptr, 3);

	if (profiles)
		profiles->Release();

	tip->Deactivate();
	tip->Release();

	threadMgr->Deactivate();
	threadMgr->Release();

	if (tipModule)
		FreeLibrary(tipModule);

	CoUninitialize();
	return 0;
}
