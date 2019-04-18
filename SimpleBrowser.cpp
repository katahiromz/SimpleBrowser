// SimpleBrowser.cpp --- simple Win32 browser
// Copyright (C) 2019 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
// This file is public domain software.

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <mshtml.h>
#include <intshcut.h>
#include <urlhist.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <cassert>
#include <strsafe.h>
#include <comdef.h>
#include <mshtmcid.h>
#include <process.h>
#include "MWebBrowserEx.hpp"
#include "MEventSink.hpp"
#include "MBindStatusCallback.hpp"
#include "AddLinkDlg.hpp"
#include "AboutBox.hpp"
#include "Settings.hpp"
#include "mime_info.h"
#include "mstr.hpp"
#include "color_value.h"
#include "AmsiScanner.hpp"
#include "resource.h"

// button size
#define BTN_WIDTH 120
#define BTN_HEIGHT 60

#define DROPDOWN_HEIGHT 500

// timer IDs
#define SOURCE_DONE_TIMER      999
#define REFRESH_TIMER   888

#define DOWNLOAD_TIMER_INTERVAL 500

#define MIN_COMMAND_ID 20000

static const TCHAR s_szName[] = TEXT("SimpleBrowser");
static HINSTANCE s_hInst = NULL;
static HACCEL s_hAccel = NULL;
static HWND s_hMainWnd = NULL;
static HWND s_hStatusBar = NULL;
static HWND s_hAddrBarComboBox = NULL;
static HWND s_hAddrBarEdit = NULL;
static std::unordered_map<HWND, BOOL> s_downloadings;
static MWebBrowserEx *s_pWebBrowser = NULL;
static HFONT s_hButtonFont = NULL;
static HFONT s_hAddressFont = NULL;
static MEventSink *s_pEventSink = MEventSink::Create();
static BOOL s_bLoadingPage = FALSE;
static HBITMAP s_hbmSecure = NULL;
static HBITMAP s_hbmInsecure = NULL;
static std::wstring s_strURL;
static std::wstring s_strTitle;
static BOOL s_bKiosk = FALSE;
static const TCHAR s_szButton[] = TEXT("BUTTON");

static std::wstring s_strStop = L"Stop";
static std::wstring s_strRefresh = L"Refresh";
static std::unordered_map<HWND, std::wstring> s_hwnd2url;
static std::unordered_map<HWND, COLORREF> s_hwnd2color;
static std::unordered_map<HWND, COLORREF> s_hwnd2bgcolor;

static DWORD s_bgcolor = RGB(255, 255, 255);
static DWORD s_color = RGB(0, 0, 0);

static std::wstring s_upside_data;
static std::vector<HWND> s_upside_hwnds;

static std::wstring s_downside_data;
static std::vector<HWND> s_downside_hwnds;

static std::wstring s_leftside_data;
static std::vector<HWND> s_leftside_hwnds;

static std::wstring s_rightside_data;
static std::vector<HWND> s_rightside_hwnds;

static std::wstring s_popup_default_data;
static std::wstring s_popup_image_data;
static std::wstring s_popup_text_data;
static std::wstring s_popup_anchor_data;

static BOOL s_bEnableForward = FALSE;
static BOOL s_bEnableBack = FALSE;
static std::vector<std::wstring> s_menu_links;

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

UINT GetCheck(HWND hwnd)
{
    DWORD style = GetWindowLong(hwnd, GWL_STYLE);
    if (!(style & BS_PUSHLIKE))
        return FALSE;

    return (BOOL)(LONG_PTR)GetWindowLongPtr(hwnd, GWLP_USERDATA);
}

void SetCheck(HWND hwnd, UINT uCheck)
{
    DWORD style = GetWindowLong(hwnd, GWL_STYLE);
    if (!(style & BS_PUSHLIKE))
        return;

    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)(LONG)uCheck);
    InvalidateRect(hwnd, NULL, TRUE);
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
        protocol == L"about" ||
        protocol == L"res")
    {
        return TRUE;
    }
    if (g_settings.m_local_file_access && !g_settings.m_kiosk_mode)
    {
        if (protocol == L"file")
            return TRUE;
    }
    return FALSE;
}

