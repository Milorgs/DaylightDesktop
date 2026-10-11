#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#include <winsock2.h>
#include <ws2ipdef.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <commctrl.h>
#include <cstdio>
#include <cwchar>
#include <vector>
#include <algorithm>
#include "schedule.h"
#include "library.h"
#include "place.h"
#include "geolocation.h"
#include "preview.h"

static const WCHAR *UI_CLASS = L"Milorgs.DaylightDesktop.Settings";
static const WCHAR *WORKER_CLASS = L"Milorgs.DaylightDesktop.Worker";
static const WCHAR *RUN_KEY = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const UINT RELOAD = WM_APP+1;
enum { DAY=101, NIGHT, DAY_PICK, NIGHT_PICK, MODE, RISE, SET, LAT, LON, ENABLE, STARTUP, SAVE, HIDE, MORE, BACK, PROJECT, MODE_TIME, MODE_SOLAR, FOLDER_PICK, AUTO_LOCATION, AUTO_POSITION, POSITION_AUTO, POSITION_CUSTOM, ID_UI_CHINESE, ID_UI_ENGLISH, THEME_SYSTEM, THEME_LIGHT, THEME_DARK, IMPORT_THEME, PREVIEW_THEME, LIBRARY_PREV, LIBRARY_NEXT, PHASE_PREV, PHASE_NEXT, LIBRARY_TILE=140, REFRESH_WALLPAPER=146 };
struct Settings { WCHAR day[MAX_PATH]={}, night[MAX_PATH]={}, folder[MAX_PATH]={},themeName[128]={},placeName[160]={}; int rise=420, set=1140,appearance=0; double lat=0, lon=0,manualLat=0,manualLon=0; bool solar=true, enabled=false,autoPosition=true,located=false,manualKnown=false,chinese=PRIMARYLANGID(GetUserDefaultUILanguage())==LANG_CHINESE; };
static WCHAR configPath[MAX_PATH], exePath[MAX_PATH], lastWallpaper[MAX_PATH];
static HWND window;
static WCHAR chosenFolder[MAX_PATH];
static Theme chosenTheme;
static HBITMAP preview;
static bool locating,autoPositionChoice=true,initializingUI=true;
static WCHAR currentPlace[160];
static HANDLE networkNotification;
static ULONGLONG lastLocationRequest;
static bool solarChoice, morePage;
static int page,libraryRows=1;
static HFONT font, heading, small;
static HBRUSH background, surface;
static bool dark, worker, dryRun, enabledChoice, startupChoice;
static bool chinese=true,importing,mica,compositedPaint,highContrast;
static bool lightPreview;
static bool refreshingWallpaper;
static int appearance;
static HTHEME textTheme;
static COLORREF baseColor,cardColor,borderColor,mutedColor,accentColor;
static int dpi=96;
static LRESULT CALLBACK switchProcedure(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
static WCHAR status[256] = L"";
extern "C" int initDrawing(void);
extern "C" void closeDrawing(void);
extern "C" void smoothRoundRect(HDC,int,int,int,int,int,COLORREF,COLORREF,int);
extern "C" void smoothCircle(HDC,int,int,int,COLORREF);
extern "C" void smoothArrow(HDC,int,int,int,COLORREF,int);
static const WCHAR *tr(const WCHAR *cn,const WCHAR *en) { return chinese?cn:en; }
static int px(int n) { return MulDiv(n,dpi,96); }
#include "geolocation_process.h"
static void resizePage()
{
    if (!window||worker) return;
    RECT r={0,0,px(460),px(page==1?372:page==2?154+libraryRows*106:page==3?356:368)};
    AdjustWindowRect(&r,(DWORD)GetWindowLongPtrW(window,GWL_STYLE),FALSE);
    RECT current; GetWindowRect(window,&current);
    if (current.right-current.left!=r.right-r.left||current.bottom-current.top!=r.bottom-r.top)
        SetWindowPos(window,nullptr,0,0,r.right-r.left,r.bottom-r.top,SWP_NOMOVE|SWP_NOZORDER);
}
struct ConfigLock {
    HANDLE handle; bool acquired;
    ConfigLock() { handle=CreateMutexW(nullptr,FALSE,L"Local\\Milorgs.DaylightDesktop.Config"); DWORD result=handle?WaitForSingleObject(handle,2000):WAIT_FAILED; acquired=result==WAIT_OBJECT_0||result==WAIT_ABANDONED; }
    ~ConfigLock() { if (acquired) ReleaseMutex(handle); if (handle) CloseHandle(handle); }
};
static Settings load(const WCHAR *path)
{
    Settings s; WCHAR value[64];
    GetPrivateProfileStringW(L"Wallpaper",L"Folder",L"",s.folder,MAX_PATH,path);
    GetPrivateProfileStringW(L"Wallpaper",L"Name",L"",s.themeName,128,path);
    GetPrivateProfileStringW(L"Wallpaper",L"Day",L"",s.day,MAX_PATH,path);
    GetPrivateProfileStringW(L"Wallpaper",L"Night",L"",s.night,MAX_PATH,path);
    s.rise=GetPrivateProfileIntW(L"Schedule",L"Sunrise",420,path);
    s.set=GetPrivateProfileIntW(L"Schedule",L"Sunset",1140,path);
    if (s.rise<0 || s.rise>=1440) s.rise=420;
    if (s.set<0 || s.set>=1440 || s.set==s.rise) s.set=(s.rise+720)%1440;
    s.solar=GetPrivateProfileIntW(L"Schedule",L"Solar",1,path)!=0;
    s.enabled=GetPrivateProfileIntW(L"Schedule",L"Enabled",0,path)!=0;
    GetPrivateProfileStringW(L"Schedule",L"Latitude",L"0",value,64,path); s.lat=wcstod(value,nullptr);
    GetPrivateProfileStringW(L"Schedule",L"Longitude",L"0",value,64,path); s.lon=wcstod(value,nullptr);
    if (!std::isfinite(s.lat) || std::fabs(s.lat)>89.9) s.lat=0;
    if (!std::isfinite(s.lon) || std::fabs(s.lon)>180) s.lon=0;
    s.autoPosition=GetPrivateProfileIntW(L"Schedule",L"AutoPosition",1,path)!=0;
    s.located=GetPrivateProfileIntW(L"Schedule",L"Located",0,path)!=0;
    GetPrivateProfileStringW(L"Schedule",L"Place",L"",s.placeName,160,path);
    s.manualKnown=GetPrivateProfileIntW(L"Schedule",L"ManualKnown",0,path)!=0;
    GetPrivateProfileStringW(L"Schedule",L"ManualLatitude",L"",value,64,path); s.manualLat=value[0]?wcstod(value,nullptr):s.lat;
    GetPrivateProfileStringW(L"Schedule",L"ManualLongitude",L"",value,64,path); s.manualLon=value[0]?wcstod(value,nullptr):s.lon;
    if (!std::isfinite(s.manualLat)||std::fabs(s.manualLat)>89.9||!std::isfinite(s.manualLon)||std::fabs(s.manualLon)>180) s.manualKnown=false;
    GetPrivateProfileStringW(L"Interface",L"Language",L"",value,64,path);
    if (value[0]) s.chinese=!lstrcmpiW(value,L"zh-CN");
    s.appearance=GetPrivateProfileIntW(L"Interface",L"Appearance",0,path); if (s.appearance<0||s.appearance>2) s.appearance=0;
    return s;
}
static bool save(const WCHAR *path,const Settings &s)
{
    ConfigLock lock; if (!lock.acquired) return false;
    WCHAR temporary[MAX_PATH], text[4096];
    if (wcslen(path)+5>=MAX_PATH) return false;
    swprintf(temporary,MAX_PATH,L"%ls.tmp",path);
    int n=swprintf(text,4096,L"\xFEFF[Wallpaper]\r\nDay=%ls\r\nNight=%ls\r\nFolder=%ls\r\nName=%ls\r\n[Schedule]\r\nSunrise=%d\r\nSunset=%d\r\nSolar=%d\r\nEnabled=%d\r\nLatitude=%.6f\r\nLongitude=%.6f\r\nAutoPosition=%d\r\nLocated=%d\r\nPlace=%ls\r\nManualKnown=%d\r\nManualLatitude=%.6f\r\nManualLongitude=%.6f\r\n[Interface]\r\nLanguage=%ls\r\nAppearance=%d\r\n",
        s.day,s.night,s.folder,s.themeName,s.rise,s.set,s.solar,s.enabled,s.lat,s.lon,s.autoPosition,s.located,s.placeName,s.manualKnown,s.manualLat,s.manualLon,s.chinese?L"zh-CN":L"en",s.appearance);
    if (n<=0) return false;
    HANDLE f=CreateFileW(temporary,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if (f==INVALID_HANDLE_VALUE) return false;
    DWORD count=0;
    bool ok=WriteFile(f,text,n*sizeof(WCHAR),&count,nullptr) && count==n*sizeof(WCHAR);
    if (ok) ok=FlushFileBuffers(f)!=0;
    CloseHandle(f);
    if (ok) ok=MoveFileExW(temporary,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH|MOVEFILE_COPY_ALLOWED)!=0;
    if (!ok) DeleteFileW(temporary);
    WritePrivateProfileStringW(nullptr,nullptr,nullptr,path);
    return ok;
}
static Plan currentPlan(const Settings &s,SYSTEMTIME *time=nullptr,double zenith=90.833)
{
    SYSTEMTIME t; GetLocalTime(&t); if (time) *time=t;
    int now=t.wHour*60+t.wMinute;
    if (!s.solar) return fixedPlan(now,s.rise,s.set);
    bool leap=(t.wYear%4==0 && (t.wYear%100!=0 || t.wYear%400==0));
    const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    int doy=t.wDay; for (int i=0;i<t.wMonth-1;++i) doy+=days[i]+(i==1 && leap);
    TIME_ZONE_INFORMATION zone; DWORD id=GetTimeZoneInformation(&zone);
    int offset=-zone.Bias-(id==TIME_ZONE_ID_DAYLIGHT ? zone.DaylightBias : zone.StandardBias);
    return solarPlan(now,doy,leap,s.lat,s.lon,offset,zenith);
}
static bool exists(const WCHAR *file) { DWORD a=GetFileAttributesW(file); return a!=INVALID_FILE_ATTRIBUTES && !(a&FILE_ATTRIBUTE_DIRECTORY); }
static bool startupExists()
{
    DWORD bytes=0; return RegGetValueW(HKEY_CURRENT_USER,RUN_KEY,L"DaylightDesktop",RRF_RT_REG_SZ,nullptr,nullptr,&bytes)==ERROR_SUCCESS;
}
static LONG startup(bool enabled)
{
    HKEY key; LONG result=RegCreateKeyExW(HKEY_CURRENT_USER,RUN_KEY,0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr);
    if (result!=ERROR_SUCCESS) return result;
    if (enabled) {
        WCHAR command[MAX_PATH+32]; swprintf(command,MAX_PATH+32,L"\"%ls\" --background",exePath);
        result=RegSetValueExW(key,L"DaylightDesktop",0,REG_SZ,(BYTE*)command,(DWORD)((wcslen(command)+1)*sizeof(WCHAR)));
    } else { result=RegDeleteValueW(key,L"DaylightDesktop"); if (result==ERROR_FILE_NOT_FOUND) result=ERROR_SUCCESS; }
    RegCloseKey(key); return result;
}
static bool spawn(const WCHAR *args)
{
    WCHAR command[MAX_PATH+64]; swprintf(command,MAX_PATH+64,L"\"%ls\" %ls",exePath,args);
    STARTUPINFOW si={}; si.cb=sizeof(si); PROCESS_INFORMATION pi={};
    if (!CreateProcessW(exePath,command,nullptr,nullptr,FALSE,0,nullptr,nullptr,&si,&pi)) return false;
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess); return true;
}
struct Playback { int index,starts[4],polar; DWORD delay; };
static bool physicalWallpaperPath(const WCHAR *file,WCHAR *path,DWORD capacity)
{
    // Desktop services do not share a packaged launcher's redirected file namespace.
    HANDLE handle=CreateFileW(file,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if (handle==INVALID_HANDLE_VALUE) return false;
    DWORD length=GetFinalPathNameByHandleW(handle,path,capacity,FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
    CloseHandle(handle);
    if (!length||length>=capacity) return false;
    if (!_wcsnicmp(path,L"\\\\?\\UNC\\",8)) { memmove(path+2,path+8,(wcslen(path+8)+1)*sizeof(WCHAR)); path[0]=path[1]=L'\\'; }
    else if (!_wcsnicmp(path,L"\\\\?\\",4)) memmove(path,path+4,(wcslen(path+4)+1)*sizeof(WCHAR));
    return true;
}
static HRESULT applyWallpaperFile(const WCHAR *file,bool force=false)
{
    WCHAR physical[4096]; HRESULT result=HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
        if (physicalWallpaperPath(file,physical,4096)) {
            HRESULT apartment=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
            result=apartment;
            if (SUCCEEDED(apartment)) {
                IDesktopWallpaper *desktop=nullptr;
                result=CoCreateInstance(CLSID_DesktopWallpaper,nullptr,CLSCTX_ALL,IID_IDesktopWallpaper,(void**)&desktop);
                if (SUCCEEDED(result)) {
                    PWSTR currentWallpaper=nullptr;
                    HRESULT currentResult=desktop->GetWallpaper(nullptr,&currentWallpaper);
                    if (!force&&currentResult==S_OK&&currentWallpaper&&!lstrcmpiW(currentWallpaper,physical)) result=S_OK;
                    else result=desktop->SetWallpaper(nullptr,physical);
                    if (currentWallpaper) CoTaskMemFree(currentWallpaper);
                    desktop->Release();
                }
                CoUninitialize();
            }
        }
    return result;
}
static Playback playbackAt(const Settings &s,const Theme &theme)
{
    SYSTEMTIME now; Plan p=currentPlan(s,&now);

    int starts[4]={p.sunrise,p.sunrise,p.sunset,p.sunset};
    if (!p.polar) {
        int span=normalizeMinute(p.sunset-p.sunrise), twilight=span/4<30?span/4:30;
        Plan low=p,high=p; if (s.solar&&(theme.lengths[0]||theme.lengths[2])) { low=currentPlan(s,nullptr,96); high=currentPlan(s,nullptr,84); }
        if (theme.lengths[0]) { starts[0]=s.solar&&!low.polar?low.sunrise:normalizeMinute(p.sunrise-twilight); starts[1]=s.solar&&!high.polar?high.sunrise:normalizeMinute(p.sunrise+twilight); }
        if (theme.lengths[2]) { starts[2]=s.solar&&!high.polar?high.sunset:normalizeMinute(p.sunset-twilight); starts[3]=s.solar&&!low.polar?low.sunset:normalizeMinute(p.sunset+twilight); }
    }
    int minute=now.wHour*60+now.wMinute;
    int index=theme.valid?themeFrame(theme,minute,starts,p.polar):0;

    if (theme.valid) p.nextMinutes=std::min(p.nextMinutes,themeNextMinutes(theme,minute,starts,p.polar));
    DWORD delay=(DWORD)(p.nextMinutes*60000-now.wSecond*1000-now.wMilliseconds);
    if (delay<1000) delay=1000;
    Playback result={index,{starts[0],starts[1],starts[2],starts[3]},p.polar,delay}; return result;
}
static void scheduleWorker(HWND h)
{
    static bool evaluating=false;
    if (evaluating) return;
    evaluating=true;
    KillTimer(h,1);
    Settings s=load(configPath); if (!s.enabled || (s.solar&&!s.located)) { if (!s.enabled) lastWallpaper[0]=0; evaluating=false; return; }
    static Theme theme; static WCHAR manifest[MAX_PATH]; static WIN32_FILE_ATTRIBUTE_DATA stamp={};
    WIN32_FILE_ATTRIBUTE_DATA current={};
    bool found=GetFileAttributesExW(s.folder,GetFileExInfoStandard,&current)!=FALSE;
    if (!theme.valid||lstrcmpiW(manifest,s.folder)||!found||CompareFileTime(&stamp.ftLastWriteTime,&current.ftLastWriteTime)||stamp.nFileSizeLow!=current.nFileSizeLow||stamp.nFileSizeHigh!=current.nFileSizeHigh) {
        theme=readTheme(s.folder); lstrcpynW(manifest,s.folder,MAX_PATH); stamp=current;
    }
    Playback playback=playbackAt(s,theme);
    const WCHAR *file=theme.valid?theme.frames.files[playback.index]:L"";
    if (!dryRun && exists(file) && wcscmp(file,lastWallpaper)) {
        HRESULT result=applyWallpaperFile(file);
        if (SUCCEEDED(result)) lstrcpynW(lastWallpaper,file,MAX_PATH);
        else {
            ConfigLock lock;
            if (lock.acquired) {
                Settings stopped=load(configPath); stopped.enabled=false; save(configPath,stopped);
                WritePrivateProfileStringW(L"Status",L"LastError",L"Windows could not apply wallpaper. Switching paused.",configPath);
            }
            HWND ui=FindWindowW(UI_CLASS,nullptr); if (ui) PostMessageW(ui,WM_APP+13,0,0);
            PostMessageW(h,RELOAD,0,0); evaluating=false; return;
        }
    }
    SetTimer(h,1,playback.delay,nullptr); /* One next-transition timer, reset after each event. */
    evaluating=false;
}
static void refreshFonts()
{
    HFONT oldFont=font,oldSmall=small,oldHeading=heading;
    const WCHAR *family=chinese?L"Microsoft YaHei UI":L"Segoe UI";
    font=CreateFontW(-px(14),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,family);
    small=CreateFontW(-px(12),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,family);
    heading=CreateFontW(-px(21),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,family);
    for (HWND child=window?GetWindow(window,GW_CHILD):nullptr;child;child=GetWindow(child,GW_HWNDNEXT)) {
        SendMessageW(child,WM_SETFONT,(WPARAM)font,FALSE);
        WCHAR kind[32]; GetClassNameW(child,kind,32);
        if (!lstrcmpiW(kind,L"EDIT")) {
            HDC dc=GetDC(child); HGDIOBJ original=SelectObject(dc,font); TEXTMETRICW metrics; GetTextMetricsW(dc,&metrics); SelectObject(dc,original); ReleaseDC(child,dc);
            RECT r; GetClientRect(child,&r); r.top=(r.bottom-metrics.tmHeight)/2;
            SendMessageW(child,EM_SETRECT,0,(LPARAM)&r);
        }
    }
    if (oldFont) DeleteObject(oldFont);
    if (oldSmall) DeleteObject(oldSmall);
    if (oldHeading) DeleteObject(oldHeading);
}
static bool palette()
{
    DWORD light=1,bytes=sizeof(light);
    RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&light,&bytes);
    dark=appearance==0?!light:appearance==2;
    if (lightPreview) dark=false;
    HIGHCONTRASTW contrast={sizeof(contrast),0,nullptr};
    SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(contrast),&contrast,0); highContrast=(contrast.dwFlags&HCF_HIGHCONTRASTON)!=0;
    baseColor=dark?RGB(32,32,32):RGB(245,245,245);
    cardColor=dark?RGB(45,45,45):RGB(255,255,255);
    borderColor=dark?RGB(68,68,68):RGB(218,221,227);
    mutedColor=dark?RGB(179,179,185):RGB(105,109,119);
    accentColor=dark?RGB(126,183,255):RGB(36,92,211);
    if (highContrast) { baseColor=cardColor=GetSysColor(COLOR_WINDOW); borderColor=mutedColor=GetSysColor(COLOR_WINDOWTEXT); accentColor=GetSysColor(COLOR_HIGHLIGHT); }
    if (background) DeleteObject(background);
    if (surface) DeleteObject(surface);
    background=CreateSolidBrush(baseColor); surface=CreateSolidBrush(cardColor);
    if (window&&!worker) {
        static HWND appliedWindow; static bool appliedDark,appliedContrast; static int appliedMaterial;
        static COLORREF appliedBase,appliedInk;
        DWORD transparency=1; bytes=sizeof(transparency);
        RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"EnableTransparency",RRF_RT_REG_DWORD,nullptr,&transparency,&bytes);
        int material=highContrast||!transparency||lightPreview?1:2;
        COLORREF foreground=highContrast?GetSysColor(COLOR_WINDOWTEXT):dark?RGB(243,243,243):RGB(30,32,36);
        if (appliedWindow==window&&appliedDark==dark&&appliedContrast==highContrast&&appliedMaterial==material&&appliedBase==baseColor&&appliedInk==foreground) return false;
        appliedWindow=window; appliedDark=dark; appliedContrast=highContrast; appliedMaterial=material; appliedBase=baseColor; appliedInk=foreground;
        BOOL value=dark; int corner=2;
        DwmSetWindowAttribute(window,20,&value,sizeof(value)); DwmSetWindowAttribute(window,33,&corner,sizeof(corner));
        mica=SUCCEEDED(DwmSetWindowAttribute(window,38,&material,sizeof(material)))&&material==2;
        COLORREF caption=mica?0xFFFFFFFF:baseColor,text=mica?0xFFFFFFFF:highContrast?GetSysColor(COLOR_WINDOWTEXT):dark?RGB(243,243,243):RGB(30,32,36);
        DwmSetWindowAttribute(window,35,&caption,sizeof(caption)); DwmSetWindowAttribute(window,36,&text,sizeof(text));
        MARGINS margins=mica?MARGINS{-1,-1,-1,-1}:MARGINS{0,0,0,0};
        if (FAILED(DwmExtendFrameIntoClientArea(window,&margins))) mica=false;
        if (textTheme) CloseThemeData(textTheme);
        textTheme=OpenThemeData(window,L"WINDOW");
    }
    return true;
}
static COLORREF ink() { return highContrast?GetSysColor(COLOR_WINDOWTEXT):dark?RGB(243,243,243):RGB(30,32,36); }
static LRESULT CALLBACK editProcedure(HWND h,UINT message,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR)
{
    if (message==WM_PAINT) {
        PAINTSTRUCT paint; HDC screen=BeginPaint(h,&paint),dc;
        RECT client; GetClientRect(h,&client);
        BP_PAINTPARAMS parameters={}; parameters.cbSize=sizeof(parameters); parameters.dwFlags=BPPF_ERASE;
        HPAINTBUFFER buffer=BeginBufferedPaint(screen,&client,BPBF_TOPDOWNDIB,&parameters,&dc);
        if (!buffer) dc=screen;
        FillRect(dc,&client,surface);
        if (buffer) BufferedPaintSetAlpha(buffer,nullptr,255);
        if (GetFocus()==h) DefSubclassProc(h,WM_PRINTCLIENT,(WPARAM)dc,PRF_CLIENT);
        else {
            WCHAR text[64]; GetWindowTextW(h,text,64); SelectObject(dc,(HFONT)SendMessageW(h,WM_GETFONT,0,0));
            COLORREF color=IsWindowEnabled(h)?ink():mutedColor;
            DTTOPTS options={}; options.dwSize=sizeof(options); options.dwFlags=DTT_COMPOSITED|DTT_TEXTCOLOR; options.crText=color;
            if (!buffer||!textTheme||FAILED(DrawThemeTextEx(textTheme,dc,0,0,text,-1,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX,&client,&options))) {
                SetBkMode(dc,TRANSPARENT); SetTextColor(dc,color); DrawTextW(dc,text,-1,&client,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
            }
        }
        if (buffer) { BufferedPaintSetAlpha(buffer,nullptr,255); EndBufferedPaint(buffer,TRUE); }
        EndPaint(h,&paint); return 0;
    }
    if (message==WM_NCDESTROY) RemoveWindowSubclass(h,editProcedure,1);
    return DefSubclassProc(h,message,w,l);
}
static HWND ctrl(const WCHAR *kind,const WCHAR *text,DWORD style,int x,int y,int w,int h,int id)
{
    HWND c=CreateWindowExW(0,kind,text,WS_CHILD|WS_VISIBLE|style,px(x),px(y),px(w),px(h),window,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);
    SendMessageW(c,WM_SETFONT,(WPARAM)font,TRUE);
    if (id==ENABLE||id==STARTUP) SetWindowSubclass(c,switchProcedure,2,0);
    if (kind[0]==L'E') SetWindowSubclass(c,editProcedure,1,0);
    if (kind[0]==L'E' && (style&ES_CENTER)) {
        HDC dc=GetDC(c); SelectObject(dc,font); TEXTMETRICW metrics; GetTextMetricsW(dc,&metrics); ReleaseDC(c,dc);
        RECT text={0,(px(h)-metrics.tmHeight)/2,px(w),px(h)};
        SendMessageW(c,EM_SETRECT,0,(LPARAM)&text); SendMessageW(c,EM_SETLIMITTEXT,63,0);
    }
    return c;
}
static void label(HDC dc,const WCHAR *text,int x,int y,int w,int h,HFONT f,COLORREF color,DWORD format=DT_LEFT|DT_WORDBREAK|DT_NOPREFIX)
{
    RECT r={px(x),px(y),px(x+w),px(y+h)}; SelectObject(dc,f); SetBkMode(dc,TRANSPARENT); SetTextColor(dc,color);
    if (compositedPaint&&textTheme) {
        DTTOPTS options={}; options.dwSize=sizeof(options); options.dwFlags=DTT_COMPOSITED|DTT_TEXTCOLOR; options.crText=color;
        if (SUCCEEDED(DrawThemeTextEx(textTheme,dc,0,0,text,-1,format,&r,&options))) return;
    }
    DrawTextW(dc,text,-1,&r,format);
}
static void applyUI(bool switchOnly=false);
static void syncLocationMonitoring(HWND target);
#include "catalog.h"
static void identifyLocation(HWND target=nullptr);
struct RefreshJob { HWND target; WCHAR file[MAX_PATH]; bool dry; HRESULT result; };
static DWORD WINAPI refreshWallpaperThread(void *context)
{
    RefreshJob *job=(RefreshJob*)context;
    job->result=job->dry?S_OK:applyWallpaperFile(job->file,true);
    if (!PostMessageW(job->target,WM_APP+15,0,(LPARAM)job)) free(job);
    return 0;
}
static void refreshWallpaperNow()
{
    if (!chosenTheme.valid||refreshingWallpaper) return;
    Settings settings=load(configPath);
    if (settings.solar&&!settings.located) {
        lstrcpyW(status,tr(L"位置尚不可用，请先设置位置或固定时间。",L"Set a location or choose Fixed time first."));
        RECT helper={px(28),px(276),px(432),px(302)}; InvalidateRect(window,&helper,FALSE); return;
    }
    Playback playback=playbackAt(settings,chosenTheme);
    RefreshJob *job=(RefreshJob*)calloc(1,sizeof(RefreshJob)); if (!job) return;
    job->target=window; job->dry=dryRun; lstrcpynW(job->file,chosenTheme.frames.files[playback.index],MAX_PATH);
    refreshingWallpaper=true;
    SetDlgItemTextW(window,REFRESH_WALLPAPER,tr(L"刷新中…",L"Refreshing…"));
    HANDLE thread=CreateThread(nullptr,0,refreshWallpaperThread,job,0,nullptr);
    if (thread) CloseHandle(thread);
    else { free(job); refreshingWallpaper=false; SetDlgItemTextW(window,REFRESH_WALLPAPER,tr(L"刷新壁纸",L"Refresh wallpaper")); }
}
static void migrateRememberedTheme()
{
    if (importing||!chosenTheme.valid) return;
    WCHAR root[MAX_PATH]; lstrcpyW(root,configPath); PathRemoveFileSpecW(root);
    if (wcslen(root)+9>=MAX_PATH) return;
    lstrcatW(root,L"\\themes");
    if (!_wcsnicmp(chosenFolder,root,wcslen(root))&&chosenFolder[wcslen(root)]==L'\\') return;
    ImportJob *job=(ImportJob*)calloc(1,sizeof(ImportJob)); if (!job) return;
    job->target=window; lstrcpyW(job->root,root); lstrcpyW(job->source,chosenFolder); PathRemoveFileSpecW(job->source);
    importing=true; EnableWindow(GetDlgItem(window,FOLDER_PICK),FALSE);
    SetDlgItemTextW(window,FOLDER_PICK,tr(L"正在保存主题…",L"Saving theme…"));
    HANDLE thread=CreateThread(nullptr,0,importThread,job,0,nullptr);
    if (thread) CloseHandle(thread);
    else { free(job); importing=false; EnableWindow(GetDlgItem(window,FOLDER_PICK),TRUE); }
}
static void chooseFolder()
{
    if (importing) return;
    IFileDialog *dialog=nullptr;
    HRESULT result=CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_IFileDialog,(void**)&dialog);
    if (FAILED(result)) return;
    DWORD options; dialog->GetOptions(&options); dialog->SetOptions(options|FOS_FORCEFILESYSTEM|FOS_PICKFOLDERS|FOS_PATHMUSTEXIST);
    dialog->SetTitle(tr(L"选择主题文件夹",L"Select theme folder"));
    WCHAR existing[MAX_PATH]; GetEnvironmentVariableW(L"LOCALAPPDATA",existing,MAX_PATH);
    if (wcslen(existing)+100<MAX_PATH) {
        lstrcatW(existing,L"\\Packages\\38719TimothyJohnson.WinDynamicDesktop_rfarkse4rn21a\\LocalState\\themes");
        IShellItem *start=nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(existing,nullptr,IID_IShellItem,(void**)&start))) { dialog->SetDefaultFolder(start); start->Release(); }
    }
    if (SUCCEEDED(dialog->Show(window))) {
        IShellItem *item=nullptr;
        if (SUCCEEDED(dialog->GetResult(&item))) {
            PWSTR path=nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))) {
                ImportJob *job=(ImportJob*)calloc(1,sizeof(ImportJob));
                if (job&&wcslen(path)<MAX_PATH) {
                    job->target=window; job->enableAfterImport=true; lstrcpyW(job->source,path); lstrcpyW(job->root,configPath); PathRemoveFileSpecW(job->root);
                    if (wcslen(job->root)+8<MAX_PATH) {
                        lstrcatW(job->root,L"\\themes"); importing=true;
                        EnableWindow(GetDlgItem(window,FOLDER_PICK),FALSE);
                        SetDlgItemTextW(window,FOLDER_PICK,tr(L"正在导入…",L"Importing…"));
                        HANDLE thread=CreateThread(nullptr,0,importThread,job,0,nullptr);
                        if (thread) { CloseHandle(thread); job=nullptr; }
                        else { importing=false; EnableWindow(GetDlgItem(window,FOLDER_PICK),TRUE); }
                    }
                }
                free(job); CoTaskMemFree(path);
            }
            item->Release();
        }
    }
    dialog->Release();
}
static void identifyLocation(HWND target)
{
    if (!target) target=window;
    if (dryRun) return;
    if (locating) return;
    lastLocationRequest=GetTickCount64(); locating=true; EnableWindow(GetDlgItem(window,AUTO_LOCATION),FALSE); SetDlgItemTextW(window,AUTO_LOCATION,tr(L"正在识别…",L"Locating…"));
    LocationRequest *request=(LocationRequest*)malloc(sizeof(LocationRequest));
    if (!request) { locating=false; EnableWindow(GetDlgItem(window,AUTO_LOCATION),TRUE); SetDlgItemTextW(window,AUTO_LOCATION,tr(L"刷新",L"Refresh")); return; }
    request->target=target; request->access=nullptr; request->chinese=load(configPath).chinese;
    if (!worker&&FAILED(requestLocationAccess(&request->access))) {
        free(request); locating=false; EnableWindow(GetDlgItem(window,AUTO_LOCATION),TRUE); SetDlgItemTextW(window,AUTO_LOCATION,tr(L"刷新",L"Refresh"));
        lstrcpyW(status,tr(L"Windows 定位权限不可用，可选择自定义坐标。",L"Windows location is unavailable. Choose Coordinates to set a location.")); InvalidateRect(window,nullptr,FALSE); return;
    }
    HANDLE thread=CreateThread(nullptr,0,worker?locationProcessThread:locateThread,request,0,nullptr);
    if (thread) CloseHandle(thread);
    else { if (request->access) request->access->Release(); free(request); locating=false; EnableWindow(GetDlgItem(window,AUTO_LOCATION),TRUE); SetDlgItemTextW(window,AUTO_LOCATION,tr(L"刷新",L"Refresh")); }
}
static void scheduleControls();
static void showPage(int nextPage)
{
    bool visible=IsWindowVisible(window)!=FALSE;
    if (visible) SendMessageW(window,WM_SETREDRAW,FALSE,0);
    page=nextPage; morePage=page!=0;
    for (HWND child=GetWindow(window,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)) ShowWindow(child,SW_HIDE);
    const int main[]={FOLDER_PICK,PREVIEW_THEME,REFRESH_WALLPAPER,MODE_TIME,MODE_SOLAR,ENABLE,MORE,POSITION_CUSTOM};
    const int extra[]={STARTUP,PROJECT,ID_UI_CHINESE,ID_UI_ENGLISH,THEME_SYSTEM,THEME_LIGHT,THEME_DARK};
    if (page==0) { for (int id:main) ShowWindow(GetDlgItem(window,id),SW_SHOW); scheduleControls(); }
    if (page==1) for (int id:extra) ShowWindow(GetDlgItem(window,id),SW_SHOW);
    if (page) ShowWindow(GetDlgItem(window,BACK),SW_SHOW);
    if (page==2) { for (int id:{IMPORT_THEME,LIBRARY_PREV,LIBRARY_NEXT}) ShowWindow(GetDlgItem(window,id),SW_SHOW); refreshLibrary(); }
    if (page==3) { for (int id:{PHASE_PREV,PHASE_NEXT}) ShowWindow(GetDlgItem(window,id),SW_SHOW); preparePhases(); }
    if (page!=2) clearLibraryImages();
    if (page!=3&&phaseImage) { DeleteObject(phaseImage); phaseImage=nullptr; }
    EnableWindow(GetDlgItem(window,PREVIEW_THEME),chosenTheme.valid);
    EnableWindow(GetDlgItem(window,REFRESH_WALLPAPER),chosenTheme.valid);
    resizePage();
    if (visible) SendMessageW(window,WM_SETREDRAW,TRUE,0);
    SetFocus(GetDlgItem(window,page?BACK:FOLDER_PICK)); RedrawWindow(window,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);
}
static void openProject()
{
    ShellExecuteW(window,L"open",L"https://github.com/Milorgs/DaylightDesktop",nullptr,nullptr,SW_SHOWNORMAL);
}
static bool number(int id,double &out)
{
    WCHAR text[64],*end; GetDlgItemTextW(window,id,text,64);
    out=wcstod(text,&end); return end!=text && !*end && std::isfinite(out);
}
static bool clockValue(int id,int &out)
{
    WCHAR text[32]; GetDlgItemTextW(window,id,text,32);
    if (wcslen(text)!=5||text[2]!=L':') return false;
    const int digits[]={0,1,3,4};
    for (int i:digits) if (text[i]<L'0'||text[i]>L'9') return false;
    int hour=(text[0]-L'0')*10+text[1]-L'0',minute=(text[3]-L'0')*10+text[4]-L'0';
    if (hour>23||minute>59) return false;
    out=hour*60+minute; return true;
}
static void scheduleControls()
{
    bool solar=solarChoice;
    bool manual=solar&&!autoPositionChoice;
    if (morePage) return;
    struct Visibility { int id; bool visible; };
    const Visibility controls[]={{RISE,!solar},{SET,!solar},{LAT,manual},{LON,manual},{AUTO_LOCATION,solar&&autoPositionChoice}};
    HDWP batch=BeginDeferWindowPos(5);
    for (const Visibility &control:controls) {
        HWND child=GetDlgItem(window,control.id);
        bool visible=(GetWindowLongPtrW(child,GWL_STYLE)&WS_VISIBLE)!=0;
        if (visible==control.visible) continue;
        if (batch) batch=DeferWindowPos(batch,child,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW|(control.visible?SWP_SHOWWINDOW:SWP_HIDEWINDOW));
    }
    if (batch) EndDeferWindowPos(batch);
    else for (const Visibility &control:controls) ShowWindow(GetDlgItem(window,control.id),control.visible?SW_SHOW:SW_HIDE);
}
static void redrawSchedule()
{
    RECT area={px(16),px(170),px(444),px(352)};
    RedrawWindow(window,&area,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);
}
static void translateControls();
static void translateControls()
{
    struct Caption { int id; const WCHAR *cn,*en; };
    const Caption controls[]={
        {MORE,L"更多",L"More"},{BACK,L"返回",L"Back"},{FOLDER_PICK,L"选择主题",L"Themes"},{PREVIEW_THEME,L"分时预览",L"Preview"},{REFRESH_WALLPAPER,L"刷新壁纸",L"Refresh wallpaper"},{IMPORT_THEME,L"导入主题文件夹…",L"Import theme folder…"},
        {MODE_TIME,L"固定时间",L"Fixed time"},{MODE_SOLAR,L"自动位置",L"Automatic"},{POSITION_CUSTOM,L"自定义坐标",L"Coordinates"},
        {ENABLE,L"自动切换",L"Automatic switching"},{STARTUP,L"开机自启动",L"Run at sign-in"},{PROJECT,L"GitHub 项目",L"GitHub project"},
        {AUTO_LOCATION,L"刷新",L"Refresh"},{THEME_SYSTEM,L"跟随系统",L"System"},{THEME_LIGHT,L"浅色",L"Light"},{THEME_DARK,L"深色",L"Dark"}
    };
    for (const Caption &caption:controls) SetDlgItemTextW(window,caption.id,tr(caption.cn,caption.en));
    if (locating) SetDlgItemTextW(window,AUTO_LOCATION,tr(L"正在识别…",L"Locating…"));
    if (refreshingWallpaper) SetDlgItemTextW(window,REFRESH_WALLPAPER,tr(L"刷新中…",L"Refreshing…"));
    if (importing) SetDlgItemTextW(window,FOLDER_PICK,tr(L"正在导入…",L"Importing…"));
    if (status[0]) {
        const Caption notices[]={
            {0,L"请输入完整、有效的时间或坐标。",L"Enter valid times or coordinates."},
            {0,L"请先选择主题。",L"Choose a theme first."},
            {0,L"正在识别位置，完成后自动应用。",L"Detecting location. The theme will apply when ready."},
            {0,L"Windows 定位权限不可用，可选择自定义坐标。",L"Windows location is unavailable. Choose Coordinates to set a location."},
            {0,L"位置暂不可用，将保留上次位置；可选择自定义坐标。",L"Location unavailable. Keeping the previous location."}
        };
        for (const Caption &notice:notices) if (!lstrcmpW(status,notice.cn)||!lstrcmpW(status,notice.en)) { lstrcpynW(status,tr(notice.cn,notice.en),256); break; }
    }
    RedrawWindow(window,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);
}
static void changeLanguage(bool useChinese)
{
    ConfigLock lock; if (!lock.acquired) return;
    Settings settings=load(configPath); settings.chinese=useChinese;
    if (save(configPath,settings)) {
        chinese=useChinese; refreshFonts(); translateControls();
        HWND backgroundWindow=FindWindowW(WORKER_CLASS,nullptr); if (backgroundWindow) PostMessageW(backgroundWindow,WM_APP+9,0,0);
        if (solarChoice&&autoPositionChoice) identifyLocation();
    }
}
static void changeAppearance(int choice)
{
    ConfigLock lock; if (!lock.acquired) return;
    Settings settings=load(configPath); settings.appearance=choice;
    if (save(configPath,settings)) { appearance=choice; palette(); RedrawWindow(window,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_FRAME); }
}
static void applyUI(bool switchOnly)
{
    syncLocationMonitoring(window);
    ConfigLock lock; if (!lock.acquired) return;
    Settings s=load(configPath); lstrcpyW(s.folder,chosenFolder);
    lstrcpynW(s.themeName,chosenTheme.title,128);
    s.enabled=enabledChoice; s.solar=solarChoice; s.autoPosition=autoPositionChoice;
    Settings stored=load(configPath); s.rise=stored.rise; s.set=stored.set; s.lat=stored.lat; s.lon=stored.lon;
    bool valid=switchOnly?(!s.enabled||!s.solar||s.located):s.solar ? (autoPositionChoice?s.located:(number(LAT,s.lat)&&number(LON,s.lon)&&std::fabs(s.lat)<=89.9&&std::fabs(s.lon)<=180)) :
        clockValue(RISE,s.rise)&&clockValue(SET,s.set)&&s.rise!=s.set;
    if (!valid) {
        lstrcpyW(status,s.solar&&autoPositionChoice?tr(L"正在识别位置，完成后自动应用。",L"Detecting location. The theme will apply when ready."):tr(L"请输入完整、有效的时间或坐标。",L"Enter valid times or coordinates."));
        if (s.solar&&autoPositionChoice) { save(configPath,s); identifyLocation(); }
        redrawSchedule(); return;
    }
    if (s.enabled && !chosenTheme.valid) {
        enabledChoice=false; s.enabled=false;
        lstrcpyW(status,tr(L"请先选择主题。",L"Choose a theme first.")); InvalidateRect(GetDlgItem(window,ENABLE),nullptr,FALSE);
    }
    if (!switchOnly&&s.solar&&!autoPositionChoice) { s.manualLat=s.lat; s.manualLon=s.lon; s.manualKnown=true; s.located=valid; lstrcpyW(s.placeName,L"手动位置"); lstrcpyW(currentPlace,s.placeName); }
    Settings previous=load(configPath);
    if (!save(configPath,s)) { MessageBoxW(window,tr(L"无法保存设置。",L"Could not save settings."),L"Daylight Desktop",MB_OK|MB_ICONWARNING); return; }
    HWND backgroundWindow=FindWindowW(WORKER_CLASS,nullptr);
    if (backgroundWindow) PostMessageW(backgroundWindow,RELOAD,0,0);
    else if (s.enabled && !spawn(dryRun?L"--measure":L"--background")) {
        save(configPath,previous);
        MessageBoxW(window,tr(L"无法启动后台程序，已恢复原设置。",L"Could not start the scheduler. Previous settings were restored."),L"Daylight Desktop",MB_OK|MB_ICONWARNING); return;
    }
    bool hadStatus=status[0]!=0; status[0]=0;
    if (switchOnly) {
        InvalidateRect(GetDlgItem(window,ENABLE),nullptr,FALSE);
        if (hadStatus) { RECT helper={px(28),px(276),px(432),px(302)}; InvalidateRect(window,&helper,FALSE); }
    } else { updateCurrentPreview(); redrawSchedule(); }
}
static void receiveLocation(HWND target,Located *value)
{
    ConfigLock lock; if (!lock.acquired) { free(value); locating=false; return; }
    locating=false;
    if (!worker) { EnableWindow(GetDlgItem(target,AUTO_LOCATION),TRUE); SetDlgItemTextW(target,AUTO_LOCATION,tr(L"刷新",L"Refresh")); }
    Settings settings=load(configPath);
    if (settings.solar&&settings.autoPosition && value && SUCCEEDED(value->result)) {
        settings.lat=value->latitude; settings.lon=value->longitude; settings.located=true;
        lstrcpynW(settings.placeName,value->name[0]?value->name:tr(L"地名暂不可用",L"Place name unavailable"),160);
        save(configPath,settings); lstrcpyW(currentPlace,settings.placeName);
        if (worker) { scheduleWorker(target); HWND ui=FindWindowW(UI_CLASS,nullptr); if (ui) PostMessageW(ui,WM_APP+5,0,0); }
        else {
            initializingUI=true;
            WCHAR text[64]; swprintf(text,64,L"%.5f",settings.lat); SetDlgItemTextW(target,LAT,text);
            swprintf(text,64,L"%.5f",settings.lon); SetDlgItemTextW(target,LON,text); initializingUI=false;
            applyUI();
        }
    } else if (!worker&&autoPositionChoice) lstrcpyW(status,tr(L"位置暂不可用，将保留上次位置；可选择自定义坐标。",L"Location unavailable. Keeping the previous location."));
    free(value); InvalidateRect(target,nullptr,FALSE);
}
static void CALLBACK networkChanged(void *context,PMIB_IPINTERFACE_ROW,MIB_NOTIFICATION_TYPE)
{
    PostMessageW((HWND)context,WM_APP+4,0,0);
}
static void syncLocationMonitoring(HWND target)
{
    Settings settings=load(configPath);
    bool active=!dryRun&&(worker?settings.solar&&settings.autoPosition&&settings.enabled:solarChoice&&autoPositionChoice);
    if (active) {
        SetTimer(target,4,15*60*1000,nullptr);
        if (!networkNotification) NotifyIpInterfaceChange(AF_UNSPEC,networkChanged,target,FALSE,&networkNotification);
    } else {
        KillTimer(target,3); KillTimer(target,4);
        if (networkNotification) { CancelMibChangeNotify2(networkNotification); networkNotification=nullptr; }
    }
}
static void automaticRefresh(HWND target)
{
    Settings settings=load(configPath);
    if (settings.solar&&settings.autoPosition && (!worker||(settings.enabled&&settings.located)) && GetTickCount64()-lastLocationRequest>10000) identifyLocation(target);
}
static void finishPaint(HPAINTBUFFER buffer,int width,int height)
{
    RGBQUAD *bits; int stride;
    if (SUCCEEDED(GetBufferedPaintBits(buffer,&bits,&stride)))
        for (int y=0;y<height;++y) for (int x=0;x<width;++x) {
            RGBQUAD *pixel=&bits[y*stride+x];
            if (!pixel->rgbReserved&&(pixel->rgbRed||pixel->rgbGreen||pixel->rgbBlue)) pixel->rgbReserved=255;
        }
    EndBufferedPaint(buffer,TRUE);
}
static HPAINTBUFFER buttonPaintBuffer;
static void buttonContent(DRAWITEMSTRUCT *d)
{
    RECT r=d->rcItem;
    if (d->CtlID>=LIBRARY_TILE&&d->CtlID<LIBRARY_TILE+6) {
        FillRect(d->hDC,&r,background); int tile=d->CtlID-LIBRARY_TILE,index=libraryPage*6+tile;
        bool selected=index<(int)library.size()&&!lstrcmpiW(chosenFolder,library[index].manifest);
        smoothRoundRect(d->hDC,1,1,r.right-2,r.bottom-2,px(10),cardColor,selected?accentColor:borderColor,1);
        drawImage(d->hDC,libraryImages[tile],12,8,184,58);
        WCHAR title[128]; GetWindowTextW(d->hwndItem,title,128); RECT text={px(12),px(72),r.right-px(12),r.bottom-px(6)};
        SelectObject(d->hDC,font); SetBkMode(d->hDC,TRANSPARENT); SetTextColor(d->hDC,ink()); DrawTextW(d->hDC,title,-1,&text,DT_LEFT|DT_SINGLELINE|DT_END_ELLIPSIS);
        return;
    }
            bool card=d->CtlID==ENABLE||d->CtlID==FOLDER_PICK||d->CtlID==PREVIEW_THEME||d->CtlID==REFRESH_WALLPAPER||d->CtlID==MODE_TIME||d->CtlID==MODE_SOLAR||d->CtlID==POSITION_CUSTOM||d->CtlID==STARTUP||d->CtlID==AUTO_POSITION||d->CtlID==AUTO_LOCATION||d->CtlID==ID_UI_CHINESE||d->CtlID==ID_UI_ENGLISH||(d->CtlID>=THEME_SYSTEM&&d->CtlID<=THEME_DARK);
            FillRect(d->hDC,&r,card?surface:mica?(HBRUSH)GetStockObject(BLACK_BRUSH):background);
            if (buttonPaintBuffer&&(card||!mica)) BufferedPaintSetAlpha(buttonPaintBuffer,nullptr,255);
            if (d->CtlID==ENABLE || d->CtlID==STARTUP || d->CtlID==AUTO_POSITION) {
                bool checked=d->CtlID==ENABLE?enabledChoice:d->CtlID==STARTUP?startupChoice:autoPositionChoice;
                COLORREF track=checked?accentColor:(dark?RGB(94,94,99):RGB(185,190,199));
                int left=r.right-px(46),top=(r.bottom-px(24))/2;
                smoothRoundRect(d->hDC,left,top,px(46),px(24),px(12),track,track,0);
                smoothCircle(d->hDC,left+px(checked?25:4),top+px(3),px(18),RGB(255,255,255));
                WCHAR text[48]; GetWindowTextW(d->hwndItem,text,48); r.right=left-px(10);
                SetBkMode(d->hDC,TRANSPARENT); SetTextColor(d->hDC,ink()); SelectObject(d->hDC,font);
                DTTOPTS options={}; options.dwSize=sizeof(options); options.dwFlags=DTT_COMPOSITED|DTT_TEXTCOLOR; options.crText=ink();
                if (!textTheme||FAILED(DrawThemeTextEx(textTheme,d->hDC,0,0,text,-1,DT_LEFT|DT_VCENTER|DT_SINGLELINE,&r,&options))) DrawTextW(d->hDC,text,-1,&r,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
                return;
            }
            bool selected=(d->CtlID==MODE_TIME&&!solarChoice)||(d->CtlID==MODE_SOLAR&&solarChoice&&autoPositionChoice)||(d->CtlID==POSITION_CUSTOM&&solarChoice&&!autoPositionChoice)||(d->CtlID==ID_UI_CHINESE&&chinese)||(d->CtlID==ID_UI_ENGLISH&&!chinese)||(d->CtlID>=THEME_SYSTEM&&d->CtlID<=THEME_DARK&&(int)d->CtlID-THEME_SYSTEM==appearance);
            COLORREF fill=d->CtlID==SAVE?(accentColor):
                selected?(dark?RGB(48,66,87):RGB(232,240,254)):(cardColor);
            if (highContrast) fill=selected?GetSysColor(COLOR_HIGHLIGHT):cardColor;
            smoothRoundRect(d->hDC,1,1,r.right-2,r.bottom-2,px(8),fill,borderColor,1);
            WCHAR text[48]; GetWindowTextW(d->hwndItem,text,48); SetBkMode(d->hDC,TRANSPARENT);
            COLORREF color=d->CtlID==SAVE?(dark?RGB(20,35,50):RGB(255,255,255)):ink(); SetTextColor(d->hDC,color); SelectObject(d->hDC,font);
            if (highContrast&&selected) { color=GetSysColor(COLOR_HIGHLIGHTTEXT); SetTextColor(d->hDC,color); }
            if (d->CtlID==LIBRARY_PREV||d->CtlID==PHASE_PREV||d->CtlID==LIBRARY_NEXT||d->CtlID==PHASE_NEXT) smoothArrow(d->hDC,(r.right-px(18))/2,(r.bottom-px(18))/2,px(18),IsWindowEnabled(d->hwndItem)?color:mutedColor,(d->CtlID==LIBRARY_NEXT||d->CtlID==PHASE_NEXT)?2:0);
            else if (d->CtlID==BACK) smoothArrow(d->hDC,(r.right-px(18))/2,(r.bottom-px(18))/2,px(18),color,0);
            else { if (d->CtlID==MORE) lstrcpyW(text,L"•••"); DrawTextW(d->hDC,text,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE); }
            return;
        }
static void paintButton(DRAWITEMSTRUCT *item)
{
    HDC dc; BP_PAINTPARAMS parameters={}; parameters.cbSize=sizeof(parameters); parameters.dwFlags=BPPF_ERASE;
    HPAINTBUFFER buffer=BeginBufferedPaint(item->hDC,&item->rcItem,BPBF_TOPDOWNDIB,&parameters,&dc);
    DRAWITEMSTRUCT copy=*item; if (buffer) copy.hDC=dc;
    buttonPaintBuffer=buffer; buttonContent(&copy); buttonPaintBuffer=nullptr;
    if (buffer) finishPaint(buffer,item->rcItem.right,item->rcItem.bottom);
}
static LRESULT CALLBACK switchProcedure(HWND h,UINT message,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR)
{
    if (message==WM_ERASEBKGND) return 1;
    if (message==WM_PAINT||message==WM_PRINTCLIENT) {
        PAINTSTRUCT paint={}; HDC dc=message==WM_PAINT?BeginPaint(h,&paint):(HDC)w;
        DRAWITEMSTRUCT item={}; item.CtlType=ODT_BUTTON; item.CtlID=GetDlgCtrlID(h); item.itemAction=ODA_DRAWENTIRE;
        item.hwndItem=h; item.hDC=dc; GetClientRect(h,&item.rcItem);
        paintButton(&item);
        if (message==WM_PAINT) EndPaint(h,&paint);
        return 0;
    }
    if (message==WM_NCDESTROY) RemoveWindowSubclass(h,switchProcedure,2);
    return DefSubclassProc(h,message,w,l);
}
static LRESULT CALLBACK procedure(HWND h,UINT m,WPARAM w,LPARAM l)
{
    if (worker) {
        if (m==WM_CREATE) { window=h; chinese=load(configPath).chinese; syncLocationMonitoring(h); if (!dryRun) PostMessageW(h,WM_APP+4,0,0); scheduleWorker(h); return 0; }
        if (m==WM_APP+9) { chinese=load(configPath).chinese; return 0; }
        if (m==WM_APP+3) { receiveLocation(h,(Located*)l); return 0; }
        if (m==WM_APP+4) { Settings settings=load(configPath); if (settings.solar&&settings.autoPosition&&(!worker||settings.enabled)) SetTimer(h,3,2000,nullptr); return 0; }
        if (m==WM_TIMER && (w==3||w==4)) { if (w==3) KillTimer(h,3); automaticRefresh(h); return 0; }
        if (m==WM_TIMER||m==WM_TIMECHANGE||m==WM_SETTINGCHANGE||m==RELOAD||(m==WM_POWERBROADCAST && (w==PBT_APMRESUMEAUTOMATIC||w==PBT_APMRESUMESUSPEND))) {
            if (m==RELOAD) syncLocationMonitoring(h);
            /* A reload keeps the already applied image to avoid redundant wallpaper broadcasts. */
            if (m==WM_POWERBROADCAST&&!dryRun) automaticRefresh(h);
            scheduleWorker(h); return m==WM_POWERBROADCAST?TRUE:0;
        }
        if (m==WM_DESTROY) { KillTimer(h,1); PostQuitMessage(0); return 0; }
    } else {
        if (m==WM_CREATE) {
            window=h; Settings s=load(configPath); WCHAR text[64];
            chinese=s.chinese; appearance=s.appearance; refreshFonts(); palette(); enabledChoice=s.enabled; startupChoice=startupExists(); solarChoice=s.solar; autoPositionChoice=s.autoPosition; lstrcpyW(currentPlace,s.placeName);
            lstrcpyW(chosenFolder,s.folder); chosenTheme=readTheme(s.folder);
            if (s.themeName[0]) lstrcpynW(chosenTheme.title,s.themeName,128);
            if (chosenTheme.valid) updateCurrentPreview();
            ctrl(L"BUTTON",tr(L"更多",L"More"),BS_OWNERDRAW|WS_TABSTOP,408,12,36,30,MORE);
            ctrl(L"BUTTON",tr(L"选择主题",L"Themes"),BS_OWNERDRAW|WS_TABSTOP,152,110,136,30,FOLDER_PICK);
            ctrl(L"BUTTON",tr(L"固定时间",L"Fixed time"),BS_OWNERDRAW|WS_TABSTOP,304,180,128,30,MODE_TIME);
            ctrl(L"BUTTON",tr(L"自动位置",L"Automatic"),BS_OWNERDRAW|WS_TABSTOP,28,180,128,30,MODE_SOLAR);
            swprintf(text,64,L"%02d:%02d",s.rise/60,s.rise%60); ctrl(L"EDIT",text,ES_CENTER|ES_MULTILINE|WS_TABSTOP,32,238,186,28,RISE);
            swprintf(text,64,L"%02d:%02d",s.set/60,s.set%60); ctrl(L"EDIT",text,ES_CENTER|ES_MULTILINE|WS_TABSTOP,242,238,186,28,SET);
            swprintf(text,64,L"%.4f",(s.manualKnown?s.manualLat:s.lat)); ctrl(L"EDIT",text,ES_CENTER|ES_MULTILINE|WS_TABSTOP,32,238,186,28,LAT);
            swprintf(text,64,L"%.4f",(s.manualKnown?s.manualLon:s.lon)); ctrl(L"EDIT",text,ES_CENTER|ES_MULTILINE|WS_TABSTOP,242,238,186,28,LON);
            WCHAR savedLatitude[64]; GetPrivateProfileStringW(L"Schedule",L"Latitude",L"",savedLatitude,64,configPath);
            if (!savedLatitude[0]) { SetDlgItemTextW(h,LAT,L""); SetDlgItemTextW(h,LON,L""); }
            ctrl(L"BUTTON",tr(L"自动切换",L"Automatic switching"),BS_OWNERDRAW|WS_TABSTOP,28,319,404,28,ENABLE);
            ctrl(L"BUTTON",tr(L"开机自启动",L"Run at sign-in"),BS_OWNERDRAW|WS_TABSTOP,28,70,404,28,STARTUP);

            ctrl(L"BUTTON",tr(L"返回",L"Back"),BS_OWNERDRAW|WS_TABSTOP,16,14,34,28,BACK);
            ctrl(L"BUTTON",tr(L"GitHub 项目",L"GitHub project"),BS_OWNERDRAW|WS_TABSTOP,16,288,428,32,PROJECT);
            ctrl(L"BUTTON",tr(L"刷新",L"Refresh"),BS_OWNERDRAW|WS_TABSTOP,332,234,100,30,AUTO_LOCATION);
            ctrl(L"BUTTON",tr(L"自定义坐标",L"Coordinates"),BS_OWNERDRAW|WS_TABSTOP,166,180,128,30,POSITION_CUSTOM);
            ctrl(L"BUTTON",L"中文",BS_OWNERDRAW|WS_TABSTOP,184,136,100,30,ID_UI_CHINESE);
            ctrl(L"BUTTON",L"English",BS_OWNERDRAW|WS_TABSTOP,296,136,136,30,ID_UI_ENGLISH);
            ctrl(L"BUTTON",tr(L"跟随系统",L"System"),BS_OWNERDRAW|WS_TABSTOP,28,236,128,30,THEME_SYSTEM);
            ctrl(L"BUTTON",tr(L"浅色",L"Light"),BS_OWNERDRAW|WS_TABSTOP,166,236,128,30,THEME_LIGHT);
            ctrl(L"BUTTON",tr(L"深色",L"Dark"),BS_OWNERDRAW|WS_TABSTOP,304,236,128,30,THEME_DARK);
            ctrl(L"BUTTON",tr(L"分时预览",L"Preview"),BS_OWNERDRAW|WS_TABSTOP,300,110,132,30,PREVIEW_THEME);
            ctrl(L"BUTTON",tr(L"刷新壁纸",L"Refresh wallpaper"),BS_OWNERDRAW|WS_TABSTOP,300,76,132,26,REFRESH_WALLPAPER);
            ctrl(L"BUTTON",tr(L"导入主题文件夹…",L"Import theme folder…"),BS_OWNERDRAW|WS_TABSTOP,16,58,428,30,IMPORT_THEME);
            ctrl(L"BUTTON",L"",BS_OWNERDRAW|WS_TABSTOP,16,426,72,30,LIBRARY_PREV);
            ctrl(L"BUTTON",L"",BS_OWNERDRAW|WS_TABSTOP,372,426,72,30,LIBRARY_NEXT);
            ctrl(L"BUTTON",L"",BS_OWNERDRAW|WS_TABSTOP,16,311,44,30,PHASE_PREV);
            ctrl(L"BUTTON",L"",BS_OWNERDRAW|WS_TABSTOP,400,311,44,30,PHASE_NEXT);
            for (int tile=0;tile<6;++tile) ctrl(L"BUTTON",L"",BS_OWNERDRAW|WS_TABSTOP,16+(tile%2)*220,104+(tile/2)*106,208,96,LIBRARY_TILE+tile);
            showPage(0); scheduleControls(); initializingUI=false; syncLocationMonitoring(h); PostMessageW(h,WM_APP+4,0,0); PostMessageW(h,WM_APP+10,0,0); PostMessageW(h,WM_APP+11,0,0); return 0;
        }
        if (m==WM_COMMAND) {
            if (LOWORD(w)>=LIBRARY_TILE&&LOWORD(w)<LIBRARY_TILE+6) {
                int index=libraryPage*6+LOWORD(w)-LIBRARY_TILE;
                if (index<(int)library.size()) {
                    Theme selected=readTheme(library[index].manifest);
                    if (selected.valid) { lstrcpyW(chosenFolder,library[index].manifest); chosenTheme=selected; previewIndex=-1; enabledChoice=true; applyUI(); showPage(0); }
                }
                return 0;
            }
            switch (LOWORD(w)) {
            case FOLDER_PICK: showPage(2); break;
            case IMPORT_THEME: if (!importing) chooseFolder(); break;
            case PREVIEW_THEME: if (chosenTheme.valid) showPage(3); break;
            case REFRESH_WALLPAPER: refreshWallpaperNow(); break;
            case LIBRARY_PREV: case LIBRARY_NEXT: libraryPage+=LOWORD(w)==LIBRARY_PREV?-1:1; refreshLibrary(false); InvalidateRect(h,nullptr,FALSE); break;
            case PHASE_PREV: case PHASE_NEXT: if (!phases.empty()) { int count=(int)phases.size(); phasePosition=(phasePosition+(LOWORD(w)==PHASE_PREV?-1:1)+count)%count; updatePhaseImage(); } break;
            case AUTO_LOCATION: identifyLocation(); break;
            case MORE: showPage(true); break;
            case BACK: showPage(0); break;
            case PROJECT: openProject(); break;
            case ID_UI_CHINESE: changeLanguage(true); break;
            case ID_UI_ENGLISH: changeLanguage(false); break;
            case THEME_SYSTEM: case THEME_LIGHT: case THEME_DARK: changeAppearance(LOWORD(w)-THEME_SYSTEM); break;
            case MODE_TIME: case MODE_SOLAR: case POSITION_CUSTOM:
                if ((LOWORD(w)==MODE_TIME&&!solarChoice)||(LOWORD(w)==MODE_SOLAR&&solarChoice&&autoPositionChoice)||(LOWORD(w)==POSITION_CUSTOM&&solarChoice&&!autoPositionChoice)) break;
                if (LOWORD(w)==POSITION_CUSTOM) {
                    Settings remembered=load(configPath); initializingUI=true;
                    WCHAR text[64];
                    if (remembered.manualKnown||remembered.located) {
                        swprintf(text,64,L"%.5f",remembered.manualKnown?remembered.manualLat:remembered.lat); SetDlgItemTextW(h,LAT,text);
                        swprintf(text,64,L"%.5f",remembered.manualKnown?remembered.manualLon:remembered.lon); SetDlgItemTextW(h,LON,text);
                    } else { SetDlgItemTextW(h,LAT,L""); SetDlgItemTextW(h,LON,L""); }
                    initializingUI=false;
                }
                solarChoice=LOWORD(w)!=MODE_TIME; autoPositionChoice=LOWORD(w)==MODE_SOLAR;
                { ConfigLock lock; if (lock.acquired) { Settings settings=load(configPath); settings.autoPosition=autoPositionChoice; settings.solar=solarChoice; save(configPath,settings); } }
                scheduleControls(); applyUI();
                if (autoPositionChoice&&!lstrcmpW(currentPlace,L"手动位置")) currentPlace[0]=0;
                redrawSchedule();
                if (solarChoice&&autoPositionChoice) identifyLocation();
                break;
            case LAT: case LON: case RISE: case SET:
                if (!initializingUI && HIWORD(w)==EN_CHANGE) SetTimer(h,2,900,nullptr);
                if (!initializingUI && HIWORD(w)==EN_KILLFOCUS) { KillTimer(h,2); applyUI(); }
                break;
            case ENABLE: enabledChoice=!enabledChoice; applyUI(true); break;
            case STARTUP:
                if (startup(!startupChoice)==ERROR_SUCCESS) startupChoice=!startupChoice;
                else MessageBoxW(h,tr(L"无法更改开机自启动。",L"Could not change startup."),L"Daylight Desktop",MB_OK|MB_ICONWARNING);
                InvalidateRect(GetDlgItem(h,STARTUP),nullptr,FALSE); break;
            }
            return 0;
        }
        if (m==WM_APP+11) { Settings settings=load(configPath); if (settings.enabled&&chosenTheme.valid&&!FindWindowW(WORKER_CLASS,nullptr)) spawn(dryRun?L"--measure":L"--background"); return 0; }
        if (m==WM_APP+10) { migrateRememberedTheme(); return 0; }
        if (m==WM_APP+8) {
            ImportJob *job=(ImportJob*)l; importing=false; EnableWindow(GetDlgItem(h,FOLDER_PICK),TRUE);
            SetDlgItemTextW(h,FOLDER_PICK,tr(L"选择主题",L"Themes"));
            if (job&&job->success) {
                lstrcpyW(chosenFolder,job->manifest); chosenTheme=readTheme(chosenFolder);
                lstrcpynW(chosenTheme.title,job->name,128);
                previewIndex=-1; updateCurrentPreview();
                if (job->enableAfterImport) enabledChoice=true;
                applyUI(); showPage(0);
            } else if (job&&job->enableAfterImport) MessageBoxW(h,tr(L"文件夹中需要有效的 theme.json 和完整图片；之前的主题仍然保留。",L"This folder needs a valid theme.json and all referenced images. Your previous theme is retained."),tr(L"无法导入主题",L"Could not import theme"),MB_OK|MB_ICONINFORMATION);
            free(job); InvalidateRect(h,nullptr,FALSE); return 0;
        }
        if (m==WM_APP+3) { receiveLocation(h,(Located*)l); return 0; }
        if (m==WM_APP+4) { Settings settings=load(configPath); if (settings.solar&&settings.autoPosition&&(!worker||settings.enabled)) SetTimer(h,3,2000,nullptr); return 0; }
        if (m==WM_APP+5) {
            Settings settings=load(configPath); lstrcpyW(currentPlace,settings.placeName); updateCurrentPreview(); InvalidateRect(h,nullptr,FALSE); return 0;
        }
        if (m==WM_APP+13) {
            enabledChoice=false;
            lstrcpyW(status,tr(L"壁纸设置失败，已暂停自动切换。",L"Wallpaper could not be applied. Switching paused."));
            redrawSchedule(); return 0;
        }
        if (m==WM_APP+15) {
            RefreshJob *job=(RefreshJob*)l; refreshingWallpaper=false;
            SetDlgItemTextW(h,REFRESH_WALLPAPER,tr(L"刷新壁纸",L"Refresh wallpaper"));
            bool hadStatus=status[0]!=0;
            if (job&&SUCCEEDED(job->result)) { status[0]=0; previewIndex=-1; updateCurrentPreview(); WritePrivateProfileStringW(L"Status",L"LastRefreshError",nullptr,configPath); }
            else {
                lstrcpyW(status,tr(L"壁纸刷新失败，原壁纸已保留。",L"Refresh failed. The previous wallpaper was retained."));
                WCHAR code[16]; swprintf(code,16,L"%08lX",(unsigned long)(job?job->result:E_FAIL));
                WritePrivateProfileStringW(L"Status",L"LastRefreshError",code,configPath);
            }
            if (hadStatus||status[0]) { RECT helper={px(28),px(276),px(432),px(302)}; InvalidateRect(h,&helper,FALSE); }
            free(job); return 0;
        }
        if (m==WM_POWERBROADCAST&&(w==PBT_APMRESUMEAUTOMATIC||w==PBT_APMRESUMESUSPEND)) { updateCurrentPreview(); automaticRefresh(h); return TRUE; }
        if (m==WM_TIMER) {
            if (w==7) { updateCurrentPreview(); return 0; }
            if (w==2) { KillTimer(h,2); applyUI(); }
            if (w==3||w==4) { if (w==3) KillTimer(h,3); automaticRefresh(h); }
            return 0;
        }
        if (m==WM_DRAWITEM) { paintButton((DRAWITEMSTRUCT*)l); return TRUE; }
        if (m==WM_PAINT) {
            PAINTSTRUCT p; HDC screen=BeginPaint(h,&p),dc; RECT client; GetClientRect(h,&client);
            BP_PAINTPARAMS parameters={}; parameters.cbSize=sizeof(parameters); parameters.dwFlags=BPPF_ERASE;
            HPAINTBUFFER buffer=BeginBufferedPaint(screen,&client,BPBF_TOPDOWNDIB,&parameters,&dc);
            if (!buffer) dc=screen;
            compositedPaint=buffer&&mica;
            FillRect(dc,&client,compositedPaint?(HBRUSH)GetStockObject(BLACK_BRUSH):background);
            COLORREF card=cardColor,muted=mutedColor,border=borderColor;
            if (page==2) {
                label(dc,tr(L"主题库",L"Theme library"),64,12,360,30,heading,ink());
                WCHAR count[64]; swprintf(count,64,tr(L"%d 个主题 · %d / %d",L"%d themes · %d / %d"),(int)library.size(),libraryPage+1,std::max(1,((int)library.size()+5)/6));
                label(dc,count,128,114+libraryRows*106,232,22,small,muted);
                if (library.empty()) label(dc,tr(L"导入一个主题文件夹，之后可在这里重新选择。",L"Import a theme folder to keep it in your library."),28,140,404,44,font,muted);
            } else if (page==3) {
                label(dc,tr(L"分时预览",L"Theme preview"),64,12,360,30,heading,ink());
                drawImage(dc,phaseImage,16,58,428,241);
                if (!phases.empty()) {
                    const WCHAR *cn[]={L"晨光",L"白天",L"黄昏",L"夜晚"},*en[]={L"Sunrise",L"Day",L"Sunset",L"Night"};
                    const PhaseEntry &entry=phases[phasePosition]; WCHAR caption[128];
                    swprintf(caption,128,L"%02d:%02d · %ls",entry.minute/60,entry.minute%60,chinese?cn[entry.group]:en[entry.group]);
                    label(dc,caption,80,315,244,22,font,ink());
                    swprintf(caption,128,L"%d / %d",phasePosition+1,(int)phases.size()); label(dc,caption,332,317,60,20,small,muted);
                }
            } else if (page==1) {
                label(dc,tr(L"更多",L"More"),64,12,360,30,heading,ink());
                smoothRoundRect(dc,px(16),px(58),px(428),px(52),px(12),card,card,0);
                smoothRoundRect(dc,px(16),px(122),px(428),px(60),px(12),card,card,0);
                label(dc,tr(L"界面语言",L"Language"),28,141,146,22,font,ink());
                smoothRoundRect(dc,px(16),px(194),px(428),px(82),px(12),card,card,0);
                label(dc,tr(L"外观",L"Appearance"),28,204,404,20,font,ink());
                label(dc,tr(L"Daylight Desktop · 1.0",L"Daylight Desktop · 1.0"),28,336,404,20,small,muted);
            } else {
                smoothRoundRect(dc,px(16),px(314),px(428),px(38),px(12),card,card,0);
                label(dc,L"Daylight Desktop",16,12,380,30,heading,ink());
                smoothRoundRect(dc,px(16),px(58),px(428),px(100),px(12),card,card,0);
                WCHAR theme[128];
                if (chosenTheme.valid) swprintf(theme,128,tr(L"壁纸主题 · %d 阶段",L"Theme · %d frames"),chosenTheme.frames.count);
                else lstrcpyW(theme,tr(L"壁纸主题",L"Wallpaper theme"));
                label(dc,theme,28,66,112,20,small,muted,DT_LEFT|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
                label(dc,chosenTheme.valid?chosenTheme.title:tr(L"未选择主题",L"No theme selected"),152,76,136,22,font,ink(),DT_LEFT|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
                if (preview) {
                    HDC image=CreateCompatibleDC(dc); HGDIOBJ old=SelectObject(image,preview);
                    BLENDFUNCTION blend={AC_SRC_OVER,0,255,AC_SRC_ALPHA}; AlphaBlend(dc,px(28),px(88),px(104),px(60),image,0,0,px(104),px(60),blend);
                    SelectObject(image,old); DeleteDC(image);
                } else DrawIconEx(dc,px(54),px(92),(HICON)LoadImageW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(1),IMAGE_ICON,px(52),px(52),LR_SHARED),px(52),px(52),0,nullptr,DI_NORMAL);
                smoothRoundRect(dc,px(16),px(170),px(428),px(132),px(12),card,card,0);
                if (solarChoice&&autoPositionChoice) {
                    label(dc,currentPlace[0]?currentPlace:locating?tr(L"正在识别位置…",L"Detecting location…"):tr(L"等待识别位置",L"Waiting for location"),28,238,280,26,font,ink());
                    if (!status[0]) label(dc,tr(L"随所在地自动更新 · 联网查询地名",L"Follows your location · online place lookup"),28,278,404,20,small,muted);
                } else {
                    smoothRoundRect(dc,px(28),px(236),px(194),px(32),px(7),card,border,1);
                    smoothRoundRect(dc,px(238),px(236),px(194),px(32),px(7),card,border,1);
                    label(dc,solarChoice?tr(L"纬度",L"Latitude"):tr(L"晨光",L"Day starts"),36,216,178,18,small,muted);
                    label(dc,solarChoice?tr(L"经度",L"Longitude"):tr(L"夜色",L"Night starts"),246,216,178,18,small,muted);
                    if (!status[0]) label(dc,solarChoice?tr(L"北纬、东经为正；南纬、西经为负。",L"North / east: positive; south / west: negative."):tr(L"24 小时制 · 输入完成后自动应用",L"24-hour time · changes apply automatically"),28,278,404,20,small,muted);
                }
                if (status[0]) label(dc,status,28,276,404,26,small,muted);
            }
            if (buffer) { if (!mica) BufferedPaintSetAlpha(buffer,nullptr,255); finishPaint(buffer,client.right,client.bottom); }
            compositedPaint=false; EndPaint(h,&p); return 0;
        }
        if (m==WM_ERASEBKGND) return 1;
        if (m==WM_CTLCOLOREDIT||m==WM_CTLCOLORSTATIC||m==WM_CTLCOLORBTN) { SetTextColor((HDC)w,IsWindowEnabled((HWND)l)?ink():RGB(125,130,142)); SetBkColor((HDC)w,cardColor); return (LRESULT)surface; }
        if (m==WM_TIMECHANGE) { updateCurrentPreview(); return 0; }
        if (m==WM_SETTINGCHANGE||m==WM_THEMECHANGED) {
            bool theme=m==WM_THEMECHANGED||(l&&(!lstrcmpiW((const WCHAR*)l,L"ImmersiveColorSet")||!lstrcmpiW((const WCHAR*)l,L"WindowsThemeElement")))||w==SPI_SETHIGHCONTRAST;
            if (theme) {
                bool changed=palette();
                if (m==WM_THEMECHANGED) { if (textTheme) CloseThemeData(textTheme); textTheme=OpenThemeData(h,L"WINDOW"); }
                if (changed) RedrawWindow(h,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);
            }
            else if (w==SPI_SETDESKWALLPAPER) updateCurrentPreview();
            return 0;
        }
        if (m==WM_DESTROY) { PostQuitMessage(0); return 0; }
    }
    return DefWindowProcW(h,m,w,l);
}
static int selfTest()
{
    int failures=0;
    if (!fixedPlan(420,420,1140).day||fixedPlan(1140,420,1140).day||fixedPlan(419,420,1140).day) ++failures;
    if (!fixedPlan(60,1320,360).day||fixedPlan(720,1320,360).day) ++failures;
    Plan p=solarPlan(720,80,false,0,0,0);
    if (p.sunrise<350||p.sunrise>380||p.sunset<1080||p.sunset>1110||!p.day) ++failures;
    if (solarPlan(720,172,false,80,0,0).polar!=1||solarPlan(720,355,false,80,0,0).polar!=-1) ++failures;
    WCHAR temp[MAX_PATH],file[MAX_PATH]; GetTempPathW(MAX_PATH,temp); swprintf(file,MAX_PATH,L"%lsDaylight-%lu.ini",temp,GetCurrentProcessId());
    Settings s; s.solar=true; s.enabled=true; s.lat=31.23; s.lon=121.47; lstrcpyW(s.day,L"C:\\壁纸\\day.jpg"); lstrcpyW(s.folder,L"C:\\壁纸\\主题");
    if (!save(file,s)) ++failures;
    Settings copy=load(file);
    if (!copy.solar||!copy.enabled||std::fabs(copy.lat-s.lat)>0.00001||wcscmp(copy.day,s.day)||wcscmp(copy.folder,s.folder)) ++failures;
    s.manualKnown=true; s.manualLat=12.3; s.manualLon=-45.6;
    if (!save(file,s)) ++failures;
    copy=load(file);
    if (!copy.manualKnown||std::fabs(copy.manualLat-12.3)>0.00001||std::fabs(copy.manualLon+45.6)>0.00001) ++failures;
    for (int minute=0;minute<1440;++minute) {
        int index=frameAt(minute,16,360,1080), next=nextFrameMinutes(minute,16,360,1080);
        if (index<0||index>=16||next<1||next>1440||frameAt(normalizeMinute(minute+next),16,360,1080)==index) ++failures;
    }
    if (frameAt(360,16,360,1080)!=0||frameAt(1080,16,360,1080)!=8||frameAt(0,16,360,1080)!=12) ++failures;
    if (frameAt(0,5,1320,360)!=0||frameAt(120,5,1320,360)!=1||frameAt(360,5,1320,360)!=3||frameAt(720,16,-1,-1,true)!=8) ++failures;
    WCHAR folder[MAX_PATH]; swprintf(folder,MAX_PATH,L"%lsDaylightFrames-%lu",temp,GetCurrentProcessId());
    if (!CreateDirectoryW(folder,nullptr)) ++failures;
    const WCHAR *names[]={L"phase10.jpg",L"phase2.jpg",L"phase1.png"};
    for (const WCHAR *name:names) {
        WCHAR image[MAX_PATH]; swprintf(image,MAX_PATH,L"%ls\\%ls",folder,name);
        HANDLE created=CreateFileW(image,GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if (created==INVALID_HANDLE_VALUE) ++failures; else CloseHandle(created);
    }
    Frames frames=readFrames(folder);
    if (frames.count!=3||frames.overflow||lstrcmpW(PathFindFileNameW(frames.files[0]),L"phase1.png")||lstrcmpW(PathFindFileNameW(frames.files[1]),L"phase2.jpg")) ++failures;
    for (const WCHAR *name:names) { WCHAR image[MAX_PATH]; swprintf(image,MAX_PATH,L"%ls\\%ls",folder,name); DeleteFileW(image); }
    RemoveDirectoryW(folder);
    const char *definition="{\"imageFilename\":\"phase_*.jpg\",\"dayImageList\":[8,3],\"nightImageList\":[1],\"sunriseImageList\":[4],\"sunsetImageList\":[2]}";
    Theme theme;
    if (!parseTheme(definition,L"C:\\test",theme,false)||theme.lengths[1]!=2||theme.lengths[3]!=1||lstrcmpW(PathFindFileNameW(theme.frames.files[theme.groups[1][0]]),L"phase_8.jpg")) ++failures;
    int phases[]={360,420,1080,1140};
    if (themeFrame(theme,420,phases,0)!=theme.groups[1][0]||themeFrame(theme,800,phases,0)!=theme.groups[1][1]||themeFrame(theme,1200,phases,0)!=theme.groups[3][0]) ++failures;
    // Compare direct boundaries against the original minute-by-minute oracle.
    const char *variants[]={definition,
        "{\"imageFilename\":\"*.jpg\",\"dayImageList\":[1,1,2],\"nightImageList\":[2,2,1]}",
        "{\"imageFilename\":\"*.jpg\",\"sunriseImageList\":[1,2,1],\"dayImageList\":[2,1],\"sunsetImageList\":[1,2],\"nightImageList\":[2,1]}"};
    const int schedules[][4]={{360,420,1080,1140},{1260,1320,300,360},{360,360,1080,1080},{420,421,1080,1081}};
    for (const char *variant:variants) {
        Theme sample; if (!parseTheme(variant,L"C:\\test",sample,false)) { ++failures; continue; }
        for (const auto &starts:schedules) for (int polar=-1;polar<=1;++polar) for (int minute=0;minute<1440;++minute) {
            int current=themeFrame(sample,minute,starts,polar),expected=1440;
            for (int next=1;next<=1440;++next) if (themeFrame(sample,normalizeMinute(minute+next),starts,polar)!=current) { expected=next; break; }
            if (themeNextMinutes(sample,minute,starts,polar)!=expected) ++failures;
        }
    }
    if (parseTheme("{\"imageFilename\":\"../*.jpg\",\"dayImageList\":[1],\"nightImageList\":[2]}",L"C:\\test",theme,false)||safeArchiveName("../theme.json")||safeArchiveName("C:theme.json")) ++failures;
    WCHAR name[160];
    if (!placeFromJson("{\"city\":\"Example City\",\"principalSubdivision\":\"Example Region\"}",name)||lstrcmpW(name,L"Example Region · Example City")) ++failures;
    if (!placeFromJson("{\"city\":\"\",\"locality\":\"Example Town\"}",name)||lstrcmpW(name,L"Example Town")) ++failures;
    if (placeFromJson("{\"city\":null}",name)) ++failures;
    DeleteFileW(file); printf("Schedule and persistence: %s\n",failures?"FAIL":"PASS"); return failures?1:0;
}
static int probeDesktop()
{
    HRESULT initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if (FAILED(initialized)) return 1;
    IDesktopWallpaper *desktop=nullptr;
    HRESULT result=CoCreateInstance(CLSID_DesktopWallpaper,nullptr,CLSCTX_ALL,IID_IDesktopWallpaper,(void**)&desktop);
    UINT count=0;
    if (SUCCEEDED(result)) { result=desktop->GetMonitorDevicePathCount(&count); desktop->Release(); }
    printf("Windows wallpaper interface: %s; monitors=%u\n",SUCCEEDED(result)?"PASS":"FAIL",count);
    CoUninitialize(); return SUCCEEDED(result)?0:1;
}
static int verifyThemeCommand(PWSTR args)
{
    int count=0; WCHAR **argv=CommandLineToArgvW(GetCommandLineW(),&count);
    if (!argv||count!=3) { if (argv) LocalFree(argv); return 1; }
    bool archive=!wcscmp(args,L"--verify-package") || !lstrcmpiW(PathFindExtensionW(argv[2]),L".ddw") || !lstrcmpiW(PathFindExtensionW(argv[2]),L".zip");
    WCHAR manifest[MAX_PATH]; lstrcpynW(manifest,argv[2],MAX_PATH);
    if (archive) {
        WCHAR root[MAX_PATH]; lstrcpyW(root,exePath); PathRemoveFileSpecW(root); PathRemoveFileSpecW(root); lstrcatW(root,L"\\_local\\import-check"); CreateDirectoryW(root,nullptr);
        if (!importArchive(argv[2],root,manifest)) { LocalFree(argv); printf("Theme import: FAIL\n"); return 1; }
    }
    Theme theme=readTheme(manifest); LocalFree(argv);
    printf("Theme: %s; images=%d; sunrise=%d day=%d sunset=%d night=%d\n",theme.valid?"PASS":"FAIL",theme.frames.count,theme.lengths[0],theme.lengths[1],theme.lengths[2],theme.lengths[3]);
    return theme.valid?0:1;
}
static int probeLocation()
{
    RoInitialize(RO_INIT_MULTITHREADED);
    GeoLocator *location=nullptr;
    HRESULT result=createLocator(&location); int state=-1;
    if (SUCCEEDED(result)) { result=location->LocationStatus(&state); location->Release(); }
    printf("Location provider: hr=%08lx status=%d\n",(unsigned long)result,(int)state);
    RoUninitialize(); return SUCCEEDED(result)?0:1;
}
static int cacheFolderCommand()
{
    int count=0; WCHAR **argv=CommandLineToArgvW(GetCommandLineW(),&count);
    if (!argv||count!=4||wcslen(argv[2])>=MAX_PATH||wcslen(argv[3])>=MAX_PATH) { if (argv) LocalFree(argv); return 1; }
    WCHAR manifest[MAX_PATH],name[128]; bool success=cacheThemeFolder(argv[2],argv[3],manifest,name); LocalFree(argv);
    if (!success) { printf("Folder import: FAIL\n"); return 1; }
    char utf8[MAX_PATH*4]; WideCharToMultiByte(CP_UTF8,0,manifest,-1,utf8,sizeof(utf8),nullptr,nullptr);
    printf("Manifest=%s\n",utf8); return 0;
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR args,int show)
{
    if (!GetModuleFileNameW(nullptr,exePath,MAX_PATH)||!GetEnvironmentVariableW(L"LOCALAPPDATA",configPath,MAX_PATH)) return 1;
    if (wcslen(configPath)+40>=MAX_PATH) return 1;
    lstrcatW(configPath,L"\\DaylightDesktop"); CreateDirectoryW(configPath,nullptr); lstrcatW(configPath,L"\\settings.ini");
    if (!wcscmp(args,L"--location-helper")) return locationHelper(false);
    if (!wcscmp(args,L"--location-helper-test")) return locationHelper(true);
    if (!wcscmp(args,L"--test-location-process")) return locationProcessTest();
    if (!wcscmp(args,L"--self-test")) return selfTest();
    if (!wcscmp(args,L"--probe-desktop")) return probeDesktop();
    if (!wcscmp(args,L"--probe-location")) return probeLocation();
    if (!wcsncmp(args,L"--verify-theme ",15)) return verifyThemeCommand(args);
    if (!wcsncmp(args,L"--cache-folder ",15)) return cacheFolderCommand();
    worker=!wcscmp(args,L"--background")||!wcscmp(args,L"--measure"); lightPreview=!wcscmp(args,L"--ui-test-light"); dryRun=!wcscmp(args,L"--measure")||!wcscmp(args,L"--ui-test")||lightPreview;
    HANDLE mutex=CreateMutexW(nullptr,FALSE,worker?L"Local\\Milorgs.DaylightDesktop.Worker":L"Local\\Milorgs.DaylightDesktop.Settings");
    if (!mutex) return 1;
    if (GetLastError()==ERROR_ALREADY_EXISTS) {
        HWND h=FindWindowW(worker?WORKER_CLASS:UI_CLASS,nullptr);
        if (h&&!worker) { ShowWindow(h,SW_RESTORE); SetForegroundWindow(h); }
        CloseHandle(mutex); return 0;
    }
    if (!worker) {
        RoInitialize(RO_INIT_SINGLETHREADED);
        SetProcessDPIAware(); HDC dc=GetDC(nullptr); dpi=GetDeviceCaps(dc,LOGPIXELSX); ReleaseDC(nullptr,dc);
        chinese=load(configPath).chinese; refreshFonts();
        if (!initDrawing()) { CloseHandle(mutex); return 1; } BufferedPaintInit(); palette();
    }
    WNDCLASSW wc={}; wc.lpfnWndProc=procedure; wc.hInstance=instance; wc.lpszClassName=worker?WORKER_CLASS:UI_CLASS;
    if (!worker) { wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(1)); wc.hCursor=LoadCursorW(nullptr,IDC_ARROW); } RegisterClassW(&wc);
    DWORD style=worker?WS_POPUP:WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_CLIPCHILDREN;
    RECT r={0,0,px(460),px(368)}; AdjustWindowRect(&r,style,FALSE);
    // Compose the parent and native child controls together, including visibility changes.
    HWND h=CreateWindowExW(worker?0:WS_EX_COMPOSITED,wc.lpszClassName,L"Daylight Desktop",style,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,nullptr,nullptr,instance,nullptr);
    if (!h) return 1;
    if (!worker) { window=h; palette(); int corners=2; DwmSetWindowAttribute(h,33,&corners,sizeof(corners)); ShowWindow(h,show); SetFocus(GetDlgItem(h,FOLDER_PICK)); }

    MSG msg; while (GetMessageW(&msg,nullptr,0,0)>0) { if (worker||!IsDialogMessageW(h,&msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); } }
    if (networkNotification) CancelMibChangeNotify2(networkNotification);
    if (!worker) RoUninitialize();
    if (!worker) { if (textTheme) CloseThemeData(textTheme); BufferedPaintUnInit(); if (preview) DeleteObject(preview); clearLibraryImages(); if (phaseImage) DeleteObject(phaseImage); closeDrawing(); DeleteObject(font); DeleteObject(small); DeleteObject(heading); DeleteObject(background); DeleteObject(surface); }
    CloseHandle(mutex); return 0;
}
