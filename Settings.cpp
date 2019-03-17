#include "Settings.hpp"
#include <windowsx.h>
#include <shlwapi.h>
#include "resource.h"

SETTINGS g_settings;

void SETTINGS::reset()
{
    LPTSTR LoadStringDx(INT nID);

    m_homepage = LoadStringDx(IDS_HOMEPAGE);
    m_url_list.clear();
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
        DWORD cb;

        cb = sizeof(szText);
        RegQueryValueEx(hApp, L"Homepage", NULL, NULL, (LPBYTE)szText, &cb);
        m_homepage = szText;

        DWORD count = 0;
        cb = sizeof(count);
        RegQueryValueEx(hApp, L"URL Count", NULL, NULL, (LPBYTE)&count, &cb);

        for (DWORD i = 0; i < count; ++i)
        {
            WCHAR szName[64];
            wsprintfW(szName, L"URL %lu", i);

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
                DWORD cb;

                cb = DWORD((m_homepage.size() + 1) * sizeof(WCHAR));
                RegSetValueEx(hApp, L"Homepage", 0, REG_SZ, (LPBYTE)m_homepage.c_str(), cb);

                DWORD count = DWORD(m_url_list.size());
                cb = DWORD(sizeof(count));
                RegSetValueEx(hApp, L"URL Count", 0, REG_DWORD, (LPBYTE)&count, cb);

                for (DWORD i = 0; i < count; ++i)
                {
                    auto& url = m_url_list[i];

                    WCHAR szName[64];
                    wsprintfW(szName, L"URL %lu", i);

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
    return TRUE;
}

static void OnOK(HWND hwnd)
{
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

void ShowSettingsDlg(HINSTANCE hInst, HWND hwnd)
{
    DialogBox(hInst, MAKEINTRESOURCE(IDD_SETTINGS), hwnd, SettingsDlgProc);
}
