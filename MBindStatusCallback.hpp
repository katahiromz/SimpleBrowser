// MBindStatusCallback.hpp --- progress info class
// Copyright (C) 2019 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
// This file is public domain software.

#ifndef MBIND_STATUS_CALLBACK_HPP_
#define MBIND_STATUS_CALLBACK_HPP_

#ifndef _INC_WINDOWS
    #include <windows.h>
#endif
#include <urlmon.h>
#include <string>

class MBindStatusCallback :
    public IBindStatusCallback,
    public IAuthenticate
{
public:
    static MBindStatusCallback *Create();
    DWORD m_dwTick;
    ULONG m_ulProgress;
    ULONG m_ulProgressMax;
    ULONG m_ulStatusCode;
    std::wstring m_strStatus;

    void SetCancelled();
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

    // IAuthenticate interface
    STDMETHODIMP Authenticate(HWND *phwnd, LPWSTR *pszUsername, LPWSTR *pszPassword);

protected:
    LONG m_nRefCount;
    BOOL m_bCompleted;
    BOOL m_bCancelled;

    MBindStatusCallback();
    ~MBindStatusCallback();
};

#endif  // ndef MBIND_STATUS_CALLBACK_HPP_
