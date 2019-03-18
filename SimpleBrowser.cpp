// SimpleBrowser.cpp --- simple Win32 browser
// Copyright (C) 2019 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
// This file is public domain software.
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <mshtml.h>
#include <intshcut.h>
#include "MWebBrowser.hpp"
#include "MEventSink.hpp"
#include "MBindStatusCallback.hpp"
#include "AddLinkDlg.hpp"
#include "AboutBox.hpp"
#include "Settings.hpp"
#include "mime_info.h"
#include <string>
#include <cassert>
#include <comdef.h>
#include "resource.h"

static const UINT s_control_ids[] =
{
    ID_BACK,
    ID_NEXT,
    ID_STOP_REFRESH,
    ID_HOME,
    ID_ADDRESS_BAR,
    ID_GO,
    ID_DOTS,
    ID_BROWSER
};

// button size
#define BTN_WIDTH 80
#define BTN_HEIGHT 30

static const TCHAR s_szName[] = TEXT("SimpleBrowser");
static HINSTANCE s_hInst = NULL;
static HACCEL s_hAccel = NULL;
static HWND s_hMainWnd = NULL;
static HWND s_hStatusBar = NULL;
static HWND s_hAddrBarComboBox = NULL;
static HWND s_hAddrBarEdit = NULL;
static MWebBrowser *s_pWebBrowser = NULL;
static HFONT s_hGUIFont = NULL;
static HFONT s_hAddressFont = NULL;
static MEventSink *s_pEventSink = MEventSink::Create();
static BOOL s_bLoadingPage = FALSE;
static HBITMAP s_hbmSecure = NULL;
static HBITMAP s_hbmInsecure = NULL;
static std::wstring s_strURL;
static std::wstring s_strTitle;

void DoUpdateURL(const WCHAR *url)
{
    ::SetWindowTextW(s_hAddrBarComboBox, url);
}

// load a resource string using rotated buffers
LPTSTR LoadStringDx(INT nID)
{
    static UINT s_index = 0;
    const UINT cchBuffMax = 1024;
    static TCHAR s_sz[4][cchBuffMax];

    TCHAR *pszBuff = s_sz[s_index];
    s_index = (s_index + 1) % _countof(s_sz);
    pszBuff[0] = 0;
    if (!::LoadString(NULL, nID, pszBuff, cchBuffMax))
        assert(0);
    return pszBuff;
}

std::wstring text2html(const WCHAR *text)
{
    std::wstring contents;
    contents.reserve(wcslen(text));

    for (; *text; ++text)
    {
        if (*text == L'<')
            contents += L"&lt;";
        else if (*text == L'>')
            contents += L"&gt;";
        else if (*text == L'&')
            contents += L"&amp;";
        else
            contents += *text;
    }

    std::wstring ret = L"<html><body><pre>";
    ret += contents;
    ret += L"</pre></body></html>";
    return ret;
}

void SetDocumentContents(IHTMLDocument2 *pDocument, const WCHAR *text,
                         bool is_html = true)
{
    std::wstring str;
    if (!is_html)
    {
        str = text2html(text);
    }
    else
    {
        str = text;
    }
    if (BSTR bstr = SysAllocString(str.c_str()))
    {
        if (SAFEARRAY *sa = SafeArrayCreateVector(VT_VARIANT, 0, 1))
        {
            VARIANT *pvar;
            HRESULT hr = SafeArrayAccessData(sa, (void **)&pvar);
            if (SUCCEEDED(hr))
            {
                pvar->vt = VT_BSTR;
                pvar->bstrVal = bstr;
                SafeArrayDestroy(sa);

                pDocument->write(sa);
            }
        }
        SysFreeString(bstr);
    }
}

void SetInternalPageContents(const WCHAR *html, bool is_html = true)
{
    IDispatch *pDisp = NULL;
    s_pWebBrowser->GetIWebBrowser2()->get_Document(&pDisp);
    if (pDisp)
    {
        if (IHTMLDocument2 *pDocument = static_cast<IHTMLDocument2 *>(pDisp))
        {
            pDocument->close();
            SetDocumentContents(pDocument, html, is_html);
        }
        pDisp->Release();
    }
}

BOOL UrlInBlackList(const WCHAR *url)
{
    std::wstring strURL = url;
    for (auto& item : g_settings.m_black_list)
    {
        if (strURL.find(item) != std::wstring::npos)
        {
            return TRUE;
        }
    }
    return FALSE;
}

BOOL IsAccessibleProtocol(const std::wstring& protocol)
{
    if (protocol == L"http" ||
        protocol == L"https" ||
        protocol == L"view-source" ||
        protocol == L"about")
    {
        return TRUE;
    }
    return FALSE;
}

BOOL IsAccessibleURL(const WCHAR *url)
{
    if (PathFileExists(url))
        return FALSE;

    if (LPWSTR pch = wcschr(url, L':'))
    {
        std::wstring protocol(url, pch - url);
        if (!IsAccessibleProtocol(protocol))
            return FALSE;
    }

    if (PathIsURL(url) || UrlIs(url, URLIS_APPLIABLE))
        return TRUE;
    if (wcsstr(url, L"www.") == url || wcsstr(url, L"ftp.") == url)
        return TRUE;

    int cch = lstrlenW(url);
    if (cch >= 4 && wcsstr(&url[cch - 4], L".com") != NULL)
        return TRUE;
    if (cch >= 5 && wcsstr(&url[cch - 5], L".com/") != NULL)
        return TRUE;
    if (cch >= 6 && wcsstr(&url[cch - 6], L".co.jp") != NULL)
        return TRUE;
    if (cch >= 7 && wcsstr(&url[cch - 7], L".co.jp/") != NULL)
        return TRUE;

    return FALSE;
}

inline LPTSTR MakeFilterDx(LPTSTR psz)
{
    for (LPTSTR pch = psz; *pch; ++pch)
    {
        if (*pch == TEXT('|'))
            *pch = 0;
    }
    return psz;
}

