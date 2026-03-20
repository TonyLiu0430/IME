#include "textService.h"

#include "core/bopomofo.hpp"
#include "core/engine.hpp"
#include "system/globals.h"
#include "utils/debugSink.hpp"
#include "utils/healper.hpp"

// #include "core/bopomofo.h"
// #include "debugSink.h"

namespace tsf {

/**
 * @brief Implements ITfTextInputProcessor::Activate.
 *
 * Delegates activation to the shared setup path.
 */
STDMETHODIMP TextService::Activate(ITfThreadMgr* pThreadMgr, TfClientId tfClientId) {
    return activate(pThreadMgr, tfClientId);
}

/**
 * @brief Implements ITfTextInputProcessor::Deactivate.
 *
 * Tears down TSF state through the shared cleanup path.
 */
STDMETHODIMP TextService::Deactivate() {
    deactivate();
    return S_OK;
}

/**
 * @brief Implements ITfTextInputProcessorEx::ActivateEx.
 *
 * Uses the same activation flow and currently ignores extra flags.
 */
STDMETHODIMP TextService::ActivateEx(ITfThreadMgr* pThreadMgr, TfClientId tfClientId, DWORD /*dwFlags*/) {
    return activate(pThreadMgr, tfClientId);
}

/**
 * @brief Shared activation helper.
 *
 * Attaches TSF sinks, stores thread manager state, and starts debug logging.
 */
HRESULT TextService::activate(ITfThreadMgr* pThreadMgr, TfClientId tfClientId) {
    if (!pThreadMgr) return E_INVALIDARG;

    threadMgr.copy_from(pThreadMgr);
    _tfClientId = tfClientId;

    winrt::com_ptr<ITfSource> itfSource;
    HRESULT hr = threadMgr->QueryInterface<ITfSource>(itfSource.put());
    if (FAILED(hr)) {
        deactivate();
        return hr;
    }

    hr = itfSource->AdviseSink(
        IID_ITfThreadMgrEventSink, static_cast<ITfThreadMgrEventSink*>(this), &dwThreadMgrEventSinkCookie);
    if (FAILED(hr)) {
        deactivate();
        return hr;
    }

    winrt::com_ptr<ITfKeystrokeMgr> itfKeystrokeMgr;
    hr = threadMgr->QueryInterface<ITfKeystrokeMgr>(itfKeystrokeMgr.put());
    if (FAILED(hr)) {
        deactivate();
        return hr;
    }

    hr = itfKeystrokeMgr->AdviseKeyEventSink(tfClientId, static_cast<ITfKeyEventSink*>(this), TRUE);
    if (FAILED(hr)) {
        deactivate();
        return hr;
    }

    DebugSink::instance().connect();
    DebugSink::instance().send(L"IME", L"Activated");

    return S_OK;
}

/**
 * @brief Shared deactivation helper.
 *
 * Releases TSF sinks, clears composition state, and stops debug logging.
 */
void TextService::deactivate() {
    DebugSink::instance().send(L"IME", L"Deactivated");
    hide_candidate_list();
    DebugSink::instance().disconnect();

    if (itfComposition) {
        itfComposition->EndComposition(TF_INVALID_COOKIE);
        itfComposition = nullptr;
    }
    compositionBuffer.clear();

    if (threadMgr) {
        winrt::com_ptr<ITfKeystrokeMgr> itfKeystrokeMgr;
        if (SUCCEEDED(threadMgr->QueryInterface<ITfKeystrokeMgr>(itfKeystrokeMgr.put()))) {
            itfKeystrokeMgr->UnadviseKeyEventSink(_tfClientId);
        }

        if (dwThreadMgrEventSinkCookie != TF_INVALID_COOKIE) {
            winrt::com_ptr<ITfSource> pSource;
            if (SUCCEEDED(threadMgr->QueryInterface(IID_PPV_ARGS(pSource.put())))) {
                pSource->UnadviseSink(dwThreadMgrEventSinkCookie);
            }
            dwThreadMgrEventSinkCookie = TF_INVALID_COOKIE;
        }

        threadMgr = nullptr;
    }
    _tfClientId = TF_CLIENTID_NULL;
}

/**
 * @brief Implements ITfThreadMgrEventSink::OnInitDocumentMgr.
 *
 * No document-manager initialization is required yet.
 */
STDMETHODIMP TextService::OnInitDocumentMgr(ITfDocumentMgr* /*pDocMgr*/) {
    return S_OK;
}

/**
 * @brief Implements ITfThreadMgrEventSink::OnUninitDocumentMgr.
 *
 * No document-manager teardown is required yet.
 */
STDMETHODIMP TextService::OnUninitDocumentMgr(ITfDocumentMgr* /*pDocMgr*/) {
    return S_OK;
}

/**
 * @brief Implements ITfThreadMgrEventSink::OnSetFocus.
 *
 * Receives document focus changes but does not react to them yet.
 */
STDMETHODIMP TextService::OnSetFocus(ITfDocumentMgr* /*pDocMgrFocus*/, ITfDocumentMgr* /*pDocMgrPrevFocus*/) {
    // TODO: Initialize or clear per-document state on focus switch.
    return S_OK;
}

/**
 * @brief Implements ITfThreadMgrEventSink::OnPushContext.
 *
 * Accepts new contexts without additional bookkeeping.
 */
STDMETHODIMP TextService::OnPushContext(ITfContext* /*pContext*/) {
    return S_OK;
}

/**
 * @brief Implements ITfThreadMgrEventSink::OnPopContext.
 *
 * Releases contexts without additional cleanup.
 */
STDMETHODIMP TextService::OnPopContext(ITfContext* /*pContext*/) {
    return S_OK;
}

/**
 * @brief Implements ITfKeyEventSink::OnSetFocus.
 *
 * Tracks foreground changes but currently keeps no extra state.
 */
STDMETHODIMP TextService::OnSetFocus(BOOL /*fForeground*/) {
    return S_OK;
}

/**
 * @brief Implements ITfKeyEventSink::OnTestKeyDown.
 *
 * Reports whether the service intends to consume the key-down event.
 */
STDMETHODIMP TextService::OnTestKeyDown(ITfContext* /*pContext*/, WPARAM wParam, LPARAM /*lParam*/, BOOL* pfEaten) {
    if (!pfEaten) return E_INVALIDARG;
    DebugSink::instance().send(L"EVENT", L"OnTestKeyDown key=" + std::to_wstring(wParam));

    if (dwUIElementId != TF_INVALID_COOKIE) {
        const bool candidate_key =
            (wParam == VK_UP || wParam == VK_DOWN || wParam == VK_LEFT || wParam == VK_RIGHT || wParam == VK_RETURN ||
             wParam == VK_SPACE || wParam == VK_ESCAPE || (wParam >= '1' && wParam <= '9') ||
             (wParam >= VK_NUMPAD1 && wParam <= VK_NUMPAD9));
        *pfEaten = candidate_key ? TRUE : FALSE;
        DebugSink::instance().send(
            L"EVENT", L"OnTestKeyDown candidate mode, eaten=" + std::wstring(*pfEaten ? L"TRUE" : L"FALSE"));
        return S_OK;
    }

    if (!compositionBuffer.empty()) {
        const bool editing_key = (wParam == VK_RETURN || wParam == VK_ESCAPE || wParam == VK_BACK ||
                                  wParam == VK_DOWN || wParam == VK_RIGHT);
        if (editing_key) {
            *pfEaten = TRUE;
            return S_OK;
        }
    }

    *pfEaten = (Bopomofo::lookup(static_cast<int>(wParam)) != std::nullopt) ? TRUE : FALSE;
    return S_OK;
}

/**
 * @brief Implements ITfKeyEventSink::OnTestKeyUp.
 *
 * Leaves key-up events unhandled by default.
 */
STDMETHODIMP TextService::OnTestKeyUp(ITfContext* /*pContext*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL* pfEaten) {
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;
    return S_OK;
}

/**
 * @brief Implements ITfKeyEventSink::OnKeyDown.
 *
 * Updates the active composition or handles commit and cancel keys.
 */
STDMETHODIMP TextService::OnKeyDown(ITfContext* pContext, WPARAM wParam, LPARAM /*lParam*/, BOOL* pfEaten) {
    winrt::check_hresult(-1);
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;
    DebugSink::instance().send(L"EVENT", L"OnKeyDown");

    if (dwUIElementId != TF_INVALID_COOKIE) {
        UINT count = 0;
        UINT selection = 0;
        UINT currentPage = 0;
        const HRESULT hrCount = candidateListUIElement->GetCount(&count);
        const HRESULT hrSelection = candidateListUIElement->GetSelection(&selection);
        const HRESULT hrCurrentPage = candidateListUIElement->GetCurrentPage(&currentPage);
        DebugSink::instance().send(
            L"INFO", L"candidate key mode, key=" + std::to_wstring(wParam) + L", count=" + std::to_wstring(count) +
                         L", selection=" + std::to_wstring(selection) + L", currentPage=" +
                         std::to_wstring(currentPage) + L", hrCount=" + std::to_wstring(hrCount) + L", hrSelection=" +
                         std::to_wstring(hrSelection) + L", hrCurrentPage=" + std::to_wstring(hrCurrentPage));

        if (SUCCEEDED(hrCount) && SUCCEEDED(hrSelection) && count > 0) {
            constexpr UINT candidate_page_size = 9;
            auto finalize_and_commit = [&]() {
                candidateListUIElement->Finalize();
                if (!compositionBuffer.empty()) {
                    set_composition_text(pContext, compositionBuffer.to_string());
                }
                end_composition(pContext);
            };

            if (wParam == VK_UP) {
                candidateListUIElement->select_prev_in_page();
                *pfEaten = TRUE;
                return S_OK;
            }
            if (wParam == VK_DOWN) {
                candidateListUIElement->select_next_in_page();
                *pfEaten = TRUE;
                return S_OK;
            }
            if (wParam == VK_LEFT) {
                candidateListUIElement->page_prev();
                *pfEaten = TRUE;
                return S_OK;
            }
            if (wParam == VK_RIGHT) {
                if (!candidateListUIElement->is_expanded()) {
                    candidateListUIElement->expand();
                } else {
                    candidateListUIElement->page_next();
                }
                *pfEaten = TRUE;
                return S_OK;
            }
            if (wParam == VK_RETURN || wParam == VK_SPACE) {
                finalize_and_commit();
                *pfEaten = TRUE;
                return S_OK;
            }
            if (wParam == VK_ESCAPE) {
                candidateListUIElement->Abort();
                hide_candidate_list();
                *pfEaten = TRUE;
                return S_OK;
            }

            if (wParam >= '1' && wParam <= '9') {
                const UINT index = currentPage * candidate_page_size + static_cast<UINT>(wParam - '1');
                if (index < count) {
                    candidateListUIElement->SetSelection(index);
                    finalize_and_commit();
                    *pfEaten = TRUE;
                    return S_OK;
                }
            }
            if (wParam >= VK_NUMPAD1 && wParam <= VK_NUMPAD9) {
                const UINT index = currentPage * candidate_page_size + static_cast<UINT>(wParam - VK_NUMPAD1);
                if (index < count) {
                    candidateListUIElement->SetSelection(index);
                    finalize_and_commit();
                    *pfEaten = TRUE;
                    return S_OK;
                }
            }
        }

        DebugSink::instance().send(L"INFO", L"candidate key mode fallback -> hide candidate list");
        hide_candidate_list();
    }

    if (wParam == VK_RETURN && !compositionBuffer.empty()) {
        DebugSink::instance().send(L"COMMIT", compositionBuffer.to_string());
        end_composition(pContext);
        *pfEaten = TRUE;
        return S_OK;
    }

    if (wParam == VK_ESCAPE && itfComposition) {
        DebugSink::instance().send(L"CANCEL", compositionBuffer.to_string());
        compositionBuffer.clear();
        set_composition_text(pContext, L"");
        end_composition(pContext);
        *pfEaten = TRUE;
        return S_OK;
    }

    if (wParam == VK_BACK && !compositionBuffer.empty()) {
        compositionBuffer.pop_back();
        if (compositionBuffer.empty()) {
            end_composition(pContext);
        } else {
            set_composition_text(pContext, compositionBuffer.to_string());
        }
        *pfEaten = TRUE;
        return S_OK;
    }

    if (wParam == VK_DOWN && !compositionBuffer.empty()) {
        DebugSink::instance().send(
            L"INFO", L"Showing candidate list for composition: " + compositionBuffer.to_string());
        if (std::holds_alternative<Word>(compositionBuffer.back())) {
            show_candidate_list(
                pContext, compositionBuffer.back(),
                WordMappingEngine::instance().lookup_all(std::get<Word>(compositionBuffer.back()).bopomofo));
        } else {
            show_candidate_list(pContext, compositionBuffer.back(),
                                WordMappingEngine::instance().lookup_all(
                                    std::get<CompositionUnit>(compositionBuffer.back()).get_bopomofo()));
        }
        *pfEaten = TRUE;
        return S_OK;
    }

    if (wParam == VK_RIGHT && !compositionBuffer.empty()) {
        DebugSink::instance().send(L"INFO", L"Expand candidate list for composition: " + compositionBuffer.to_string());
        if (std::holds_alternative<Word>(compositionBuffer.back())) {
            show_candidate_list(
                pContext, compositionBuffer.back(),
                WordMappingEngine::instance().lookup_all(std::get<Word>(compositionBuffer.back()).bopomofo));
        } else {
            show_candidate_list(pContext, compositionBuffer.back(),
                                WordMappingEngine::instance().lookup_all(
                                    std::get<CompositionUnit>(compositionBuffer.back()).get_bopomofo()));
        }
        candidateListUIElement->expand();
        *pfEaten = TRUE;
        return S_OK;
    }
    auto cur_char = Bopomofo::lookup(static_cast<int>(wParam));
    if (cur_char == std::nullopt) {
        hide_candidate_list();
        // TODO: Handle non-Bopomofo keys.
        *pfEaten = FALSE;
        return S_OK;
    }

    // if (!itfComposition) {
    //     start_composition(pContext);
    // }
    compositionBuffer.add(cur_char.value());
    DebugSink::instance().send(L"KEY", compositionBuffer.to_string());
    set_composition_text(pContext, compositionBuffer.to_string());
    *pfEaten = TRUE;
    return S_OK;
}

/**
 * @brief Implements ITfKeyEventSink::OnKeyUp.
 *
 * Leaves key-up events unconsumed after key-down handling.
 */
STDMETHODIMP TextService::OnKeyUp(ITfContext* /*pContext*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL* pfEaten) {
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;
    return S_OK;
}

/**
 * @brief Implements ITfKeyEventSink::OnPreservedKey.
 *
 * Declines preserved-key handling because no preserved keys are registered.
 */
STDMETHODIMP TextService::OnPreservedKey(ITfContext* /*pContext*/, REFGUID /*rguid*/, BOOL* pfEaten) {
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;
    return S_OK;
}

/**
 * @brief Implements ITfCompositionSink::OnCompositionTerminated.
 *
 * Clears local composition state when TSF ends the composition externally.
 */
STDMETHODIMP TextService::OnCompositionTerminated(TfEditCookie /*ecWrite*/, ITfComposition* /*pComposition*/) {
    hide_candidate_list();
    itfComposition = nullptr;
    compositionBuffer.clear();
    return S_OK;
}

/**
 * @brief Implements ITfDisplayAttributeProvider::EnumDisplayAttributeInfo.
 *
 * Returns not implemented because display attributes are not exposed yet.
 */
STDMETHODIMP TextService::EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** ppEnum) {
    (void)ppEnum;
    return E_NOTIMPL;
}

/**
 * @brief Implements ITfDisplayAttributeProvider::GetDisplayAttributeInfo.
 *
 * Returns not implemented because no display attribute metadata is defined yet.
 */
STDMETHODIMP_(HRESULT __stdcall) TextService::GetDisplayAttributeInfo(REFGUID guid, ITfDisplayAttributeInfo** ppInfo) {
    (void)guid;
    (void)ppInfo;
    return E_NOTIMPL;
}

/**
 * @brief Starts a new TSF composition session.
 *
 * Creates the TSF composition objects needed for a new input session.
 */
HRESULT TextService::start_composition(ITfContext* pContext) {
    if (!pContext) return E_INVALIDARG;
    if (itfComposition) return S_OK;

    winrt::com_ptr<ITfContextComposition> contextComposition;
    HRESULT hr = pContext->QueryInterface<ITfContextComposition>(contextComposition.put());
    if (FAILED(hr)) return hr;

    winrt::com_ptr<ITfInsertAtSelection> insertAtSelection;
    hr = pContext->QueryInterface<ITfInsertAtSelection>(insertAtSelection.put());
    if (FAILED(hr)) return hr;

    winrt::com_ptr<EditSession> editSession = winrt::make_self<EditSession>();

    editSession->set_operation([this, contextComposition, insertAtSelection](TfEditCookie ec) {
        winrt::com_ptr<ITfRange> range;
        if (FAILED(insertAtSelection->InsertTextAtSelection(ec, TF_IAS_QUERYONLY, L"", 0, range.put()))) {
            return;
        }
        if (FAILED(contextComposition->StartComposition(
                ec, range.get(), static_cast<ITfCompositionSink*>(this), itfComposition.put()))) {
            return;
        }
    });

    HRESULT hrSession;
    pContext->RequestEditSession(_tfClientId, editSession.get(), TF_ES_READWRITE | TF_ES_SYNC, &hrSession) |
        win::check();

    return S_OK;
}

/**
 * @brief Ends the active TSF composition session.
 *
 * Clears the current composition object and buffered text.
 */
HRESULT TextService::end_composition(ITfContext* pContext) {
    hide_candidate_list();
    if (!itfComposition) return S_OK;

    winrt::com_ptr<EditSession> editSession = winrt::make_self<EditSession>();
    editSession->set_operation([this](TfEditCookie ec) {
        if (itfComposition) {
            itfComposition->EndComposition(ec);
            itfComposition = nullptr;
            compositionBuffer.clear();
        }
    });

    HRESULT hrSession;
    pContext->RequestEditSession(_tfClientId, editSession.get(), TF_ES_READWRITE | TF_ES_SYNC, &hrSession) |
        win::check();

    return S_OK;
}

/**
 * @brief Updates the active composition text.
 *
 * Applies the visible composition string to the current TSF context.
 */
HRESULT TextService::set_composition_text(ITfContext* pContext, const std::wstring& text) {
    // if (!itfComposition) return E_FAIL;

    winrt::com_ptr<ITfContextComposition> contextComposition;
    HRESULT hr = pContext->QueryInterface<ITfContextComposition>(contextComposition.put());
    if (FAILED(hr)) return hr;

    winrt::com_ptr<ITfInsertAtSelection> insertAtSelection;
    hr = pContext->QueryInterface<ITfInsertAtSelection>(insertAtSelection.put());
    if (FAILED(hr)) return hr;

    // TODO: Update composition string via ITfRange and ITfProperty.
    winrt::com_ptr<EditSession> editSession = winrt::make_self<EditSession>();
    editSession->set_operation([=](TfEditCookie ec) {
        winrt::com_ptr<ITfRange> range;
        // insertAtSelection->InsertTextAtSelection(ec, TF_IAS_QUERYONLY, L"", 0, range.put()) | win::check();
        if (!itfComposition) {
            insertAtSelection->InsertTextAtSelection(ec, TF_IAS_QUERYONLY, nullptr, 0, range.put()) | win::check();
            contextComposition->StartComposition(
                ec, range.get(), static_cast<ITfCompositionSink*>(this), itfComposition.put()) |
                win::check();
        }
        range = nullptr;
        itfComposition->GetRange(range.put()) | win::check();
        range->SetText(ec, 0, text.data(), ULONG(text.size())) | win::check();
        range->Collapse(ec, TF_ANCHOR_END) | win::check();

        TF_SELECTION selection = {};
        selection.range = range.get();
        selection.style.ase = TF_AE_END;
        selection.style.fInterimChar = FALSE;
        pContext->SetSelection(ec, 1, &selection) | win::check();
    });
    pContext->RequestEditSession(_tfClientId, editSession.get(), TF_ES_READWRITE | TF_ES_SYNC, &hr) | win::check();
    return S_OK;
}

bool TextService::query_candidate_anchor(ITfContext* pContext, POINT* anchor) {
    if (!pContext || !anchor) {
        return false;
    }

    winrt::com_ptr<ITfContextView> contextView;
    HRESULT hr = pContext->GetActiveView(contextView.put());
    if (FAILED(hr) || !contextView) {
        DebugSink::instance().send(L"INFO", L"query_candidate_anchor GetActiveView failed hr=" + std::to_wstring(hr));
        return false;
    }

    POINT point = {};
    bool found = false;
    winrt::com_ptr<EditSession> editSession = winrt::make_self<EditSession>();
    editSession->set_operation([pContext, contextView, &point, &found](TfEditCookie ec) {
        TF_SELECTION selection = {};
        ULONG fetched = 0;
        const HRESULT hrSelection = pContext->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &selection, &fetched);
        if (FAILED(hrSelection) || fetched == 0 || !selection.range) {
            DebugSink::instance().send(
                L"INFO", L"query_candidate_anchor GetSelection failed hr=" + std::to_wstring(hrSelection));
            return;
        }

        winrt::com_ptr<ITfRange> range;
        range.attach(selection.range);

        RECT rc = {};
        BOOL clipped = FALSE;
        const HRESULT hrTextExt = contextView->GetTextExt(ec, range.get(), &rc, &clipped);
        if (FAILED(hrTextExt)) {
            DebugSink::instance().send(
                L"INFO", L"query_candidate_anchor GetTextExt failed hr=" + std::to_wstring(hrTextExt));
            return;
        }

        point.x = rc.left;
        point.y = rc.bottom;
        found = true;
        DebugSink::instance().send(
            L"INFO", L"query_candidate_anchor rect=(" + std::to_wstring(rc.left) + L"," + std::to_wstring(rc.top) +
                         L"," + std::to_wstring(rc.right) + L"," + std::to_wstring(rc.bottom) + L"), clipped=" +
                         std::wstring(clipped ? L"TRUE" : L"FALSE"));
    });

    HRESULT hrSession = E_FAIL;
    hr = pContext->RequestEditSession(_tfClientId, editSession.get(), TF_ES_READ | TF_ES_SYNC, &hrSession);
    if (FAILED(hr) || FAILED(hrSession)) {
        DebugSink::instance().send(L"INFO", L"query_candidate_anchor RequestEditSession failed hr=" +
                                                std::to_wstring(hr) + L", hrSession=" + std::to_wstring(hrSession));
        return false;
    }

    if (!found) {
        return false;
    }

    *anchor = point;
    return true;
}

void TextService::hide_candidate_list() {
    candidateListUIElement->Show(FALSE);
    candidateListUIElement->clear_anchor_point();

    if (dwUIElementId == TF_INVALID_COOKIE) {
        DebugSink::instance().send(L"INFO", L"hide_candidate_list no active ui id");
        return;
    }

    DebugSink::instance().send(L"INFO", L"hide_candidate_list ui_id=" + std::to_wstring(dwUIElementId));

    if (threadMgr) {
        winrt::com_ptr<ITfUIElementMgr> itfUIElementMgr;
        const HRESULT hr = threadMgr->QueryInterface<ITfUIElementMgr>(itfUIElementMgr.put());
        if (SUCCEEDED(hr) && itfUIElementMgr) {
            const HRESULT end_hr = itfUIElementMgr->EndUIElement(dwUIElementId);
            DebugSink::instance().send(L"INFO", L"hide_candidate_list EndUIElement hr=" + std::to_wstring(end_hr) +
                                                    L", ui_id=" + std::to_wstring(dwUIElementId));
        } else {
            DebugSink::instance().send(
                L"INFO", L"hide_candidate_list QueryInterface ITfUIElementMgr failed hr=" + std::to_wstring(hr));
        }
    }

    dwUIElementId = TF_INVALID_COOKIE;
}

/**
 * @brief Displays the candidate list UI with the given candidates.
 */
void TextService::show_candidate_list(ITfContext* pContext, std::variant<Word, CompositionUnit>& pos,
                                      const std::vector<std::wstring>& candidates) {
    DebugSink::instance().send(L"INFO", L"show_candidate_list enter, candidates=" + std::to_wstring(candidates.size()) +
                                            L", existing_ui_id=" + std::to_wstring(dwUIElementId));

    winrt::com_ptr<ITfUIElementMgr> itfUIElementMgr;
    HRESULT hr = threadMgr->QueryInterface<ITfUIElementMgr>(itfUIElementMgr.put());
    if (FAILED(hr)) {
        DebugSink::instance().send(
            L"ERROR", L"show_candidate_list QueryInterface ITfUIElementMgr failed: " + std::to_wstring(hr));
        return;
    }

    POINT anchor = {};
    if (query_candidate_anchor(pContext, &anchor)) {
        candidateListUIElement->set_anchor_point(anchor);
    } else {
        candidateListUIElement->clear_anchor_point();
    }

    candidateListUIElement->update(candidates, [this, itfUIElementMgr, &pos](std::wstring word) {
        DebugSink::instance().send(L"INFO", L"candidate finalize callback word=" + word);
        if (std::holds_alternative<Word>(pos)) {
            auto& w = std::get<Word>(pos);
            w.word = word;
        } else {
            auto& v = std::get<CompositionUnit>(pos);
            pos = Word(word, v.get_bopomofo());
        }
        const HRESULT end_hr = itfUIElementMgr->EndUIElement(dwUIElementId);
        DebugSink::instance().send(
            L"INFO", L"EndUIElement hr=" + std::to_wstring(end_hr) + L", ui_id=" + std::to_wstring(dwUIElementId));
        dwUIElementId = TF_INVALID_COOKIE;
        candidateListUIElement->Show(FALSE);
        candidateListUIElement->clear_anchor_point();
    });

    if (dwUIElementId != TF_INVALID_COOKIE) {
        DebugSink::instance().send(
            L"INFO", L"show_candidate_list reusing existing UI element id=" + std::to_wstring(dwUIElementId));
        const HRESULT update_hr = itfUIElementMgr->UpdateUIElement(dwUIElementId);
        DebugSink::instance().send(
            L"INFO",
            L"UpdateUIElement hr=" + std::to_wstring(update_hr) + L", ui_id=" + std::to_wstring(dwUIElementId));
        const HRESULT show_hr = candidateListUIElement->Show(TRUE);
        DebugSink::instance().send(L"INFO", L"Force Show(TRUE) hr=" + std::to_wstring(show_hr));
        return;
    }

    BOOL bShow = TRUE;
    hr = itfUIElementMgr->BeginUIElement(candidateListUIElement.get(), &bShow, &dwUIElementId);
    DebugSink::instance().send(
        L"INFO", L"BeginUIElement returned: " + std::to_wstring(hr) + L", bShow: " + (bShow ? L"TRUE" : L"FALSE") +
                     L", dwUIElementId: " + std::to_wstring(dwUIElementId) + L", candidate_count=" +
                     std::to_wstring(candidates.size()));

    if (FAILED(hr)) {
        return;
    }
    if (bShow) {
        const HRESULT show_hr = candidateListUIElement->Show(TRUE);
        DebugSink::instance().send(
            L"INFO", L"BeginUIElement requested service-side UI, Show(TRUE) hr=" + std::to_wstring(show_hr));
    } else {
        DebugSink::instance().send(L"INFO", L"BeginUIElement bShow=FALSE, app side may render UI");
    }
}

}  // namespace tsf
