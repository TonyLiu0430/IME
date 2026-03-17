#pragma once

#include <unknwn.h>
#include <windows.h>

/**
 * @brief Class factory for creating TSF service instances
 */
class CClassFactory : public IClassFactory {
public:
    CClassFactory();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IClassFactory
    STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv) override;
    STDMETHODIMP LockServer(BOOL fLock) override;

private:
    LONG _cRef;
};