struct MEventHandler : MEventSinkListener
{
    virtual void OnBeforeNavigate2(
        IDispatch *pDispatch,
        VARIANT *url,
        VARIANT *Flags,
        VARIANT *TargetFrameName,
        VARIANT *PostData,
        VARIANT *Headers,
        VARIANT_BOOL *Cancel)
    {
        IDispatch *pApp = NULL;
        HRESULT hr = s_pWebBrowser->get_Application(&pApp);
        if (SUCCEEDED(hr))
        {
            if (pApp == pDispatch)
            {
                if (UrlInBlackList(url->bstrVal))
                {
                    SetInternalPageContents(LoadStringDx(IDS_HITBLACKLIST));
                    *Cancel = VARIANT_TRUE;
                    return;
                }
                if (!IsAccessibleURL(url->bstrVal))
                {
                    SetInternalPageContents(LoadStringDx(IDS_ACCESS_FAIL));
                    *Cancel = VARIANT_TRUE;
                    return;
                }

                s_bLoadingPage = TRUE;

                DoUpdateURL(url->bstrVal);
                ::SetDlgItemText(s_hMainWnd, ID_STOP_REFRESH, LoadStringDx(IDS_STOP));
            }
            pApp->Release();
        }
    }

    virtual void OnNavigateComplete2(
        IDispatch *pDispatch,
        VARIANT *URL)
    {
        IDispatch *pApp = NULL;
        HRESULT hr = s_pWebBrowser->get_Application(&pApp);
        if (SUCCEEDED(hr))
        {
            if (pApp == pDispatch)
            {
                s_strURL = URL->bstrVal;
                ::SetDlgItemText(s_hMainWnd, ID_STOP_REFRESH, LoadStringDx(IDS_REFRESH));
                s_pWebBrowser->Zoom();
                s_bLoadingPage = FALSE;
                PostMessage(s_hMainWnd, WM_COMMAND, ID_DOCUMENT_COMPLETE, 0);
            }
            pApp->Release();
        }
    }

    virtual void OnNewWindow3(
        IDispatch **ppDisp,
        VARIANT_BOOL *Cancel,
        DWORD dwFlags,
        BSTR bstrUrlContext,
        BSTR bstrUrl)
    {
        if (g_settings.m_dont_open_popups)
        {
            // prevent new window open
            *Cancel = VARIANT_TRUE;
        }
    }

    virtual void OnCommandStateChange(
        long Command,
        VARIANT_BOOL Enable)
    {
        static BOOL bEnableForward = FALSE, bEnableBack = FALSE;

        if (Command == CSC_NAVIGATEFORWARD)
        {
            bEnableForward = (Enable == VARIANT_TRUE);
        }
        else if (Command == CSC_NAVIGATEBACK)
        {
            bEnableBack = (Enable == VARIANT_TRUE);
        }

        ::EnableWindow(::GetDlgItem(s_hMainWnd, ID_BACK), bEnableBack);
        ::EnableWindow(::GetDlgItem(s_hMainWnd, ID_NEXT), bEnableForward);
    }

    virtual void OnStatusTextChange(BSTR Text)
    {
        SetWindowTextW(s_hStatusBar, Text);
    }

    virtual void OnTitleTextChange(BSTR Text)
    {
        WCHAR szText[256];
        wsprintfW(szText, LoadStringDx(IDS_TITLE_TEXT), Text);
        SetWindowTextW(s_hMainWnd, szText);
        s_strTitle = Text;
    }

    virtual void OnFileDownload(
        VARIANT_BOOL ActiveDocument,
        VARIANT_BOOL *Cancel)
    {
        *Cancel = TRUE;
    }
};
MEventHandler s_listener;

LPTSTR DoGetTemporaryFile(void)
{
    static TCHAR s_szFile[MAX_PATH];
    TCHAR szPath[MAX_PATH];
    if (GetTempPath(ARRAYSIZE(szPath), szPath))
    {
        if (GetTempFileName(szPath, TEXT("sbt"), 0, s_szFile))
        {
            return s_szFile;
        }
    }
    return NULL;
}

void DoNavigate(HWND hwnd, const WCHAR *url)
{
    std::wstring strURL;
    WCHAR *pszURL = _wcsdup(url);
    if (pszURL)
    {
        StrTrimW(pszURL, L" \t\n\r\f\v");
        strURL = pszURL;
        free(pszURL);
    }
    else
    {
        assert(0);
        return;
    }

    if (UrlInBlackList(strURL.c_str()))
    {
        SetInternalPageContents(LoadStringDx(IDS_HITBLACKLIST));
        return;
    }
    if (!IsAccessibleURL(strURL.c_str()))
    {
        SetInternalPageContents(LoadStringDx(IDS_ACCESS_FAIL));
        return;
    }

    if (strURL.find(L"view-source:") == 0)
    {
        if (WCHAR *file = DoGetTemporaryFile())
        {
            MBindStatusCallback *pCallback = MBindStatusCallback::Create();
            std::wstring new_url, substr = strURL.substr(wcslen(L"view-source:"));
            HRESULT hr = E_FAIL;
            if (FAILED(hr))
            {
                new_url = substr;
                hr = URLDownloadToFile(NULL, new_url.c_str(), file, 0, pCallback);
            }
            if (FAILED(hr))
            {
                new_url = L"https:" + substr;
                hr = URLDownloadToFile(NULL, new_url.c_str(), file, 0, pCallback);
            }
            if (FAILED(hr))
            {
                new_url = L"https://" + substr;
                hr = URLDownloadToFile(NULL, new_url.c_str(), file, 0, pCallback);
            }
            if (FAILED(hr))
            {
                new_url = L"http:" + substr;
                hr = URLDownloadToFile(NULL, new_url.c_str(), file, 0, pCallback);
            }
            if (FAILED(hr))
            {
                new_url = L"http://" + substr;
                hr = URLDownloadToFile(NULL, new_url.c_str(), file, 0, pCallback);
            }

            if (SUCCEEDED(hr))
            {
                while (!pCallback->IsCompleted() && !pCallback->IsCancelled() &&
                       GetAsyncKeyState(VK_ESCAPE) >= 0)
                {
                    Sleep(100);
                }

                if (pCallback->IsCompleted())
                {
                    std::string contents;
                    char buf[512];
                    if (FILE *fp = _wfopen(file, L"rb"))
                    {
                        while (size_t count = fread(buf, 1, 512, fp))
                        {
                            contents.append(buf, count);
                        }
                        fclose(fp);

                        // contents to wide
                        UINT nCodePage = CP_UTF8;
                        if (contents.find("Shift_JIS") != std::string::npos ||
                            contents.find("shift_jis") != std::string::npos ||
                            contents.find("x-sjis") != std::string::npos)
                        {
                            nCodePage = 932;
                        }
                        else if (contents.find("ISO-8859-1") != std::string::npos ||
                                 contents.find("iso-8859-1") != std::string::npos)
                        {
                            nCodePage = 28591;
                        }

                        int ret;
                        ret = MultiByteToWideChar(nCodePage, 0, contents.c_str(), -1, NULL, 0);
                        std::wstring wide(ret + 1, 0);
                        ret = MultiByteToWideChar(nCodePage, 0, contents.c_str(), -1, &wide[0], ret + 1);
                        DWORD error = GetLastError();
                        wide.resize(ret);

                        SetInternalPageContents(wide.c_str(), false);
                    }
                    else
                    {
                        assert(0);
                    }
                }
                else
                {
                    assert(0);
                }
            }
            else
            {
                assert(0);
            }
            pCallback->Release();

            DeleteFile(file);
        }
        else
        {
            assert(0);
        }
        DoUpdateURL(strURL.c_str());
        SetTimer(s_hMainWnd, 999, 500, NULL);
    }
    else
    {
        s_pWebBrowser->Navigate(url);
    }
}

