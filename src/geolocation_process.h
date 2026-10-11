#pragma once
// Short-lived use of the same EXE keeps location-provider DLLs out of the scheduler.
struct LocationQuery { DWORD magic=0x444C5101; bool chinese=true; PlaceCache cache; };
struct LocationReply { DWORD magic=0x444C5201; Located value; PlaceCache cache; };
static bool pipeRead(HANDLE pipe,void *data,DWORD bytes)
{
    BYTE *next=(BYTE*)data;
    while (bytes) { DWORD count=0; if (!ReadFile(pipe,next,bytes,&count,nullptr)||!count) return false; bytes-=count; next+=count; }
    return true;
}
static bool pipeWrite(HANDLE pipe,const void *data,DWORD bytes)
{
    const BYTE *next=(const BYTE*)data;
    while (bytes) { DWORD count=0; if (!WriteFile(pipe,next,bytes,&count,nullptr)||!count) return false; bytes-=count; next+=count; }
    return true;
}
static int locationHelper(bool test)
{
    LocationQuery query;
    HANDLE input=GetStdHandle(STD_INPUT_HANDLE),output=GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetFileType(input)!=FILE_TYPE_PIPE||GetFileType(output)!=FILE_TYPE_PIPE||!pipeRead(input,&query,sizeof(query))||query.magic!=0x444C5101) return 1;
    query.cache.name[159]=0; placeCache=query.cache;
    LocationReply reply;
    if (test) {
        reply.value.latitude=12.5; reply.value.longitude=-45.5; reply.value.result=S_OK;
        lstrcpyW(reply.value.name,query.chinese?L"示例城市":L"Example city");
    } else {
        LocationRequest *request=(LocationRequest*)calloc(1,sizeof(LocationRequest));
        if (!request) return 1;
        request->chinese=query.chinese; locateOnce(request,&reply.value);
    }
    reply.cache=placeCache;
    return pipeWrite(output,&reply,sizeof(reply))?0:1;
}
static bool locationRoundTrip(bool chinese,const PlaceCache &cache,LocationReply &reply,bool test=false)
{
    HANDLE inputRead=nullptr,inputWrite=nullptr,outputRead=nullptr,outputWrite=nullptr,job=nullptr;
    PROCESS_INFORMATION process={}; STARTUPINFOEXW startup={}; startup.StartupInfo.cb=sizeof(startup);
    LPPROC_THREAD_ATTRIBUTE_LIST attributes=nullptr;
    SECURITY_ATTRIBUTES security={sizeof(security),nullptr,TRUE}; bool ok=false;
    do {
        if (!CreatePipe(&inputRead,&inputWrite,&security,4096)||!CreatePipe(&outputRead,&outputWrite,&security,4096)) break;
        if (!SetHandleInformation(inputWrite,HANDLE_FLAG_INHERIT,0)||!SetHandleInformation(outputRead,HANDLE_FLAG_INHERIT,0)) break;
        SIZE_T size=0; InitializeProcThreadAttributeList(nullptr,1,0,&size);
        attributes=(LPPROC_THREAD_ATTRIBUTE_LIST)malloc(size);
        if (!attributes||!InitializeProcThreadAttributeList(attributes,1,0,&size)) { free(attributes); attributes=nullptr; break; }
        HANDLE inherited[]={inputRead,outputWrite};
        if (!UpdateProcThreadAttribute(attributes,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherited,sizeof(inherited),nullptr,nullptr)) break;
        startup.lpAttributeList=attributes;
        startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW; startup.StartupInfo.wShowWindow=SW_HIDE;
        startup.StartupInfo.hStdInput=inputRead; startup.StartupInfo.hStdOutput=outputWrite; startup.StartupInfo.hStdError=outputWrite;
        job=CreateJobObjectW(nullptr,nullptr); JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits={}; limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!job||!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits))) break;
        WCHAR command[MAX_PATH+64]; swprintf(command,MAX_PATH+64,L"\"%ls\" --location-helper%ls",exePath,test?L"-test":L"");
        if (!CreateProcessW(exePath,command,nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED|EXTENDED_STARTUPINFO_PRESENT,nullptr,nullptr,&startup.StartupInfo,&process)) break;
        if (!AssignProcessToJobObject(job,process.hProcess)) break;
        CloseHandle(inputRead); inputRead=nullptr; CloseHandle(outputWrite); outputWrite=nullptr;
        LocationQuery query; query.chinese=chinese; query.cache=cache;
        if (ResumeThread(process.hThread)==(DWORD)-1||!pipeWrite(inputWrite,&query,sizeof(query))) break;
        CloseHandle(inputWrite); inputWrite=nullptr;
        if (WaitForSingleObject(process.hProcess,60000)!=WAIT_OBJECT_0) break;
        DWORD code=1; if (!GetExitCodeProcess(process.hProcess,&code)||code) break;
        if (!pipeRead(outputRead,&reply,sizeof(reply))||reply.magic!=0x444C5201) break;
        reply.value.name[159]=0; reply.cache.name[159]=0;
        if (SUCCEEDED(reply.value.result)&&(!std::isfinite(reply.value.latitude)||!std::isfinite(reply.value.longitude)||std::fabs(reply.value.latitude)>89.9||std::fabs(reply.value.longitude)>180)) break;
        ok=true;
    } while (false);
    if (process.hProcess) {
        if (WaitForSingleObject(process.hProcess,0)==WAIT_TIMEOUT) TerminateProcess(process.hProcess,1);
        CloseHandle(process.hThread); CloseHandle(process.hProcess);
    }
    if (job) CloseHandle(job);
    if (attributes) { DeleteProcThreadAttributeList(attributes); free(attributes); }
    for (HANDLE handle:{inputRead,inputWrite,outputRead,outputWrite}) if (handle) CloseHandle(handle);
    return ok;
}
static DWORD WINAPI locationProcessThread(void *context)
{
    LocationRequest *request=(LocationRequest*)context; HWND target=request->target;
    LocationReply reply;
    if (!locationRoundTrip(request->chinese,placeCache,reply)) return locateThread(context);
    placeCache=reply.cache; free(request);
    Located *value=(Located*)malloc(sizeof(Located));
    if (value) *value=reply.value;
    if (!PostMessageW(target,WM_APP+3,0,(LPARAM)value)) free(value);
    return 0;
}
static int locationProcessTest()
{
    PlaceCache cache; cache.latitude=1.25; cache.longitude=-2.5; cache.chinese=true; lstrcpyW(cache.name,L"已缓存的地名");
    LocationReply reply;
    bool ok=locationRoundTrip(false,cache,reply,true)&&SUCCEEDED(reply.value.result)&&reply.value.latitude==12.5&&reply.value.longitude==-45.5&&
        !lstrcmpW(reply.value.name,L"Example city")&&!lstrcmpW(reply.cache.name,cache.name)&&reply.cache.latitude==cache.latitude&&reply.cache.longitude==cache.longitude&&reply.cache.chinese==cache.chinese;
    PlaceCache original=placeCache; placeCache=reply.cache; WCHAR reused[160];
    ok=ok&&reversePlace(cache.latitude+0.01,cache.longitude+0.01,reused,cache.chinese)&&!lstrcmpW(reused,cache.name);
    placeCache=original;
    printf("Location process and cache: %s\n",ok?"PASS":"FAIL"); return ok?0:1;
}
