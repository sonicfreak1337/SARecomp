#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmeapi.h>
#include <mmdeviceapi.h>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <string_view>
#include <stdexcept>
#include <new>
#include "sonic_recovery_status.hpp"

namespace sonic::audio_device {
inline std::atomic<std::uint64_t> changes{1};
inline std::atomic<std::uint64_t> resumes{0};
inline void changed() noexcept { changes.fetch_add(1,std::memory_order_release); }
inline void resumed() noexcept {resumes.fetch_add(1,std::memory_order_release);changed();}
inline std::uint64_t real_now() noexcept {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline std::uint64_t working_time() noexcept {
    using Precise=void(WINAPI*)(PULONGLONG);
    static const auto precise=[]{
        auto address=GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"QueryUnbiasedInterruptTimePrecise");
        if(!address)address=GetProcAddress(GetModuleHandleW(L"kernelbase.dll"),"QueryUnbiasedInterruptTimePrecise");
        return reinterpret_cast<Precise>(address);
    }();
    ULONGLONG ticks=0;
    if(precise)precise(&ticks);
    else if(!QueryUnbiasedInterruptTime(&ticks))return real_now();
    return ticks*100;
}
// Test substitution runs only in a hidden process, before its audio domain
// starts. Production retains the actual WinMM functions and sample cursor.
struct Api {
    decltype(&waveOutOpen) open=&waveOutOpen;
    decltype(&waveOutClose) close=&waveOutClose;
    decltype(&waveOutReset) reset=&waveOutReset;
    decltype(&waveOutPrepareHeader) prepare=&waveOutPrepareHeader;
    decltype(&waveOutUnprepareHeader) unprepare=&waveOutUnprepareHeader;
    decltype(&waveOutWrite) write=&waveOutWrite;
    decltype(&waveOutGetPosition) position=&waveOutGetPosition;
    decltype(&waveOutPause) pause=&waveOutPause;
    decltype(&waveOutRestart) restart=&waveOutRestart;
    std::uint64_t (*now)() noexcept=&real_now;
};
inline std::atomic<const Api*> test_api{nullptr};
inline const Api& api() noexcept {static const Api native;const auto* p=test_api.load();return p?*p:native;}
inline void set_test_api(const Api* value) {
    const auto* hidden=std::getenv("KATANA_PORT_BACKGROUND_TEST");
    if(!hidden || std::string_view(hidden)!="1")throw std::logic_error("audio-device-test-requires-hidden-process");
    test_api.store(value);
}
inline std::uint64_t now() noexcept {return api().now();}

class Notifications final : public IMMNotificationClient {
    std::atomic<ULONG> refs{1};
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override {
        if(!out)return E_POINTER;*out=nullptr;
        if(id!=__uuidof(IUnknown)&&id!=__uuidof(IMMNotificationClient))return E_NOINTERFACE;
        *out=static_cast<IMMNotificationClient*>(this);AddRef();return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release() override {const auto n=--refs;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow,ERole role,LPCWSTR) override {
        if(flow==eRender&&(role==eConsole||role==eMultimedia))changed();return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR,DWORD) override {changed();return S_OK;}
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override {changed();return S_OK;}
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override {changed();return S_OK;}
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR,const PROPERTYKEY) override {return S_OK;}
};
// Created and destroyed on the audio-domain owner. Callbacks only touch a
// process-lifetime atomic: no stream, domain, COM release or blocking work.
class Watch {
    IMMDeviceEnumerator* enumerator=nullptr;
    Notifications* notifications=nullptr;
    bool com=false,registered=false;
public:
    Watch() noexcept {
        if(test_api.load())return;
        const auto result=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
        com=SUCCEEDED(result);
        if(FAILED(result)&&result!=RPC_E_CHANGED_MODE)return;
        if(FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_INPROC_SERVER,
            __uuidof(IMMDeviceEnumerator),reinterpret_cast<void**>(&enumerator))))return;
        notifications=new(std::nothrow) Notifications;
        if(notifications)registered=SUCCEEDED(enumerator->RegisterEndpointNotificationCallback(notifications));
    }
    ~Watch(){
        if(registered) {
            // A failed unregister cannot prove callback lifetime ended. Keep
            // its atomic-only receiver alive rather than free live COM data.
            if(FAILED(enumerator->UnregisterEndpointNotificationCallback(notifications)))notifications=nullptr;
        }
        if(notifications)notifications->Release();
        if(enumerator)enumerator->Release();
        if(com)CoUninitialize();
    }
    Watch(const Watch&)=delete;
};
}
