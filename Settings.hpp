// Settings.hpp --- SimpleBrowser settings
// Copyright (C) 2019 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
// This file is public domain software.

#ifndef SETTINGS_HPP_
#define SETTINGS_HPP_

#ifndef _INC_WINDOWS
    #include <windows.h>
#endif
#include <string>
#include <vector>

struct SETTINGS
{
    INT m_x, m_y, m_cx, m_cy;
    BOOL m_bMaximized;
    std::wstring m_homepage;
    std::vector<std::wstring> m_url_list;
    std::vector<std::wstring> m_black_list;
    BOOL m_secure;
    BOOL m_dont_r_click;
    BOOL m_local_file_access;
    BOOL m_dont_popup;
    BOOL m_ignore_errors;
    BOOL m_kiosk_mode;
    DWORD m_emulation;

    BOOL load();
    BOOL save();
    void reset();
};
extern SETTINGS g_settings;

void ShowSettingsDlg(HINSTANCE hInst, HWND hwnd, const std::wstring& strCurPage);

#endif  // ndef SETTINGS_HPP_
