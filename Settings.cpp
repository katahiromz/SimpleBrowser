// Settings.cpp --- SimpleBrowser settings
// Copyright (C) 2019 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
// This file is public domain software.

#include "Settings.hpp"
#include <windowsx.h>
#include <shlwapi.h>
#include <strsafe.h>
#include "URLListDlg.hpp"
#include "BlackListDlg.hpp"
#include "resource.h"

SETTINGS g_settings;
static std::wstring s_strCurPage;

LPTSTR LoadStringDx(INT nID);

void SETTINGS::reset()
{
    m_x = CW_USEDEFAULT;
    m_y = CW_USEDEFAULT;
    m_cx = CW_USEDEFAULT;
    m_cy = CW_USEDEFAULT;
    m_bMaximized = FALSE;

    m_homepage = LoadStringDx(IDS_HOMEPAGE);
    m_url_list.clear();
    m_black_list.clear();
    m_secure = TRUE;
    m_dont_r_click = FALSE;
    m_local_file_access = TRUE;
    m_dont_popup = FALSE;
    m_ignore_errors = TRUE;
    m_kiosk_mode = FALSE;
    m_no_virus_scan = FALSE;
    m_zone_ident = TRUE;
    m_emulation = 11001;
    m_refresh_interval = (30 * 1000);  // 30 seconds
}

BOOL SETTINGS::load()
{
    reset();

    static const TCHAR s_szSubKey[] =
        TEXT("SOFTWARE\\Katayama Hirofumi MZ\\SimpleBrowser");

    BOOL bOK = FALSE;
    HKEY hApp = NULL;
    RegOpenKeyEx(HKEY_CURRENT_USER, s_szSubKey, 0, KEY_READ, &hApp);
    if (hApp)
    {
        WCHAR szText[256];
        DWORD value, cb;

        value = m_x;
        cb = sizeof(szText);
        RegQueryValueEx(hApp, L"X", NULL, NULL, (LPBYTE)&value, &cb);
        m_x = value;

        value = m_y;
        cb = sizeof(szText);
        RegQueryValueEx(hApp, L"Y", NULL, NULL, (LPBYTE)&value, &cb);
        m_y = value;

        value = m_cx;
        cb = sizeof(szText);
        RegQueryValueEx(hApp, L"CX", NULL, NULL, (LPBYTE)&value, &cb);
        m_cx = value;

        value = m_cy;
        cb = sizeof(szText);
        RegQueryValueEx(hApp, L"CY", NULL, NULL, (LPBYTE)&value, &cb);
        m_cy = value;

        value = m_bMaximized;
        cb = sizeof(szText);
        RegQueryValueEx(hApp, L"Maximized", NULL, NULL, (LPBYTE)&value, &cb);
        m_bMaximized = !!value;

        cb = sizeof(szText);
        RegQueryValueEx(hApp, L"Homepage", NULL, NULL, (LPBYTE)szText, &cb);
        m_homepage = szText;

        value = m_secure;
        cb = sizeof(value);
        RegQueryValueEx(hApp, L"Secure", NULL, NULL, (LPBYTE)&value, &cb);
        m_secure = !!value;

        value = m_dont_r_click;
        cb = sizeof(value);
        RegQueryValueEx(hApp, L"DontRClick", NULL, NULL, (LPBYTE)&value, &cb);
        m_dont_r_click = !!value;

        value = m_local_file_access;
        cb = sizeof(value);
        RegQueryValueEx(hApp, L"LocalFileAccess", NULL, NULL, (LPBYTE)&value, &cb);
        m_local_file_access = !!value;

        value = m_dont_popup;
        cb = sizeof(value);
        RegQueryValueEx(hApp, L"DontPopup", NULL, NULL, (LPBYTE)&value, &cb);
        m_dont_popup = !!value;

        value = m_ignore_errors;
        cb = sizeof(value);
        RegQueryValueEx(hApp, L"IgnoreErrors", NULL, NULL, (LPBYTE)&value, &cb);
        m_ignore_errors = !!value;

        //value = m_kiosk_mode;
        //cb = sizeof(value);
        //RegQueryValueEx(hApp, L"KioskMode", NULL, NULL, (LPBYTE)&value, &cb);
        //m_kiosk_mode = !!value;

        value = m_no_virus_scan;
        cb = sizeof(value);
        RegQueryValueEx(hApp, L"NoVirusScan", NULL, NULL, (LPBYTE)&value, &cb);
        m_no_virus_scan = !!value;

        value = m_zone_ident;
        cb = sizeof(value);
        RegQueryValueEx(hApp, L"ZoneIdent", NULL, NULL, (LPBYTE)&value, &cb);
        m_zone_ident = !!value;

        value = m_emulation;
        cb = sizeof(value);
        RegQueryValueEx(hApp, L"Emulation", NULL, NULL, (LPBYTE)&value, &cb);
        m_emulation = value;

        value = m_refresh_interval;
        cb = sizeof(value);
        RegQueryValueEx(hApp, L"RefreshInterval", NULL, NULL, (LPBYTE)&value, &cb);
        m_refresh_interval = value;

        DWORD count = 0;
        cb = sizeof(count);
        RegQueryValueEx(hApp, L"URLCount", NULL, NULL, (LPBYTE)&count, &cb);

        WCHAR szName[64];

        for (DWORD i = 0; i < count; ++i)
        {
            StringCbPrintfW(szName, sizeof(szName), L"URL%lu", i);

            cb = sizeof(szText);
            if (!RegQueryValueEx(hApp, szName, NULL, NULL, (LPBYTE)szText, &cb))
            {
                StrTrimW(szText, L" \t\n\r\f\v");
                m_url_list.push_back(szText);
            }
            else
            {
                break;
            }
        }

        cb = sizeof(count);
        RegQueryValueEx(hApp, L"ForbiddenCount", NULL, NULL, (LPBYTE)&count, &cb);

        for (DWORD i = 0; i < count; ++i)
        {
            StringCbPrintfW(szName, sizeof(szName), L"Forbidden%lu", i);

            cb = sizeof(szText);
            if (!RegQueryValueEx(hApp, szName, NULL, NULL, (LPBYTE)szText, &cb))
            {
                StrTrimW(szText, L" \t\n\r\f\v");
                m_black_list.push_back(szText);
            }
            else
            {
                break;
            }
        }

        bOK = TRUE;
    }

    return bOK;
}

