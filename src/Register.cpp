#include "Register.h"

#include <msctf.h>
#include <objbase.h>  // StringFromGUID2

#include <string>

#include "Globals.h"

// ============================================================
//  內部輔助：寫入 REG_SZ 登錄值
// ============================================================
static HRESULT SetRegString(HKEY hKey, const wchar_t* pszName, const wchar_t* pszValue) {
    DWORD cb = static_cast<DWORD>((wcslen(pszValue) + 1) * sizeof(wchar_t));
    LONG lr = RegSetValueExW(hKey, pszName, 0, REG_SZ, reinterpret_cast<const BYTE*>(pszValue), cb);
    return HRESULT_FROM_WIN32(lr);
}

// ============================================================
//  在 HKEY_LOCAL_MACHINE 下登錄 COM In-Process Server
// ============================================================
static HRESULT RegisterCOMServer(const wchar_t* pszCLSID, const wchar_t* pszDllPath) {
    // HKLM\Software\Classes\CLSID\{...}
    const std::wstring clsidKey = std::wstring(L"Software\\Classes\\CLSID\\") + pszCLSID;
    HKEY hKey = nullptr;
    LONG lr = RegCreateKeyExW(
        HKEY_LOCAL_MACHINE, clsidKey.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
    if (lr != ERROR_SUCCESS) return HRESULT_FROM_WIN32(lr);

    HRESULT hr = SetRegString(hKey, nullptr, c_szDesc);
    RegCloseKey(hKey);
    if (FAILED(hr)) return hr;

    // HKLM\Software\Classes\CLSID\{...}\InprocServer32
    const std::wstring inprocKey = clsidKey + L"\\InprocServer32";
    lr = RegCreateKeyExW(
        HKEY_LOCAL_MACHINE, inprocKey.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
    if (lr != ERROR_SUCCESS) return HRESULT_FROM_WIN32(lr);

    hr = SetRegString(hKey, nullptr, pszDllPath);
    if (SUCCEEDED(hr)) hr = SetRegString(hKey, L"ThreadingModel", L"Apartment");

    RegCloseKey(hKey);
    return hr;
}

// ============================================================
//  RegisterServer  – 由 DllRegisterServer 呼叫
// ============================================================
HRESULT RegisterServer() {
    // 取得本 DLL 的完整路徑
    wchar_t szDllPath[MAX_PATH] = {};
    if (GetModuleFileNameW(g_hInst, szDllPath, MAX_PATH) == 0) return HRESULT_FROM_WIN32(GetLastError());

    // 將 CLSID 轉為字串形式  {xxxxxxxx-...}
    wchar_t szCLSID[64] = {};
    if (StringFromGUID2(c_clsidTextService, szCLSID, ARRAYSIZE(szCLSID)) == 0) return E_FAIL;

    // 1. 登錄 COM In-Process Server
    HRESULT hr = RegisterCOMServer(szCLSID, szDllPath);
    if (FAILED(hr)) return hr;

    // 2. 向 TSF 登錄 Text Service
    ITfInputProcessorProfiles* pProfiles = nullptr;
    hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER, IID_ITfInputProcessorProfiles,
                          reinterpret_cast<void**>(&pProfiles));
    if (FAILED(hr)) return hr;

    hr = pProfiles->Register(c_clsidTextService);
    if (SUCCEEDED(hr)) {
        hr = pProfiles->AddLanguageProfile(
            c_clsidTextService, c_LangId, c_guidProfile, c_szDesc, static_cast<ULONG>(wcslen(c_szDesc)),
            szDllPath,  // 圖示來源（可換為獨立 .ico）
            static_cast<ULONG>(wcslen(szDllPath)),
            0  // 圖示索引
        );
    }
    pProfiles->Release();
    if (FAILED(hr)) return hr;

    // 3. 向 TSF 註冊分類：將此 Text Service 標記為「鍵盤」類型
    //    缺少此步驟的話，Windows 設定中新增的輸入法無法持久保留。
    ITfCategoryMgr* pCategoryMgr = nullptr;
    hr = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr,
                          reinterpret_cast<void**>(&pCategoryMgr));
    if (SUCCEEDED(hr)) {
        hr = pCategoryMgr->RegisterCategory(c_clsidTextService, GUID_TFCAT_TIP_KEYBOARD, c_clsidTextService);
        pCategoryMgr->Release();
    }

    return hr;
}

// ============================================================
//  UnregisterServer  – 由 DllUnregisterServer 呼叫
// ============================================================
HRESULT UnregisterServer() {
    wchar_t szCLSID[64] = {};
    if (StringFromGUID2(c_clsidTextService, szCLSID, ARRAYSIZE(szCLSID)) == 0) return E_FAIL;

    // 1. 向 TSF 反登錄分類
    ITfCategoryMgr* pCategoryMgr = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr,
                                   reinterpret_cast<void**>(&pCategoryMgr)))) {
        pCategoryMgr->UnregisterCategory(c_clsidTextService, GUID_TFCAT_TIP_KEYBOARD, c_clsidTextService);
        pCategoryMgr->Release();
    }

    // 2. 向 TSF 反登錄 Profile
    ITfInputProcessorProfiles* pProfiles = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_ITfInputProcessorProfiles, reinterpret_cast<void**>(&pProfiles)))) {
        pProfiles->RemoveLanguageProfile(c_clsidTextService, c_LangId, c_guidProfile);
        pProfiles->Unregister(c_clsidTextService);
        pProfiles->Release();
    }

    // 3. 從登錄檔移除 COM 項目
    const std::wstring clsidKey = std::wstring(L"Software\\Classes\\CLSID\\") + szCLSID;
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, clsidKey.c_str());

    return S_OK;
}
