// MWebBrowserEx.cpp --- Win32 Web Browser Extended
// Copyright (C) 2019 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
// This file is public domain software.

#ifndef MWEB_BROWSER_EX_HPP_
#define MWEB_BROWSER_EX_HPP_   1   // Version 1

#include "MWebBrowser.hpp"

class MWebBrowserEx : public MWebBrowser
{
public:
    static MWebBrowserEx *Create(HWND hwndParent);

    // IDocHostUIHandler interface
    STDMETHODIMP ShowContextMenu(
        DWORD dwID,
        POINT *ppt,
        IUnknown *pcmdtReserved,
        IDispatch *pdispReserved);

protected:
    MWebBrowserEx(HWND hwndParent);
    virtual ~MWebBrowserEx();

private:
    MWebBrowserEx(const MWebBrowserEx&);
    MWebBrowserEx& operator=(const MWebBrowserEx&);
};

#endif  // ndef MWEB_BROWSER_EX_HPP_
