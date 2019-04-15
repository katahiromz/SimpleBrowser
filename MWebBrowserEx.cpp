// MWebBrowserEx.cpp --- simple Win32 Web Browser Extended
// Copyright (C) 2019 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
// This file is public domain software.

#include "MWebBrowserEx.hpp"
#include "MBindStatusCallback.hpp"
#include <cwchar>
#include <comdef.h>
#include <cstring>
#include <mshtmhst.h>

/*static*/ MWebBrowserEx *
MWebBrowserEx::Create(HWND hwndParent)
{
    MWebBrowserEx *pBrowser = new MWebBrowserEx(hwndParent);
    if (!pBrowser->IsCreated())
    {
        pBrowser->Release();
        pBrowser = NULL;
    }
    return pBrowser;
}

MWebBrowserEx::MWebBrowserEx(HWND hwndParent) : MWebBrowser(hwndParent)
{
}

MWebBrowserEx::~MWebBrowserEx()
{
}

// IUnknown interface
STDMETHODIMP MWebBrowserEx::QueryInterface(REFIID riid, void **ppvObj)
{
    if (riid == __uuidof(IDownloadManager))
    {
        *ppvObj = static_cast<IDownloadManager *>(this);
    }
    else
    {
        return MWebBrowser::QueryInterface(riid, ppvObj);
    }

    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) MWebBrowserEx::AddRef()
{
    return MWebBrowser::AddRef();
}

STDMETHODIMP_(ULONG) MWebBrowserEx::Release()
{
    return MWebBrowser::Release();
}

// IDownloadManager interface
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
    MessageBoxW(NULL, L"OK", NULL, 0);
    return S_OK;
}
