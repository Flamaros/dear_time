#pragma once

#define _WIN32_DCOM
#include <comdef.h>
#include <Wbemidl.h>

#include <unordered_map>

class EventSink : public IWbemObjectSink
{
    volatile LONG* m_lRef;
    bool bDone;
    std::unordered_map<uint32_t, HANDLE> handles;

    friend VOID CALLBACK process_termination_callback(_In_ PVOID lpParameter, _In_ BOOLEAN TimerOrWaitFired);

public:
    EventSink();
    ~EventSink();

    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();
    virtual HRESULT
        STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv);

    virtual HRESULT STDMETHODCALLTYPE Indicate(
        LONG lObjectCount,
        IWbemClassObject __RPC_FAR* __RPC_FAR* apObjArray
    );

    virtual HRESULT STDMETHODCALLTYPE SetStatus(
        /* [in] */ LONG lFlags,
        /* [in] */ HRESULT hResult,
        /* [in] */ BSTR strParam,
        /* [in] */ IWbemClassObject __RPC_FAR* pObjParam
    );
};
