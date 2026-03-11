#include "pch.h"
#include "Globals.h"

HINSTANCE g_hInst;
std::atomic<long> g_cRefDll{0};

void DllAddRef()
{
	g_cRefDll.fetch_add(1, std::memory_order_relaxed);
}

void DllRelease()
{
	g_cRefDll.fetch_sub(1, std::memory_order_relaxed);
}
