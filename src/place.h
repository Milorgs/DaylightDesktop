#pragma once
#include <winhttp.h>
inline bool placeFromJson(const char *json,WCHAR out[160])
{
    jsmntok_t tokens[1024]; jsmn_parser parser; jsmn_init(&parser);
    int count=jsmn_parse(&parser,json,strlen(json),tokens,1024);
    if (count<1||tokens[0].type!=JSMN_OBJECT) return false;
    WCHAR city[128]={},region[128]={};
    int cityToken=jsonField(json,tokens,count,"city");
    if (cityToken>=0) jsonWide(json,tokens[cityToken],city,128);
    if (!city[0]) { int token=jsonField(json,tokens,count,"locality"); if (token>=0) jsonWide(json,tokens[token],city,128); }
    int token=jsonField(json,tokens,count,"principalSubdivision");
    if (token>=0) jsonWide(json,tokens[token],region,128);
    if (!city[0]) lstrcpynW(city,region,128);
    if (!city[0]) return false;
    if (region[0]&&lstrcmpW(region,city)&&wcslen(region)+wcslen(city)+4<160) swprintf(out,160,L"%ls · %ls",region,city);
    else lstrcpynW(out,city,160);
    return true;
}
inline bool reversePlace(double latitude,double longitude,WCHAR out[160],bool useChinese=true)
{
    static double cachedLat=1000,cachedLon=1000; static WCHAR cachedName[160]; static bool cachedChinese;
    if (cachedName[0]&&cachedChinese==useChinese&&std::fabs(latitude-cachedLat)<0.05&&std::fabs(longitude-cachedLon)<0.05) { lstrcpyW(out,cachedName); return true; }
    WCHAR path[256];
    swprintf(path,256,L"/data/reverse-geocode-client?latitude=%.2f&longitude=%.2f&localityLanguage=%ls",latitude,longitude,useChinese?L"zh":L"en");
    HINTERNET session=WinHttpOpen(L"DaylightDesktop/1.0",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
    if (!session) return false;
    WinHttpSetTimeouts(session,5000,5000,5000,7000);
    HINTERNET connection=WinHttpConnect(session,L"api.bigdatacloud.net",INTERNET_DEFAULT_HTTPS_PORT,0);
    HINTERNET request=connection?WinHttpOpenRequest(connection,L"GET",path,nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE):nullptr;
    bool ok=false;
    if (request&&WinHttpSendRequest(request,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)&&WinHttpReceiveResponse(request,nullptr)) {
        DWORD code=0,bytes=sizeof(code);
        if (WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&code,&bytes,nullptr)&&code==200) {
            char body[65537]; DWORD total=0,read=0; bool complete=false;
            while (total<65536&&WinHttpReadData(request,body+total,65536-total,&read)) { if (!read) { complete=true; break; } total+=read; }
            body[total]=0; ok=complete&&placeFromJson(body,out);
        }
    }
    if (request) WinHttpCloseHandle(request);
    if (connection) WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    if (ok) { cachedLat=latitude; cachedLon=longitude; cachedChinese=useChinese; lstrcpyW(cachedName,out); }
    return ok;
}
