#pragma once
#define JSMN_PARENT_LINKS
#define JSMN_STATIC
#include "../vendor/jsmn/jsmn.h"
#include "../vendor/miniz/miniz.h"
#include "frames.h"
#include <cstdlib>
struct Theme {
    Frames frames;
    int groups[4][96]={}, lengths[4]={};
    WCHAR title[128]={};
    bool valid=false;
};
inline int jsonField(const char *text,const jsmntok_t *tokens,int count,const char *key)
{
    for (int i=1;i<count-1;++i)
        if (tokens[i].parent==0 && tokens[i].type==JSMN_STRING && tokens[i].end-tokens[i].start==(int)strlen(key) &&
            !memcmp(text+tokens[i].start,key,strlen(key))) return i+1;
    return -1;
}
inline bool jsonWide(const char *text,const jsmntok_t &token,WCHAR *out,int capacity)
{
    if (token.type!=JSMN_STRING) return false;
    WCHAR raw[1024]; int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text+token.start,token.end-token.start,raw,1023);
    if (!n && token.end!=token.start) return false;
    int k=0;
    for (int i=0;i<n;++i) {
        WCHAR c=raw[i];
        if (c==L'\\') {
            if (++i==n) return false;
            c=raw[i];
            if (c==L'u') {
                if (i+4>=n) return false;
                c=0;
                for (int j=0;j<4;++j) {
                    WCHAR digit=raw[++i]; int v=digit>=L'0'&&digit<=L'9'?digit-L'0':digit>=L'a'&&digit<=L'f'?digit-L'a'+10:digit>=L'A'&&digit<=L'F'?digit-L'A'+10:-1;
                    if (v<0) return false;
                    c=(WCHAR)(c*16+v);
                }
            } else if (c!=L'"'&&c!=L'\\'&&c!=L'/') return false;
        }
        if (!c||c<L' '||k+1>=capacity) return false;
        out[k++]=c;
    }
    out[k]=0; return true;
}
inline bool parseTheme(const char *json,const WCHAR *folder,Theme &theme,bool checkImages=true)
{
    theme=Theme{};
    jsmntok_t tokens[2048]; jsmn_parser parser; jsmn_init(&parser);
    int total=jsmn_parse(&parser,json,strlen(json),tokens,2048);
    if (total<1||tokens[0].type!=JSMN_OBJECT) return false;
    int fileToken=jsonField(json,tokens,total,"imageFilename"); WCHAR pattern[MAX_PATH];
    if (fileToken<0||!jsonWide(json,tokens[fileToken],pattern,MAX_PATH)||wcspbrk(pattern,L"/\\:")||wcsstr(pattern,L"..")) return false;
    WCHAR *star=wcschr(pattern,L'*'); if (!star||wcschr(star+1,L'*')) return false;
    int title=jsonField(json,tokens,total,"displayName");
    if (title>=0) { if (!jsonWide(json,tokens[title],theme.title,128)) return false; }
    else lstrcpynW(theme.title,PathFindFileNameW(folder),128);
    const char *keys[]={"sunriseImageList","dayImageList","sunsetImageList","nightImageList"};
    for (int group=0;group<4;++group) {
        int array=jsonField(json,tokens,total,keys[group]);
        if (array<0) { if (group==1||group==3) return false; continue; }
        if (tokens[array].type!=JSMN_ARRAY||tokens[array].size>96) return false;
        for (int i=array+1;i<total && tokens[i].start<tokens[array].end;++i) {
            if (tokens[i].parent!=array||tokens[i].type!=JSMN_PRIMITIVE) return false;
            int id=0; if (tokens[i].end==tokens[i].start) return false;
            for (int j=tokens[i].start;j<tokens[i].end;++j) { if (json[j]<'0'||json[j]>'9') return false; id=id*10+json[j]-'0'; if (id>999999) return false; }
            WCHAR filename[MAX_PATH],path[MAX_PATH],prefix[MAX_PATH];
            int before=(int)(star-pattern); wmemcpy(prefix,pattern,before); prefix[before]=0;
            if (swprintf(filename,MAX_PATH,L"%ls%d%ls",prefix,id,star+1)<=0||!imageName(filename)) return false;
            if (wcslen(folder)+wcslen(filename)+2>=MAX_PATH) return false;
            swprintf(path,MAX_PATH,L"%ls\\%ls",folder,filename);
            DWORD attributes=GetFileAttributesW(path);
            if (checkImages && (attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_DIRECTORY))) return false;
            int index=0; while (index<theme.frames.count && lstrcmpiW(theme.frames.files[index],path)) ++index;
            if (index==theme.frames.count) { if (index==96) return false; lstrcpyW(theme.frames.files[index],path); ++theme.frames.count; }
            theme.groups[group][theme.lengths[group]++]=index;
        }
        if ((group==1||group==3)&&!theme.lengths[group]) return false;
    }
    theme.valid=theme.frames.count>=2; return theme.valid;
}
inline Theme readTheme(const WCHAR *manifest)
{
    Theme theme;
    HANDLE file=CreateFileW(manifest,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if (file==INVALID_HANDLE_VALUE) return theme;
    DWORD bytes=GetFileSize(file,nullptr),read=0;
    if (bytes>131072) { CloseHandle(file); return theme; }
    char *json=(char*)malloc(bytes+1);
    if (!json) { CloseHandle(file); return theme; }
    bool ok=ReadFile(file,json,bytes,&read,nullptr)&&read==bytes; CloseHandle(file); json[bytes]=0;
    WCHAR folder[MAX_PATH]; lstrcpynW(folder,manifest,MAX_PATH); PathRemoveFileSpecW(folder);
    if (ok) parseTheme(bytes>=3&&(unsigned char)json[0]==0xef?json+3:json,folder,theme);
    free(json); return theme;
}
inline int themeFrame(const Theme &theme,int now,const int starts[4],int polar)
{
    if (polar) { int group=polar>0?1:3; return theme.groups[group][now*theme.lengths[group]/1440]; }
    int elapsed=normalizeMinute(now-starts[0]);
    for (int group=3;group>=0;--group) {
        int start=normalizeMinute(starts[group]-starts[0]);
        int end=group==3?1440:normalizeMinute(starts[group+1]-starts[0]);
        if (elapsed>=start&&elapsed<end&&theme.lengths[group])
            return theme.groups[group][(elapsed-start)*theme.lengths[group]/(end-start)];
    }
    return theme.groups[3][0];
}
inline int themeNextMinutes(const Theme &theme,int now,const int starts[4],int polar)
{
    int current=themeFrame(theme,now,starts,polar),next=1440;
    for (int group=0;group<4;++group) {
        if (!polar) {
            int boundary=normalizeMinute(starts[group]),distance=normalizeMinute(boundary-now);
            if (distance&&distance<next&&themeFrame(theme,boundary,starts,polar)!=current) next=distance;
        }
        int count=theme.lengths[group]; if (!count||(polar&&group!=(polar>0?1:3))) continue;
        int span=polar?1440:normalizeMinute((group==3?starts[0]:starts[group+1])-starts[group]);
        if (!span) continue;
        for (int i=0;i<count;++i) {
            int boundary=normalizeMinute((polar?0:starts[group])+(i*span+count-1)/count);
            int distance=normalizeMinute(boundary-now); if (!distance) distance=1440;
            if (distance<next&&themeFrame(theme,boundary,starts,polar)!=current) next=distance;
        }
    }
    return next;
}
inline bool safeArchiveName(const char *name)
{
    return name[0] && !strstr(name,"..")&&!strpbrk(name,"/\\:")&&strlen(name)<160;
}
inline bool importArchive(const WCHAR *archive,const WCHAR *root,WCHAR manifest[MAX_PATH])
{
    HANDLE file=CreateFileW(archive,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if (file==INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size; bool ok=GetFileSizeEx(file,&size)&&size.QuadPart>0&&size.QuadPart<=1024LL*1024*1024; CloseHandle(file);
    if (!ok) return false;
    char utf8[MAX_PATH*4]; WideCharToMultiByte(CP_UTF8,0,archive,-1,utf8,sizeof(utf8),nullptr,nullptr);
    mz_zip_archive zip={}; if (!mz_zip_reader_init_file(&zip,utf8,0)) return false;
    WCHAR folder[MAX_PATH]; swprintf(folder,MAX_PATH,L"%ls\\theme-%lu-%llu",root,GetCurrentProcessId(),GetTickCount64());
    bool created=false; mz_uint count=mz_zip_reader_get_num_files(&zip); unsigned long long total=0; int jsonIndex=-1;
    ok=count<=100;
    for (mz_uint i=0;ok&&i<count;++i) {
        mz_zip_archive_file_stat stat;
        ok=mz_zip_reader_file_stat(&zip,i,&stat)&&!mz_zip_reader_is_file_a_directory(&zip,i)&&safeArchiveName(stat.m_filename)&&
            !stat.m_is_encrypted&&stat.m_uncomp_size<=128ULL*1024*1024;
        if (!ok) break;
        total+=stat.m_uncomp_size; if (total>2ULL*1024*1024*1024) ok=false;
        WCHAR name[160]; if (!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,stat.m_filename,-1,name,160)) { ok=false; break; }
        if (!lstrcmpiW(name,L"theme.json")) jsonIndex=(int)i;
        else if (!imageName(name)) ok=false;
        if (wcslen(folder)+wcslen(name)+2>=MAX_PATH) ok=false;
    }
    ok=ok&&jsonIndex>=0;
    if (ok) { created=CreateDirectoryW(folder,nullptr)!=0; ok=created; }
    for (mz_uint i=0;ok&&i<count;++i) {
        mz_zip_archive_file_stat stat; mz_zip_reader_file_stat(&zip,i,&stat);
        WCHAR name[160],path[MAX_PATH]; MultiByteToWideChar(CP_UTF8,0,stat.m_filename,-1,name,160); swprintf(path,MAX_PATH,L"%ls\\%ls",folder,name);
        size_t bytes=0; void *data=mz_zip_reader_extract_to_heap(&zip,i,&bytes,0);
        if (!data) { ok=false; break; }
        HANDLE output=CreateFileW(path,GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        DWORD written=0;
        ok=output!=INVALID_HANDLE_VALUE&&WriteFile(output,data,(DWORD)bytes,&written,nullptr)&&written==bytes;
        if (output!=INVALID_HANDLE_VALUE) CloseHandle(output);
        mz_free(data);
    }
    mz_zip_reader_end(&zip);
    swprintf(manifest,MAX_PATH,L"%ls\\theme.json",folder);
    if (ok) ok=readTheme(manifest).valid;
    if (!ok&&created) {
        WCHAR search[MAX_PATH]; swprintf(search,MAX_PATH,L"%ls\\*",folder); WIN32_FIND_DATAW entry; HANDLE find=FindFirstFileW(search,&entry);
        if (find!=INVALID_HANDLE_VALUE) { do { if (!(entry.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)) { WCHAR path[MAX_PATH]; swprintf(path,MAX_PATH,L"%ls\\%ls",folder,entry.cFileName); DeleteFileW(path); } } while(FindNextFileW(find,&entry)); FindClose(find); }
        RemoveDirectoryW(folder);
    }
    return ok;
}