BOOL DoSetBrowserEmulation(DWORD dwValue)
{
    static const TCHAR s_szFeatureControl[] =
        TEXT("SOFTWARE\\Microsoft\\Internet Explorer\\Main\\FeatureControl");

    TCHAR szPath[MAX_PATH], *pchFileName;
    GetModuleFileName(NULL, szPath, ARRAYSIZE(szPath));
    pchFileName = PathFindFileName(szPath);

    BOOL bOK = FALSE;
    HKEY hkeyControl = NULL;
    RegOpenKeyEx(HKEY_CURRENT_USER, s_szFeatureControl, 0, KEY_ALL_ACCESS, &hkeyControl);
    if (hkeyControl)
    {
        HKEY hkeyEmulation = NULL;
        RegCreateKeyEx(hkeyControl, TEXT("FEATURE_BROWSER_EMULATION"), 0, NULL, 0,
                       KEY_ALL_ACCESS, NULL, &hkeyEmulation, NULL);
        if (hkeyEmulation)
        {
            if (dwValue)
            {
                DWORD value = dwValue, size = sizeof(value);
                LONG result = RegSetValueEx(hkeyEmulation, pchFileName, 0,
                                            REG_DWORD, (LPBYTE)&value, size);
                bOK = (result == ERROR_SUCCESS);
            }
            else
            {
                RegDeleteValue(hkeyEmulation, pchFileName);
                bOK = TRUE;
            }

            RegCloseKey(hkeyEmulation);
        }

        RegCloseKey(hkeyControl);
    }

    return bOK;
}

LRESULT CALLBACK
AddressBarWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    WNDPROC fn = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    LRESULT result = CallWindowProc(fn, hwnd, uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_PAINT:
        //if (HDC hDC = GetDC(hwnd))
        //{
        //    if (HDC hdcMem = CreateCompatibleDC(NULL))
        //    {
        //        IDispatch *pDisp = NULL;
        //        s_pWebBrowser->GetIWebBrowser2()->get_Document(&pDisp);
        //        if (pDisp)
        //        {
        //            if (IHTMLDocument2 *pDocument = static_cast<IHTMLDocument2 *>(pDisp))
        //            {
        //                BSTR bstr;
        //                pDocument->get_security(&bstr);
        //                //MessageBoxW(NULL, bstr, L"bstr", 0);
        //                SysFreeString(bstr);
        //            }
        //            pDisp->Release();
        //        }
        //        DeleteDC(hdcMem);
        //    }
        //    ReleaseDC(hwnd, hDC);
        //}
        break;
    }
    return result;
}


