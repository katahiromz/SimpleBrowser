#ifndef SETTINGS_HPP_
#define SETTINGS_HPP_

#ifndef _INC_WINDOWS
    #include <windows.h>
#endif

struct SETTINGS
{
    bool load();
    bool save();
    void reset();
};

void ShowSettingsDlg(HINSTANCE hInst, HWND hwnd);

#endif  // ndef SETTINGS_HPP_