BOOL SETTINGS::save()
{
    HKEY hSoftware = NULL;
    HKEY hCompany = NULL;
    HKEY hApp = NULL;
    BOOL bOK = FALSE;
    RegCreateKeyEx(HKEY_CURRENT_USER, TEXT("SOFTWARE"), 0, NULL, 0,
                   KEY_ALL_ACCESS, NULL, &hSoftware, NULL);
    if (hSoftware)
    {
        RegCreateKeyEx(hSoftware, TEXT("Katayama Hirofumi MZ"), 0, NULL, 0,
                       KEY_ALL_ACCESS, NULL, &hCompany, NULL);
        if (hCompany)
        {
            RegCreateKeyEx(hCompany, TEXT("SimpleBrowser"), 0, NULL, 0,
                           KEY_ALL_ACCESS, NULL, &hApp, NULL);
            if (hApp)
            {
                DWORD value, cb;
                WCHAR szName[64];

                cb = DWORD((m_homepage.size() + 1) * sizeof(WCHAR));
                RegSetValueEx(hApp, L"Homepage", 0, REG_SZ, (LPBYTE)m_homepage.c_str(), cb);

                value = DWORD(m_x);
                cb = DWORD(sizeof(value));
                RegSetValueEx(hApp, L"X", 0, REG_DWORD, (LPBYTE)&value, cb);

                value = DWORD(m_y);
                cb = DWORD(sizeof(value));
                RegSetValueEx(hApp, L"Y", 0, REG_DWORD, (LPBYTE)&value, cb);

                value = DWORD(m_cx);
                cb = DWORD(sizeof(value));
                RegSetValueEx(hApp, L"CX", 0, REG_DWORD, (LPBYTE)&value, cb);

                value = DWORD(m_cy);
                cb = DWORD(sizeof(value));
                RegSetValueEx(hApp, L"CY", 0, REG_DWORD, (LPBYTE)&value, cb);

                value = DWORD(m_bMaximized);
                cb = DWORD(sizeof(value));
                RegSetValueEx(hApp, L"Maximized", 0, REG_DWORD, (LPBYTE)&value, cb);

                value = DWORD(m_secure);
                cb = DWORD(sizeof(value));
                RegSetValueEx(hApp, L"Secure", 0, REG_DWORD, (LPBYTE)&value, cb);

                value = DWORD(m_dont_r_click);
                cb = DWORD(sizeof(value));
                RegSetValueEx(hApp, L"DontRClick", 0, REG_DWORD, (LPBYTE)&value, cb);

                value = DWORD(m_local_file_access);
                cb = DWORD(sizeof(value));
                RegSetValueEx(hApp, L"LocalFileAccess", 0, REG_DWORD, (LPBYTE)&value, cb);

                value = DWORD(m_dont_popup);
                cb = DWORD(sizeof(value));
                RegSetValueEx(hApp, L"DontPopup", 0, REG_DWORD, (LPBYTE)&value, cb);

                value = DWORD(m_ignore_errors);
                cb = DWORD(sizeof(value));
                RegSetValueEx(hApp, L"IgnoreErrors", 0, REG_DWORD, (LPBYTE)&value, cb);

                //value = DWORD(m_kiosk_mode);
                //cb = DWORD(sizeof(value));
                //RegSetValueEx(hApp, L"KioskMode", 0, REG_DWORD, (LPBYTE)&value, cb);

                value = DWORD(m_no_virus_scan);
                cb = DWORD(sizeof(value));
                RegSetValueEx(hApp, L"NoVirusScan", 0, REG_DWORD, (LPBYTE)&value, cb);

                value = DWORD(m_zone_ident);
                cb = DWORD(sizeof(value));
                RegSetValueEx(hApp, L"ZoneIdent", 0, REG_DWORD, (LPBYTE)&value, cb);

                value = DWORD(m_emulation);
                cb = DWORD(sizeof(value));
                RegSetValueEx(hApp, L"Emulation", 0, REG_DWORD, (LPBYTE)&value, cb);

                value = DWORD(m_refresh_interval);
                cb = DWORD(sizeof(value));
                RegSetValueEx(hApp, L"RefreshInterval", 0, REG_DWORD, (LPBYTE)&value, cb);

                DWORD count = DWORD(m_url_list.size());
                cb = DWORD(sizeof(count));
                RegSetValueEx(hApp, L"URLCount", 0, REG_DWORD, (LPBYTE)&count, cb);

                for (DWORD i = 0; i < count; ++i)
                {
                    std::wstring& url = m_url_list[i];

                    StringCbPrintfW(szName, sizeof(szName), L"URL%lu", i);

                    cb = DWORD((url.size() + 1) * sizeof(WCHAR));
                    RegSetValueEx(hApp, szName, 0, REG_DWORD, (LPBYTE)url.c_str(), cb);
                }

                count = DWORD(m_black_list.size());
                cb = DWORD(sizeof(count));
                RegSetValueEx(hApp, L"ForbiddenCount", 0, REG_DWORD, (LPBYTE)&count, cb);

                for (DWORD i = 0; i < count; ++i)
                {
                    std::wstring& url = m_black_list[i];

                    StringCbPrintfW(szName, sizeof(szName), L"Forbidden%lu", i);

                    cb = DWORD((url.size() + 1) * sizeof(WCHAR));
                    RegSetValueEx(hApp, szName, 0, REG_DWORD, (LPBYTE)url.c_str(), cb);
                }

                bOK = TRUE;
                RegCloseKey(hApp);
            }
            RegCloseKey(hCompany);
        }
        RegCloseKey(hSoftware);
    }

    return bOK;
}