BOOL OnCreate(HWND hwnd, LPCREATESTRUCT lpCreateStruct)
{
    s_hMainWnd = hwnd;

    s_hbmSecure = LoadBitmap(s_hInst, MAKEINTRESOURCE(IDB_SECURE));
    s_hbmInsecure = LoadBitmap(s_hInst, MAKEINTRESOURCE(IDB_INSECURE));
    s_hAccel = LoadAccelerators(s_hInst, MAKEINTRESOURCE(1));

    DoSetBrowserEmulation(11001);   // EDGE
    //DoSetBrowserEmulation(11000);   // IE
    //DoSetBrowserEmulation(0);

    s_pWebBrowser = MWebBrowser::Create(hwnd);
    if (!s_pWebBrowser)
        return FALSE;

    IWebBrowser2 *pBrowser2 = s_pWebBrowser->GetIWebBrowser2();

    // Don't show JavaScript errors
    pBrowser2->put_Silent(VARIANT_TRUE);

    s_pEventSink->Connect(pBrowser2, &s_listener);

    s_hGUIFont = GetStockFont(DEFAULT_GUI_FONT);

    INT x, y, cx, cy;
    DWORD style = WS_CHILD | WS_VISIBLE;

    x = y = 0;
    cx = BTN_WIDTH;
    cy = BTN_HEIGHT;
    static const TCHAR s_szButton[] = TEXT("BUTTON");
    CreateWindowEx(0, s_szButton, LoadStringDx(IDS_BACK),
                   style, x, y, cx, cy,
                   hwnd, (HMENU) ID_BACK, s_hInst, NULL);
    x += cx;
    CreateWindowEx(0, s_szButton, LoadStringDx(IDS_NEXT),
                   style, x, y, cx, cy,
                   hwnd, (HMENU) ID_NEXT, s_hInst, NULL);
    x += cx;
    CreateWindowEx(0, s_szButton, LoadStringDx(IDS_REFRESH),
                   style, x, y, cx, cy,
                   hwnd, (HMENU) ID_STOP_REFRESH, s_hInst, NULL);
    x += cx;
    CreateWindowEx(0, s_szButton, LoadStringDx(IDS_HOME),
                   style, x, y, cx, cy,
                   hwnd, (HMENU) ID_HOME, s_hInst, NULL);
    x += cx;

    LOGFONT lf;
    GetObject(s_hGUIFont, sizeof(lf), &lf);
    lf.lfHeight = -(BTN_HEIGHT - 8);
    s_hAddressFont = CreateFontIndirect(&lf);

    cx = 260;
    style = WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_AUTOHSCROLL |
            CBS_DROPDOWN | CBS_HASSTRINGS | CBS_NOINTEGRALHEIGHT;
    CreateWindowEx(WS_EX_CLIENTEDGE, L"COMBOBOX", NULL,
                   style, x, y, cx, 300,
                   hwnd, (HMENU)ID_ADDRESS_BAR, s_hInst, NULL);
    s_hAddrBarComboBox = GetDlgItem(hwnd, ID_ADDRESS_BAR);
    ComboBox_LimitText(s_hAddrBarComboBox, 255);
    x += cx;

    cx = BTN_WIDTH;
    cy = BTN_HEIGHT;
    style = WS_CHILD | WS_VISIBLE;
    CreateWindowEx(0, s_szButton, LoadStringDx(IDS_GO),
                   style, x, y, cx, cy,
                   hwnd, (HMENU) ID_GO, s_hInst, NULL);
    x += cx;

    cx = BTN_WIDTH;
    cy = BTN_HEIGHT;
    style = WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_PUSHLIKE;
    CreateWindowEx(0, s_szButton, LoadStringDx(IDS_DOTS),
                   style, x, y, cx, cy,
                   hwnd, (HMENU) ID_DOTS, s_hInst, NULL);
    x += cx;

    // set font
    SendDlgItemMessage(hwnd, ID_BACK, WM_SETFONT, (WPARAM)s_hGUIFont, TRUE);
    SendDlgItemMessage(hwnd, ID_NEXT, WM_SETFONT, (WPARAM)s_hGUIFont, TRUE);
    SendDlgItemMessage(hwnd, ID_STOP_REFRESH, WM_SETFONT, (WPARAM)s_hGUIFont, TRUE);
    SendDlgItemMessage(hwnd, ID_HOME, WM_SETFONT, (WPARAM)s_hGUIFont, TRUE);
    SendDlgItemMessage(hwnd, ID_ADDRESS_BAR, WM_SETFONT, (WPARAM)s_hAddressFont, TRUE);
    SendDlgItemMessage(hwnd, ID_GO, WM_SETFONT, (WPARAM)s_hGUIFont, TRUE);

    style = WS_CHILD | WS_VISIBLE | SBS_SIZEGRIP;
    s_hStatusBar = CreateStatusWindow(style, LoadStringDx(IDS_LOADING), hwnd, stc1);
    if (!s_hStatusBar)
        return FALSE;

    s_hAddrBarEdit = GetTopWindow(s_hAddrBarComboBox);
    SHAutoComplete(s_hAddrBarEdit, SHACF_URLALL | SHACF_AUTOSUGGEST_FORCE_ON);

    g_settings.load();

    if (g_settings.m_secure)
        s_pWebBrowser->AllowInsecure(FALSE);
    else
        s_pWebBrowser->AllowInsecure(TRUE);

    if (g_settings.m_x != CW_USEDEFAULT)
    {
        UINT uFlags;
        uFlags = SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOSIZE;
        SetWindowPos(hwnd, NULL, g_settings.m_x, g_settings.m_y, 0, 0, uFlags);
    }
    if (g_settings.m_cx != CW_USEDEFAULT)
    {
        UINT uFlags;
        uFlags = SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOMOVE;
        SetWindowPos(hwnd, NULL, 0, 0, g_settings.m_cx, g_settings.m_cy, uFlags);
    }
    if (g_settings.m_bMaximized)
    {
        ShowWindowAsync(hwnd, SW_MAXIMIZE);
    }

    for (size_t i = 0; i < g_settings.m_url_list.size(); ++i)
    {
        auto& url = g_settings.m_url_list[i];
        ComboBox_AddString(s_hAddrBarComboBox, url.c_str());
    }

    int argc = 0;
    if (LPWSTR *wargv = CommandLineToArgvW(GetCommandLineW(), &argc))
    {
        if (argc >= 2)
        {
            DoNavigate(hwnd, wargv[1]);
        }
        LocalFree(wargv);
    }

    if (argc <= 1)
    {
        DoNavigate(hwnd, g_settings.m_homepage.c_str());
    }

    WNDPROC fn = SubclassWindow(s_hAddrBarComboBox, AddressBarWindowProc);
    SetWindowLongPtr(s_hAddrBarComboBox, GWLP_USERDATA, (LONG_PTR)fn);

    PostMessage(hwnd, WM_SIZE, 0, 0);

    return TRUE;
}

void OnMove(HWND hwnd, int x, int y)
{
    RECT rc;

    if (!IsZoomed(hwnd) && !IsIconic(hwnd))
    {
        GetWindowRect(hwnd, &rc);
        g_settings.m_x = rc.left;
        g_settings.m_y = rc.top;
    }
}

