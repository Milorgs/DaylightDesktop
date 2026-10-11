// UI-only theme catalogue. Images are decoded only for the visible page.
struct SavedTheme { WCHAR manifest[MAX_PATH],title[128]; };
static std::vector<SavedTheme> library;
static HBITMAP libraryImages[6],phaseImage;
static int libraryPage,phasePosition,previewIndex=-1;
struct PhaseEntry { int frame,group,minute; };
static std::vector<PhaseEntry> phases;
static void showPage(int page);
static void clearLibraryImages()
{
    for (HBITMAP &image:libraryImages) { if (image) DeleteObject(image); image=nullptr; }
}
static void updateCurrentPreview()
{
    if (!chosenTheme.valid) return;
    Settings settings=load(configPath); Playback playback=playbackAt(settings,chosenTheme);
    if (previewIndex!=playback.index||!preview) {
        if (preview) DeleteObject(preview);
        preview=themePreview(chosenTheme.frames.files[playback.index],px(104),px(60),px(8)); previewIndex=playback.index;
        RECT area={px(28),px(88),px(132),px(148)}; InvalidateRect(window,&area,FALSE);
    }
    SetTimer(window,7,playback.delay,nullptr);
}
static void refreshLibrary(bool rescan=true)
{
    clearLibraryImages();
    if (rescan) {
        library.clear();
        WCHAR root[MAX_PATH],search[MAX_PATH]; lstrcpyW(root,configPath); PathRemoveFileSpecW(root); lstrcatW(root,L"\\themes");
        swprintf(search,MAX_PATH,L"%ls\\*",root); WIN32_FIND_DATAW entry; HANDLE find=FindFirstFileW(search,&entry);
        if (find!=INVALID_HANDLE_VALUE) {
            do {
                if (!(entry.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)||entry.cFileName[0]==L'.'||(entry.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)) continue;
                WCHAR folder[MAX_PATH]; if (wcslen(root)+wcslen(entry.cFileName)+2>=MAX_PATH) continue;
                swprintf(folder,MAX_PATH,L"%ls\\%ls",root,entry.cFileName); SavedTheme saved={};
                if (!findFolderTheme(folder,saved.manifest)) continue;
                Theme theme=readTheme(saved.manifest); if (!theme.valid) continue;
                lstrcpynW(saved.title,theme.title,128); library.push_back(saved);
            } while (FindNextFileW(find,&entry)); FindClose(find);
        }
        std::sort(library.begin(),library.end(),[](const SavedTheme &a,const SavedTheme &b){ return lstrcmpiW(a.title,b.title)<0; });
    }
    int last=library.empty()?0:((int)library.size()-1)/6; libraryPage=std::max(0,std::min(libraryPage,last));
    libraryRows=std::max(1,std::min(3,((int)library.size()-libraryPage*6+1)/2));
    for (int id:{LIBRARY_PREV,LIBRARY_NEXT}) { HWND button=GetDlgItem(window,id); SetWindowPos(button,nullptr,px(id==LIBRARY_PREV?16:372),px(108+libraryRows*106),px(72),px(30),SWP_NOZORDER); }
    resizePage();
    Settings settings=load(configPath);
    for (int tile=0;tile<6;++tile) {
        int index=libraryPage*6+tile; HWND button=GetDlgItem(window,LIBRARY_TILE+tile);
        ShowWindow(button,page==2&&index<(int)library.size()?SW_SHOW:SW_HIDE);
        if (index<(int)library.size()) {
            Theme theme=readTheme(library[index].manifest); Playback playback=playbackAt(settings,theme);
            libraryImages[tile]=themePreview(theme.frames.files[playback.index],px(184),px(58),px(7));
            SetWindowTextW(button,library[index].title);
        }
    }
    EnableWindow(GetDlgItem(window,LIBRARY_PREV),libraryPage>0); EnableWindow(GetDlgItem(window,LIBRARY_NEXT),libraryPage<last);
}
static void updatePhaseImage()
{
    if (phaseImage) DeleteObject(phaseImage);
    phaseImage=phases.empty()?nullptr:themePreview(chosenTheme.frames.files[phases[phasePosition].frame],px(428),px(241),px(12));
    EnableWindow(GetDlgItem(window,PHASE_PREV),phases.size()>1);
    EnableWindow(GetDlgItem(window,PHASE_NEXT),phases.size()>1);
    InvalidateRect(window,nullptr,FALSE);
}
static void preparePhases()
{
    phases.clear(); Playback playback=playbackAt(load(configPath),chosenTheme);
    for (int group=0;group<4;++group) {
        int count=chosenTheme.lengths[group];
        if (playback.polar&&group!=(playback.polar>0?1:3)) continue;
        int span=playback.polar?1440:group==3?normalizeMinute(playback.starts[0]-playback.starts[3]):normalizeMinute(playback.starts[group+1]-playback.starts[group]);
        if (!span) continue;
        for (int i=0;i<count;++i) phases.push_back({chosenTheme.groups[group][i],group,normalizeMinute((playback.polar?0:playback.starts[group])+(i*span+count-1)/count)});
    }
    std::sort(phases.begin(),phases.end(),[](const PhaseEntry &a,const PhaseEntry &b){return a.minute<b.minute;});
    SYSTEMTIME now; GetLocalTime(&now); int minute=now.wHour*60+now.wMinute;
    phasePosition=phases.empty()?0:(int)phases.size()-1;
    for (int i=0;i<(int)phases.size();++i) if (phases[i].minute<=minute) phasePosition=i;
    updatePhaseImage();
}
static void drawImage(HDC dc,HBITMAP bitmap,int x,int y,int width,int height)
{
    if (!bitmap) return;
    HDC image=CreateCompatibleDC(dc); HGDIOBJ old=SelectObject(image,bitmap);
    BLENDFUNCTION blend={AC_SRC_OVER,0,255,AC_SRC_ALPHA}; AlphaBlend(dc,px(x),px(y),px(width),px(height),image,0,0,px(width),px(height),blend);
    SelectObject(image,old); DeleteDC(image);
}
