// MWebBrowserEx.cpp --- Win32 Web Browser Extended
// Copyright (C) 2019 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
// This file is public domain software.

#ifndef MWEB_BROWSER_EX_HPP_
#define MWEB_BROWSER_EX_HPP_   1   // Version 1

#include "MWebBrowser.hpp"
#include <downloadmgr.h>

class MWebBrowserEx :
    public MWebBrowser,
    public IDownloadManager
{
public:
    static MWebBrowserEx *Create(HWND hwndParent);

    // IUnknown interface
    STDMETHODIMP QueryInterface(REFIID riid, void **ppvObj);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // IDocHostUIHandler interface
    STDMETHODIMP ShowContextMenu(
        DWORD dwID,
        POINT *ppt,
        IUnknown *pcmdtReserved,
        IDispatch *pdispReserved);

    // IDownloadManager interface
    STDMETHODIMP Download(
        IMoniker *pmk,
        IBindCtx *pbc,
        DWORD dwBindVerb,
        LONG grfBINDF,
        BINDINFO *pBindInfo,
        LPCOLESTR pszHeaders,
        LPCOLESTR pszRedir,
        UINT uiCP);

protected:
    MWebBrowserEx(HWND hwndParent);
    virtual ~MWebBrowserEx();

private:
    MWebBrowserEx(const MWebBrowserEx&);
    MWebBrowserEx& operator=(const MWebBrowserEx&);
};

#endif  // ndef MWEB_BROWSER_EX_HPP_
