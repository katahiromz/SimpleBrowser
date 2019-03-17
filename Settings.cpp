#include "Settings.hpp"
#include <windowsx.h>
#include "resource.h"

bool SETTINGS::load()
{
    return false;
}

bool SETTINGS::save()
{
    return false;
}

void SETTINGS::reset()
{
}

static BOOL OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    return TRUE;
}

static void OnOK(HWND hwnd)
{
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
SettingsDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_INITDIALOG, OnInitDialog);
        HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);
    }
    return 0;
}

void ShowSettingsDlg(HINSTANCE hInst, HWND hwnd)
{
    DialogBox(hInst, MAKEINTRESOURCE(IDD_SETTINGS), hwnd, SettingsDlgProc);
}
