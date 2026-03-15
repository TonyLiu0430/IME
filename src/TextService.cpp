#include "TextService.h"

#include <new>

#include "DebugSink.h"
#include "Globals.h"
#include "bopomofo.h"

// ============================================================
//  建構 / 解構
// ============================================================

CTextService::CTextService() {
    ++g_cDllRef;
}

CTextService::~CTextService() {
    --g_cDllRef;
}

// ============================================================
//  IUnknown
// ============================================================

STDMETHODIMP CTextService::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_INVALIDARG;
    *ppv = nullptr;

    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfTextInputProcessor)) {
        *ppv = static_cast<ITfTextInputProcessor*>(this);
    } else if (IsEqualIID(riid, IID_ITfTextInputProcessorEx)) {
        *ppv = static_cast<ITfTextInputProcessorEx*>(this);
    } else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink)) {
        *ppv = static_cast<ITfThreadMgrEventSink*>(this);
    } else if (IsEqualIID(riid, IID_ITfKeyEventSink)) {
        *ppv = static_cast<ITfKeyEventSink*>(this);
    } else if (IsEqualIID(riid, IID_ITfCompositionSink)) {
        *ppv = static_cast<ITfCompositionSink*>(this);
    }

    if (*ppv) {
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CTextService::AddRef() {
    return static_cast<ULONG>(InterlockedIncrement(&_cRef));
}

STDMETHODIMP_(ULONG) CTextService::Release() {
    LONG cRef = InterlockedDecrement(&_cRef);
    if (cRef == 0) delete this;
    return static_cast<ULONG>(cRef);
}

// ============================================================
//  ITfTextInputProcessor
// ============================================================

STDMETHODIMP CTextService::Activate(ITfThreadMgr* pThreadMgr, TfClientId tfClientId) {
    return _Activate(pThreadMgr, tfClientId);
}

STDMETHODIMP CTextService::Deactivate() {
    _Deactivate();
    return S_OK;
}

// ============================================================
//  ITfTextInputProcessorEx
// ============================================================

STDMETHODIMP CTextService::ActivateEx(ITfThreadMgr* pThreadMgr, TfClientId tfClientId, DWORD /*dwFlags*/) {
    return _Activate(pThreadMgr, tfClientId);
}

// ============================================================
//  核心啟動邏輯
// ============================================================

HRESULT CTextService::_Activate(ITfThreadMgr* pThreadMgr, TfClientId tfClientId) {
    RETURN_IF_FAILED(pThreadMgr->QueryInterface(IID_PPV_ARGS(_pThreadMgr.put())));
    _tfClientId = tfClientId;

    // 失敗時自動回滾，RETURN_IF_FAILED 觸發早返時 scope_exit 會呼叫 _Deactivate
    auto cleanup = wil::scope_exit([&] {
        _Deactivate();
    });

    // ── 訂閱 ITfThreadMgrEventSink ────────────────────────────
    {
        wil::com_ptr_nothrow<ITfSource> pSource;
        RETURN_IF_FAILED(_pThreadMgr->QueryInterface(IID_PPV_ARGS(pSource.put())));
        RETURN_IF_FAILED(pSource->AdviseSink(
            IID_ITfThreadMgrEventSink, static_cast<ITfThreadMgrEventSink*>(this), &_dwThreadMgrEventSinkCookie));
    }

    // ── 訂閱 ITfKeyEventSink ──────────────────────────────────
    {
        wil::com_ptr_nothrow<ITfKeystrokeMgr> pKeystrokeMgr;
        RETURN_IF_FAILED(_pThreadMgr->QueryInterface(IID_PPV_ARGS(pKeystrokeMgr.put())));
        RETURN_IF_FAILED(pKeystrokeMgr->AdviseKeyEventSink(_tfClientId, static_cast<ITfKeyEventSink*>(this), TRUE));
    }

    cleanup.release();  // 成功，取消回滾

    // ── 偵錯：連線至 Python 接收端 ───────────────────────────
    DebugSink::instance().connect();
    DebugSink::instance().send(L"IME", L"Activated");

    return S_OK;
}

void CTextService::_Deactivate() {
    // ── 偵錯：通知 Python 並斷線 ─────────────────────────────
    DebugSink::instance().send(L"IME", L"Deactivated");
    DebugSink::instance().disconnect();

    // 結束組字
    if (_pComposition) {
        _pComposition->EndComposition(TF_INVALID_COOKIE);
        _pComposition.reset();
    }
    _compositionBuffer.clear();

    if (_pThreadMgr) {
        // 取消訂閱 ITfKeyEventSink
        if (wil::com_ptr_nothrow<ITfKeystrokeMgr> pKM;
            SUCCEEDED(_pThreadMgr->QueryInterface(IID_PPV_ARGS(pKM.put())))) {
            pKM->UnadviseKeyEventSink(_tfClientId);
        }

        // 取消訂閱 ITfThreadMgrEventSink
        if (_dwThreadMgrEventSinkCookie != TF_INVALID_COOKIE) {
            if (wil::com_ptr_nothrow<ITfSource> pSource;
                SUCCEEDED(_pThreadMgr->QueryInterface(IID_PPV_ARGS(pSource.put())))) {
                pSource->UnadviseSink(_dwThreadMgrEventSinkCookie);
            }
            _dwThreadMgrEventSinkCookie = TF_INVALID_COOKIE;
        }

        _pThreadMgr.reset();
    }
    _tfClientId = TF_CLIENTID_NULL;
}

// ============================================================
//  ITfThreadMgrEventSink
// ============================================================

STDMETHODIMP CTextService::OnInitDocumentMgr(ITfDocumentMgr* /*pDocMgr*/) {
    return S_OK;
}

STDMETHODIMP CTextService::OnUninitDocumentMgr(ITfDocumentMgr* /*pDocMgr*/) {
    return S_OK;
}

STDMETHODIMP CTextService::OnSetFocus(ITfDocumentMgr* /*pDocMgrFocus*/, ITfDocumentMgr* /*pDocMgrPrevFocus*/) {
    // TODO: 焦點切換時可在此初始化 / 清除每文件的狀態
    return S_OK;
}

STDMETHODIMP CTextService::OnPushContext(ITfContext* /*pContext*/) {
    return S_OK;
}

STDMETHODIMP CTextService::OnPopContext(ITfContext* /*pContext*/) {
    return S_OK;
}

// ============================================================
//  ITfKeyEventSink
// ============================================================

STDMETHODIMP CTextService::OnSetFocus(BOOL /*fForeground*/) {
    return S_OK;
}

STDMETHODIMP CTextService::OnTestKeyDown(ITfContext* /*pContext*/, WPARAM wParam, LPARAM /*lParam*/, BOOL* pfEaten) {
    if (!pfEaten) return E_INVALIDARG;

    // TODO: 判斷此按鍵是否由 IME 消耗。
    //       OnKeyDown 用 ToUnicodeEx 轉換字元，這裡只需判斷 VK 範圍即可。
    *pfEaten = (wParam >= 'A' && wParam <= 'Z') ? TRUE : FALSE;
    return S_OK;
}

STDMETHODIMP CTextService::OnTestKeyUp(ITfContext* /*pContext*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL* pfEaten) {
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP CTextService::OnKeyDown(ITfContext* pContext, WPARAM wParam, LPARAM /*lParam*/, BOOL* pfEaten) {
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;

    // ── Enter：送出組字 ─────────────────────────────────────
    if (wParam == VK_RETURN && !_compositionBuffer.empty()) {
        DebugSink::instance().send(L"COMMIT", _compositionBuffer);
        _EndComposition(pContext);
        *pfEaten = TRUE;
        return S_OK;
    }

    // ── Escape：取消組字 ────────────────────────────────────
    if (wParam == VK_ESCAPE && _pComposition) {
        DebugSink::instance().send(L"CANCEL", _compositionBuffer);
        _compositionBuffer.clear();
        _SetCompositionText(pContext, L"");
        _EndComposition(pContext);
        *pfEaten = TRUE;
        return S_OK;
    }

    // ── Backspace：刪除最後一個字元 ─────────────────────────
    if (wParam == VK_BACK && !_compositionBuffer.empty()) {
        _compositionBuffer.pop_back();
        if (_compositionBuffer.empty())
            _EndComposition(pContext);
        else
            _SetCompositionText(pContext, _compositionBuffer);
        *pfEaten = TRUE;
        return S_OK;
    }

    // ── 一般可印字元 ────────────────────────────────────────
    // if (wParam >= 'A' && wParam <= 'Z') {
    //     // 使用 ToUnicodeEx 讓系統依目前鍵盤佈局 + Shift + Caps Lock 狀態轉換字元
    //     BYTE keyState[256]{};
    //     GetKeyboardState(keyState);
    //     wchar_t buf[4]{};
    //     const int len = ToUnicodeEx(
    //         static_cast<UINT>(wParam), MapVirtualKey(static_cast<UINT>(wParam), MAPVK_VK_TO_VSC), keyState, buf,
    //         ARRAYSIZE(buf), 0, GetKeyboardLayout(0));
    //     if (len != 1) return S_OK;  // dead key 或轉換失敗，不消耗

    //     if (!_pComposition) _StartComposition(pContext);

    //     _compositionBuffer += buf[0];
    //     _SetCompositionText(pContext, _compositionBuffer);
    //     *pfEaten = TRUE;
    // }
    auto cur_char = Bopomofo::lookup(static_cast<int>(wParam));
    if (cur_char == std::nullopt) {
        // 非注音
        // TODO
        return S_OK;
    } else {
        if (!_pComposition) {
            _StartComposition(pContext);
        }
        _compositionBuffer.push_back(cur_char.value());
        DebugSink::instance().send(L"KEY", _compositionBuffer);
        _SetCompositionText(pContext, _compositionBuffer);
        *pfEaten = TRUE;
        return S_OK;
    }
}

STDMETHODIMP CTextService::OnKeyUp(ITfContext* /*pContext*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL* pfEaten) {
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP CTextService::OnPreservedKey(ITfContext* /*pContext*/, REFGUID /*rguid*/, BOOL* pfEaten) {
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;
    return S_OK;
}

// ============================================================
//  ITfCompositionSink
// ============================================================

STDMETHODIMP CTextService::OnCompositionTerminated(TfEditCookie /*ecWrite*/, ITfComposition* /*pComposition*/) {
    _pComposition.reset();
    _compositionBuffer.clear();
    return S_OK;
}

// ============================================================
//  組字輔助函式
// ============================================================

HRESULT CTextService::_StartComposition(ITfContext* pContext) {
    if (_pComposition) return S_OK;  // 已在組字中

    wil::com_ptr_nothrow<ITfContextComposition> pContextComposition;
    RETURN_IF_FAILED(pContext->QueryInterface(IID_PPV_ARGS(pContextComposition.put())));

    wil::com_ptr_nothrow<ITfInsertAtSelection> pInsertAtSelection;
    RETURN_IF_FAILED(pContext->QueryInterface(IID_PPV_ARGS(pInsertAtSelection.put())));

    // TODO: 實作完整的 StartComposition + EditSession
    return S_OK;
}

HRESULT CTextService::_EndComposition(ITfContext* /*pContext*/) {
    if (!_pComposition) return S_OK;

    // TODO: 以 EditSession 送出最終文字後再結束組字
    _pComposition->EndComposition(TF_INVALID_COOKIE);
    _pComposition.reset();
    _compositionBuffer.clear();
    return S_OK;
}

HRESULT CTextService::_SetCompositionText(ITfContext* /*pContext*/, const std::wstring& /*text*/) {
    // TODO: 以 ITfRange + ITfProperty 更新組字字串與顯示屬性
    return S_OK;
}