BOOL IsURL(const WCHAR *url)
{
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

BOOL IsAccessible(const WCHAR *url)
{
    if (PathFileExists(url) || UrlIsFileUrl(url) ||
        PathIsUNC(url) || PathIsNetworkPath(url))
    {
        return g_settings.m_local_file_access && !g_settings.m_kiosk_mode;
    }

    if (LPCWSTR pch = wcschr(url, L':'))
    {
        std::wstring protocol(url, pch - url);
        if (!IsAccessibleProtocol(protocol))
            return FALSE;
        if (g_settings.m_local_file_access && !g_settings.m_kiosk_mode)
        {
            if (protocol == L"file")
                return TRUE;
        }
    }

    if (IsURL(url))
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

std::wstring URL_encode(const std::wstring& url)
{
    std::string str;

    size_t len = url.size() * 4;
    str.resize(len);
    WideCharToMultiByte(CP_UTF8, 0, url.c_str(), -1, &str[0], (INT)len, NULL, NULL);

    len = strlen(str.c_str());
    str.resize(len);

    std::wstring ret;
    WCHAR buf[4];
    static const WCHAR s_hex[] = L"0123456789ABCDEF";
    for (size_t i = 0; i < str.size(); ++i)
    {
        if (str[i] == ' ')
        {
            ret += L'+';
        }
        else if (std::isalnum(str[i]))
        {
            ret += (char)str[i];
        }
        else
        {
            switch (str[i])
            {
            case L'.':
            case L'-':
            case L'_':
            case L'*':
                ret += (char)str[i];
                break;
            default:
                buf[0] = L'%';
                buf[1] = s_hex[(str[i] >> 4) & 0xF];
                buf[2] = s_hex[str[i] & 0xF];
                buf[3] = 0;
                ret += buf;
                break;
            }
        }
    }

    return ret;
}

std::string URL_decode(const std::string& str)
{
    std::string ret;
    char buf[3];
    buf[2] = 0;
    for (size_t i = 0; i < str.size(); ++i)
    {
        if (str[i] == '+')
        {
            ret += ' ';
        }
        else if (str[i] == '%' && i + 2 < str.size())
        {
            buf[0] = str[i + 1];
            buf[1] = str[i + 2];
            if (std::isxdigit(buf[0]) && std::isxdigit(buf[1]))
            {
                i += 2;
                ret += (char)std::strtoul(buf, NULL, 16);
            }
            else
            {
                ret += '%';
            }
        }
        else
        {
            ret += str[i];
        }
    }
    return ret;
}

void DoNavigate(HWND hwnd, const WCHAR *url, DWORD dwFlags = 0);
void OnNew(HWND hwnd, LPCWSTR url);
BOOL DoSaveURL(HWND hwnd, LPCWSTR pszURL);

void DoSearch(HWND hwnd, LPCWSTR str)
{
    std::wstring query = LoadStringDx(IDS_QUERY_URL);
    std::wstring encoded = URL_encode(str);
    query += encoded;

    DoNavigate(hwnd, query.c_str(), navNoHistory);
}

struct MEventHandler : MEventSinkListener
{
    virtual void OnBeforeNavigate2(
        IDispatch *pDispatch,
        VARIANT *url,
        VARIANT *flags,
        VARIANT *target,
        VARIANT *PostData,
        VARIANT *headers,
        VARIANT_BOOL *Cancel)
    {
        assert(url->vt == VT_BSTR);
        assert(flags->vt == VT_I4);
        assert(target->vt == VT_BSTR);
        assert(PostData->vt == VT_BYREF | VT_VARIANT);
        assert(headers->vt == VT_BSTR);
        BSTR bstrURL = url->bstrVal;
        DWORD dwFlags = flags->lVal;
        BSTR bstrTarget = target->bstrVal;
        BSTR bstrHeaders = headers->bstrVal;

        IDispatch *pApp = NULL;
        HRESULT hr = s_pWebBrowser->get_Application(&pApp);

        printf("OnBeforeNavigate2: (%p, %p): '%ls', '%ls', '%ls': %08lX\n",
               pDispatch, pApp, bstrURL, bstrTarget, bstrHeaders, dwFlags);

        if (SUCCEEDED(hr))
        {
            if (pApp == pDispatch)
            {
                if (UrlInBlackList(bstrURL))
                {
                    printf("in black list: %ls\n", bstrURL);
                    s_pWebBrowser->Stop();
                    s_strURL = bstrURL;
                    SetInternalPageContents(LoadStringDx(IDS_HITBLACKLIST));
                    *Cancel = VARIANT_TRUE;
                    PostMessage(s_hMainWnd, WM_COMMAND, ID_DOCUMENT_COMPLETE, 0);
                    return;
                }
                if (!IsAccessible(bstrURL))
                {
                    printf("inaccessible: %ls\n", bstrURL);
                    s_pWebBrowser->Stop();
                    s_strURL = bstrURL;
                    SetInternalPageContents(LoadStringDx(IDS_ACCESS_FAIL));
                    *Cancel = VARIANT_TRUE;
                    PostMessage(s_hMainWnd, WM_COMMAND, ID_DOCUMENT_COMPLETE, 0);
                    return;
                }

                s_bLoadingPage = TRUE;

                DoUpdateURL(bstrURL);
                ::SetDlgItemText(s_hMainWnd, ID_STOP_REFRESH, s_strStop.c_str());
            }
            pApp->Release();
        }
    }

    virtual void OnNavigateComplete2(
        IDispatch *pDispatch,
        BSTR url)
    {
        IDispatch *pApp = NULL;
        HRESULT hr = s_pWebBrowser->get_Application(&pApp);

        printf("OnNavigateComplete2: (%p, %p): '%ls'\n",
               pDispatch, pApp, url);

        if (SUCCEEDED(hr))
        {
            if (pApp == pDispatch)
            {
                s_strURL = url;
                ::SetDlgItemText(s_hMainWnd, ID_STOP_REFRESH, s_strRefresh.c_str());
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
        printf("OnNewWindow3: '%ls', '%ls', 0x%08lX\n", bstrUrl, bstrUrlContext, dwFlags);
        //*Cancel = VARIANT_TRUE;

        IDispatch *pApp = NULL;
        HRESULT hr = s_pWebBrowser->get_Application(&pApp);
        *ppDisp = pApp;

        std::wstring url = bstrUrl;
        if (g_settings.m_dont_popup || g_settings.m_kiosk_mode)
        {
            DoNavigate(s_hMainWnd, url.c_str());
        }
        else
        {
            OnNew(s_hMainWnd, url.c_str());
        }
    }

    virtual void OnCommandStateChange(
        long Command,
        VARIANT_BOOL Enable)
    {
        printf("OnCommandStateChange: 0x%08lX, %d\n", Command, Enable);
        //*Cancel = VARIANT_TRUE;

        if (Command == CSC_NAVIGATEFORWARD)
        {
            s_bEnableForward = (Enable == VARIANT_TRUE);
        }
        else if (Command == CSC_NAVIGATEBACK)
        {
            s_bEnableBack = (Enable == VARIANT_TRUE);
        }

        ::EnableWindow(::GetDlgItem(s_hMainWnd, ID_BACK), s_bEnableBack);
        ::EnableWindow(::GetDlgItem(s_hMainWnd, ID_NEXT), s_bEnableForward);
    }

    virtual void OnStatusTextChange(BSTR Text)
    {
        printf("OnStatusTextChange: '%ls'\n", Text);
        SetWindowTextW(s_hStatusBar, Text);
    }

    virtual void OnTitleTextChange(BSTR Text)
    {
        WCHAR szText[256];
        printf("OnTitleTextChange: '%ls'\n", Text);
        StringCbPrintfW(szText, sizeof(szText), LoadStringDx(IDS_TITLE_TEXT), Text);
        SetWindowTextW(s_hMainWnd, szText);
        s_strTitle = Text;
    }

    virtual void OnFileDownload(
        VARIANT_BOOL ActiveDocument,
        VARIANT_BOOL *Cancel)
    {
        printf("OnFileDownload: %d\n", ActiveDocument);
        if (g_settings.m_kiosk_mode)
        {
            *Cancel = VARIANT_TRUE;
        }
    }

    virtual void OnDocumentComplete(
        IDispatch *pDisp,
        BSTR bstrURL)
    {
        printf("OnDocumentComplete: %p, '%ls'\n", pDisp, bstrURL);
    }

    virtual void OnNavigateError(
        IDispatch *pDisp,
        VARIANT *url,
        VARIANT *target,
        LONG StatusCode,
        VARIANT_BOOL *Cancel)
    {
        assert(url->vt == VT_BSTR);
        assert(target->vt == VT_BSTR);
        BSTR bstrURL = url->pvarVal->bstrVal;
        BSTR bstrTarget = target->pvarVal->bstrVal;

        printf("OnNavigateError: %p, '%ls', '%ls', %08lX\n", pDisp, bstrURL, bstrTarget, StatusCode);
        if (!IsURL(bstrURL))
        {
            DoSearch(s_hMainWnd, bstrURL);
        }
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

void DoNavigate(HWND hwnd, const WCHAR *url, DWORD dwFlags)
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
        SetTimer(s_hMainWnd, SOURCE_DONE_TIMER, 500, NULL);
    }
    else
    {
        HRESULT hr = s_pWebBrowser->Navigate2(url, dwFlags);

        if (FAILED(hr))
        {
            MessageBoxW(NULL, L"OK", NULL, 0);
        }
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
AddressBarEditWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    WNDPROC fn = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (uMsg)
    {
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            if (ComboBox_GetDroppedState(s_hAddrBarComboBox))
            {
                ComboBox_ShowDropdown(s_hAddrBarComboBox, FALSE);
                return 0;
            }
        }
        else if (wParam == VK_DELETE)
        {
            if (ComboBox_GetDroppedState(s_hAddrBarComboBox))
            {
                INT iItem = ComboBox_GetCurSel(s_hAddrBarComboBox);
                if (iItem != CB_ERR)
                {
                    ComboBox_DeleteString(s_hAddrBarComboBox, iItem);
                    g_settings.m_url_list.erase(g_settings.m_url_list.begin() + iItem);
                    return 0;
                }
            }
        }
        break;
    }
    LRESULT result = CallWindowProc(fn, hwnd, uMsg, wParam, lParam);
    return result;
}

void InitAddrBarComboBox(void)
{
    INT cch = GetWindowTextLengthW(s_hAddrBarComboBox);

    std::wstring str;
    str.resize(cch);

    GetWindowText(s_hAddrBarComboBox, &str[0], cch + 1);

    ComboBox_ResetContent(s_hAddrBarComboBox);
    for (auto& url : g_settings.m_url_list)
    {
        ComboBox_AddString(s_hAddrBarComboBox, url.c_str());
    }

    SetWindowText(s_hAddrBarComboBox, str.c_str());
}

void OnRefresh(HWND hwnd);

void DoMakeItKiosk(HWND hwnd, BOOL bKiosk)
{
    if (s_bKiosk == bKiosk)
        return;

    s_bKiosk = bKiosk;

    static DWORD s_old_style;
    static DWORD s_old_exstyle;
    static BOOL s_old_maximized;
    static RECT s_old_rect;

    if (bKiosk)
    {
        s_old_style = GetWindowLong(hwnd, GWL_STYLE);
        s_old_exstyle = GetWindowLong(hwnd, GWL_EXSTYLE);
        s_old_maximized = g_settings.m_bMaximized;
        GetWindowRect(hwnd, &s_old_rect);

        DWORD style = s_old_exstyle & ~(WS_CAPTION | WS_THICKFRAME);
        DWORD exstyle = s_old_exstyle & ~(WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE |
                                          WS_EX_DLGMODALFRAME | WS_EX_STATICEDGE);
        exstyle |= WS_EX_TOPMOST;
        SetWindowLong(hwnd, GWL_STYLE, style);
        SetWindowLong(hwnd, GWL_EXSTYLE, exstyle);

        HMONITOR hMonitor = ::MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);

        MONITORINFO mi;
        mi.cbSize = sizeof(mi);
        ::GetMonitorInfo(hMonitor, &mi);

        RECT& rect = mi.rcMonitor;
        ::MoveWindow(hwnd, rect.left, rect.top,
                     rect.right - rect.left, rect.bottom - rect.top,
                     TRUE);
        ShowWindow(hwnd, SW_SHOWNORMAL);
    }
    else
    {
        SetWindowLong(hwnd, GWL_STYLE, s_old_style);
        SetWindowLong(hwnd, GWL_EXSTYLE, s_old_exstyle);
        MoveWindow(hwnd, s_old_rect.left, s_old_rect.top,
                   s_old_rect.right - s_old_rect.left,
                   s_old_rect.bottom - s_old_rect.top,
                   TRUE);
        if (s_old_maximized)
            ShowWindow(hwnd, SW_MAXIMIZE);
        else
            ShowWindow(hwnd, SW_SHOWNORMAL);
    }

    InvalidateRect(hwnd, NULL, TRUE);
    PostMessage(hwnd, WM_MOVE, 0, 0);
    PostMessage(hwnd, WM_SIZE, 0, 0);

    OnRefresh(hwnd);
}

static
BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam)
{
    std::vector<HWND> *pbuttons = (std::vector<HWND> *)lParam;

    TCHAR szClass[64];
    GetClassName(hwnd, szClass, ARRAYSIZE(szClass));
    if (lstrcmpi(szClass, s_szButton) == 0)
    {
        pbuttons->push_back(hwnd);
    }

    return TRUE;
}

void DoDeleteButtons(HWND hwnd)
{
    std::vector<HWND> buttons;
    EnumChildWindows(hwnd, EnumChildProc, (LPARAM)&buttons);

    for (size_t i = 0; i < buttons.size(); ++i)
    {
        DestroyWindow(buttons[i]);
    }

    HWND hAddressBar = GetDlgItem(hwnd, ID_ADDRESS_BAR);
    DestroyWindow(hAddressBar);
}

BOOL LoadDataFile(HWND hwnd, const WCHAR *path, std::wstring& data)
{
    FILE *fp = _wfopen(path, L"r");
    if (!fp)
        return FALSE;

    std::vector<std::wstring> lines;
    std::vector<std::wstring> fields;
    char buf[256];
    WCHAR szText[256];
    while (fgets(buf, ARRAYSIZE(buf), fp))
    {
        if (char *pch = strchr(buf, ';'))
            *pch = 0;

        if (memcmp(buf, "\xEF\xBB\xBF", 3) == 0)
        {
            buf[0] = buf[1] = buf[2] = ' ';
        }

        MultiByteToWideChar(CP_UTF8, 0, buf, -1, szText, ARRAYSIZE(szText));
        StrTrimW(szText, L" \t\n\r\f\v");

        mstr_split(fields, szText, L"\t");
        if (fields.size() < 2)
            continue;

        for (size_t i = 0; i < fields.size(); ++i)
        {
            mstr_trim(fields[i], L" \t\n\r\f\v");
        }

        std::wstring line = mstr_join(fields, L"\t");
        lines.push_back(line);
    }

    fclose(fp);

    // Delete "..." and print preview if kiosk for security
    if (g_settings.m_kiosk_mode)
    {
        for (size_t i = 0; i < lines.size(); ++i)
        {
            mstr_split(fields, lines[i], L"\t");
            if (fields.size() >= 3 && fields[2].c_str()[0] == L'#')
            {
                INT id = _wtoi(&fields[2][1]);
                if (id == ID_DOTS)   // dots menu
                {
                    lines.erase(lines.begin() + i);
                    break;
                }
            }
        }
        for (size_t i = 0; i < lines.size(); ++i)
        {
            mstr_split(fields, lines[i], L"\t");
            if (fields.size() >= 3 && fields[2].c_str()[0] == L'#')
            {
                INT id = _wtoi(&fields[2][1]);
                if (id == ID_PRINT_PREVIEW ||
                    id == IDM_PRINTPREVIEW)
                {
                    // print preview
                    lines.erase(lines.begin() + i);
                    break;
                }
            }
        }
    }

    data = mstr_join(lines, L"\n");

    return TRUE;
}

BOOL LoadDataFile2(HWND hwnd, const WCHAR *filename, std::wstring& data)
{
    WCHAR szPath[MAX_PATH];

    GetModuleFileNameW(NULL, szPath, ARRAYSIZE(szPath));
    *PathFindFileNameW(szPath) = 0;
    PathAppendW(szPath, filename);
    if (LoadDataFile(hwnd, szPath, data))
        return TRUE;

    GetModuleFileNameW(NULL, szPath, ARRAYSIZE(szPath));
    *PathFindFileNameW(szPath) = 0;
    PathAppendW(szPath, L"..");
    PathAppendW(szPath, filename);
    if (LoadDataFile(hwnd, szPath, data))
        return TRUE;

    GetModuleFileNameW(NULL, szPath, ARRAYSIZE(szPath));
    *PathFindFileNameW(szPath) = 0;
    PathAppendW(szPath, L"..");
    PathAppendW(szPath, L"..");
    PathAppendW(szPath, filename);
    if (LoadDataFile(hwnd, szPath, data))
        return TRUE;

    return FALSE;
}

BOOL DoParseLines(HWND hwnd, const std::vector<std::wstring>& lines,
                  std::vector<HWND>& hwnds, HFONT hButtonFont)
{
    hwnds.clear();

    for (size_t i = 1; i < lines.size(); ++i)
    {
        std::vector<std::wstring> fields;
        mstr_split(fields, lines[i], L"\t");
        if (fields.size() < 3)
            continue;

        INT id;
        if (fields[2][0] == L'#')
        {
            id = _wtoi(&fields[2][1]);
        }
        else if (IsURL(fields[2].c_str()))
        {
            id = ID_GO_URL;
        }
        else
        {
            id = ID_EXECUTE_CMD;
        }

        HWND hCtrl = NULL;
        std::wstring& text = fields[0];
        if (id == ID_ADDRESS_BAR)
        {
            DWORD style = WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_AUTOHSCROLL |
                          CBS_DROPDOWN | CBS_HASSTRINGS | CBS_NOINTEGRALHEIGHT;
            hCtrl = CreateWindowEx(WS_EX_CLIENTEDGE, L"COMBOBOX", NULL,
                                   style, 0, 0, 0, DROPDOWN_HEIGHT,
                                   hwnd, (HMENU)ID_ADDRESS_BAR, s_hInst, NULL);
        }
        else if (id == ID_STOP_REFRESH)
        {
            INT k = text.find(L'/');
            if (k != std::wstring::npos)
            {
                s_strStop = text.substr(0, k);
                s_strRefresh = text.substr(k + 1);
            }
            else
            {
                s_strStop = L"Stop";
                s_strRefresh = L"Refresh";
            }
            DWORD style = WS_CHILD | WS_VISIBLE | BS_OWNERDRAW;
            hCtrl = CreateWindowEx(0, s_szButton, s_strRefresh.c_str(), style, 0, 0, 0, 0,
                                   hwnd, (HMENU)id, s_hInst, NULL);
            SendDlgItemMessage(hwnd, id, WM_SETFONT, (WPARAM)hButtonFont, TRUE);
        }
        else if (id == ID_DOTS)
        {
            DWORD style = WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | BS_PUSHLIKE;
            hCtrl = CreateWindowEx(0, s_szButton, text.c_str(), style, 0, 0, 0, 0,
                                   hwnd, (HMENU)id, s_hInst, NULL);
            SendDlgItemMessage(hwnd, id, WM_SETFONT, (WPARAM)hButtonFont, TRUE);
        }
        else if (text.size() && id != 0)
        {
            DWORD style = WS_CHILD | WS_VISIBLE | BS_OWNERDRAW;
            hCtrl = CreateWindowEx(0, s_szButton, text.c_str(), style, 0, 0, 0, 0,
                                   hwnd, (HMENU)id, s_hInst, NULL);
            SendDlgItemMessage(hwnd, id, WM_SETFONT, (WPARAM)hButtonFont, TRUE);
        }
        else
        {
            hCtrl = NULL;
        }

        s_hwnd2url[hCtrl] = fields[2];
        s_hwnd2color[hCtrl] = s_color;
        s_hwnd2bgcolor[hCtrl] = s_bgcolor;

        //printf("%p: %08X, %08X\n", hCtrl, s_color, s_bgcolor);

        hwnds.push_back(hCtrl);
    }

    return TRUE;
}

BOOL DoParseColors(HWND hwnd, const std::vector<std::wstring>& lines)
{
    s_color = RGB(0, 0, 0);
    s_bgcolor = RGB(255, 255, 255);
    if (lines.size())
    {
        std::vector<std::wstring> fields;
        mstr_split(fields, lines[0], L"\t");
        if (fields.size() >= 3)
        {
            char buf[32];
            WideCharToMultiByte(CP_UTF8, 0, fields[1].c_str(), -1, buf, 32, NULL, NULL);
            s_bgcolor = color_value_fix(color_value_parse(buf));

            WideCharToMultiByte(CP_UTF8, 0, fields[2].c_str(), -1, buf, 32, NULL, NULL);
            s_color = color_value_fix(color_value_parse(buf));
        }
    }
    return TRUE;
}

BOOL DoParseUpside(HWND hwnd, HFONT hButtonFont)
{
    std::wstring data;
    if (!LoadDataFile2(hwnd, LoadStringDx(IDS_UPSIDE), data))
    {
        assert(0);
        return FALSE;
    }

    s_upside_data = data;

    std::vector<std::wstring> lines;
    mstr_split(lines, s_upside_data, L"\n");

    DoParseColors(hwnd, lines);
    DoParseLines(hwnd, lines, s_upside_hwnds, hButtonFont);

    return TRUE;
}

BOOL DoParseDownside(HWND hwnd, HFONT hButtonFont)
{
    std::wstring data;
    if (!LoadDataFile2(hwnd, LoadStringDx(IDS_DOWNSIDE), data))
    {
        assert(0);
        return FALSE;
    }

    s_downside_data = data;

    std::vector<std::wstring> lines;
    mstr_split(lines, s_downside_data, L"\n");

    DoParseColors(hwnd, lines);
    DoParseLines(hwnd, lines, s_downside_hwnds, hButtonFont);

    return TRUE;
}

BOOL DoParseLeftSide(HWND hwnd, HFONT hButtonFont)
{
    std::wstring data;
    if (!LoadDataFile2(hwnd, LoadStringDx(IDS_LEFTSIDE), data))
    {
        assert(0);
        return FALSE;
    }

    s_leftside_data = data;

    std::vector<std::wstring> lines;
    mstr_split(lines, s_leftside_data, L"\n");

    DoParseColors(hwnd, lines);
    DoParseLines(hwnd, lines, s_leftside_hwnds, hButtonFont);

    return TRUE;
}

BOOL DoParseRightSide(HWND hwnd, HFONT hButtonFont)
{
    std::wstring data;
    if (!LoadDataFile2(hwnd, LoadStringDx(IDS_RIGHTSIDE), data))
    {
        assert(0);
        return FALSE;
    }

    s_rightside_data = data;

    std::vector<std::wstring> lines;
    mstr_split(lines, s_rightside_data, L"\n");

    DoParseColors(hwnd, lines);
    DoParseLines(hwnd, lines, s_rightside_hwnds, hButtonFont);

    return TRUE;
}

BOOL DoLoadMenu(HWND hwnd, UINT id, std::wstring& data)
{
    if (!LoadDataFile2(hwnd, LoadStringDx(id), data))
    {
        assert(0);
        return FALSE;
    }
    return TRUE;
}

BSTR GetActiveImgSrc(HWND hwnd);
BSTR GetActiveHREF(HWND hwnd);

HMENU DoCreateMenu(HWND hwnd, std::wstring& data)
{
    std::vector<std::wstring> lines;
    mstr_split(lines, data, L"\n");
    if (lines.empty())
        return NULL;

    HMENU hMenu = CreatePopupMenu();
    if (hMenu == NULL)
        return NULL;

    BSTR bstrImgSrc = GetActiveImgSrc(hwnd);
    BSTR bstrHREF = GetActiveHREF(hwnd);

    size_t count = 0;
    INT LinkID = ID_CUSTOM_LINK_01;
    s_menu_links.clear();
    for (size_t i = 0; i < lines.size(); ++i)
    {
        std::wstring& line = lines[i];
        if (line.c_str()[0] == L';')
            continue;

        std::vector<std::wstring> fields;
        mstr_split(fields, line, L"\t");

        if (fields.size() >= 2)
        {
            INT id = 0;
            if (fields[1].c_str()[0] == L'#')
            {
                id = _wtoi(fields[1].c_str() + 1);

                if (!bstrImgSrc && id == ID_SAVE_IMAGE_AS)
                {
                    continue;
                }
                if (!bstrHREF && id == ID_SAVE_TARGET_AS)
                {
                    continue;
                }
                if (!bstrHREF)
                {
                    if (id == IDM_FOLLOWLINKC || id == IDM_FOLLOWLINKN || id == IDM_COPYSHORTCUT)
                        continue;
                }
            }
            else
            {
                if (!IsURL(fields[1].c_str()))
                    continue;

                s_menu_links.push_back(fields[1].c_str());
                id = LinkID++;
                if (LinkID > ID_CUSTOM_LINK_16)
                    --LinkID;
            }

            if (id == 0 || fields[0].empty())
            {
                AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
            }
            else
            {
                AppendMenu(hMenu, MF_STRING, id, fields[0].c_str());
            }
            ++count;
        }
    }

    if (bstrImgSrc)
        SysFreeString(bstrImgSrc);

    if (bstrHREF)
        SysFreeString(bstrHREF);

    if (count == 0)
    {
        DestroyMenu(hMenu);
        hMenu = NULL;
    }
    return hMenu;
}

void DoPopupMenu(HWND hwnd, HMENU hMenu, POINT *ppt)
{
    SetForegroundWindow(hwnd);

    UINT uFlags = TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD;
    UINT id = TrackPopupMenu(hMenu, uFlags, ppt->x, ppt->y, 0, hwnd, NULL);

    PostMessage(hwnd, WM_COMMAND, id, 0);
    PostMessage(hwnd, WM_NULL, 0, 0);
}

BOOL DoReloadLayout(HWND hwnd, HFONT hButtonFont)
{
    s_hwnd2url.clear();
    DoDeleteButtons(hwnd);

    DoParseUpside(hwnd, hButtonFont);
    DoParseDownside(hwnd, hButtonFont);
    DoParseLeftSide(hwnd, hButtonFont);
    DoParseRightSide(hwnd, hButtonFont);

    if (GetDlgItem(hwnd, ID_ADDRESS_BAR) == NULL)
    {
        DWORD style = WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_AUTOHSCROLL |
                      CBS_DROPDOWN | CBS_HASSTRINGS | CBS_NOINTEGRALHEIGHT;
        CreateWindowEx(WS_EX_CLIENTEDGE, L"COMBOBOX", NULL, style, 0, 0, 0, DROPDOWN_HEIGHT,
                       hwnd, (HMENU)ID_ADDRESS_BAR, s_hInst, NULL);
    }

    s_hAddrBarComboBox = GetDlgItem(hwnd, ID_ADDRESS_BAR);

    INT cy1 = _wtoi(s_upside_data.c_str());
    if (cy1 == 0)
        cy1 = BTN_HEIGHT;
    INT cy2 = _wtoi(s_downside_data.c_str());
    if (cy2 == 0)
        cy2 = BTN_HEIGHT;
    INT height = cy1 ? cy1 : cy2;

    LOGFONT lf;
    GetObject(s_hButtonFont, sizeof(lf), &lf);
    lf.lfHeight = -(height - 8);
    s_hAddressFont = CreateFontIndirect(&lf);

    SendMessage(s_hAddrBarComboBox, WM_SETFONT, (WPARAM)s_hAddressFont, TRUE);
    InitAddrBarComboBox();
    ComboBox_LimitText(s_hAddrBarComboBox, 255);

    PostMessage(hwnd, WM_SIZE, 0, 0);
}

BOOL OnCreate(HWND hwnd, LPCREATESTRUCT lpCreateStruct)
{
    s_hMainWnd = hwnd;

    s_hbmSecure = LoadBitmap(s_hInst, MAKEINTRESOURCE(IDB_SECURE));
    s_hbmInsecure = LoadBitmap(s_hInst, MAKEINTRESOURCE(IDB_INSECURE));
    s_hAccel = LoadAccelerators(s_hInst, MAKEINTRESOURCE(1));

    g_settings.load();

    DoSetBrowserEmulation(g_settings.m_emulation);

    s_pWebBrowser = MWebBrowserEx::Create(hwnd);
    if (!s_pWebBrowser)
        return FALSE;

    IWebBrowser2 *pBrowser2 = s_pWebBrowser->GetIWebBrowser2();

    if (g_settings.m_ignore_errors || g_settings.m_kiosk_mode)
    {
        // Don't show script errors
        s_pWebBrowser->put_Silent(VARIANT_TRUE);
    }
    else
    {
        s_pWebBrowser->put_Silent(VARIANT_FALSE);
    }

    s_pEventSink->Connect(pBrowser2, &s_listener);

    s_hButtonFont = GetStockFont(DEFAULT_GUI_FONT);

    DoReloadLayout(hwnd, s_hButtonFont);

    DWORD style = WS_CHILD | WS_VISIBLE | SBS_SIZEGRIP;
    s_hStatusBar = CreateStatusWindow(style, LoadStringDx(IDS_LOADING), hwnd, stc1);
    if (!s_hStatusBar)
        return FALSE;

    s_hAddrBarEdit = GetTopWindow(s_hAddrBarComboBox);
    SHAutoComplete(s_hAddrBarEdit, SHACF_URLALL | SHACF_AUTOSUGGEST_FORCE_ON);

    if (g_settings.m_secure || g_settings.m_kiosk_mode)
        s_pWebBrowser->AllowInsecure(FALSE);
    else
        s_pWebBrowser->AllowInsecure(TRUE);

    if (!g_settings.m_kiosk_mode)
    {
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
    }
    else
    {
        DoMakeItKiosk(hwnd, TRUE);
    }

    WNDPROC fn = SubclassWindow(s_hAddrBarEdit, AddressBarEditWndProc);
    SetWindowLongPtr(s_hAddrBarEdit, GWLP_USERDATA, (LONG_PTR)fn);

    PostMessage(hwnd, WM_MOVE, 0, 0);
    PostMessage(hwnd, WM_SIZE, 0, 0);
    PostMessage(hwnd, WM_COMMAND, ID_PARSE_CMDLINE, 0);

    return TRUE;
}

void OnMove(HWND hwnd, int x, int y)
{
    RECT rc;

    if (!IsZoomed(hwnd) && !IsIconic(hwnd) && !s_bKiosk && !g_settings.m_kiosk_mode)
    {
        GetWindowRect(hwnd, &rc);
        g_settings.m_x = rc.left;
        g_settings.m_y = rc.top;
    }
}

INT DoResizeUpDownSide(HWND hwnd, LPRECT prc, const std::vector<HWND>& hwnds,
                       const std::wstring& data, BOOL bDown)
{
    RECT& rc = *prc;

    std::vector<std::wstring> lines;
    mstr_split(lines, data, L"\n");

    if (lines.size() <= 1)
    {
        return 0;
    }
    if (hwnds.size() != lines.size() - 1)
    {
        return 0;
    }

    INT x, y, cx, cy;
    cy = wcstoul(lines[0].c_str(), NULL, 10);
    if (cy == 0)
        cy = BTN_HEIGHT;
    x = rc.left;
    y = bDown ? rc.bottom - cy : rc.top;

    size_t i = 1;
    for (; i < lines.size(); ++i)
    {
        std::wstring str = lines[i];
        std::vector<std::wstring> fields;
        mstr_split(fields, str, L"\t");
        if (fields.size() < 3)
            continue;
        if (fields[1] == L"*")
            break;

        HWND hwndCtrl = hwnds[i - 1];

        cx = wcstoul(fields[1].c_str(), NULL, 10);
        if (cx == 0)
            cx = BTN_WIDTH;

        if (rc.right <= x + cx)
            cx = rc.right - x;

        if (hwndCtrl)
            MoveWindow(hwndCtrl, x, y, cx, cy, TRUE);
        x += cx;
    }

    INT x1 = x;
    x = rc.right;
    size_t k = i;

    for (i = lines.size(); i-- > k; )
    {
        std::wstring str = lines[i];
        std::vector<std::wstring> fields;
        mstr_split(fields, str, L"\t");
        if (fields.size() < 3)
            continue;

        if (fields[1] == L"*")
        {
            cx = x - x1;
            x -= cx;
        }
        else
        {
            cx = wcstoul(fields[1].c_str(), NULL, 10);
            if (cx == 0)
                cx = BTN_WIDTH;
            x -= cx;
        }

        HWND hwndCtrl = hwnds[i - 1];

        if (hwndCtrl)
            MoveWindow(hwndCtrl, x, y, cx, cy, TRUE);

        if (fields[1] == L"*")
            break;
    }

    return cy;
}

INT DoResizeLeftRightSide(HWND hwnd, LPRECT prc, const std::vector<HWND>& hwnds,
                          const std::wstring& data, BOOL bRight)
{
    RECT& rc = *prc;

    std::vector<std::wstring> lines;
    mstr_split(lines, data, L"\n");

    if (lines.size() <= 1)
    {
        return 0;
    }
    if (hwnds.size() != lines.size() - 1)
    {
        return 0;
    }

    INT x, y, cx, cy;
    cx = wcstoul(lines[0].c_str(), NULL, 10);
    if (cx == 0)
        cx = BTN_WIDTH;
    x = bRight ? rc.right - cx : rc.left;
    y = rc.top;

    size_t i = 1;
    for (; i < lines.size(); ++i)
    {
        std::wstring str = lines[i];
        std::vector<std::wstring> fields;
        mstr_split(fields, str, L"\t");
        if (fields.size() < 3)
            continue;
        if (fields[1] == L"*")
            break;

        HWND hwndCtrl = hwnds[i - 1];

        cy = wcstoul(fields[1].c_str(), NULL, 10);
        if (cy == 0)
            cy = BTN_HEIGHT;

        if (rc.bottom <= y + cy)
            cy = rc.bottom - y;

        if (hwndCtrl)
            MoveWindow(hwndCtrl, x, y, cx, cy, TRUE);
        y += cy;
    }

    INT y1 = y;
    y = rc.bottom;
    size_t k = i;

    for (i = lines.size(); i-- > k; )
    {
        std::wstring str = lines[i];
        std::vector<std::wstring> fields;
        mstr_split(fields, str, L"\t");
        if (fields.size() < 3)
            continue;

        if (fields[1] == L"*")
        {
            cy = y - y1;
            y += cy;
        }
        else
        {
            cy = wcstoul(fields[1].c_str(), NULL, 10);
            if (cy == 0)
                cy = BTN_HEIGHT;
            y += cy;
        }

        HWND hwndCtrl = hwnds[i - 1];

        if (hwndCtrl)
            MoveWindow(hwndCtrl, x, y, cx, cy, TRUE);

        if (fields[1] == L"*")
            break;
    }

    return cx;
}

void OnSize(HWND hwnd, UINT state, int cx, int cy)
{
    RECT rc;

    if (!IsZoomed(hwnd) && !IsIconic(hwnd) && !s_bKiosk && !g_settings.m_kiosk_mode)
    {
        GetWindowRect(hwnd, &rc);
        g_settings.m_cx = rc.right - rc.left;
        g_settings.m_cy = rc.bottom - rc.top;
    }

    GetClientRect(hwnd, &rc);

    RECT rcStatus;
    SendMessage(s_hStatusBar, WM_SIZE, 0, 0);
    GetWindowRect(s_hStatusBar, &rcStatus);
    rc.bottom -= rcStatus.bottom - rcStatus.top;

    INT cyUpSide = DoResizeUpDownSide(hwnd, &rc, s_upside_hwnds, s_upside_data, FALSE);
    rc.top += cyUpSide;

    INT cyDownSide = DoResizeUpDownSide(hwnd, &rc, s_downside_hwnds, s_downside_data, TRUE);
    rc.bottom -= cyDownSide;

    INT cxLeftSide = DoResizeLeftRightSide(hwnd, &rc, s_leftside_hwnds, s_leftside_data, FALSE);
    rc.left += cxLeftSide;

    INT cxRightSide = DoResizeLeftRightSide(hwnd, &rc, s_rightside_hwnds, s_rightside_data, TRUE);
    rc.right -= cxRightSide;

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

void OnRefresh(HWND hwnd)
{
    DoReloadLayout(hwnd, s_hButtonFont);
    s_pWebBrowser->Refresh();
    SetDlgItemText(hwnd, ID_ADDRESS_BAR, s_strURL.c_str());
}

void OnStop(HWND hwnd)
{
    s_pWebBrowser->Stop();
    s_pWebBrowser->StopDownload();
}

void OnStopRefresh(HWND hwnd)
{
    if (s_bLoadingPage)
    {
        OnStop(hwnd);
    }
    else
    {
        OnRefresh(hwnd);
    }
}

void OnGoToAddressBar(HWND hwnd)
{
    ComboBox_SetEditSel(s_hAddrBarComboBox, 0, -1);
    SetFocus(s_hAddrBarComboBox);
}

void OnGo(HWND hwnd)
{
    INT cch = GetWindowTextLengthW(s_hAddrBarEdit);

    std::wstring str;
    str.resize(cch);

    GetWindowTextW(s_hAddrBarEdit, &str[0], cch + 1);

    StrTrimW(&str[0], L" \t\n\r\f\v");
    str.resize(wcslen(str.c_str()));

    if (str.find(L' ') != std::wstring::npos)
    {
        DoSearch(hwnd, str.c_str());
    }
    else
    {
        if (str.empty())
            DoNavigate(hwnd, L"about:blank");
        else
            DoNavigate(hwnd, str.c_str());
    }
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

BOOL DoThreatScan(HWND hwnd, LPCWSTR file, AmsiResult& result)
{
    AmsiScanner scanner(L"katahiromz's SimpleBrowser");
    if (!scanner.IsLoaded())
    {
        return FALSE;
    }

    HAMSISESSION hSession;
    HRESULT hr = scanner.OpenSession(&hSession);
    if (FAILED(hr))
    {
        return FALSE;
    }

    hr = scanner.DoScanFile(hSession, file, result);

    scanner.CloseSession(&hSession);

    return SUCCEEDED(hr);
}

struct DOWNLOADING
{
    HWND hDlg;
    std::wstring strURL;
    std::wstring strFilename;
    MBindStatusCallback *pCallback;
    double progressOld;
    double speeds[32];
    DWORD dwTick;
};

unsigned __stdcall downloading_proc(void *arg)
{
    DOWNLOADING *pDownloading = (DOWNLOADING *)arg;
    MBindStatusCallback *pCallback = pDownloading->pCallback;
    HWND hwnd = pDownloading->hDlg;
    pDownloading->dwTick = GetTickCount();

    HRESULT hr = URLDownloadToFile(NULL,
        pDownloading->strURL.c_str(),
        pDownloading->strFilename.c_str(), 0, pCallback);
    if (FAILED(hr))
    {
        pCallback->SetCancelled();
        return 1;
    }

    while (!pCallback->IsCompleted() && !pCallback->IsCancelled())
    {
        Sleep(200);
    }

    s_downloadings[hwnd] = FALSE;

    KillTimer(hwnd, 999);

    if (pCallback->IsCompleted() && !pCallback->IsCancelled())
    {
        // update dialog info
        SetDlgItemTextW(hwnd, IDCANCEL, LoadStringDx(IDS_CLOSE));
        SetDlgItemTextW(hwnd, stc3, LoadStringDx(IDS_DL_COMPLETE));
        SetDlgItemTextW(hwnd, stc4, LoadStringDx(IDS_WAIT_SCAN_PLEASE));
        SetWindowTextW(hwnd, LoadStringDx(IDS_DL_COMPLETE));
        SendDlgItemMessage(hwnd, ctl1, PBM_SETRANGE32, 0, 100);
        SendDlgItemMessage(hwnd, ctl1, PBM_SETPOS, 100, 0);

        // alternate data stream (ADS)
        ADS_ENTRY entry;
        entry.name = L":Zone.Identifier";
        std::string data = "[ZoneTransfer]\r\nZoneId=3\r\n";
        ADS_put_data(pDownloading->strFilename.c_str(), entry, data);

        // virus scan
        AmsiResult result;
        LPCWSTR file = pDownloading->strFilename.c_str();
        if (DoThreatScan(hwnd, file, result))
        {
            if (result.is_malware)
            {
                if (DeleteFileW(file))
                {
                    SetDlgItemTextW(hwnd, stc4, LoadStringDx(IDS_VIRUS_FOUND_DELETED));
                }
                else
                {
                    SetDlgItemTextW(hwnd, stc4, LoadStringDx(IDS_VIRUS_FOUND));
                }
            }
            else
            {
                SetDlgItemTextW(hwnd, stc4, LoadStringDx(IDS_NO_VIRUS));
            }
        }
        else
        {
            SetDlgItemTextW(hwnd, stc4, LoadStringDx(IDS_CANT_SCAN_VIRUS));
        }
    }

    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)0);
    Sleep(100);

    if (!pCallback->IsCompleted() || pCallback->IsCancelled())
    {
        DeleteFileW(pDownloading->strFilename.c_str());
    }

    pCallback->Release();
    delete pDownloading;

    return 0;
}

BOOL Downloading_OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    printf("Downloading_OnInitDialog\n");
    DOWNLOADING *pDownloading = (DOWNLOADING *)lParam;
    MBindStatusCallback *pCallback = pDownloading->pCallback;
    assert(pCallback);
    pDownloading->hDlg = hwnd;
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pDownloading);

    SetDlgItemTextW(hwnd, stc1, pDownloading->strURL.c_str());
    SetDlgItemTextW(hwnd, stc2, pDownloading->strFilename.c_str());

    printf("strURL: %ls\n", pDownloading->strURL.c_str());
    printf("strFilename: %ls\n", pDownloading->strFilename.c_str());
    fflush(stdout);

    HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, downloading_proc, pDownloading, 0, NULL);
    if (!hThread)
    {
        pCallback->SetCancelled();
        printf("FAILED\n");
        MessageBoxW(hwnd, L"FAILED", NULL, MB_ICONERROR);
        pCallback->SetCancelled();
        return TRUE;
    }
    CloseHandle(hThread);

    printf("Downloading_OnInitDialog: end\n");
    SetTimer(hwnd, 999, DOWNLOAD_TIMER_INTERVAL, NULL);
    return TRUE;
}

