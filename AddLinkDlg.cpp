// AddLinkDlg.cpp --- add link dialog
// Copyright (C) 2019 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
// This file is public domain software.

#include "AddLinkDlg.hpp"
#include <windowsx.h>
#include <shlwapi.h>
#include "resource.h"

#ifndef ARRAYSIZE
    #define ARRAYSIZE(array) (sizeof(array) / sizeof(array[0]))
#endif

static std::wstring s_text;

static BOOL OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    SetDlgItemText(hwnd, edt1, s_text.c_str());
    return TRUE;
}

static void OnOK(HWND hwnd)
{
    TCHAR szText[256];
    GetDlgItemText(hwnd, edt1, szText, ARRAYSIZE(szText));
    StrTrimW(szText, L" \t\n\r\f\v");

    if (szText[0] == 0)
    {
        LPTSTR LoadStringDx(INT nID);
        MessageBoxW(hwnd, LoadStringDx(IDS_ENTER_TEXT), NULL, MB_ICONERROR);
    }

    s_text = szText;
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
AddLinkDlgDialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_INITDIALOG, OnInitDialog);
        HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);
    }
    return 0;
}

BOOL ShowAddLinkDlg(HINSTANCE hInst, HWND hwnd, std::wstring& text)
{
    s_text = text;

    if (DialogBox(hInst, MAKEINTRESOURCE(IDD_ADDLINK), hwnd,
                  AddLinkDlgDialogProc) == IDOK)
    {
        text = s_text;
        return TRUE;
    }
    return FALSE;
}
