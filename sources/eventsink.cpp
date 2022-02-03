#include "eventsink.h"

#include "application.h"

#include <cassert>
#include <iostream>

#pragma comment(lib, "wbemuuid.lib")

struct CallbackData
{
    std::string tracking_group_name;
    std::wstring process_name;
    EventSink* event_sink;
    uint32_t process_id;
    HANDLE wait_handle;
};

static VOID CALLBACK process_termination_callback(_In_ PVOID lpParameter, _In_ BOOLEAN TimerOrWaitFired)
{
    assert(lpParameter);

    EnterCriticalSection(&g_dear_time.is_quitting_critical_section);
    // Don't let the application entering in quiting state during this processing
    {
        CallbackData*   data = (CallbackData*)lpParameter;
        RunningEntry    entry;
        FILETIME        kernel_time;
        FILETIME        user_time;

        if (g_dear_time.is_quitting)
        {
            // We can't register the record when the application is quiting.
            UnregisterWait(data->wait_handle);

            LeaveCriticalSection(&g_dear_time.is_quitting_critical_section);
            return;
        }

        auto it = data->event_sink->handles.find(data->process_id);
        assert(it != data->event_sink->handles.end());
        bool is_current_group = false;

        GetProcessTimes(it->second, (LPFILETIME)&entry.start_time, (LPFILETIME)&entry.end_time, &kernel_time, &user_time);

        EnterCriticalSection(&g_dear_time.editing_groups_critical_section);
        Group* tracking_group = get_tracking_group(data->tracking_group_name);

        if (tracking_group) // Group may have been destroyed
        {
            is_current_group = tracking_group->name == g_dear_time.current_group_name;
            EnterCriticalSection(&tracking_group->executions_critical_section);
            {
                tracking_group->executions.push_back(entry);
            }
            LeaveCriticalSection(&tracking_group->executions_critical_section);
        }
        LeaveCriticalSection(&g_dear_time.editing_groups_critical_section);

        CloseHandle(it->second);
        data->event_sink->handles.erase(it);

        UnregisterWait(data->wait_handle);

        delete data;

        if (is_current_group)
            request_redraw();
    }
    LeaveCriticalSection(&g_dear_time.is_quitting_critical_section);
}

// =============================================================================

EventSink::EventSink()
{
    m_lRef = (LONG*)_aligned_malloc(4, 32);
    InterlockedExchange(m_lRef, 0);
}

inline EventSink::~EventSink()
{

    bDone = true;
}

ULONG EventSink::AddRef()
{
    return InterlockedIncrement(m_lRef);
}

ULONG EventSink::Release()
{
    LONG lRef = InterlockedDecrement(m_lRef);
    if (lRef == 0)
        delete this;
    return lRef;
}

HRESULT EventSink::QueryInterface(REFIID riid, void** ppv)
{
    if (riid == IID_IUnknown || riid == IID_IWbemObjectSink)
    {
        *ppv = (IWbemObjectSink*)this;
        AddRef();
        return WBEM_S_NO_ERROR;
    }
    else return E_NOINTERFACE;
}

HRESULT EventSink::Indicate(long lObjectCount,
    IWbemClassObject** apObjArray)
{
    HRESULT hr = S_OK;
    _variant_t vtProp;
    bool is_delete = false;

    for (int i = 0; i < lObjectCount; i++)
    {
        std::wstring process_name;
        uint32_t process_id = 0;
        Group* tracking_group = nullptr;

        hr = apObjArray[i]->Get(_bstr_t(L"TargetInstance"), 0, &vtProp, 0, 0);
        if (FAILED(hr))
            continue;

        IUnknown* str = vtProp;
        hr = str->QueryInterface(IID_IWbemClassObject, reinterpret_cast<void**>(&apObjArray[i]));
        VariantClear(&vtProp);

        if (FAILED(hr))
        {
            apObjArray[i]->Release();
            continue;
        }

        _variant_t cn;
        hr = apObjArray[i]->Get(L"Name", 0, &cn, NULL, NULL);
        if (FAILED(hr) || (cn.vt == VT_NULL) || (cn.vt == VT_EMPTY))
        {
            VariantClear(&cn);
            apObjArray[i]->Release();
            continue;
        }

        EnterCriticalSection(&g_dear_time.editing_groups_critical_section);
        {
            process_name = cn.bstrVal;
            tracking_group = get_tracking_group_by_process(process_name);
            VariantClear(&cn);

            if (!tracking_group)
            {
                LeaveCriticalSection(&g_dear_time.editing_groups_critical_section);
                apObjArray[i]->Release();
                continue;
            }
            hr = apObjArray[i]->Get(L"ProcessId", 0, &cn, NULL, NULL);
            if (FAILED(hr))
            {
                LeaveCriticalSection(&g_dear_time.editing_groups_critical_section);
                VariantClear(&cn);
                apObjArray[i]->Release();
                continue;
            }
            process_id = cn.uintVal;
            VariantClear(&cn);

            // With SYNCHRONIZE the computed life time doesn't seems accurate, I got things like 252s when launching tracy and killing it immediately (around a s or two)
            HANDLE process_handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, process_id);
            if (process_handle != NULL)
            {
                handles.insert(std::make_pair(process_id, process_handle));

                CallbackData* data = new CallbackData();

                // @Warning we now use name of the group as it can be deleted before the callback is called
                // Using the pointer isn't safe anymore.
                data->tracking_group_name = tracking_group->name;
                data->process_name = std::move(process_name);
                data->event_sink = this;
                data->process_id = process_id;
                BOOL result = RegisterWaitForSingleObject(&data->wait_handle, process_handle, process_termination_callback, data, INFINITE, WT_EXECUTEONLYONCE);
                int i = 0;
            }
        }
        LeaveCriticalSection(&g_dear_time.editing_groups_critical_section);
        apObjArray[i]->Release();
    }

    return WBEM_S_NO_ERROR;
}

HRESULT EventSink::SetStatus(
    /* [in] */ LONG lFlags,
    /* [in] */ HRESULT hResult,
    /* [in] */ BSTR strParam,
    /* [in] */ IWbemClassObject __RPC_FAR* pObjParam
)
{
    if (lFlags == WBEM_STATUS_COMPLETE)
    {
        printf("Call complete. hResult = 0x%X\n", hResult);
    }
    else if (lFlags == WBEM_STATUS_PROGRESS)
    {
        printf("Call in progress.\n");
    }

    return WBEM_S_NO_ERROR;
}    // end of EventSink.cpp