void Downloading_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    DOWNLOADING *pDownloading = (DOWNLOADING *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    MBindStatusCallback *pCallback = (pDownloading ? pDownloading->pCallback: NULL);
    switch (id)
    {
    case IDCANCEL:
        printf("IDCANCEL\n");
        KillTimer(hwnd, 999);
        if (pDownloading)
        {
            if (pCallback && !pCallback->IsCompleted())
            {
                pCallback->SetCancelled();
            }
        }
        DestroyWindow(hwnd);
        break;
    }
}

void Downloading_OnTimer(HWND hwnd, UINT id)
{
    DOWNLOADING *pDownloading = (DOWNLOADING *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    MBindStatusCallback *pCallback = pDownloading->pCallback;
    if (id == 999)
    {
        SendDlgItemMessage(hwnd, ctl1, PBM_SETRANGE32, 0, pCallback->m_ulProgressMax);
        SendDlgItemMessage(hwnd, ctl1, PBM_SETPOS, pCallback->m_ulProgress, 0);

        DWORD dwTick1 = pDownloading->dwTick;
        DWORD dwTick2 = GetTickCount();
        double timeSpan = (dwTick2 - dwTick1) / 1000.0;
        double progress = pCallback->m_ulProgress;
        double progressMax = pCallback->m_ulProgressMax;
        double speed = (progress - pDownloading->progressOld) / timeSpan;

        for (size_t i = 1; i < ARRAYSIZE(pDownloading->speeds); ++i)
        {
            pDownloading->speeds[i - 1] = pDownloading->speeds[i];
        }
        pDownloading->speeds[ARRAYSIZE(pDownloading->speeds) - 1] = speed;

        pDownloading->progressOld = progress;
        pDownloading->dwTick = GetTickCount();

        if (pDownloading->speeds[0] != 0)
        {
            double sum = 0;
            for (size_t i = 0; i < ARRAYSIZE(pDownloading->speeds); ++i)
            {
                sum += pDownloading->speeds[i];
            }
            speed = sum / ARRAYSIZE(pDownloading->speeds);

            DWORD dwSECS = DWORD((progressMax - progress) / speed) + 10;
            DWORD dwMIN = (dwSECS / 60) % 60;
            DWORD dwHRS = dwSECS / (60 * 60);
            dwSECS %= 60;

            WCHAR szText[128];
            if (dwHRS > 0)
            {
                StringCbPrintfW(szText, sizeof(szText), LoadStringDx(IDS_DOWNLOAD_PROGRESS_3),
                    (ULONG)progress, (ULONG)progressMax, dwHRS, dwMIN);
            }
            else if (dwMIN > 3)
            {
                StringCbPrintfW(szText, sizeof(szText), LoadStringDx(IDS_DOWNLOAD_PROGRESS_2),
                    (ULONG)progress, (ULONG)progressMax, dwMIN);
            }
            else if (dwMIN > 0)
            {
                StringCbPrintfW(szText, sizeof(szText), LoadStringDx(IDS_DOWNLOAD_PROGRESS_1),
                    (ULONG)progress, (ULONG)progressMax, dwMIN, dwSECS);
            }
            else
            {
                StringCbPrintfW(szText, sizeof(szText), LoadStringDx(IDS_DOWNLOAD_PROGRESS_0),
                    (ULONG)progress, (ULONG)progressMax, dwSECS);
            }
            SetDlgItemTextW(hwnd, stc4, szText);
        }
        else
        {
            SetDlgItemTextW(hwnd, stc4, NULL);
        }
    }
}

void Downloading_OnDestroy(HWND hwnd)
{
    printf("Downloading_OnDestroy\n");
    KillTimer(hwnd, 999);

    DOWNLOADING *pDownloading = (DOWNLOADING *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (pDownloading)
    {
        MBindStatusCallback *pCallback = pDownloading->pCallback;

        if (!pCallback->IsCompleted())
        {
            if (!pCallback->IsCancelled())
                pCallback->SetCancelled();
        }
    }

    s_downloadings.erase(hwnd);

    printf("Downloading_OnDestroy: end\n");
}

INT_PTR CALLBACK
DownloadingDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_INITDIALOG, Downloading_OnInitDialog);
        HANDLE_MSG(hwnd, WM_COMMAND, Downloading_OnCommand);
        HANDLE_MSG(hwnd, WM_TIMER, Downloading_OnTimer);
        HANDLE_MSG(hwnd, WM_DESTROY, Downloading_OnDestroy);
    }
    return 0;
}