static BOOL OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    if (g_settings.m_secure)
        CheckDlgButton(hwnd, chx1, BST_CHECKED);
    if (g_settings.m_local_file_access)
        CheckDlgButton(hwnd, chx2, BST_CHECKED);
    if (g_settings.m_dont_r_click)
        CheckDlgButton(hwnd, chx3, BST_CHECKED);
    if (g_settings.m_dont_popup)
        CheckDlgButton(hwnd, chx4, BST_CHECKED);
    if (g_settings.m_ignore_errors)
        CheckDlgButton(hwnd, chx5, BST_CHECKED);
    if (g_settings.m_kiosk_mode)
        CheckDlgButton(hwnd, chx6, BST_CHECKED);
    if (g_settings.m_no_virus_scan)
        CheckDlgButton(hwnd, chx7, BST_CHECKED);
    if (g_settings.m_zone_ident)
        CheckDlgButton(hwnd, chx8, BST_CHECKED);

    SetDlgItemText(hwnd, edt1, g_settings.m_homepage.c_str());
    SetDlgItemInt(hwnd, edt2, g_settings.m_emulation, TRUE);

    if (g_settings.m_kiosk_mode)
    {
        EnableWindow(GetDlgItem(hwnd, chx1), FALSE);
        EnableWindow(GetDlgItem(hwnd, chx2), FALSE);
        EnableWindow(GetDlgItem(hwnd, chx3), FALSE);
        EnableWindow(GetDlgItem(hwnd, chx4), FALSE);
        EnableWindow(GetDlgItem(hwnd, chx5), FALSE);
    }
    else
    {
        EnableWindow(GetDlgItem(hwnd, chx1), TRUE);
        EnableWindow(GetDlgItem(hwnd, chx2), TRUE);
        EnableWindow(GetDlgItem(hwnd, chx3), TRUE);
        EnableWindow(GetDlgItem(hwnd, chx4), TRUE);
        EnableWindow(GetDlgItem(hwnd, chx5), TRUE);
    }

    return TRUE;
}

