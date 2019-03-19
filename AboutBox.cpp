// AboutBox.cpp --- version info dialog
// Copyright (C) 2019 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
// This file is public domain software.

#include "AboutBox.hpp"
#include <windowsx.h>
#include "resource.h"

static BOOL OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    return TRUE;
}

static void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    switch (id)
    {
    case IDOK:
    case IDCANCEL:
        EndDialog(hwnd, id);
        break;
    }
}

static INT_PTR CALLBACK
AboutBoxDialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_INITDIALOG, OnInitDialog);
        HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);
    }
    return 0;
}

void ShowAboutBox(HINSTANCE hInst, HWND hwnd)
{
    DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hwnd, AboutBoxDialogProc);
}
