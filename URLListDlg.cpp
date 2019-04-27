// URLListDlg.cpp --- URL list dialog
// Copyright (C) 2019 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
// This file is public domain software.

#include "URLListDlg.hpp"
#include "Settings.hpp"
#include <windowsx.h>
#include <shlwapi.h>
#include "resource.h"

static BOOL OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    HWND hLst1 = GetDlgItem(hwnd, lst1);
    SETTINGS::list_type::const_iterator it, end = g_settings.m_url_list.end();
    for (it = g_settings.m_url_list.begin(); it != end; ++it)
    {
        ListBox_AddString(hLst1, it->c_str());
    }
    return TRUE;
}

static void OnOK(HWND hwnd)
{
    HWND hLst1 = GetDlgItem(hwnd, lst1);
    g_settings.m_url_list.clear();

    TCHAR szText[256];
    INT i, nCount = ListBox_GetCount(hLst1);
    for (i = 0; i < nCount; ++i)
    {
        ListBox_GetText(hLst1, i, szText);
        g_settings.m_url_list.push_back(szText);
    }

    EndDialog(hwnd, IDOK);
}

static void OnPsh1(HWND hwnd)
{
    HWND hLst1 = GetDlgItem(hwnd, lst1);

    TCHAR szText[256];
    GetDlgItemText(hwnd, edt1, szText, ARRAYSIZE(szText));

    StrTrimW(szText, L" \t\n\r\f\v");

    if (szText[0] == 0)
        return;

    INT iItem = ListBox_AddString(hLst1, szText);
    ListBox_SetCurSel(hLst1, iItem);
}

static void OnPsh2(HWND hwnd)
{
    HWND hLst1 = GetDlgItem(hwnd, lst1);

    INT iItem = ListBox_GetCurSel(hLst1);
    if (iItem == LB_ERR || iItem == 0)
        return;

    TCHAR szText1[256], szText2[256];
    ListBox_GetText(hLst1, iItem - 1, szText1);
    ListBox_GetText(hLst1, iItem, szText2);

    ListBox_DeleteString(hLst1, iItem - 1);
    ListBox_DeleteString(hLst1, iItem - 1);

    ListBox_InsertString(hLst1, iItem - 1, szText2);
    ListBox_InsertString(hLst1, iItem, szText1);

    ListBox_SetCurSel(hLst1, iItem - 1);
}

static void OnPsh3(HWND hwnd)
{
    HWND hLst1 = GetDlgItem(hwnd, lst1);

    INT iItem = ListBox_GetCurSel(hLst1);
    INT nCount = ListBox_GetCount(hLst1);
    if (iItem == LB_ERR || iItem == nCount - 1)
        return;

    TCHAR szText1[256], szText2[256];
    ListBox_GetText(hLst1, iItem, szText1);
    ListBox_GetText(hLst1, iItem + 1, szText2);

    ListBox_DeleteString(hLst1, iItem);
    ListBox_DeleteString(hLst1, iItem);

    ListBox_InsertString(hLst1, iItem, szText2);
    ListBox_InsertString(hLst1, iItem + 1, szText1);

    ListBox_SetCurSel(hLst1, iItem + 1);
}

static void OnPsh4(HWND hwnd)
{
    HWND hLst1 = GetDlgItem(hwnd, lst1);

    INT iItem = ListBox_GetCurSel(hLst1);
    if (iItem != LB_ERR)
        ListBox_DeleteString(hLst1, iItem);
}

static void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    HWND hLst1 = GetDlgItem(hwnd, lst1);
    switch (id)
    {
    case IDOK:
        OnOK(hwnd);
        break;
    case IDCANCEL:
        EndDialog(hwnd, id);
        break;
    case psh1:
        OnPsh1(hwnd);
        break;
    case psh2:
        OnPsh2(hwnd);
        break;
    case psh3:
        OnPsh3(hwnd);
        break;
    case psh4:
        OnPsh4(hwnd);
        break;
    case psh5:
        ListBox_ResetContent(hLst1);
        break;
    }
}

static INT_PTR CALLBACK
URLListDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_INITDIALOG, OnInitDialog);
        HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);
    }
    return 0;
}

void ShowURLListDlg(HINSTANCE hInst, HWND hwnd)
{
    DialogBox(hInst, MAKEINTRESOURCE(IDD_URLLIST), hwnd, URLListDlgProc);
}