void OnSize(HWND hwnd, UINT state, int cx, int cy)
{
    RECT rc;

    if (!IsZoomed(hwnd) && !IsIconic(hwnd))
    {
        GetWindowRect(hwnd, &rc);
        g_settings.m_cx = rc.right - rc.left;
        g_settings.m_cy = rc.bottom - rc.top;
    }

    GetClientRect(hwnd, &rc);

    INT x, y;

    x = rc.left;
    y = rc.top;
    cx = BTN_WIDTH;
    cy = BTN_HEIGHT;
    MoveWindow(GetDlgItem(hwnd, ID_BACK), x, y, cx, cy, TRUE);
    x += cx;
    MoveWindow(GetDlgItem(hwnd, ID_NEXT), x, y, cx, cy, TRUE);
    x += cx;
    MoveWindow(GetDlgItem(hwnd, ID_STOP_REFRESH), x, y, cx, cy, TRUE);
    x += cx;
    MoveWindow(GetDlgItem(hwnd, ID_HOME), x, y, cx, cy, TRUE);
    x += cx;

    INT x1 = x;

    x = rc.right - cx;
    MoveWindow(GetDlgItem(hwnd, ID_DOTS), x, y, cx, cy, TRUE);
    x -= cx;
    MoveWindow(GetDlgItem(hwnd, ID_GO), x, y, cx, cy, TRUE);

    cx = x - x1;
    x -= cx;
    MoveWindow(s_hAddrBarComboBox, x, y, cx, cy, TRUE);

    rc.top += BTN_HEIGHT;

    RECT rcStatus;
    SendMessage(s_hStatusBar, WM_SIZE, 0, 0);
    GetWindowRect(s_hStatusBar, &rcStatus);

    rc.bottom -= rcStatus.bottom - rcStatus.top;

    s_pWebBrowser->MoveWindow(rc);
}

void OnBack(HWND hwnd)
{
    s_pWebBrowser->GoBack();
}

void OnNext(HWND hwnd)
{
    s_pWebBrowser->GoForward();
}

void OnStopRefresh(HWND hwnd)
{
    if (s_bLoadingPage)
    {
        s_pWebBrowser->Stop();
        s_pWebBrowser->StopDownload();
    }
    else
    {
        s_pWebBrowser->Refresh();
    }
}

void OnRefresh(HWND hwnd)
{
    s_pWebBrowser->Refresh();
}

void OnStop(HWND hwnd)
{
    s_pWebBrowser->Stop();
    s_pWebBrowser->StopDownload();
}

void OnGoToAddressBar(HWND hwnd)
{
    ComboBox_SetEditSel(s_hAddrBarComboBox, 0, -1);
    SetFocus(s_hAddrBarComboBox);
}

void OnGo(HWND hwnd)
{
    WCHAR szURL[256];
    GetWindowTextW(s_hAddrBarEdit, szURL, 256);

    StrTrimW(szURL, L" \t\n\r\f\v");

    if (szURL[0] == 0)
        lstrcpyW(szURL, L"about:blank");

    DoNavigate(hwnd, szURL);
}

void OnHome(HWND hwnd)
{
    DoNavigate(hwnd, g_settings.m_homepage.c_str());
}

void OnPrint(HWND hwnd)
{
    s_pWebBrowser->Print(FALSE);
}

void OnPrintBang(HWND hwnd)
{
    s_pWebBrowser->Print(TRUE);
}

void OnPrintPreview(HWND hwnd)
{
    s_pWebBrowser->PrintPreview();
}

void OnPageSetup(HWND hwnd)
{
    s_pWebBrowser->PageSetup();
}

void OnSave(HWND hwnd)
{
    BSTR bstrURL = NULL;
    if (FAILED(s_pWebBrowser->get_LocationURL(&bstrURL)))
    {
        assert(0);
        return;
    }

    LPWSTR pch = wcsrchr(bstrURL, L'?');
    if (pch)
        *pch = 0;

    pch = wcsrchr(bstrURL, L'/');
    if (pch)
        ++pch;
    else
        pch = bstrURL;

    pch = PathFindExtension(pch);
    char extension[64];
    ::WideCharToMultiByte(CP_ACP, 0, pch, -1, extension, 64, NULL, NULL);
    const char *pszMime = mime_info_mime_from_extension(extension);
    if (pszMime == NULL)
        pszMime = "application/octet-stream";

    //MessageBoxA(NULL, pszMime, NULL, 0);

    TCHAR file[MAX_PATH] = L"*";

    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = file;
    ofn.nMaxFile = ARRAYSIZE(file);
    ofn.Flags = OFN_EXPLORER | OFN_ENABLESIZING | OFN_PATHMUSTEXIST |
                OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;

    if (strcmp(pszMime, "text/plain") == 0)
    {
        ofn.lpstrFilter = MakeFilterDx(LoadStringDx(IDS_TXTFILTER));
        ofn.lpstrDefExt = L"txt";
    }
    else if (strcmp(pszMime, "text/html") == 0)
    {
        ofn.lpstrFilter = MakeFilterDx(LoadStringDx(IDS_HTMLFILTER));
        ofn.lpstrDefExt = L"html";
    }
    else if (strcmp(pszMime, "image/jpeg") == 0)
    {
        ofn.lpstrFilter = MakeFilterDx(LoadStringDx(IDS_IMGFILTER));
        ofn.lpstrDefExt = L"jpg";
    }
    else if (strcmp(pszMime, "image/png") == 0)
    {
        ofn.lpstrFilter = MakeFilterDx(LoadStringDx(IDS_IMGFILTER));
        ofn.lpstrDefExt = L"png";
    }
    else if (strcmp(pszMime, "image/gif") == 0)
    {
        ofn.lpstrFilter = MakeFilterDx(LoadStringDx(IDS_IMGFILTER));
        ofn.lpstrDefExt = L"gif";
    }
    else if (strcmp(pszMime, "image/tiff") == 0)
    {
        ofn.lpstrFilter = MakeFilterDx(LoadStringDx(IDS_IMGFILTER));
        ofn.lpstrDefExt = L"tif";
    }
    else if (strcmp(pszMime, "image/bmp") == 0)
    {
        ofn.lpstrFilter = MakeFilterDx(LoadStringDx(IDS_IMGFILTER));
        ofn.lpstrDefExt = L"bmp";
    }
    else if (strcmp(pszMime, "application/pdf") == 0)
    {
        ofn.lpstrFilter = MakeFilterDx(LoadStringDx(IDS_PDFFILTER));
        ofn.lpstrDefExt = L"pdf";
    }
    else
    {
        ofn.lpstrFilter = MakeFilterDx(LoadStringDx(IDS_ALLFILTER));
        ofn.lpstrDefExt = NULL;
    }

    if (::GetSaveFileName(&ofn))
    {
        s_pWebBrowser->Save(file);
    }

    ::SysFreeString(bstrURL);
}

