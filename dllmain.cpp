// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

#include <mmsystem.h>
#include <msctf.h>
#include <strsafe.h>
#include <vector>
#include <wrl/client.h>

#include "Globals.h"

namespace
{
	struct CoInitScope final
	{
		HRESULT hr;

		explicit CoInitScope(DWORD coinit) : hr(CoInitializeEx(nullptr, coinit))
		{
		}

		~CoInitScope()
		{
			if (SUCCEEDED(hr))
				CoUninitialize();
		}

		CoInitScope(const CoInitScope&) = delete;
		CoInitScope& operator=(const CoInitScope&) = delete;
		CoInitScope(CoInitScope&&) = delete;
		CoInitScope& operator=(CoInitScope&&) = delete;
	};

	struct RegKeyScope final
	{
		RegKeyScope() = default;
		HKEY h = nullptr;

		~RegKeyScope()
		{
			if (h)
				RegCloseKey(h);
		}

		RegKeyScope(const RegKeyScope&) = delete;
		RegKeyScope& operator=(const RegKeyScope&) = delete;
		RegKeyScope(RegKeyScope&&) = delete;
		RegKeyScope& operator=(RegKeyScope&&) = delete;
	};

	HRESULT CreateInputProcessorProfiles(Microsoft::WRL::ComPtr<ITfInputProcessorProfiles>& profiles)
	{
		return CoCreateInstance(
			CLSID_TF_InputProcessorProfiles,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_ITfInputProcessorProfiles,
			reinterpret_cast<void**>(profiles.ReleaseAndGetAddressOf()));
	}

	std::vector<LANGID> GetUniqueLangIdsFromInstalledKeyboardLayouts()
	{
		const int count = GetKeyboardLayoutList(0, nullptr);
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
			if (SystemParametersInfoW(SPI_GETDEFAULTINPUTLANG, 0, (PVOID)&defaultLayout, 0))
				langs.push_back(LOWORD(reinterpret_cast<DWORD_PTR>(defaultLayout)));
			else
				langs.push_back(GetUserDefaultLangID());
		}

		return langs;
	}

	HRESULT RegisterComServer()
	{
		WCHAR modulePath[MAX_PATH] = {};
		if (!GetModuleFileNameW(g_hInst, modulePath, MAX_PATH))
			return HRESULT_FROM_WIN32(GetLastError());

		WCHAR clsidStr[64] = {};
		if (!StringFromGUID2(CLSID_LangSound, clsidStr, ARRAYSIZE(clsidStr)))
			return E_FAIL;

		WCHAR keyPath[128] = {};
		if (FAILED(
			StringCchPrintfW(keyPath, ARRAYSIZE(keyPath), L"Software\\Classes\\CLSID\\%s\\InprocServer32", clsidStr)))
			return E_FAIL;

		RegKeyScope key;
		DWORD disp = 0;
		LONG rc = RegCreateKeyExW(
			HKEY_CURRENT_USER,
			keyPath,
			0,
			nullptr,
			REG_OPTION_NON_VOLATILE,
			KEY_SET_VALUE,
			nullptr,
			&key.h,
			&disp);

		if (rc != ERROR_SUCCESS)
			return HRESULT_FROM_WIN32(rc);

		rc = RegSetValueExW(
			key.h,
			nullptr,
			0,
			REG_SZ,
			reinterpret_cast<const BYTE*>(modulePath),
			(static_cast<DWORD>(lstrlenW(modulePath)) + 1) * sizeof(WCHAR));

		if (rc == ERROR_SUCCESS)
		{
			constexpr WCHAR threadingModel[] = L"Apartment";
			rc = RegSetValueExW(
				key.h,
				L"ThreadingModel",
				0,
				REG_SZ,
				reinterpret_cast<const BYTE*>(threadingModel),
				(static_cast<DWORD>(lstrlenW(threadingModel)) + 1) * sizeof(WCHAR));
		}
		return (rc == ERROR_SUCCESS) ? S_OK : HRESULT_FROM_WIN32(rc);
	}

	HRESULT UnregisterComServer()
	{
		WCHAR clsidStr[64] = {};
		if (!StringFromGUID2(CLSID_LangSound, clsidStr, ARRAYSIZE(clsidStr)))
			return E_FAIL;

		WCHAR keyPath[128] = {};
		if (FAILED(StringCchPrintfW(keyPath, ARRAYSIZE(keyPath), L"Software\\Classes\\CLSID\\%s", clsidStr)))
			return E_FAIL;

		const LONG rc = RegDeleteTreeW(HKEY_CURRENT_USER, keyPath);
		if (rc == ERROR_FILE_NOT_FOUND)
			return S_OK;
		return (rc == ERROR_SUCCESS) ? S_OK : HRESULT_FROM_WIN32(rc);
	}
}

