#pragma once
#include "theme.h"
inline bool findFolderTheme(const WCHAR *source,WCHAR manifest[MAX_PATH])
{
    if (wcslen(source)+12>=MAX_PATH) return false;
    swprintf(manifest,MAX_PATH,L"%ls\\theme.json",source);
    if (readTheme(manifest).valid) return true;
    WCHAR search[MAX_PATH]; swprintf(search,MAX_PATH,L"%ls\\*.json",source);
    WIN32_FIND_DATAW entry; HANDLE find=FindFirstFileW(search,&entry); int matches=0;
    if (find==INVALID_HANDLE_VALUE) return false;
    do {
        if (!(entry.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&wcslen(source)+wcslen(entry.cFileName)+2<MAX_PATH) {
            WCHAR candidate[MAX_PATH]; swprintf(candidate,MAX_PATH,L"%ls\\%ls",source,entry.cFileName);
            if (readTheme(candidate).valid) { ++matches; lstrcpyW(manifest,candidate); }
        }
    } while (FindNextFileW(find,&entry));
    FindClose(find); return matches==1;
}
inline void discardImportedFolder(const WCHAR *folder)
{
    /* Only used for the flat directory created by this import attempt. */
    WCHAR pattern[MAX_PATH]; swprintf(pattern,MAX_PATH,L"%ls\\*",folder);
    WIN32_FIND_DATAW entry; HANDLE find=FindFirstFileW(pattern,&entry);
    if (find!=INVALID_HANDLE_VALUE) {
        do { if (!(entry.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)) { WCHAR file[MAX_PATH]; swprintf(file,MAX_PATH,L"%ls\\%ls",folder,entry.cFileName); DeleteFileW(file); } } while (FindNextFileW(find,&entry));
        FindClose(find);
    }
    RemoveDirectoryW(folder);
}
inline bool cacheThemeFolder(const WCHAR *source,const WCHAR *root,WCHAR manifest[MAX_PATH],WCHAR name[128])
{
    WCHAR input[MAX_PATH]; if (!findFolderTheme(source,input)) return false;
    Theme theme=readTheme(input); if (!theme.valid) return false;
    WCHAR absoluteRoot[MAX_PATH];
    DWORD resolved=GetFullPathNameW(root,MAX_PATH,absoluteRoot,nullptr);
    if (!resolved||resolved>=MAX_PATH||wcslen(absoluteRoot)+58>=MAX_PATH) return false;
    if (!CreateDirectoryW(absoluteRoot,nullptr)&&GetLastError()!=ERROR_ALREADY_EXISTS) return false;
    WCHAR output[MAX_PATH]; swprintf(output,MAX_PATH,L"%ls\\theme-%lu-%llu",absoluteRoot,GetCurrentProcessId(),GetTickCount64());
    if (!CreateDirectoryW(output,nullptr)) return false;
    bool ok=true; unsigned long long total=0;
    for (int i=0;ok&&i<theme.frames.count;++i) {
        HANDLE file=CreateFileW(theme.frames.files[i],GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        LARGE_INTEGER size={};
        ok=file!=INVALID_HANDLE_VALUE&&GetFileSizeEx(file,&size)&&size.QuadPart<=128LL*1024*1024;
        if (file!=INVALID_HANDLE_VALUE) CloseHandle(file);
        total+=(unsigned long long)size.QuadPart; if (total>2ULL*1024*1024*1024) ok=false;
        const WCHAR *base=PathFindFileNameW(theme.frames.files[i]); WCHAR destination[MAX_PATH];
        if (wcslen(output)+wcslen(base)+2>=MAX_PATH) { ok=false; break; }
        swprintf(destination,MAX_PATH,L"%ls\\%ls",output,base);
        if (ok) ok=CopyFileW(theme.frames.files[i],destination,TRUE)!=0;
    }
    swprintf(manifest,MAX_PATH,L"%ls\\theme.json",output);
    if (ok) ok=CopyFileW(input,manifest,TRUE)!=0;
    if (ok) ok=readTheme(manifest).valid;
    if (!ok) discardImportedFolder(output);
    else lstrcpynW(name,theme.title,128);
    return ok;
}
struct ImportJob { HWND target; WCHAR source[MAX_PATH],root[MAX_PATH],manifest[MAX_PATH],name[128]; bool success,enableAfterImport; };
static DWORD WINAPI importThread(void *context)
{
    ImportJob *job=(ImportJob*)context;
    job->success=cacheThemeFolder(job->source,job->root,job->manifest,job->name);
    if (!PostMessageW(job->target,WM_APP+8,0,(LPARAM)job)) free(job);
    return 0;
}