void TranslateFileName(LPWSTR file, size_t cchMax)
{
    char buf[256];
    WideCharToMultiByte(CP_UTF8, 0, file, -1, buf, ARRAYSIZE(buf), NULL, NULL);

    WCHAR sz[256];
    std::string str = URL_decode(buf);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, sz, ARRAYSIZE(sz));

    StringCbCopyW(file, cchMax, sz);

    while (*file)
    {
        if (wcschr(L"\\/:*?\"<>|", *file) != NULL)
        {
            *file = L'_';
        }
        ++file;
    }
}

BOOL DoSaveURL(HWND hwnd, LPCWSTR pszURL)
{
    printf("DoSaveURL: %ls\n", pszURL);
    LPWSTR url = wcsdup(pszURL);

    LPWSTR pch = wcsrchr(url, L'?');
    if (pch)
        *pch = 0;

    pch = wcsrchr(url, L'/');
    if (pch)
        ++pch;
    else
        pch = url;

    LPWSTR pchFileName = pch;

    pch = PathFindExtension(pch);
    char extension[64];
    ::WideCharToMultiByte(CP_ACP, 0, pch, -1, extension, 64, NULL, NULL);
    const char *pszMime = mime_info_mime_from_extension(extension);
    if (pszMime == NULL)
        pszMime = "application/octet-stream";

    //MessageBoxA(NULL, pszMime, NULL, 0);
    //MessageBoxW(NULL, pch, L"extension", 0);
    //MessageBoxW(NULL, pchFileName, L"filename", 0);

    WCHAR file[MAX_PATH] = L"*";

    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = OPENFILENAME_SIZE_VERSION_400W;
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = file;
    ofn.nMaxFile = ARRAYSIZE(file);
    ofn.Flags = OFN_EXPLORER | OFN_ENABLESIZING | OFN_PATHMUSTEXIST |
                OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;

    if (lstrcmpiA(extension, ".exe") == 0)
    {
        ofn.lpstrFilter = MakeFilterDx(LoadStringDx(IDS_EXEFILTER));
        ofn.lpstrDefExt = L"exe";
        if (*pchFileName && pchFileName != pch)
            StringCbCopy(file, sizeof(file), pchFileName);
    }
    else if (lstrcmpiA(extension, ".dll") == 0)
    {
        ofn.lpstrFilter = MakeFilterDx(LoadStringDx(IDS_DLLFILTER));
        ofn.lpstrDefExt = L"dll";
        if (*pchFileName && pchFileName != pch)
            StringCbCopy(file, sizeof(file), pchFileName);
    }
    else if (strcmp(pszMime, "text/plain") == 0)
    {
        ofn.lpstrFilter = MakeFilterDx(LoadStringDx(IDS_TXTFILTER));
        ofn.lpstrDefExt = L"txt";
        if (*pchFileName && pchFileName != pch)
            StringCbCopy(file, sizeof(file), pchFileName);
    }
    else if (strcmp(pszMime, "text/html") == 0)
    {
        ofn.lpstrFilter = MakeFilterDx(LoadStringDx(IDS_HTMLFILTER));
        ofn.lpstrDefExt = L"html";
        if (*pchFileName && pchFileName != pch)
            StringCbCopy(file, sizeof(file), pchFileName);
    }
    else if (strcmp(pszMime, "image/jpeg") == 0)
    {
        ofn.lpstrFilter = MakeFilterDx(LoadStringDx(IDS_IMGFILTER));
        ofn.lpstrDefExt = L"jpg";
        if (*pchFileName && pchFileName != pch)
            StringCbCopy(file, sizeof(file), pchFileName);
    }
    else if (strcmp(pszMime, "image/png") == 0)
    {
        ofn.lpstrFilter = MakeFilterDx(LoadStringDx(IDS_IMGFILTER));
        ofn.lpstrDefExt = L"png";
        if (*pchFileName && pchFileName != pch)
            StringCbCopy(file, sizeof(file), pchFileName);
    }
    else if (strcmp(pszMime, "image/gif") == 0)
    {
        ofn.lpstrFilter = MakeFilterDx(LoadStringDx(IDS_IMGFILTER));
        ofn.lpstrDefExt = L"gif";
        if (*pchFileName && pchFileName != pch)
            StringCbCopy(file, sizeof(file), pchFileName);
    }
    else if (strcmp(pszMime, "image/tiff") == 0)
    {
        ofn.lpstrFilter = MakeFilterDx(LoadStringDx(IDS_IMGFILTER));
        ofn.lpstrDefExt = L"tif";
        if (*pchFileName && pchFileName != pch)
            StringCbCopy(file, sizeof(file), pchFileName);
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
        if (*pchFileName && pchFileName != pch)
            StringCbCopy(file, sizeof(file), pchFileName);
    }
    else if (*pch == 0)
    {
        ofn.lpstrFilter = MakeFilterDx(LoadStringDx(IDS_HTMLFILTER));
        ofn.lpstrDefExt = L"html";
        if (*pchFileName && pchFileName != pch)
            StringCbCopy(file, sizeof(file), pchFileName);
    }
    else
    {
        ofn.lpstrFilter = MakeFilterDx(LoadStringDx(IDS_ALLFILTER));
        ofn.lpstrDefExt = NULL;
        if (*pchFileName && pchFileName != pch)
            StringCbCopy(file, sizeof(file), pchFileName);
    }

    TranslateFileName(file, ARRAYSIZE(file));

    if (::GetSaveFileName(&ofn))
    {
        DOWNLOADING *pDownloading = new DOWNLOADING;
        pDownloading->strURL = pszURL;
        pDownloading->strFilename = file;
        pDownloading->pCallback = MBindStatusCallback::Create();
        assert(pDownloading->pCallback);

        for (size_t i = 0; i < ARRAYSIZE(pDownloading->speeds); ++i)
        {
            pDownloading->speeds[i] = 0;
        }

        HWND hDlg = CreateDialogParam(s_hInst, MAKEINTRESOURCE(IDD_DOWNLOADING),
                                      hwnd, DownloadingDlgProc, (LPARAM)pDownloading);
        ShowWindow(hDlg, SW_SHOWNORMAL);
        UpdateWindow(hDlg);
        s_downloadings.insert(std::make_pair(hDlg, TRUE));
    }

    std::free(url);
}

