#include <wil/com.h>

#include "tsf/classFactory.h"
#include "tsf/globals.h"
#include "tsf/register.h"

// ============================================================
//  DllMain
// ============================================================

BOOL APIENTRY DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID /*lpReserved*/) {
    switch (dwReason) {
        case DLL_PROCESS_ATTACH:
            g_hInst = hInstance;
            DisableThreadLibraryCalls(hInstance);
            break;

        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}

// ============================================================
//  COM 標準匯出函式
// ============================================================

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
    RETURN_HR_IF_NULL(E_INVALIDARG, ppv);
    *ppv = nullptr;
    RETURN_HR_IF(CLASS_E_CLASSNOTAVAILABLE, !IsEqualCLSID(rclsid, c_clsidTextService));

    wil::com_ptr_nothrow<CClassFactory> pFactory;
    pFactory.attach(new (std::nothrow) CClassFactory());
    RETURN_IF_NULL_ALLOC(pFactory);

    return pFactory->QueryInterface(riid, ppv);
}

STDAPI DllCanUnloadNow() {
    return (g_cDllRef == 0) ? S_OK : S_FALSE;
}

// ============================================================
//  自我登錄 / 反登錄（由 regsvr32 呼叫，需管理員權限）
// ============================================================

STDAPI DllRegisterServer() {
    return RegisterServer();
}

STDAPI DllUnregisterServer() {
    return UnregisterServer();
}
