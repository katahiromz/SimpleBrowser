// MEventSink.hpp --- MZC4 event sink
// Copyright (C) 2019 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
// This file is public domain software.

#ifndef MZC4_MEVENTSINK_HPP_
#define MZC4_MEVENTSINK_HPP_    3   // Version 3

#ifndef _INC_WINDOWS
    #include <windows.h>
#endif
#include <ocidl.h>
#include <exdispid.h>

struct MEventSinkListener;
class MEventSink;

////////////////////////////////////////////////////////////////////////////

struct MEventSinkListener
{
    virtual void BeforeNavigate2(
        IDispatch *pDisp,
        VARIANT *url,
        VARIANT *flags,
        VARIANT *target,
        VARIANT *PostData,
        VARIANT *headers,
        VARIANT_BOOL *Cancel)
    {
    }
    virtual void NavigateComplete2(
        IDispatch *pDispatch,
        BSTR url)
    {
    }
    virtual void NewWindow3(
        IDispatch **ppDisp,
        VARIANT_BOOL *Cancel,
        DWORD dwFlags,
        BSTR bstrUrlContext,
        BSTR bstrUrl)
    {
    }
    virtual void CommandStateChange(
        long Command,
        VARIANT_BOOL Enable)
    {
    }
    virtual void StatusTextChange(BSTR Text)
    {
    }
    virtual void TitleTextChange(BSTR Text)
    {
    }
    virtual void FileDownload(
        VARIANT_BOOL ActiveDocument,
        VARIANT_BOOL *Cancel)
    {
    }
    virtual void DocumentComplete(
        IDispatch *pDisp,
        BSTR bstrURL)
    {
    }
    virtual void NavigateError(
        IDispatch *pDisp,
        VARIANT *url,
        VARIANT *target,
        LONG StatusCode,
        VARIANT_BOOL *Cancel)
    {
    }
    virtual void DownloadBegin()
    {
    }
    virtual void DownloadComplete()
    {
    }
    virtual void SetSecureLockIcon(DWORD SecureLockIcon)
    {
    }
    virtual void ProgressChange(LONG Progress, LONG ProgressMax)
    {
    }
    virtual void BeforeScriptExecute(IDispatch *pDisp)
    {
    }
};

////////////////////////////////////////////////////////////////////////////

class MEventSink : public IDispatch
{
public:
    static MEventSink *Create();

    bool Connect(IUnknown *pUnknown, MEventSinkListener *pListener);
    void Disconnect();

    // IUnknown interface
    STDMETHODIMP QueryInterface(REFIID riid, void **ppvObj);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // IDispatch interface
    STDMETHODIMP GetTypeInfoCount(UINT *pctinfo);
    STDMETHODIMP GetTypeInfo(
        UINT iTInfo,
        LCID lcid,
        ITypeInfo **ppTInfo);
    STDMETHODIMP GetIDsOfNames(
        REFIID riid,
        LPOLESTR *rgszNames,
        UINT cNames,
        LCID lcid,
        DISPID *rgDispId);
    STDMETHODIMP Invoke(
        DISPID dispIdMember,
        REFIID riid,
        LCID lcid,
        WORD wFlags,
        DISPPARAMS *pDispParams,
        VARIANT *pVarResult,
        EXCEPINFO *pExcepInfo,
        UINT *puArgErr);

protected:
    LONG                m_cRefs;
    MEventSinkListener *m_pListener;
    DWORD               m_dwCookie;
    IConnectionPoint   *m_pConnectPoint;
    virtual ~MEventSink();

    MEventSink();

private:
    MEventSink(const MEventSink&);
    MEventSink& operator=(const MEventSink&);
};

////////////////////////////////////////////////////////////////////////////

#endif  // ndef MZC4_MEVENTSINK_HPP_
