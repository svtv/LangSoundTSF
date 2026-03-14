#pragma once

#include <msctf.h>
#include <optional>
#include <wrl/client.h>

class LangSound final :
	public ITfTextInputProcessor,
	public ITfActiveLanguageProfileNotifySink
{
public:
	LangSound();
	LangSound(const LangSound&) = delete;
	LangSound& operator=(const LangSound&) = delete;
	LangSound(LangSound&&) = delete;
	LangSound& operator=(LangSound&&) = delete;
	~LangSound();

	// IUnknown
	STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
	STDMETHODIMP_(ULONG) AddRef() override;
	STDMETHODIMP_(ULONG) Release() override;

	// ITfTextInputProcessor
	STDMETHODIMP Activate(ITfThreadMgr* ptim, TfClientId tid) override;
	STDMETHODIMP Deactivate() override;

	// Language notification
	STDMETHODIMP OnActivated(REFCLSID clsid, REFGUID guidProfile, BOOL fActivated) override;

private:
	LONG _refCount;

	Microsoft::WRL::ComPtr<ITfThreadMgr> _threadMgr;
	Microsoft::WRL::ComPtr<ITfInputProcessorProfiles> _profiles;
	TfClientId _clientId;
	DWORD _sinkCookie;

	std::optional<GUID> _lastProfileGuid;
};
