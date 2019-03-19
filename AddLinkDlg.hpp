// AddLinkDlg.hpp --- add link dialog
// Copyright (C) 2019 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
// This file is public domain software.

#ifndef ADDLINKDLG_HPP_
#define ADDLINKDLG_HPP_
#ifndef _INC_WINDOWS
    #include <windows.h>
#endif
#include <string>

BOOL ShowAddLinkDlg(HINSTANCE hInst, HWND hwnd, std::wstring& text);

#endif  // ndef ADDLINKDLG_HPP_
