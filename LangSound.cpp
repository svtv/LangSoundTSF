#include "pch.h"
#include "LangSound.h"

#include <mmsystem.h>

#include "Globals.h"
#include "resource.h"

LangSound::LangSound()
	: _refCount(1)
	, _clientId(0)
	, _sinkCookie(TF_INVALID_COOKIE)
	, _lastLangId(std::nullopt)
{
	_threadMgr.Reset();

	DllAddRef();
}

LangSound::~LangSound()
{
	DllRelease();
}

ULONG LangSound::AddRef()
{
	return InterlockedIncrement(&_refCount);
}

ULONG LangSound::Release()
{
	const LONG r = InterlockedDecrement(&_refCount);
	if (!r)
		delete this;
	return r;
}

HRESULT LangSound::QueryInterface(REFIID riid, void** ppv)
{
	if (riid == IID_IUnknown ||
		riid == IID_ITfTextInputProcessor)
	{
		*ppv = static_cast<ITfTextInputProcessor*>(this);
	}
	else if (riid ==
		IID_ITfActiveLanguageProfileNotifySink)
	{
		*ppv =
			static_cast<ITfActiveLanguageProfileNotifySink*>(this);
	}
	else
	{
		*ppv = nullptr;
		return E_NOINTERFACE;
	}

	AddRef();
	return S_OK;
}

HRESULT LangSound::Activate(ITfThreadMgr* ptim, TfClientId tid)
{
	_threadMgr = ptim;
	_clientId = tid;

	Microsoft::WRL::ComPtr<ITfSource> source;
	if (SUCCEEDED(_threadMgr.As(&source)))
	{
		const HRESULT hr = source->AdviseSink(
			IID_ITfActiveLanguageProfileNotifySink,
			static_cast<ITfActiveLanguageProfileNotifySink*>(this),
			&_sinkCookie);
		if (FAILED(hr))
			_sinkCookie = TF_INVALID_COOKIE;
	}

	return S_OK;
}

HRESULT LangSound::Deactivate()
{
	if (_threadMgr)
	{
		Microsoft::WRL::ComPtr<ITfSource> source;
		if (SUCCEEDED(_threadMgr.As(&source)))
		{
			if (_sinkCookie != TF_INVALID_COOKIE)
			{
				source->UnadviseSink(_sinkCookie);
				_sinkCookie = TF_INVALID_COOKIE;
			}
		}

		_threadMgr.Reset();
	}

	return S_OK;
}

HRESULT LangSound::OnActivated(REFCLSID, REFGUID, BOOL activated)
{
	if (!activated)
		return S_OK;

	HKL layout = GetKeyboardLayout(0);
	LANGID lang =
		LOWORD(reinterpret_cast<DWORD_PTR>(layout));

	HKL defaultLayout = nullptr;
	LANGID defaultLang;
	if (SystemParametersInfoW(SPI_GETDEFAULTINPUTLANG, 0, (PVOID)&defaultLayout, 0))
		defaultLang = LOWORD(reinterpret_cast<DWORD_PTR>(defaultLayout));
	else
		defaultLang = lang;

	if (!_lastLangId.has_value())
	{
		_lastLangId = defaultLang;
		return S_OK;
	}

	if (lang == *_lastLangId)
		return S_OK;

	if (lang != defaultLang)
	{
		PlaySoundW(
			MAKEINTRESOURCEW(IDR_WAVE_LANGSOUND_NONDEFAULT),
			g_hInst,
			SND_RESOURCE |
			SND_ASYNC |
			SND_NODEFAULT |
			SND_SENTRY |
			SND_SYSTEM);
	}
	else
	{
		PlaySoundW(
			MAKEINTRESOURCEW(IDR_WAVE_LANGSOUND_DEFAULT),
			g_hInst,
			SND_RESOURCE |
			SND_ASYNC |
			SND_NODEFAULT |
			SND_SENTRY |
			SND_SYSTEM);
	}

	_lastLangId = lang;

	return S_OK;
}