void OnSave(HWND hwnd)
{
    BSTR bstrURL = NULL;
    if (SUCCEEDED(s_pWebBrowser->get_LocationURL(&bstrURL)))
    {
        DoSaveURL(hwnd, bstrURL);

        ::SysFreeString(bstrURL);
    }
}

void OnViewSourceDone(HWND hwnd)
{
    s_listener.OnTitleTextChange(LoadStringDx(IDS_SOURCE));
}

void OnDots(HWND hwnd)
{
    HWND hwndDots = GetDlgItem(hwnd, ID_DOTS);

    if (GetAsyncKeyState(VK_MENU) < 0 &&
        GetAsyncKeyState(L'F') < 0)
    {
        // Alt+F
        SetCheck(hwndDots, TRUE);
    }
    else
    {
        SetCheck(hwndDots, !GetCheck(hwndDots));
        if (!GetCheck(hwndDots))
        {
            return;
        }
    }

    RECT rc;
    if (IsWindow(hwndDots))
        GetWindowRect(hwndDots, &rc);
    else
        GetWindowRect(hwnd, &rc);

    POINT pt;
    GetCursorPos(&pt);

    if (!PtInRect(&rc, pt))
    {
        pt.x = (rc.left + rc.right) / 2;
        pt.y = (rc.top + rc.bottom) / 2;
    }

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
        SetCheck(hwndDots, FALSE);
    }
}

