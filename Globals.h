#pragma once

#include <atomic>

// GUID сервиса
constexpr CLSID CLSID_LangSound =
	{0x4d3f6e91, 0x71e2, 0x4f1b, {0x92, 0x10, 0x2a, 0x44, 0x71, 0x0f, 0x11, 0x33}};

constexpr GUID GUID_PROFILE_LangSound_En =
	{0xa5f31b1a, 0x0cde, 0x4e64, {0x8b, 0xe9, 0x6c, 0x49, 0x6c, 0x96, 0x9f, 0x32}};

constexpr GUID GUID_PROFILE_LangSound_Ru =
	{0x1bb92467, 0xc840, 0x4ab5, {0x93, 0xb7, 0x92, 0x7c, 0x0f, 0x42, 0x36, 0x9a}};

constexpr GUID GUID_PROFILE_LangSound_Base =
	{0x4d3f0000, 0x71e2, 0x4f1b, {0x92, 0x10, 0x2a, 0x44, 0x71, 0x0f, 0x11, 0x34}};

inline GUID GetProfileGuidForLang(LANGID langId)
{
	if (langId == MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US))
		return GUID_PROFILE_LangSound_En;
	if (langId == MAKELANGID(LANG_RUSSIAN, SUBLANG_DEFAULT))
		return GUID_PROFILE_LangSound_Ru;

	GUID g = GUID_PROFILE_LangSound_Base;
	g.Data1 = (g.Data1 & 0xFFFF0000u) | static_cast<UINT>(langId);
	return g;
}


extern HINSTANCE g_hInst;
extern std::atomic<long> g_cRefDll;

void DllAddRef();
void DllRelease();
