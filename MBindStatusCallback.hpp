#ifndef MBIND_STATUS_CALLBACK_HPP_
#define MBIND_STATUS_CALLBACK_HPP_

#ifndef _INC_WINDOWS
    #include <windows.h>
#endif
#include <urlmon.h>

class MBindStatusCallback : public IBindStatusCallback
{
public:
    static MBindStatusCallback *Create();

    BOOL IsCancelled() const;
    BOOL IsCompleted() const;

    // IUnknown interface
    STDMETHODIMP QueryInterface(REFIID riid, void **ppvObj);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // IBindStatusCallback interface
    STDMETHODIMP OnStartBinding(DWORD dwReserved, IBinding *pib);
    STDMETHODIMP GetPriority(LONG *pnPriority);
    STDMETHODIMP OnLowResource(DWORD reserved);
    STDMETHODIMP OnProgress(
        ULONG ulProgress,
        ULONG ulProgressMax,
        ULONG ulStatusCode,
        LPCWSTR szStatusText);
    STDMETHODIMP OnStopBinding(HRESULT hresult, LPCWSTR szError);
    STDMETHODIMP GetBindInfo(DWORD *grfBINDF, BINDINFO *pbindinfo);
    STDMETHODIMP OnDataAvailable(
        DWORD grfBSCF,
        DWORD dwSize,
        FORMATETC *pformatetc,
        STGMEDIUM *pstgmed);
    STDMETHODIMP OnObjectAvailable(REFIID riid, IUnknown *punk);

protected:
    LONG m_nRefCount;
    BOOL m_bCompleted;
    BOOL m_bCancelled;

    MBindStatusCallback();
    ~MBindStatusCallback();
};

#endif  // ndef MBIND_STATUS_CALLBACK_HPP_