void OnViewSource(HWND hwnd)
{
    INT cch = GetWindowTextLengthW(s_hAddrBarEdit);

    std::wstring str;
    str.resize(cch);

    GetWindowTextW(s_hAddrBarEdit, &str[0], cch + 1);
    StrTrimW(&str[0], L" \t\n\r\f\v");

    std::wstring url = str.c_str();
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
    for (wchar_t wch : str)
    {
        if (wcschr(L"\\/:*?\"<>|", wch) != NULL)
        {
            ret += L'_';
        }
        else
        {
            ret += wch;
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
    WCHAR sz[32];
    for (INT i = 1; i < 64; ++i)
    {
        strPath = szPath;
        if (i > 1)
        {
            StringCbPrintfW(sz, sizeof(sz), L" (%u)", i);
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

    InitAddrBarComboBox();

    if (g_settings.m_ignore_errors || g_settings.m_kiosk_mode)
    {
        // Don't show script errors
        s_pWebBrowser->put_Silent(VARIANT_TRUE);
    }
    else
    {
        s_pWebBrowser->put_Silent(VARIANT_FALSE);
    }

    if (g_settings.m_secure || g_settings.m_kiosk_mode)
        s_pWebBrowser->AllowInsecure(FALSE);
    else
        s_pWebBrowser->AllowInsecure(TRUE);

    if (g_settings.m_kiosk_mode)
        DoMakeItKiosk(hwnd, TRUE);
    else
        DoMakeItKiosk(hwnd, FALSE);

    PostMessage(hwnd, WM_MOVE, 0, 0);
    PostMessage(hwnd, WM_SIZE, 0, 0);
}

void OnAddToComboBox(HWND hwnd)
{
    INT cch = ComboBox_GetTextLength(s_hAddrBarComboBox);

    std::wstring str;
    str.resize(cch);

    ComboBox_GetText(s_hAddrBarComboBox, &str[0], cch + 1);
    printf("OnAddToComboBox: %ls\n", str.c_str());

    std::wstring url = str.c_str();
    INT iItem = ComboBox_FindStringExact(s_hAddrBarComboBox, -1, (LPARAM)url.c_str());
    if (iItem != CB_ERR)
    {
        ComboBox_DeleteString(s_hAddrBarComboBox, iItem);
    }

    for (size_t i = 0; i < g_settings.m_url_list.size(); ++i)
    {
        if (g_settings.m_url_list[i] == url)
        {
            g_settings.m_url_list.erase(g_settings.m_url_list.begin() + i);
            break;
        }
    }

    ComboBox_InsertString(s_hAddrBarComboBox, 0, url.c_str());
    g_settings.m_url_list.insert(g_settings.m_url_list.begin(), url);

    ComboBox_SetText(s_hAddrBarComboBox, str.c_str());
}

void OnDocumentComplete(HWND hwnd)
{
    SetWindowTextW(s_hAddrBarComboBox, s_strURL.c_str());
}

void OnAddressBar(HWND hwnd, HWND hwndCtl, UINT codeNotify)
{
    switch (codeNotify)
    {
    case CBN_SELENDOK:
        {
            INT iItem = (INT)ComboBox_GetCurSel(s_hAddrBarComboBox);
            if (iItem != CB_ERR)
            {
                INT cch = ComboBox_GetLBTextLen(s_hAddrBarComboBox, iItem);

                std::wstring str;
                str.resize(cch);

                ComboBox_GetLBText(s_hAddrBarComboBox, iItem, &str[0]);
                DoNavigate(hwnd, str.c_str());
            }
        }
        break;
    }
}

void OnExit(HWND hwnd)
{
    DestroyWindow(hwnd);
}

void OnNew(HWND hwnd, LPCWSTR url)
{
    TCHAR szPath[MAX_PATH];
    GetModuleFileName(NULL, szPath, ARRAYSIZE(szPath));

    if (url)
    {
        ShellExecute(hwnd, NULL, szPath, url, NULL, SW_SHOWNORMAL);
    }
    else
    {
        ShellExecute(hwnd, NULL, szPath, g_settings.m_homepage.c_str(), NULL, SW_SHOWNORMAL);
    }
}

void OnKiosk(HWND hwnd)
{
    g_settings.m_kiosk_mode = !s_bKiosk;
    DoMakeItKiosk(hwnd, !s_bKiosk);
}

void OnKioskOff(HWND hwnd)
{
    g_settings.m_kiosk_mode = FALSE;
    DoMakeItKiosk(hwnd, FALSE);
}

void OnKioskOn(HWND hwnd)
{
    g_settings.m_kiosk_mode = TRUE;
    DoMakeItKiosk(hwnd, TRUE);
}

void OnGoURL(HWND hwnd, HWND hwndCtl)
{
    auto it = s_hwnd2url.find(hwndCtl);
    if (it != s_hwnd2url.end())
    {
        DoNavigate(hwnd, it->second.c_str());
    }
}

BOOL DoExecute(HWND hwnd, LPCWSTR pszCmd, INT nCmdShow)
{
    WCHAR szText[256];
    StringCbCopyW(szText, sizeof(szText), pszCmd);

    WCHAR szPath[MAX_PATH];
    GetModuleFileName(NULL, szPath, ARRAYSIZE(szPath));
    *PathFindFileName(szPath) = 0;

    STARTUPINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.wShowWindow = nCmdShow;
    si.dwFlags = STARTF_USESHOWWINDOW;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    HANDLE hProcess;
    BOOL ret = CreateProcessW(NULL, szText, NULL, NULL, TRUE, 0, NULL, szPath, &si, &pi);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return ret;
}

void OnExecuteCmd(HWND hwnd, HWND hwndCtl)
{
    auto it = s_hwnd2url.find(hwndCtl);
    if (it != s_hwnd2url.end())
    {
        DoExecute(hwnd, it->second.c_str(), SW_SHOWNORMAL);
    }
}

void OnCancelPrinting(HWND hwnd)
{
    DoExecute(hwnd, L"CancelPrinting.bat", SW_HIDE);
}

void OnUp(HWND hwnd)
{
    if (IHTMLDocument2 *pDocument = s_pWebBrowser->GetIHTMLDocument2())
    {
        IHTMLWindow2 *pWindow = NULL;
        pDocument->get_parentWindow(&pWindow);
        if (pWindow)
        {
            for (int i = 0; i < 10; ++i)
            {
                pWindow->scrollBy(0, -40);
            }
            pWindow->Release();
        }
        pDocument->Release();
    }
}

void OnDown(HWND hwnd)
{
    if (IHTMLDocument2 *pDocument = s_pWebBrowser->GetIHTMLDocument2())
    {
        IHTMLWindow2 *pWindow = NULL;
        pDocument->get_parentWindow(&pWindow);
        if (pWindow)
        {
            for (int i = 0; i < 10; ++i)
            {
                pWindow->scrollBy(0, 40);
            }
            pWindow->Release();
        }
        pDocument->Release();
    }
}

void OnZoomUp(HWND hwnd)
{
    s_pWebBrowser->ZoomUp();
}

void OnZoomDown(HWND hwnd)
{
    s_pWebBrowser->ZoomDown();
}

void OnZoom100(HWND hwnd)
{
    s_pWebBrowser->Zoom100();
}

BSTR DoGetImageSrcFromImg(IHTMLElement *pElement)
{
    if (BSTR bstrSRC = SysAllocString(L"src"))
    {
        VARIANT var;
        VariantInit(&var);
        // IHTMLElement::getAttribute
        pElement->getAttribute(bstrSRC, 2 | 4, &var);
        SysFreeString(bstrSRC);
        return var.bstrVal;
    }
    return NULL;
}

BSTR DoGetHrefFromATag(IHTMLElement *pElement)
{
    if (BSTR bstrHREF = SysAllocString(L"href"))
    {
        VARIANT var;
        VariantInit(&var);
        // IHTMLElement::getAttribute
        pElement->getAttribute(bstrHREF, 2 | 4, &var);
        SysFreeString(bstrHREF);
        return var.bstrVal;
    }
    return NULL;
}

BSTR DoGetImageSrcFromElement(IHTMLElement *pElement)
{
    BSTR bstrTag;
    pElement->get_tagName(&bstrTag);
    if (!bstrTag)
        return NULL;

    BSTR bstrSRC = NULL;
    if (lstrcmpiW(bstrTag, L"img") == 0)
    {
        bstrSRC = DoGetImageSrcFromImg(pElement);
        return bstrSRC;
    }

    IDispatch *pdisp = NULL;
    pElement->get_children(&pdisp);
    if (pdisp)
    {
        IHTMLElementCollection *pColl = NULL;
        pdisp->QueryInterface(&pColl);
        if (pColl)
        {
            long n = 0;
            pColl->get_length(&n);
            if (n)
            {
                // IHTMLElementCollection::item
                VARIANT varName, varIndex;
                for (long i = 0; i < n; ++i)
                {
                    VariantInit(&varName);
                    varName.vt = VT_I4;
                    varName.lVal = i;

                    VariantInit(&varIndex);
                    varIndex.vt = VT_I4;
                    varIndex.lVal = 0;

                    IDispatch *pDispatch = NULL;
                    pColl->item(varName, varIndex, &pDispatch);
                    if (pDispatch)
                    {
                        IHTMLElement *pElement = NULL;
                        pDispatch->QueryInterface(&pElement);
                        if (pElement)
                        {
                            bstrSRC = DoGetImageSrcFromImg(pElement);
                            pElement->Release();
                        }

                        pDispatch->Release();
                    }

                    if (bstrSRC)
                        break;
                }
            }
            pColl->Release();
        }
        pdisp->Release();
    }

    return bstrSRC;
}

BSTR DoGetHyperlinkHrefFromElement(IHTMLElement *pElement)
{
    BSTR bstrTag;
    pElement->get_tagName(&bstrTag);
    if (!bstrTag)
        return NULL;

    BSTR bstrHREF = NULL;
    if (lstrcmpiW(bstrTag, L"a") == 0)
    {
        bstrHREF = DoGetHrefFromATag(pElement);
        return bstrHREF;
    }

    IDispatch *pdisp = NULL;
    pElement->get_children(&pdisp);
    if (pdisp)
    {
        IHTMLElementCollection *pColl = NULL;
        pdisp->QueryInterface(&pColl);
        if (pColl)
        {
            long n = 0;
            pColl->get_length(&n);
            if (n)
            {
                // IHTMLElementCollection::item
                VARIANT varName, varIndex;
                for (long i = 0; i < n; ++i)
                {
                    VariantInit(&varName);
                    varName.vt = VT_I4;
                    varName.lVal = i;

                    VariantInit(&varIndex);
                    varIndex.vt = VT_I4;
                    varIndex.lVal = 0;

                    IDispatch *pDispatch = NULL;
                    pColl->item(varName, varIndex, &pDispatch);
                    if (pDispatch)
                    {
                        IHTMLElement *pElement = NULL;
                        pDispatch->QueryInterface(&pElement);
                        if (pElement)
                        {
                            bstrHREF = DoGetHrefFromATag(pElement);
                            pElement->Release();
                        }

                        pDispatch->Release();
                    }

                    if (bstrHREF)
                        break;
                }
            }
            pColl->Release();
        }
        pdisp->Release();
    }

    return bstrHREF;
}

BSTR GetActiveImgSrc(HWND hwnd)
{
    BSTR ret = NULL;
    IDispatch *pDisp = NULL;
    s_pWebBrowser->GetIWebBrowser2()->get_Document(&pDisp);
    if (pDisp)
    {
        if (IHTMLDocument2 *pDocument = static_cast<IHTMLDocument2 *>(pDisp))
        {
            IHTMLElement *pElement = NULL;
            pDocument->get_activeElement(&pElement);
            if (pElement)
            {
                if (BSTR bstrURL = DoGetImageSrcFromElement(pElement))
                {
                    ret = bstrURL;
                }
                pElement->Release();
            }
        }
        pDisp->Release();
    }
    return ret;
}

BSTR GetActiveHREF(HWND hwnd)
{
    BSTR ret = NULL;
    IDispatch *pDisp = NULL;
    s_pWebBrowser->GetIWebBrowser2()->get_Document(&pDisp);
    if (pDisp)
    {
        if (IHTMLDocument2 *pDocument = static_cast<IHTMLDocument2 *>(pDisp))
        {
            IHTMLElement *pElement = NULL;
            pDocument->get_activeElement(&pElement);
            if (pElement)
            {
                if (BSTR bstrURL = DoGetHyperlinkHrefFromElement(pElement))
                {
                    ret = bstrURL;
                }
                pElement->Release();
            }
        }
        pDisp->Release();
    }

    return ret;
}

void OnSaveImageAs(HWND hwnd)
{
    if (BSTR bstrURL = GetActiveImgSrc(hwnd))
    {
        DoSaveURL(hwnd, bstrURL);
        SysFreeString(bstrURL);
    }
}

void OnSaveTargetAs(HWND hwnd)
{
    if (BSTR bstrURL = GetActiveHREF(hwnd))
    {
        DoSaveURL(hwnd, bstrURL);
        SysFreeString(bstrURL);
    }
    else
    {
        OnSave(hwnd);
    }
}

void OnCustomLink(HWND hwnd, UINT nIndex)
{
    if (nIndex < s_menu_links.size())
    {
        DoNavigate(hwnd, s_menu_links[nIndex].c_str());
    }
}

void OnParseCmdLien(HWND hwnd)
{
    int argc = 0;
    if (LPWSTR *wargv = CommandLineToArgvW(GetCommandLineW(), &argc))
    {
        std::wstring url;

        for (int i = 1; i < argc; ++i)
        {
            if (lstrcmpiW(wargv[i], L"-kiosk") == 0 ||
                lstrcmpiW(wargv[i], L"--kiosk") == 0 ||
                lstrcmpiW(wargv[i], L"/kiosk") == 0)
            {
                OnKioskOn(hwnd);
            }
            else
            {
                url = wargv[i];
            }
        }

        if (url.size())
        {
            DoNavigate(hwnd, url.c_str());
        }
        else
        {
            DoNavigate(hwnd, g_settings.m_homepage.c_str());
        }

        LocalFree(wargv);
    }
}

void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    static INT s_nLevel = 0;

    KillTimer(hwnd, REFRESH_TIMER);

    if (s_nLevel == 0)
    {
        SendMessage(s_hStatusBar, SB_SETTEXT, 0, (LPARAM)LoadStringDx(IDS_EXECUTING_CMD));
    }
    s_nLevel++;

    if (id < MIN_COMMAND_ID)
    {
        HWND hwndIE = s_pWebBrowser->GetIEServerWindow();
        PostMessage(hwndIE, WM_COMMAND, id, 0);
    }
    else
    {
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
        case ID_EXIT:
            OnExit(hwnd);
            break;
        case ID_NEW:
            OnNew(hwnd, NULL);
            break;
        case ID_KIOSK:
            OnKiosk(hwnd);
            break;
        case ID_KIOSK_OFF:
            OnKioskOff(hwnd);
            break;
        case ID_KIOSK_ON:
            OnKioskOn(hwnd);
            break;
        case ID_GO_URL:
            OnGoURL(hwnd, hwndCtl);
            break;
        case ID_EXECUTE_CMD:
            OnExecuteCmd(hwnd, hwndCtl);
            break;
        case ID_CANCEL_PRINTING:
            OnCancelPrinting(hwnd);
            break;
        case ID_UP:
            OnUp(hwnd);
            break;
        case ID_DOWN:
            OnDown(hwnd);
            break;
        case ID_ZOOM_UP:
            OnZoomUp(hwnd);
            break;
        case ID_ZOOM_DOWN:
            OnZoomDown(hwnd);
            break;
        case ID_ZOOM_100:
            OnZoom100(hwnd);
            break;
        case ID_SAVE_IMAGE_AS:
            OnSaveImageAs(hwnd);
            break;
        case ID_SAVE_TARGET_AS:
            OnSaveTargetAs(hwnd);
            break;
        case ID_CUSTOM_LINK_01:
        case ID_CUSTOM_LINK_02:
        case ID_CUSTOM_LINK_03:
        case ID_CUSTOM_LINK_04:
        case ID_CUSTOM_LINK_05:
        case ID_CUSTOM_LINK_06:
        case ID_CUSTOM_LINK_07:
        case ID_CUSTOM_LINK_08:
        case ID_CUSTOM_LINK_09:
        case ID_CUSTOM_LINK_10:
        case ID_CUSTOM_LINK_11:
        case ID_CUSTOM_LINK_12:
        case ID_CUSTOM_LINK_13:
        case ID_CUSTOM_LINK_14:
        case ID_CUSTOM_LINK_15:
        case ID_CUSTOM_LINK_16:
            OnCustomLink(hwnd, id - ID_CUSTOM_LINK_01);
            break;
        case ID_PARSE_CMDLINE:
            OnParseCmdLien(hwnd);
            break;
        }
    }

    --s_nLevel;
    if (s_nLevel == 0)
    {
        SendMessage(s_hStatusBar, SB_SETTEXT, 0, (LPARAM)LoadStringDx(IDS_READY));
    }

    if (g_settings.m_refresh_interval)
    {
        SetTimer(hwnd, REFRESH_TIMER, g_settings.m_refresh_interval, NULL);
    }
}

void OnDestroy(HWND hwnd)
{
    KillTimer(hwnd, REFRESH_TIMER);

    if (!g_settings.m_kiosk_mode)
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
    if (s_pEventSink)
    {
        s_pEventSink->Disconnect();
        s_pEventSink->Release();
        s_pEventSink = NULL;
    }
    if (s_pWebBrowser)
    {
        s_pWebBrowser->Destroy();
    }
    PostQuitMessage(0);
}

void OnResetKiosk(HWND hwnd)
{
    DoNavigate(hwnd, g_settings.m_homepage.c_str(), navNoHistory);
    SendMessage(hwnd, WM_COMMAND, ID_ZOOM_100, 0);

    s_bEnableForward = s_bEnableBack = FALSE;
    ::EnableWindow(::GetDlgItem(s_hMainWnd, ID_BACK), s_bEnableBack);
    ::EnableWindow(::GetDlgItem(s_hMainWnd, ID_NEXT), s_bEnableForward);
    SetForegroundWindow(hwnd);
}

void OnTimer(HWND hwnd, UINT id)
{
    switch (id)
    {
    case SOURCE_DONE_TIMER:
        KillTimer(hwnd, id);
        PostMessage(hwnd, WM_COMMAND, ID_VIEW_SOURCE_DONE, 0);
        break;
    case REFRESH_TIMER:
        if (g_settings.m_kiosk_mode)
        {
            OnResetKiosk(hwnd);
        }
        break;
    }
}

void OnInitMenuPopup(HWND hwnd, HMENU hMenu, UINT item, BOOL fSystemMenu)
{
    if (g_settings.m_kiosk_mode)
    {
        CheckMenuItem(hMenu, ID_KIOSK, MF_CHECKED | MF_BYCOMMAND);
    }
    else
    {
        CheckMenuItem(hMenu, ID_KIOSK, MF_UNCHECKED | MF_BYCOMMAND);
    }
}

void OnDrawItem(HWND hwnd, const DRAWITEMSTRUCT * lpDrawItem)
{
    if (lpDrawItem->CtlType != ODT_BUTTON)
        return;

    HWND hwndItem = lpDrawItem->hwndItem;
    HDC hDC = lpDrawItem->hDC;
    RECT rcItem = lpDrawItem->rcItem;

    WCHAR szText[64];
    GetWindowTextW(hwndItem, szText, ARRAYSIZE(szText));

    DWORD style = GetWindowStyle(hwndItem);

    if (!IsWindowEnabled(hwndItem))
    {
        DrawFrameControl(hDC, &rcItem, DFC_BUTTON, DFCS_BUTTONPUSH | DFCS_MONO | DFCS_ADJUSTRECT);
    }
    else if (GetCheck(hwndItem))
    {
        DrawFrameControl(hDC, &rcItem, DFC_BUTTON, DFCS_BUTTONPUSH | DFCS_PUSHED | DFCS_ADJUSTRECT);
    }
    else if (lpDrawItem->itemState & ODS_SELECTED)
    {
        DrawFrameControl(hDC, &rcItem, DFC_BUTTON, DFCS_BUTTONPUSH | DFCS_PUSHED | DFCS_ADJUSTRECT);
    }
    else
    {
        DrawFrameControl(hDC, &rcItem, DFC_BUTTON, DFCS_BUTTONPUSH | DFCS_ADJUSTRECT);
    }

    COLORREF bgColor = RGB(255, 255, 255);
    {
        auto it = s_hwnd2bgcolor.find(hwndItem);
        if (it != s_hwnd2bgcolor.end())
            bgColor = it->second;
    }
    HBRUSH hbr = CreateSolidBrush(bgColor);
    FillRect(hDC, &rcItem, hbr);
    DeleteObject(hbr);

    if (IsWindowEnabled(hwndItem))
    {
        RECT rc = rcItem;
        InflateRect(&rc, -2, -2);
        SelectObject(hDC, GetStockObject(NULL_BRUSH));
        SelectObject(hDC, GetStockObject(BLACK_PEN));
        Rectangle(hDC, rc.left, rc.top, rc.right, rc.bottom);
    }

    COLORREF color = RGB(0, 0, 0);
    {
        auto it = s_hwnd2color.find(hwndItem);
        if (it != s_hwnd2color.end())
            color = it->second;
    }
    SetTextColor(hDC, color);

    HFONT hFont = GetWindowFont(hwndItem);

    LOGFONTW lf;
    GetObject(hFont, sizeof(lf), &lf);
    lf.lfHeight = -(rcItem.bottom - rcItem.top) * 9 / 10;
    hFont = CreateFontIndirectW(&lf);

    UINT uFormat = DT_SINGLELINE | DT_CENTER | DT_VCENTER;
    for (INT k = 0; k < 16; ++k)
    {
        RECT rc = rcItem;
        HGDIOBJ hFontOld = SelectObject(hDC, hFont);
        {
            DrawText(hDC, szText, -1, &rc, uFormat | DT_CALCRECT);
        }
        SelectObject(hDC, hFontOld);

        SIZE siz;
        siz.cx = rc.right - rc.left;
        siz.cy = rc.bottom - rc.top;
        if (siz.cx < (rcItem.right - rcItem.left) * 9 / 10 &&
            siz.cy < rcItem.bottom - rcItem.top)
        {
            break;
        }

        DeleteObject(hFont);
        lf.lfHeight = -lf.lfHeight * 9 / 10;
        hFont = CreateFontIndirectW(&lf);
    }

    if (GetCheck(hwndItem) || (lpDrawItem->itemState & ODS_SELECTED))
    {
        OffsetRect(&rcItem, 1, 1);
    }

    SetBkMode(hDC, TRANSPARENT);
    HGDIOBJ hFontOld = SelectObject(hDC, hFont);
    if (!IsWindowEnabled(hwndItem))
    {
        SetTextColor(hDC, bgColor);
    }
    DrawTextW(hDC, szText, -1, &rcItem, uFormat);
    SelectObject(hDC, hFontOld);
    DeleteObject(hFont);

    //printf("%p: %08X, %08X\n", hwndItem, color, bgColor);
}

void OnClose(HWND hwnd)
{
    BOOL bFound = FALSE;
    for (auto& pair : s_downloadings)
    {
        if (pair.first && pair.second)
        {
            bFound = TRUE;
            break;
        }
    }
    if (bFound)
    {
        INT id = MessageBoxW(hwnd, LoadStringDx(IDS_DL_QUIT_QUESTION),
                             s_szName, MB_ICONINFORMATION | MB_YESNOCANCEL);
        switch (id)
        {
        case IDYES:
            break;
        default:
            return;
        }
    }
    auto downloadings = s_downloadings;
    s_downloadings.clear();
    for (auto& pair : downloadings)
    {
        if (pair.first && pair.second)
        {
            PostMessage(pair.first, WM_COMMAND, IDCANCEL, 0);
            pair.second = FALSE;
        }
    }
    Sleep(250);
    DestroyWindow(hwnd);
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
    HANDLE_MSG(hwnd, WM_INITMENUPOPUP, OnInitMenuPopup);
    HANDLE_MSG(hwnd, WM_DRAWITEM, OnDrawItem);
    HANDLE_MSG(hwnd, WM_CLOSE, OnClose);
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
                if (g_settings.m_dont_r_click || g_settings.m_kiosk_mode)
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
                    case 'N':   // Ctrl+N
                    case 'K':   // Ctrl+K
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
                INT cch = GetWindowTextLengthW(s_hAddrBarEdit);

                std::wstring str;
                str.resize(cch);

                GetWindowTextW(s_hAddrBarEdit, &str[0], cch + 1);
                StrTrimW(&str[0], L" \t\n\r\f\v");

                std::wstring url = str.c_str();
                if (url.find(L"view-source:") == 0)
                {
                    url.erase(0, wcslen(L"view-source:"));
                    DoNavigate(s_hMainWnd, url.c_str());
                    return TRUE;
                }
            }
        }
        break;
    }

    return FALSE;
}