void OnViewSourceDone(HWND hwnd)
{
    s_listener.OnTitleTextChange(LoadStringDx(IDS_SOURCE));
}

void OnDots(HWND hwnd)
{
    if (SendDlgItemMessage(hwnd, ID_DOTS, BM_GETCHECK, 0, 0) == BST_UNCHECKED)
        return;

    RECT rc;
    GetWindowRect(GetDlgItem(hwnd, ID_DOTS), &rc);

    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = LoadMenu(s_hInst, MAKEINTRESOURCE(IDR_DOTSMENU));
    if (!hMenu)
        return;

    HMENU hSubMenu = GetSubMenu(hMenu, 0);
    TPMPARAMS params;
    params.cbSize = sizeof(params);
    params.rcExclude = rc;

    SetForegroundWindow(hwnd);
    UINT uFlags = TPM_LEFTBUTTON | TPM_LEFTALIGN | TPM_VERTICAL | TPM_RETURNCMD;
    UINT nCmd = TrackPopupMenuEx(hSubMenu, uFlags, rc.left, pt.y, hwnd, &params);
    DestroyMenu(hMenu);

    PostMessage(hwnd, WM_NULL, 0, 0);

    if (nCmd != 0)
    {
        PostMessage(hwnd, WM_COMMAND, nCmd, 0);
    }

    GetCursorPos(&pt);
    if (!PtInRect(&rc, pt) || GetAsyncKeyState(VK_LBUTTON) >= 0)
    {
        SendDlgItemMessage(hwnd, ID_DOTS, BM_SETCHECK, FALSE, 0);
    }
}

void OnViewSource(HWND hwnd)
{
    WCHAR szURL[256];
    GetWindowTextW(s_hAddrBarEdit, szURL, ARRAYSIZE(szURL));
    StrTrimW(szURL, L" \t\n\r\f\v");

    std::wstring url = szURL;
    if (url.find(L"view-source:") == 0)
    {
        url.erase(0, wcslen(L"view-source:"));
        DoNavigate(hwnd, url.c_str());
    }
    else
    {
        DoNavigate(hwnd, (L"view-source:" + url).c_str());
    }
}

void OnAbout(HWND hwnd)
{
    ShowAboutBox(s_hInst, hwnd);
}

BOOL CreateInternetShortcut(
    LPCTSTR pszUrlFileName, 
    LPCTSTR pszURL)
{
    IPersistFile*   ppf;
    IUniformResourceLocator *purl;
    HRESULT hr;
#ifndef UNICODE
    WCHAR   wsz[MAX_PATH];
#endif

    hr = CoCreateInstance(CLSID_InternetShortcut, NULL, 
        CLSCTX_INPROC_SERVER, IID_IUniformResourceLocator, 
        (LPVOID*)&purl);
    if (SUCCEEDED(hr))
    {
        purl->SetURL(pszURL, 0);

        hr = purl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf);
        if (SUCCEEDED(hr))
        {
#ifdef UNICODE
            hr = ppf->Save(pszUrlFileName, TRUE);
#else
            MultiByteToWideChar(CP_ACP, 0, pszUrlFileName, -1, wsz, 
                                MAX_PATH);
            hr = ppf->Save(wsz, TRUE);
#endif
            ppf->Release();
        }
        purl->Release();
    }

    return SUCCEEDED(hr);
}

std::wstring ConvertStringToFilename(const std::wstring& str)
{
    std::wstring ret;
    for (size_t i = 0; i < str.size(); ++i)
    {
        if (wcschr(L"\\/:*?\"<>|", str[i]) != NULL)
        {
            ret += L'_';
        }
        else
        {
            ret += str[i];
        }
    }
    return ret;
}

void OnCreateShortcut(HWND hwnd)
{
    TCHAR szPath[MAX_PATH];
    SHGetFolderPath(hwnd, CSIDL_DESKTOPDIRECTORY, NULL, SHGFP_TYPE_CURRENT, szPath);

    std::wstring file_title;
    if (s_strTitle.empty())
        file_title = LoadStringDx(IDS_NONAME);
    else
        file_title = ConvertStringToFilename(s_strTitle);

    if (file_title.size() >= 64)
        file_title.resize(64);

    if (!ShowAddLinkDlg(s_hInst, hwnd, file_title))
        return;

    file_title = ConvertStringToFilename(file_title);

    PathAppend(szPath, file_title.c_str());

    std::wstring strPath;
    for (INT i = 1; i < 64; ++i)
    {
        strPath = szPath;
        if (i > 1)
        {
            WCHAR sz[32];
            wsprintfW(sz, L" (%u)", i);
            strPath += sz;
        }
        strPath += L".url";
        if (!PathFileExists(strPath.c_str()))
        {
            break;
        }
    }

    if (!PathFileExists(strPath.c_str()))
    {
        std::wstring url = s_strURL;
        if (url.find(L"view-source:") == 0)
        {
            url.erase(0, wcslen(L"view-source:"));
        }
        CreateInternetShortcut(strPath.c_str(), url.c_str());
    }
}

void OnSettings(HWND hwnd)
{
    ShowSettingsDlg(s_hInst, hwnd, s_strURL);
}

void OnAddToComboBox(HWND hwnd)
{
    WCHAR szText[256];
    ComboBox_GetText(s_hAddrBarComboBox, szText, ARRAYSIZE(szText));

    std::wstring url = s_strURL;
    INT iItem = ComboBox_FindStringExact(s_hAddrBarComboBox, -1, (LPARAM)url.c_str());
    if (iItem != CB_ERR)
    {
        ComboBox_DeleteString(s_hAddrBarComboBox, iItem);
    }

    ComboBox_InsertString(s_hAddrBarComboBox, 0, url.c_str());

    ComboBox_SetText(s_hAddrBarComboBox, szText);
}

void OnDocumentComplete(HWND hwnd)
{
    SetWindowTextW(s_hAddrBarComboBox, s_strURL.c_str());
}

