#include <windows.h>
#include <urlmon.h>

int main(void)
{
    URLDownloadToFileW(NULL, L"https://dforest.watch.impress.co.jp/library/g/gimp/10739/gimp-2.10.10-setup.exe", L"gimp-2.10.10-setup.exe", 0, NULL);
    return 0;
}