// IDownloadManager::Download
STDMETHODIMP MWebBrowserEx::Download(
    IMoniker *pmk,
    IBindCtx *pbc,
    DWORD dwBindVerb,
    LONG grfBINDF,
    BINDINFO *pBindInfo,
    LPCOLESTR pszHeaders,
    LPCOLESTR pszRedir,
    UINT uiCP)
{
    LPOLESTR pszURL = NULL;
    pmk->GetDisplayName(pbc, NULL, &pszURL);
    printf("IDownloadManager::Download: %ls\n", pszURL);
    if (pszURL)
    {
        DoSaveURL(s_hMainWnd, pszURL);
        CoTaskMemFree(pszURL);
        return S_OK;
    }
    return E_FAIL;
}

STDMETHODIMP MWebBrowserEx::ShowContextMenu(
    DWORD dwID,
    POINT *ppt,
    IUnknown *pcmdtReserved,
    IDispatch *pdispReserved)
{
    if (g_settings.m_kiosk_mode)
        return S_OK;

    std::wstring data;
    HMENU hMenu = NULL;
    switch (dwID)
    {
    case CONTEXT_MENU_DEFAULT:
        if (DoLoadMenu(s_hMainWnd, IDS_DEFAULTMENU, data))
        {
            hMenu = DoCreateMenu(s_hMainWnd, data);
        }
        break;
    case CONTEXT_MENU_IMAGE:
        if (DoLoadMenu(s_hMainWnd, IDS_IMAGEMENU, data))
        {
            hMenu = DoCreateMenu(s_hMainWnd, data);
        }
        break;
    case CONTEXT_MENU_CONTROL:
        return S_FALSE;
    case CONTEXT_MENU_TABLE:
        return S_FALSE;
    case CONTEXT_MENU_TEXTSELECT:
        if (DoLoadMenu(s_hMainWnd, IDS_TEXTMENU, data))
        {
            hMenu = DoCreateMenu(s_hMainWnd, data);
        }
        break;
    case CONTEXT_MENU_ANCHOR:
        if (DoLoadMenu(s_hMainWnd, IDS_ANCHORMENU, data))
        {
            hMenu = DoCreateMenu(s_hMainWnd, data);
        }
        break;
    case CONTEXT_MENU_UNKNOWN:
        return S_FALSE;
    case CONTEXT_MENU_VSCROLL:
    case CONTEXT_MENU_HSCROLL:
    default:
        return S_OK;
    }

    if (hMenu)
    {
        DoPopupMenu(s_hMainWnd, hMenu, ppt);
        DestroyMenu(hMenu);
        return S_OK;
    }

    return S_FALSE;
}

