// MWebBrowser.cpp --- simple Win32 Web Browser
// Copyright (C) 2019 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
// This file is public domain software.

#ifndef MWEB_BROWSER_HPP_
#define MWEB_BROWSER_HPP_   11   // Version 11

#include <windows.h>
#include <exdisp.h>
#ifndef NO_DOWNLOADMGR
    #include <downloadmgr.h>
#endif

class MWebBrowser :
    public IOleClientSite,
    public IOleInPlaceSite,
    public IStorage,
    public IServiceProvider,
    public IHttpSecurity
#ifndef NO_DOWNLOADMGR
    , public IDownloadManager
#endif
{
public:
    static MWebBrowser *Create(HWND hwndParent);

    RECT PixelToHIMETRIC(const RECT& rc);
    HWND GetControlWindow();
    HWND GetIEServerWindow();
    void MoveWindow(const RECT& rc);

    void GoHome();
    void GoBack();
    void GoForward();
    void Stop();
    void StopDownload();
    void Refresh();
    void Navigate(const WCHAR *url = L"about:blank");
    void Zoom(LONG iZoomFactor = 2);
    void Print(BOOL bBang = FALSE);
    void PrintPreview();
    void PageSetup();
    HRESULT Save(LPCWSTR file);
    void Destroy();
    BOOL TranslateAccelerator(LPMSG pMsg);
    IWebBrowser2 *GetIWebBrowser2();
    void AllowInsecure(BOOL bAllow);

    HRESULT get_Application(IDispatch **ppApplication) const;
    HRESULT get_LocationURL(BSTR *bstrURL) const;
    HRESULT get_mimeType(BSTR *bstrMIME) const;
    HRESULT put_Silent(VARIANT_BOOL bSilent);
    BOOL is_busy() const;

    // IUnknown interface
    STDMETHODIMP QueryInterface(REFIID riid, void **ppvObj);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // IOleWindow interface
    STDMETHODIMP GetWindow(HWND *phwnd);
    STDMETHODIMP ContextSensitiveHelp(BOOL fEnterMode);

    // IOleInPlaceSite interface
    STDMETHODIMP CanInPlaceActivate();
    STDMETHODIMP OnInPlaceActivate();
    STDMETHODIMP OnUIActivate();
    STDMETHODIMP GetWindowContext(
        IOleInPlaceFrame **ppFrame,
        IOleInPlaceUIWindow **ppDoc,
        LPRECT lprcPosRect,
        LPRECT lprcClipRect,
        LPOLEINPLACEFRAMEINFO lpFrameInfo);
    STDMETHODIMP Scroll(SIZE scrollExtant);
    STDMETHODIMP OnUIDeactivate(BOOL fUndoable);
    STDMETHODIMP OnInPlaceDeactivate();
    STDMETHODIMP DiscardUndoState();
    STDMETHODIMP DeactivateAndUndo();
    STDMETHODIMP OnPosRectChange(LPCRECT lprcPosRect);

    // IOleClientSite interface
    STDMETHODIMP SaveObject();
    STDMETHODIMP GetMoniker(
        DWORD dwAssign,
        DWORD dwWhichMoniker,
        IMoniker **ppmk);
    STDMETHODIMP GetContainer(IOleContainer **ppContainer);
    STDMETHODIMP ShowObject();
    STDMETHODIMP OnShowWindow(BOOL fShow);
    STDMETHODIMP RequestNewObjectLayout();

    // IStorage interface
    STDMETHODIMP CreateStream(
        const OLECHAR *pwcsName,
        DWORD grfMode,
        DWORD reserved1,
        DWORD reserved2,
        IStream **ppstm);
    STDMETHODIMP OpenStream(
        const OLECHAR *pwcsName,
        void *reserved1,
        DWORD grfMode,
        DWORD reserved2,
        IStream **ppstm);
    STDMETHODIMP CreateStorage(
        const OLECHAR *pwcsName,
        DWORD grfMode,
        DWORD reserved1,
        DWORD reserved2,
        IStorage **ppstg);
    STDMETHODIMP OpenStorage(
        const OLECHAR *pwcsName,
        IStorage *pstgPriority,
        DWORD grfMode,
        SNB snbExclude,
        DWORD reserved,
        IStorage **ppstg);
    STDMETHODIMP CopyTo(
        DWORD ciidExclude,
        const IID *rgiidExclude,
        SNB snbExclude,
        IStorage *pstgDest);
    STDMETHODIMP MoveElementTo(
        const OLECHAR *pwcsName,
        IStorage *pstgDest,
        const OLECHAR *pwcsNewName,
        DWORD grfFlags);
    STDMETHODIMP Commit(DWORD grfCommitFlags);
    STDMETHODIMP Revert();
    STDMETHODIMP EnumElements(
        DWORD reserved1,
        void *reserved2,
        DWORD reserved3,
        IEnumSTATSTG **ppenum);
    STDMETHODIMP DestroyElement(const OLECHAR *pwcsName);
    STDMETHODIMP RenameElement(
        const OLECHAR *pwcsOldName,
        const OLECHAR *pwcsNewName);
    STDMETHODIMP SetElementTimes(
        const OLECHAR *pwcsName,
        const FILETIME *pctime,
        const FILETIME *patime,
        const FILETIME *pmtime);
    STDMETHODIMP SetClass(REFCLSID clsid);
    STDMETHODIMP SetStateBits(DWORD grfStateBits, DWORD grfMask);
    STDMETHODIMP Stat(STATSTG *pstatstg, DWORD grfStatFlag);

    // IServiceProvider interface
    STDMETHODIMP QueryService(
        REFGUID guidService,
        REFIID riid,
        void **ppvObject);

    // IWindowForBindingUI interface
    STDMETHODIMP GetWindow(REFGUID rguidReason, HWND *phwnd);

    // IHttpSecurity interface
    STDMETHODIMP OnSecurityProblem(DWORD dwProblem);

#ifndef NO_DOWNLOADMGR
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
#endif

protected:
    LONG m_nRefCount;
    HWND m_hwndParent;
    HWND m_hwndCtrl;
    HWND m_hwndIEServer;
    IWebBrowser2 *m_web_browser2;
    IOleObject *m_ole_object;
    IOleInPlaceObject *m_ole_inplace_object;
    RECT m_rc;
    HRESULT m_hr;
    BOOL m_bAllowInsecure;

    MWebBrowser(HWND hwndParent);
    virtual ~MWebBrowser();

    HRESULT CreateBrowser(HWND hwndParent);
    BOOL IsCreated() const;

private:
    MWebBrowser(const MWebBrowser&);
    MWebBrowser& operator=(const MWebBrowser&);
};

#endif  // ndef MWEB_BROWSER_HPP_
