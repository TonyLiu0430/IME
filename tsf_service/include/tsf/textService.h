#pragma once

#include <msctf.h>
#include <wil/com.h>
#include <windows.h>

#include <string>

class CTextService
    : public ITfTextInputProcessorEx,
      public ITfThreadMgrEventSink,
      public ITfKeyEventSink,
      public ITfCompositionSink,
      public ITfDisplayAttributeProvider {
public:
    CTextService();
    ~CTextService();

    // ── IUnknown ─────────────────────────────────────────────
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // ── ITfTextInputProcessor ─────────────────────────────────
    STDMETHODIMP Activate(ITfThreadMgr* pThreadMgr, TfClientId tfClientId) override;
    STDMETHODIMP Deactivate() override;

    // ── ITfTextInputProcessorEx ───────────────────────────────
    STDMETHODIMP ActivateEx(ITfThreadMgr* pThreadMgr, TfClientId tfClientId, DWORD dwFlags) override;

    // ── ITfThreadMgrEventSink ─────────────────────────────────
    STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr* pDocMgr) override;
    STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr* pDocMgr) override;
    STDMETHODIMP OnSetFocus(ITfDocumentMgr* pDocMgrFocus, ITfDocumentMgr* pDocMgrPrevFocus) override;
    STDMETHODIMP OnPushContext(ITfContext* pContext) override;
    STDMETHODIMP OnPopContext(ITfContext* pContext) override;

    // ── ITfKeyEventSink ───────────────────────────────────────
    STDMETHODIMP OnSetFocus(BOOL fForeground) override;
    STDMETHODIMP OnTestKeyDown(ITfContext* pContext, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) override;
    STDMETHODIMP OnTestKeyUp(ITfContext* pContext, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) override;
    STDMETHODIMP OnKeyDown(ITfContext* pContext, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) override;
    STDMETHODIMP OnKeyUp(ITfContext* pContext, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) override;
    STDMETHODIMP OnPreservedKey(ITfContext* pContext, REFGUID rguid, BOOL* pfEaten) override;

    // ── ITfCompositionSink ────────────────────────────────────
    STDMETHODIMP OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition* pComposition) override;

    // ── ITfDisplayAttributeProvider ─────────────────────────────
    STDMETHODIMP EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** ppEnum) override;
    STDMETHODIMP GetDisplayAttributeInfo(REFGUID guid, ITfDisplayAttributeInfo** ppInfo) override;

private:
    HRESULT _Activate(ITfThreadMgr* pThreadMgr, TfClientId tfClientId);
    void _Deactivate();

    HRESULT _StartComposition(ITfContext* pContext);
    HRESULT _EndComposition(ITfContext* pContext);
    HRESULT _SetCompositionText(ITfContext* pContext, const std::wstring& text);
    LONG _cRef = 1;
    wil::com_ptr_nothrow<ITfThreadMgr> _pThreadMgr;
    TfClientId _tfClientId = TF_CLIENTID_NULL;
    DWORD _dwThreadMgrEventSinkCookie = TF_INVALID_COOKIE;
    wil::com_ptr_nothrow<ITfComposition> _pComposition;
    std::wstring _compositionBuffer;
};