INT WINAPI
WinMain(HINSTANCE   hInstance,
        HINSTANCE   hPrevInstance,
        LPSTR       lpCmdLine,
        INT         nCmdShow)
{
    if (strstr(lpCmdLine, "kiosk") != NULL)
    {
        INT i = 0;
        while (HWND hwnd = FindWindow(s_szName, NULL))
        {
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            Sleep(100);
            if (++i > 100)
                break;
        }
    }

    OleInitialize(NULL);

    InitCommonControls();

    s_hInst = hInstance;

    WNDCLASSEX wcx;
    ZeroMemory(&wcx, sizeof(wcx));
    wcx.cbSize = sizeof(wcx);
    wcx.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcx.lpfnWndProc = WindowProc;
    wcx.hInstance = hInstance;
    wcx.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(1));
    wcx.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcx.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
    wcx.lpszClassName = s_szName;
    wcx.hIconSm = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(1), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    if (!RegisterClassEx(&wcx))
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
        BOOL bFound = FALSE;
        for (auto& pair : s_downloadings)
        {
            if (IsWindow(pair.first) && IsDialogMessage(pair.first, &msg))
            {
                bFound = TRUE;
                break;
            }
        }
        if (bFound)
            continue;

        if ((WM_KEYFIRST <= msg.message && msg.message <= WM_KEYLAST) ||
            (WM_MOUSEFIRST <= msg.message && msg.message <= WM_MOUSELAST))
        {
            KillTimer(s_hMainWnd, REFRESH_TIMER);
            if (g_settings.m_refresh_interval)
            {
                SetTimer(s_hMainWnd, REFRESH_TIMER, g_settings.m_refresh_interval, NULL);
            }
        }

        if (PreProcessBrowserKeys(&msg))
            continue;

        if (s_hAccel && TranslateAccelerator(hwnd, s_hAccel, &msg))
            continue;

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (s_pWebBrowser)
    {
        s_pWebBrowser->Release();
        s_pWebBrowser = NULL;
    }

    OleUninitialize();

#if (WINVER >= 0x0500)
    HANDLE hProcess = GetCurrentProcess();
    TCHAR szText[128];
    StringCbPrintf(szText, sizeof(szText), TEXT("Count of GDI objects: %ld\n"),
                   GetGuiResources(hProcess, GR_GDIOBJECTS));
    OutputDebugString(szText);
    StringCbPrintf(szText, sizeof(szText), TEXT("Count of USER objects: %ld\n"),
                   GetGuiResources(hProcess, GR_USEROBJECTS));
    OutputDebugString(szText);
#endif

#if defined(_MSC_VER) && !defined(NDEBUG)
    // for detecting memory leak (MSVC only)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    return 0;
}
