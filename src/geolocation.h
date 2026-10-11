#pragma once
#include <roapi.h>
#include <winstring.h>
#include <inspectable.h>
/* Minimal Windows Runtime ABI declarations, checked against Microsoft's
   Windows.Devices.Geolocation metadata. No language runtime is required. */
struct GeoCoordinate : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE Latitude(double*)=0;
    virtual HRESULT STDMETHODCALLTYPE Longitude(double*)=0;
};
struct GeoPosition : IInspectable { virtual HRESULT STDMETHODCALLTYPE Coordinate(GeoCoordinate**)=0; };
struct GeoOperation : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE PutCompleted(IUnknown*)=0;
    virtual HRESULT STDMETHODCALLTYPE GetCompleted(IUnknown**)=0;
    virtual HRESULT STDMETHODCALLTYPE Results(GeoPosition**)=0;
};
struct GeoInfo : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE Id(UINT32*)=0;
    virtual HRESULT STDMETHODCALLTYPE Status(int*)=0;
    virtual HRESULT STDMETHODCALLTYPE Error(HRESULT*)=0;
    virtual HRESULT STDMETHODCALLTYPE Cancel()=0;
    virtual HRESULT STDMETHODCALLTYPE Close()=0;
};
struct GeoAccess : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE PutCompleted(IUnknown*)=0;
    virtual HRESULT STDMETHODCALLTYPE GetCompleted(IUnknown**)=0;
    virtual HRESULT STDMETHODCALLTYPE Results(int*)=0;
};
struct GeoStatics : IInspectable { virtual HRESULT STDMETHODCALLTYPE RequestAccess(GeoAccess**)=0; };
struct GeoLocator : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE Accuracy(int*)=0;
    virtual HRESULT STDMETHODCALLTYPE SetAccuracy(int)=0;
    virtual HRESULT STDMETHODCALLTYPE Movement(double*)=0;
    virtual HRESULT STDMETHODCALLTYPE SetMovement(double)=0;
    virtual HRESULT STDMETHODCALLTYPE Interval(UINT32*)=0;
    virtual HRESULT STDMETHODCALLTYPE SetInterval(UINT32)=0;
    virtual HRESULT STDMETHODCALLTYPE LocationStatus(int*)=0;
    virtual HRESULT STDMETHODCALLTYPE PositionAsync(GeoOperation**)=0;
    virtual HRESULT STDMETHODCALLTYPE PositionWithTimeout(INT64,INT64,GeoOperation**)=0;
};
static const GUID GEO_LOCATOR_IID={0xa9c3bf62,0x4524,0x4989,{0x8a,0xa9,0xde,0x01,0x9d,0x2e,0x55,0x1f}};
static const GUID GEO_INFO_IID={0x00000036,0,0,{0xc0,0,0,0,0,0,0,0x46}};
struct Located { double latitude=0,longitude=0; HRESULT result=E_FAIL; WCHAR name[160]={}; };
struct LocationRequest { HWND target; GeoAccess *access; bool chinese; };
inline HRESULT requestLocationAccess(GeoAccess **access)
{
    const GUID iid={0x9a8e7571,0x2df5,0x4591,{0x9f,0x87,0xeb,0x5f,0xd8,0x94,0xe9,0xb7}};
    const WCHAR *name=L"Windows.Devices.Geolocation.Geolocator";
    HSTRING cls; HRESULT result=WindowsCreateString(name,(UINT32)wcslen(name),&cls);
    if (FAILED(result)) return result;
    GeoStatics *factory=nullptr; result=RoGetActivationFactory(cls,iid,(void**)&factory); WindowsDeleteString(cls);
    if (SUCCEEDED(result)) { result=factory->RequestAccess(access); factory->Release(); }
    return result;
}
inline HRESULT createLocator(GeoLocator **locator)
{
    const WCHAR *name=L"Windows.Devices.Geolocation.Geolocator";
    HSTRING cls; HRESULT result=WindowsCreateString(name,(UINT32)wcslen(name),&cls);
    if (FAILED(result)) return result;
    IInspectable *instance=nullptr; result=RoActivateInstance(cls,&instance); WindowsDeleteString(cls);
    if (SUCCEEDED(result)) { result=instance->QueryInterface(GEO_LOCATOR_IID,(void**)locator); instance->Release(); }
    return result;
}
static void locateOnce(LocationRequest *request,Located *value)
{
    HWND target=request->target; bool useChinese=request->chinese;
    *value=Located{};
    HRESULT initialized=RoInitialize(RO_INIT_MULTITHREADED);
    bool allowed=true;
    if (SUCCEEDED(initialized)&&request->access) {
        GeoInfo *info=nullptr; HRESULT result=request->access->QueryInterface(GEO_INFO_IID,(void**)&info); int permission=0;
        if (SUCCEEDED(result)) {
            int state=0; ULONGLONG deadline=GetTickCount64()+20000;
            while (SUCCEEDED(info->Status(&state))&&state==0&&GetTickCount64()<deadline&&(!target||IsWindow(target))) Sleep(200);
            result=state==1?request->access->Results(&permission):E_ACCESSDENIED;
            if (state!=1) info->Cancel();
            info->Close(); info->Release();
        }
        allowed=SUCCEEDED(result)&&permission==1;
    }
    if (request->access) request->access->Release();
    free(request);
    if (SUCCEEDED(initialized)&&allowed) {
        GeoLocator *location=nullptr; value->result=createLocator(&location);
        if (SUCCEEDED(value->result)) {
            GeoOperation *operation=nullptr;
            value->result=location->PositionWithTimeout(0,200000000LL,&operation);
            if (SUCCEEDED(value->result)) {
                GeoInfo *info=nullptr; value->result=operation->QueryInterface(GEO_INFO_IID,(void**)&info);
                if (SUCCEEDED(value->result)) {
                    ULONGLONG deadline=GetTickCount64()+22000; int state=0;
                    while (SUCCEEDED(info->Status(&state))&&state==0&&GetTickCount64()<deadline&&(!target||IsWindow(target))) Sleep(200);
                    if (state!=1) { info->Cancel(); value->result=E_FAIL; }
                    else {
                        GeoPosition *position=nullptr; value->result=operation->Results(&position);
                        if (SUCCEEDED(value->result)) {
                            GeoCoordinate *coordinates=nullptr; value->result=position->Coordinate(&coordinates); position->Release();
                            if (SUCCEEDED(value->result)) {
                                value->result=coordinates->Latitude(&value->latitude);
                                if (SUCCEEDED(value->result)) value->result=coordinates->Longitude(&value->longitude);
                                coordinates->Release();
                                if (SUCCEEDED(value->result)&&(!std::isfinite(value->latitude)||!std::isfinite(value->longitude)||std::fabs(value->latitude)>89.9||std::fabs(value->longitude)>180)) value->result=E_INVALIDARG;
                            }
                        }
                    }
                    info->Close(); info->Release();
                }
                operation->Release();
            }
            location->Release();
        }
        RoUninitialize();
    } else { value->result=SUCCEEDED(initialized)?E_ACCESSDENIED:initialized; if (SUCCEEDED(initialized)) RoUninitialize(); }
    if (SUCCEEDED(value->result)) reversePlace(value->latitude,value->longitude,value->name,useChinese);

}
static DWORD WINAPI locateThread(void *context)
{
    LocationRequest *request=(LocationRequest*)context; HWND target=request->target;
    Located *value=(Located*)malloc(sizeof(Located));
    if (!value) { if (request->access) request->access->Release(); free(request); PostMessageW(target,WM_APP+3,0,0); return 0; }
    locateOnce(request,value);
    if (!PostMessageW(target,WM_APP+3,0,(LPARAM)value)) free(value);
    return 0;
}