void OnAddressBar(HWND hwnd, HWND hwndCtl, UINT codeNotify)
{
    switch (codeNotify)
    {
    case CBN_SELCHANGE:
        {
            INT iItem = (INT)ComboBox_GetCurSel(s_hAddrBarComboBox);
            if (iItem != CB_ERR)
            {
                WCHAR szText[256];
                ComboBox_GetLBText(s_hAddrBarComboBox, iItem, szText);
                DoNavigate(hwnd, szText);
            }
        }
        break;
    }
}

void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    static INT s_nLevel = 0;

    if (s_nLevel == 0)
    {
        SendMessage(s_hStatusBar, SB_SETTEXT, 0, (LPARAM)LoadStringDx(IDS_EXECUTING_CMD));
    }
    s_nLevel++;

    switch (id)
    {
    case ID_BACK:
        OnBack(hwnd);
        break;
    case ID_NEXT:
        OnNext(hwnd);
        break;
    case ID_STOP_REFRESH:
        OnStopRefresh(hwnd);
        break;
    case ID_GO:
        OnGo(hwnd);
        break;
    case ID_HOME:
        OnHome(hwnd);
        break;
    case ID_ADDRESS_BAR:
        OnAddressBar(hwnd, hwndCtl, codeNotify);
        break;
    case ID_REFRESH:
        OnRefresh(hwnd);
        break;
    case ID_STOP:
        OnStop(hwnd);
        break;
    case ID_GO_TO_ADDRESS_BAR:
        OnGoToAddressBar(hwnd);
        break;
    case ID_PRINT:
        OnPrint(hwnd);
        break;
    case ID_PRINT_BANG:
        OnPrintBang(hwnd);
        break;
    case ID_PRINT_PREVIEW:
        OnPrintPreview(hwnd);
        break;
    case ID_PAGE_SETUP:
        OnPageSetup(hwnd);
        break;
    case ID_SAVE:
        OnSave(hwnd);
        break;
    case ID_VIEW_SOURCE_DONE:
        OnViewSourceDone(hwnd);
        break;
    case ID_DOTS:
        OnDots(hwnd);
        break;
    case ID_VIEW_SOURCE:
        OnViewSource(hwnd);
        break;
    case ID_ABOUT:
        OnAbout(hwnd);
        break;
    case ID_CREATE_SHORTCUT:
        OnCreateShortcut(hwnd);
        break;
    case ID_SETTINGS:
        OnSettings(hwnd);
        break;
    case ID_ADD_TO_COMBOBOX:
        OnAddToComboBox(hwnd);
        break;
    case ID_DOCUMENT_COMPLETE:
        OnDocumentComplete(hwnd);
        break;
    }

    --s_nLevel;
    if (s_nLevel == 0)
    {
        SendMessage(s_hStatusBar, SB_SETTEXT, 0, (LPARAM)LoadStringDx(IDS_READY));
    }
}

void OnDestroy(HWND hwnd)
{
    g_settings.m_bMaximized = IsZoomed(hwnd);

    g_settings.m_url_list.clear();
    TCHAR szText[256];
    INT nCount = (INT)ComboBox_GetCount(s_hAddrBarComboBox);
    for (INT i = 0; i < nCount; ++i)
    {
        ComboBox_GetLBText(s_hAddrBarComboBox, i, szText);
        g_settings.m_url_list.push_back(szText);
    }

    g_settings.save();

    if (s_hAddressFont)
    {
        DeleteObject(s_hAddressFont);
        s_hAddressFont = NULL;
    }
    if (s_hbmSecure)
    {
        DeleteObject(s_hbmSecure);
        s_hbmSecure = NULL;
    }
    if (s_hbmInsecure)
    {
        DeleteObject(s_hbmInsecure);
        s_hbmInsecure = NULL;
    }
    if (s_hAccel)
    {
        DestroyAcceleratorTable(s_hAccel);
        s_hAccel = NULL;
    }
    if (s_pWebBrowser)
    {
        s_pWebBrowser->Destroy();
        s_pWebBrowser->Release();
        s_pWebBrowser = NULL;
    }
    if (s_pEventSink)
    {
        s_pEventSink->Disconnect();
        s_pEventSink->Release();
        s_pEventSink = NULL;
    }
    PostQuitMessage(0);
}

void OnTimer(HWND hwnd, UINT id)
{
    KillTimer(hwnd, id);
    PostMessage(s_hMainWnd, WM_COMMAND, ID_VIEW_SOURCE_DONE, 0);
}

LRESULT CALLBACK
WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    HANDLE_MSG(hwnd, WM_CREATE, OnCreate);
    HANDLE_MSG(hwnd, WM_MOVE, OnMove);
    HANDLE_MSG(hwnd, WM_SIZE, OnSize);
    HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);
    HANDLE_MSG(hwnd, WM_TIMER, OnTimer);
    HANDLE_MSG(hwnd, WM_DESTROY, OnDestroy);
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