extern "C" HRESULT STDAPICALLTYPE DllRegisterServer()
{
	const CoInitScope com(COINIT_APARTMENTTHREADED);
	HRESULT hr = com.hr;
	if (FAILED(hr))
		return hr;

	hr = RegisterComServer();
	if (FAILED(hr))
	{
		return hr;
	}

	Microsoft::WRL::ComPtr<ITfInputProcessorProfiles> profiles;
	hr = CreateInputProcessorProfiles(profiles);

	if (FAILED(hr))
	{
		(void)UnregisterComServer();
		return hr;
	}

	hr = profiles->Register(CLSID_LangSound);
	if (FAILED(hr))
	{
		(void)UnregisterComServer();
		return hr;
	}

	WCHAR modulePath[MAX_PATH] = {};
	GetModuleFileNameW(g_hInst, modulePath, MAX_PATH);

	constexpr WCHAR desc[] = L"LangSound";
	const ULONG descLen = static_cast<ULONG>(lstrlenW(desc));
	const ULONG pathLen = static_cast<ULONG>(lstrlenW(modulePath));

	const std::vector<LANGID> langs = GetUniqueLangIdsFromInstalledKeyboardLayouts();
	std::vector<LANGID> registeredLangs;
	registeredLangs.reserve(langs.size());
	for (const LANGID langId : langs)
	{
		GUID profileGuid = GetProfileGuidForLang(langId);

		const HRESULT hrAdd = profiles->AddLanguageProfile(
			CLSID_LangSound,
			langId,
			profileGuid,
			desc,
			descLen,
			modulePath,
			pathLen,
			0);

		if (FAILED(hrAdd) && hrAdd != TF_E_ALREADY_EXISTS)
		{
			hr = hrAdd;
			break;
		}

		const HRESULT hrEnable = profiles->EnableLanguageProfile(
			CLSID_LangSound,
			langId,
			profileGuid,
			TRUE);
		if (FAILED(hrEnable))
		{
			hr = hrEnable;
			break;
		}

		registeredLangs.push_back(langId);
	}

	if (FAILED(hr))
	{
		for (const LANGID langId : registeredLangs)
		{
			GUID profileGuid = GetProfileGuidForLang(langId);
			profiles->EnableLanguageProfile(CLSID_LangSound, langId, profileGuid, FALSE);
			profiles->RemoveLanguageProfile(CLSID_LangSound, langId, profileGuid);
		}
		profiles->Unregister(CLSID_LangSound);
		(void)UnregisterComServer();
		return hr;
	}

	return hr;
}

extern "C" HRESULT STDAPICALLTYPE DllUnregisterServer()
{
	const CoInitScope com(COINIT_APARTMENTTHREADED);
	HRESULT hr = com.hr;
	if (FAILED(hr))
	{
		(void)UnregisterComServer();
		return hr;
	}

	Microsoft::WRL::ComPtr<ITfInputProcessorProfiles> profiles;
	hr = CreateInputProcessorProfiles(profiles);

	if (FAILED(hr))
	{
		(void)UnregisterComServer();
		return hr;
	}

	const std::vector<LANGID> langs = GetUniqueLangIdsFromInstalledKeyboardLayouts();

	for (const LANGID langId : langs)
	{
		GUID profileGuid = GetProfileGuidForLang(langId);
		profiles->EnableLanguageProfile(
			CLSID_LangSound,
			langId,
			profileGuid,
			FALSE);
		profiles->RemoveLanguageProfile(
			CLSID_LangSound,
			langId,
			profileGuid);
	}

	hr = profiles->Unregister(CLSID_LangSound);

	UnregisterComServer();

	return hr;
}

BOOL APIENTRY DllMain(HMODULE hModule,
                      DWORD ul_reason_for_call,
                      LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		g_hInst = hModule;
		DisableThreadLibraryCalls(hModule);
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
	default:
		break;
	}
	return TRUE;
}
