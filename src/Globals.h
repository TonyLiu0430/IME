#pragma once

#include <windows.h>

#include <atomic>

inline HINSTANCE g_hInst = nullptr;      // DLL 模組 handle
inline std::atomic<LONG> g_cDllRef = 0;  // DLL 參考計數

// ============================================================
//  TSF 語言 ID  —  0x0404 = 正體中文（台灣）
// ============================================================
constexpr LANGID c_LangId = MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL);

// ============================================================
//  Text Service 的 CLSID
//  {C262415B-2D5F-4DA8-A7F5-0798802ECFAC}
// ============================================================
inline const CLSID c_clsidTextService = {0xc262415b, 0x2d5f, 0x4da8, {0xa7, 0xf5, 0x07, 0x98, 0x80, 0x2e, 0xcf, 0xac}};

// ============================================================
//  語言設定檔 GUID
//  {B2C3D4E5-F6A7-8901-BCDE-F01234567891}
// ============================================================
inline const GUID c_guidProfile = {0xb2c3d4e5, 0xf6a7, 0x8901, {0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x91}};

// ============================================================
//  Text Service 顯示名稱
// ============================================================
constexpr wchar_t c_szDesc[] = L"My TSF IME " __DATE__ " " __TIME__;