BOOL PreProcessBrowserKeys(LPMSG pMsg)
{
    if (s_pWebBrowser)
    {
        if (pMsg->hwnd == s_pWebBrowser->GetIEServerWindow())
        {
            BOOL bIgnore = FALSE;
            switch (pMsg->message)
            {
            case WM_RBUTTONDBLCLK:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
                if (g_settings.m_dont_r_click)
                    return TRUE;
                break;
            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_CHAR:
            case WM_IME_KEYDOWN:
            case WM_IME_KEYUP:
            case WM_IME_CHAR:
                if (GetAsyncKeyState(VK_CONTROL) < 0)
                {
                    switch (pMsg->wParam)
                    {
                    case 'L':   // Ctrl+L
                    case 'S':   // Ctrl+S
                    case 'O':   // Ctrl+O
                        bIgnore = TRUE;
                        break;
                    }
                }
                break;
            }

            if (!bIgnore && s_pWebBrowser->TranslateAccelerator(pMsg))
                return TRUE;
        }
    }

    //switch (pMsg->message)
    //{
    //case WM_SYSKEYDOWN:
    //    if (pMsg->wParam == 'D')
    //    {
    //        // Alt+D
    //        SetFocus(s_hAddrBarEdit);
    //        SendMessage(s_hAddrBarEdit, EM_SETSEL, 0, -1);
    //        return TRUE;
    //    }
    //    break;
    //}

    if (pMsg->hwnd == s_hAddrBarEdit || pMsg->hwnd == s_hAddrBarComboBox)
    {
        switch (pMsg->message)
        {
        case WM_KEYDOWN:
            if (pMsg->wParam == VK_RETURN)
            {
                // [Enter] key
                SendMessage(s_hMainWnd, WM_COMMAND, ID_GO, 0);
                return TRUE;
            }
            else if (pMsg->wParam == VK_ESCAPE && s_pWebBrowser)
            {
                // [Esc] key
                if (IWebBrowser2 *pBrowser2 = s_pWebBrowser->GetIWebBrowser2())
                {
                    BSTR bstrURL = NULL;
                    pBrowser2->get_LocationURL(&bstrURL);
                    if (bstrURL)
                    {
                        DoUpdateURL(bstrURL);
                        ::SysFreeString(bstrURL);
                    }
                }
                ::SetFocus(s_pWebBrowser->GetControlWindow());
                return TRUE;
            }
            else if (pMsg->wParam == 'A' && ::GetAsyncKeyState(VK_CONTROL) < 0)
            {
                // Ctrl+A
                SendMessage(s_hAddrBarEdit, EM_SETSEL, 0, -1);
                return TRUE;
            }
            break;
        }
    }

    switch (pMsg->message)
    {
    case WM_KEYDOWN:
        if (pMsg->wParam == VK_ESCAPE)
        {
            if (pMsg->hwnd == s_pWebBrowser->GetControlWindow() ||
                pMsg->hwnd == s_pWebBrowser->GetIEServerWindow() ||
                pMsg->hwnd == s_hMainWnd)
            {
                WCHAR szURL[256];
                GetWindowTextW(s_hAddrBarEdit, szURL, ARRAYSIZE(szURL));
                StrTrimW(szURL, L" \t\n\r\f\v");

                std::wstring url = szURL;
                if (url.find(L"view-source:") == 0)
                {
                    url.erase(0, wcslen(L"view-source:"));
                    DoNavigate(s_hMainWnd, url.c_str());
                    return TRUE;
                }
            }
        }
        else if (pMsg->wParam == VK_TAB)
        {
            UINT nCtrlID = GetDlgCtrlID(pMsg->hwnd);
            if (pMsg->hwnd == s_pWebBrowser->GetControlWindow() ||
                pMsg->hwnd == s_pWebBrowser->GetIEServerWindow() ||
                pMsg->hwnd == s_hMainWnd)
            {
                nCtrlID = ID_BROWSER;
            }
            INT nCount = 0;
            if (::GetAsyncKeyState(VK_SHIFT) < 0)
            {
                for (size_t i = 0; i < ARRAYSIZE(s_control_ids); ++i)
                {
                    if (s_control_ids[i] == nCtrlID)
                    {
                        HWND hwnd = NULL;
                        do
                        {
                            i += ARRAYSIZE(s_control_ids) - 1;
                            i %= (INT)ARRAYSIZE(s_control_ids);
                            nCtrlID = s_control_ids[i];
                            if (nCtrlID == ID_BROWSER)
                            {
                                HWND hwndServer = s_pWebBrowser->GetIEServerWindow();
                                ::SetFocus(hwndServer);
                                return TRUE;
                            }
                            hwnd = GetDlgItem(s_hMainWnd, s_control_ids[i]);
                            if (++nCount > ARRAYSIZE(s_control_ids))
                                return TRUE;
                        } while (!::IsWindowEnabled(hwnd));
                        if (nCtrlID == ID_ADDRESS_BAR)
                        {
                            SendMessage(s_hAddrBarEdit, EM_SETSEL, 0, -1);
                        }
                        ::SetFocus(hwnd);
                        return TRUE;
                    }
                }
            }
            else
            {
                for (size_t i = 0; i < ARRAYSIZE(s_control_ids); ++i)
                {
                    if (s_control_ids[i] == nCtrlID)
                    {
                        HWND hwnd = NULL;
                        do
                        {
                            i += 1;
                            i %= (INT)ARRAYSIZE(s_control_ids);
                            nCtrlID = s_control_ids[i];
                            if (nCtrlID == ID_BROWSER)
                            {
                                HWND hwndServer = s_pWebBrowser->GetIEServerWindow();
                                ::SetFocus(hwndServer);
                                return TRUE;
                            }
                            hwnd = GetDlgItem(s_hMainWnd, s_control_ids[i]);
                            if (++nCount > ARRAYSIZE(s_control_ids))
                                return TRUE;
                        } while (!::IsWindowEnabled(hwnd));
                        if (nCtrlID == ID_ADDRESS_BAR)
                        {
                            SendMessage(s_hAddrBarEdit, EM_SETSEL, 0, -1);
                        }
                        ::SetFocus(hwnd);
                        return TRUE;
                    }
                }
            }
        }
        break;
    }

    return FALSE;
}

INT WINAPI
WinMain(HINSTANCE   hInstance,
        HINSTANCE   hPrevInstance,
        LPSTR       lpCmdLine,
        INT         nCmdShow)
{
    WNDCLASS wc;

    OleInitialize(NULL);

    InitCommonControls();

    s_hInst = hInstance;

    ZeroMemory(&wc, sizeof(wc));
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(1));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
    wc.lpszClassName = s_szName;
    if (!RegisterClass(&wc))
    {
        MessageBox(NULL, LoadStringDx(IDS_REGISTER_WND_FAIL), NULL, MB_ICONERROR);
        return 1;
    }

    DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    DWORD exstyle = 0;
    HWND hwnd = CreateWindowEx(exstyle, s_szName, s_szName, style,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, hInstance, NULL);
    if (!hwnd)
    {
        MessageBox(NULL, LoadStringDx(IDS_CREATE_WND_FAIL), NULL, MB_ICONERROR);
        return 2;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (PreProcessBrowserKeys(&msg))
            continue;

        if (TranslateAccelerator(hwnd, s_hAccel, &msg))
            continue;

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    OleUninitialize();

    return 0;
}
