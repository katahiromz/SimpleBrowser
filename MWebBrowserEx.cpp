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
