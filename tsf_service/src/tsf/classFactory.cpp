#include "tsf/classFactory.h"

#include <wil/com.h>

#include "tsf/globals.h"
#include "tsf/textService.h"

CClassFactory::CClassFactory() : _cRef(1) {
    ++g_cDllRef;
}

STDMETHODIMP CClassFactory::QueryInterface(REFIID riid, void** ppv) {
    RETURN_HR_IF_NULL(E_INVALIDARG, ppv);
    *ppv = nullptr;

    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
        *ppv = static_cast<IClassFactory*>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CClassFactory::AddRef() {
    return InterlockedIncrement(&_cRef);
}

STDMETHODIMP_(ULONG) CClassFactory::Release() {
    LONG cRef = InterlockedDecrement(&_cRef);
    if (cRef == 0) {
        --g_cDllRef;
        delete this;
    }
    return static_cast<ULONG>(cRef);
}

STDMETHODIMP CClassFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv) {
    RETURN_HR_IF_NULL(E_INVALIDARG, ppv);
    *ppv = nullptr;
    RETURN_HR_IF(CLASS_E_NOAGGREGATION, pUnkOuter != nullptr);

    // wil::com_ptr_nothrow 接管生命週期，離開 scope 自動 Release
    wil::com_ptr_nothrow<CTextService> pService;
    pService.attach(new (std::nothrow) CTextService());
    RETURN_IF_NULL_ALLOC(pService);

    return pService->QueryInterface(riid, ppv);
}

STDMETHODIMP CClassFactory::LockServer(BOOL fLock) {
    if (fLock)
        ++g_cDllRef;
    else
        --g_cDllRef;
    return S_OK;
}
