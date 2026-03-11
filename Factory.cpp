#include "pch.h"

#include <new>

#include "Globals.h"
#include "LangSound.h"

class Factory final : public IClassFactory
{
public:
	STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override
	{
		if (riid == IID_IUnknown ||
			riid == IID_IClassFactory)
		{
			*ppv = this;
			AddRef();
			return S_OK;
		}

		*ppv = nullptr;
		return E_NOINTERFACE;
	}

	STDMETHODIMP_(ULONG) AddRef() override
	{
		return 2;
	}

	STDMETHODIMP_(ULONG) Release() override
	{
		return 1;
	}

	STDMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override
	{
		if (outer)
			return CLASS_E_NOAGGREGATION;

		LangSound* obj = new(std::nothrow) LangSound();
		if (!obj)
			return E_OUTOFMEMORY;

		const HRESULT hr =
			obj->QueryInterface(
				riid,
				ppv);

		obj->Release();

		return hr;
	}

	STDMETHODIMP LockServer(BOOL) override
	{
		return S_OK;
	}

	Factory() = default;
	Factory(const Factory&) = delete;
	Factory& operator=(const Factory&) = delete;
	Factory(Factory&&) = delete;
	Factory& operator=(Factory&&) = delete;
	~Factory() = default;
};

namespace
{
	Factory g_factory;
}

extern "C" STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
	if (!ppv)
		return E_POINTER;

	*ppv = nullptr;

	if (rclsid != CLSID_LangSound)
		return CLASS_E_CLASSNOTAVAILABLE;

	return g_factory.QueryInterface(riid, ppv);
}

extern "C" STDAPI DllCanUnloadNow()
{
	const long refs = g_cRefDll.load(std::memory_order_relaxed);
	return (refs == 0) ? S_OK : S_FALSE;
}
