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
    virtual void OnBeforeNavigate2(
        IDispatch *pDisp,
        VARIANT *url,
        VARIANT *flags,
        VARIANT *target,
        VARIANT *PostData,
        VARIANT *headers,
        VARIANT_BOOL *Cancel) = 0;
    virtual void OnNavigateComplete2(
        IDispatch *pDispatch,
        BSTR url) = 0;
    virtual void OnNewWindow3(
        IDispatch **ppDisp,
        VARIANT_BOOL *Cancel,
        DWORD dwFlags,
        BSTR bstrUrlContext,
        BSTR bstrUrl) = 0;
    virtual void OnCommandStateChange(
        long Command,
        VARIANT_BOOL Enable) = 0;
    virtual void OnStatusTextChange(BSTR Text) = 0;
    virtual void OnTitleTextChange(BSTR Text) = 0;
    virtual void OnFileDownload(
        VARIANT_BOOL ActiveDocument,
        VARIANT_BOOL *Cancel) = 0;
    virtual void OnDocumentComplete(
        IDispatch *pDisp,
        BSTR bstrURL) = 0;
    virtual void OnNavigateError(
        IDispatch *pDisp,
        VARIANT *url,
        VARIANT *target,
        LONG StatusCode,
        VARIANT_BOOL *Cancel) = 0;
    virtual void OnDownloadBegin() = 0;
    virtual void OnDownloadComplete() = 0;
    virtual void OnSetSecureLockIcon(DWORD SecureLockIcon) = 0;
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
