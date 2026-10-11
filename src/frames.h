#pragma once
#include <windows.h>
#include <shlwapi.h>
#include <cwchar>
struct Frames { WCHAR files[96][MAX_PATH]; int count=0; bool overflow=false; };
inline bool imageName(const WCHAR *name)
{
    const WCHAR *ext=wcsrchr(name,L'.');
    return ext && (!lstrcmpiW(ext,L".jpg")||!lstrcmpiW(ext,L".jpeg")||!lstrcmpiW(ext,L".png")||!lstrcmpiW(ext,L".bmp"));
}
inline Frames readFrames(const WCHAR *folder)
{
    Frames result;
    if (!folder[0] || wcslen(folder)+4>=MAX_PATH) return result;
    WCHAR pattern[MAX_PATH]; swprintf(pattern,MAX_PATH,L"%ls\\*",folder);
    WIN32_FIND_DATAW entry; HANDLE find=FindFirstFileW(pattern,&entry);
    if (find==INVALID_HANDLE_VALUE) return result;
    do {
        if (!(entry.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) && imageName(entry.cFileName)) {
            if (result.count==96) { result.overflow=true; break; }
            if (wcslen(folder)+wcslen(entry.cFileName)+2>=MAX_PATH) { result.overflow=true; break; }
            swprintf(result.files[result.count++],MAX_PATH,L"%ls\\%ls",folder,entry.cFileName);
        }
    } while (FindNextFileW(find,&entry));
    FindClose(find);
    for (int i=1;i<result.count;++i) {
        WCHAR item[MAX_PATH]; lstrcpyW(item,result.files[i]); int j=i;
        while (j>0 && StrCmpLogicalW(result.files[j-1],item)>0) { lstrcpyW(result.files[j],result.files[j-1]); --j; }
        lstrcpyW(result.files[j],item);
    }
    return result;
}