static void OnOK(HWND hwnd)
{
    g_settings.m_secure = (IsDlgButtonChecked(hwnd, chx1) == BST_CHECKED);
    g_settings.m_local_file_access = (IsDlgButtonChecked(hwnd, chx2) == BST_CHECKED);
    g_settings.m_dont_r_click = (IsDlgButtonChecked(hwnd, chx3) == BST_CHECKED);
    g_settings.m_dont_popup = (IsDlgButtonChecked(hwnd, chx4) == BST_CHECKED);
    g_settings.m_ignore_errors = (IsDlgButtonChecked(hwnd, chx5) == BST_CHECKED);
    g_settings.m_kiosk_mode = (IsDlgButtonChecked(hwnd, chx6) == BST_CHECKED);
    g_settings.m_no_virus_scan = (IsDlgButtonChecked(hwnd, chx7) == BST_CHECKED);
    g_settings.m_zone_ident = (IsDlgButtonChecked(hwnd, chx8) == BST_CHECKED);

    TCHAR szText[256];
    GetDlgItemText(hwnd, edt1, szText, ARRAYSIZE(szText));
    g_settings.m_homepage = szText;

    GetDlgItemText(hwnd, edt2, szText, ARRAYSIZE(szText));
    g_settings.m_emulation = wcstol(szText, NULL, 0);

    EndDialog(hwnd, IDOK);
}

static void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    switch (id)
    {
    case IDOK:
        OnOK(hwnd);
        break;
    case IDCANCEL:
        EndDialog(hwnd, id);
        break;
    case psh1:
        SetDlgItemText(hwnd, edt1, s_strCurPage.c_str());
        break;
    case psh2:
        SetDlgItemText(hwnd, edt1, LoadStringDx(IDS_HOMEPAGE));
        break;
    case psh3:
        ShowURLListDlg(GetModuleHandle(NULL), hwnd);
        break;
    case psh4:
        ShowBlackListDlg(GetModuleHandle(NULL), hwnd);
        break;
    case psh5:
        g_settings.m_emulation = 11001;
        SetDlgItemInt(hwnd, edt2, g_settings.m_emulation, TRUE);
        break;
    case psh6:
        g_settings.reset();
        EndDialog(hwnd, psh6);
        break;
    case chx6:
        if (IsDlgButtonChecked(hwnd, chx6) == BST_CHECKED)
        {
            EnableWindow(GetDlgItem(hwnd, chx1), FALSE);
            EnableWindow(GetDlgItem(hwnd, chx2), FALSE);
            EnableWindow(GetDlgItem(hwnd, chx3), FALSE);
            EnableWindow(GetDlgItem(hwnd, chx4), FALSE);
            EnableWindow(GetDlgItem(hwnd, chx5), FALSE);
        }
        else
        {
            EnableWindow(GetDlgItem(hwnd, chx1), TRUE);
            EnableWindow(GetDlgItem(hwnd, chx2), TRUE);
            EnableWindow(GetDlgItem(hwnd, chx3), TRUE);
            EnableWindow(GetDlgItem(hwnd, chx4), TRUE);
            EnableWindow(GetDlgItem(hwnd, chx5), TRUE);
        }
        break;
    }
}

static INT_PTR CALLBACK
SettingsDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_INITDIALOG, OnInitDialog);
        HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);
    }
    return 0;
}

void ShowSettingsDlg(HINSTANCE hInst, HWND hwnd, const std::wstring& strCurPage)
{
    s_strCurPage = strCurPage;
    DialogBox(hInst, MAKEINTRESOURCE(IDD_SETTINGS), hwnd, SettingsDlgProc);
}
