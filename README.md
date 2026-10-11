# Daylight Desktop

A small native Windows utility that advances through a sequence of wallpaper images as the light changes throughout the day.

[Download 1.0](https://github.com/Milorgs/DaylightDesktop/releases/tag/v1.0) · Windows 11 x64 · Single EXE

## Features

- Choose a WinDynamicDesktop theme folder containing `theme.json` and its referenced images. The app copies the required files into its own local theme library and remembers the selection.
- Previously imported themes remain available in the theme library. Browse scheduled images in a loop in either direction without changing the desktop; the home thumbnail follows the current time.
- Read the theme's sunrise, day, sunset, and night image lists in their defined order. Fixed local times or approximate solar times set the pace.
- Transition boundaries are calculated directly. The scheduler reuses theme metadata until the manifest changes; theme-library pagination avoids rescanning the library.
- One timer for the next transition; recalculates after wake, time changes, and at midnight in solar mode.
- Separate settings and background processes. Closing settings leaves the scheduler running.
- A compact main window with three choices: Automatic location, Custom coordinates, or Fixed times. Time and coordinate fields are centered.
- Startup, Chinese/English language selection, and the GitHub project link are under More. The scheduler runs without a window or tray icon; run the EXE again to reopen settings.
- Copilot Key Binding-style colors and rounded controls, with a Windows 11 Mica backdrop where available and a coordinated solid fallback. Theme previews are cropped to fit and have antialiased rounded corners.
- More includes System, Light, and Dark appearance choices. They apply immediately and survive restarts; high-contrast colors take priority. Numeric fields use the same font as the current UI language.
- Automatically detect the current city/region through Windows location and reverse geocoding. Refresh at startup, wake, network changes, and every 15 minutes while Automatic location is selected.
- Choose a theme, mode, or toggle to apply it immediately. Text edits apply after a short pause or on leaving the field; there is no Save button.
- A single EXE with an embedded landscape icon and small JSON/ZIP readers. No separate compiler runtime, .NET, or web runtime to install. Low-frequency location requests replace continuous GPS tracking.

## Use

Run `dist/DaylightDesktop.exe`, open Themes, click Import theme folder, and select the folder that contains `theme.json` and its images. Unpack `.ddw`/ZIP packages first if necessary. The app copies the referenced images and JSON into `%LOCALAPPDATA%\DaylightDesktop\themes`, validates the copy, and remembers it. After import completes, the original folder can be removed; reopening the app restores the cached theme. A valid theme enables automatic switching and applies the current frame when location is available. Settings are stored in `%LOCALAPPDATA%\DaylightDesktop\settings.ini`. Existing selections that still point to an external JSON are migrated automatically when opened, if the source files are available.

More contains startup, language, and the project link. Language changes apply immediately and survive restarts without resetting the theme or schedule. Automatic location displays a city or region name. Custom coordinates lets you enter latitude (−89.9 to 89.9) and longitude (−180 to 180) directly; north/east are positive, south/west are negative. Custom coordinates are remembered separately from automatic readings and apply when valid input is complete. Invalid input retains the previous valid setting.

Windows location access and Internet access are needed for automatic city names. Reverse geocoding uses BigDataCloud's free device-side endpoint with the current coordinates rounded to two decimal places (about 1 km). It does not upload wallpapers or keep a location history. Recent city names are cached to avoid repeated lookups while stationary. Choosing Custom coordinates or Fixed times stops future automatic location requests; an in-flight request may finish but its result is discarded.

If location detection fails, the last valid position is retained. If only the city-name lookup fails, the new coordinates still determine solar times and the UI shows that the name is unavailable. Startup/wake/network notifications and a 15-minute refresh interval detect travel; this is not instantaneous continuous tracking.

Coordinates use positive values for north/east and negative values for south/west. Sunrise/sunset is an approximation using the [NOAA equations](https://gml.noaa.gov/grad/solcalc/solareqns.PDF) and the Windows time zone. Images are applied to all monitors, retaining Windows wallpaper positioning.

Keep the EXE at its saved location after enabling startup, and keep the app's local theme library intact. Original import folders are no longer required once copying finishes. Disable other automatic wallpaper tools before enabling this scheduler. Exiting does not restore the previous wallpaper; disabling startup affects the next sign-in.

## Build and checks

Install MinGW-w64 GCC/G++, then run `./build.ps1` from PowerShell. Output: `dist/DaylightDesktop.exe`.

- `--self-test`: schedule boundaries, overnight schedules, solar estimates, polar day/night, and Unicode settings persistence. Uses a temporary INI file.
- `--probe-desktop`: checks the Windows wallpaper interface without changing the wallpaper.
- `--measure`: starts the scheduler without applying wallpapers, for diagnostic sampling.

Theme definitions use `imageFilename`, `dayImageList`, `nightImageList`, optional `sunriseImageList` and `sunsetImageList`, and optional `displayName`. Sunrise and sunset periods approximate the sun moving between −6° and +6°. Fixed-time mode uses a short twilight interval around the selected times.

This version uses time-varying static frames rather than video. Direct Apple HEIC import, theme downloads, per-frame solar-angle metadata, separate monitor schedules, and crossfade animation are not included yet. At polar latitudes, only the appropriate day or night list progresses over 24 hours. A missing or invalid theme leaves the current wallpaper in place. Archives must contain a flat `theme.json` and image files; nested paths and unrelated files are rejected.

After applying a wallpaper on the development PC, the fixed-time background used about 12.9 MB of working-set memory and automatic location about 19.0 MB. No CPU-time increase was measured during the following 10-second idle samples. Settings use additional memory for previews and drawing buffers. These are local measurements, not a memory limit; usage varies by system, theme, and activity. Diagnostic mode `--measure` excludes wallpaper application and location requests.

## 中文说明

一个轻量的原生 Windows 壁纸工具，让一组静态图片随一天的光线变化，依次呈现晨光、白天、黄昏和夜色。

[下载 1.0](https://github.com/Milorgs/DaylightDesktop/releases/tag/v1.0) · Windows 11 x64 · 单个 EXE

### 功能与使用

- 选择包含 `theme.json` 和完整图片的 WinDynamicDesktop 主题文件夹，程序自动复制到本地主题库并记住选择。
- 已导入主题保留在主题库，可直接重新选择。分时预览可双向循环浏览各时间段图片，不更改桌面；主页缩略图随当前时间更新。
- 根据主题定义的日出、白天、日落、夜间图片列表确定顺序，按固定时间或当地太阳时间调整节奏。
- 直接计算下一次切换节点；主题配置未变化时复用缓存，主题库翻页不重新扫描整个文件夹。固定时间和自定义坐标模式不监听网络变化，也不定时检查位置。
- 后台只等待下一次切换事件，唤醒或系统时间改变时重新计算；日出日落模式每天重新计算。
- 设置窗口和后台进程分开，关闭设置窗口后继续运行。
- 切换方式采用圆角分段选择，时间和经纬度数字居中。
- “更多”中设置开机自启动、中文／英文界面，并访问 GitHub 项目；后台无窗口、无托盘图标，重新运行 EXE 可打开设置。
- 采用接近 CopilotKeyBinding 的配色和圆角控件，支持 Windows 11 Mica；不支持时使用与标题栏协调的纯色背景。主题缩略图按比例裁切，并使用平滑圆角。
- “更多”中可选择跟随系统、浅色或深色，立即生效并记住选择，高对比度配色优先。数字输入框与当前界面语言使用同一字体。
- 主界面提供“自动位置、自定义坐标、固定时间”三个选项，时间和坐标居中显示。
- 自动模式识别城市／地区，在启动、唤醒、网络变化后刷新，并每 15 分钟检查一次。
- 选择主题、切换方式或开关后立即生效；输入内容在短暂停顿或离开输入框后应用，没有保存按钮。
- 单个 EXE，内置风景图标及小型 JSON／ZIP 读取器，无需额外安装编译器运行库、.NET 或网页运行库；使用低频定位请求，不持续跟踪 GPS。

运行 `dist/DaylightDesktop.exe`，点击“选择主题”，在主题库中导入包含 `theme.json` 和图片的文件夹；使用 `.ddw`／ZIP 主题包时先解压。程序将主题引用的图片和配置复制到 `%LOCALAPPDATA%\DaylightDesktop\themes`，验证后记住选择。导入完成后可以删除原文件夹，重新打开仍使用缓存主题。有效主题会开启自动切换，并在位置可用后应用当前阶段。设置保存在 `%LOCALAPPDATA%\DaylightDesktop\settings.ini`。旧版已选主题若仍引用外部 JSON，会在原文件可用时自动迁移到主题库。

“更多”保留自启动、语言切换和项目链接。语言立即切换并记住，不重置主题或时间设置。自动模式显示城市／地区名称；“自定义坐标”可直接填写纬度（−89.9～89.9）和经度（−180～180）。北纬、东经为正，南纬、西经为负。自定义值会独立记住，不被自动识别覆盖；有效输入完成后生效，无效输入保留上次有效设置。

自动获取地名需要 Windows 定位权限和网络。城市反查使用 BigDataCloud 的设备端接口，只发送当前坐标四舍五入到小数点后两位的值（约 1 公里精度），不上传壁纸，不记录位置历史。停留在同一地区时缓存地名，减少联网请求。选择“自定义坐标”或“固定时间”后不再发起自动定位，已进行的请求可能结束，但不会采用其结果。

定位失败时保留上次有效位置；若只有地名查询失败，新坐标仍用于计算太阳时间，并显示地名暂不可用。启动、唤醒、网络变化及每 15 分钟刷新用于识别旅行后的变化，不是实时持续追踪。

北纬、东经为正，南纬、西经为负。日出日落使用 NOAA 近似公式和 Windows 时区计算。所有显示器使用相同图片，保留 Windows 原有壁纸显示方式。

开启前请暂停其他自动壁纸程序，避免互相覆盖。开启自启动后请保持 EXE 路径不变，并保留程序自己的本地主题库；导入完成后不再依赖原文件夹。退出程序不会恢复上一张壁纸，关闭自启动影响下次登录。

### 构建与范围

安装 MinGW-w64 GCC/G++ 后，在 PowerShell 中运行 `./build.ps1`。输出为 `dist/DaylightDesktop.exe`。

自检覆盖时间边界、跨午夜时间、太阳时间估算、极昼极夜和中文路径保存；系统壁纸接口可通过 `--probe-desktop` 只读检查。

主题使用 `imageFilename`、`dayImageList`、`nightImageList`，可选 `sunriseImageList`、`sunsetImageList` 和 `displayName`。太阳模式的日出／日落阶段近似覆盖太阳高度 −6° 至 +6°；固定时间模式在所选时间附近安排短暂过渡时段。

本版按时间切换多张静态图片，不播放视频。暂不直接解析苹果 HEIC 主题，不含主题下载、逐帧太阳角度、独立多屏计划和渐变动画。极昼极夜时仅使用对应的白天或夜间列表；主题失效时保留当前壁纸。压缩包应直接包含 `theme.json` 与图片，不支持嵌套目录或无关文件。

开发电脑上，实际应用壁纸后的固定时间后台工作集约 12.9 MB，自动位置后台约 19.0 MB；之后的 10 秒空闲采样均未测到 CPU 时间增加。设置窗口还会占用预览图片和绘制缓冲所需内存。这是本机样本，不是内存上限，实际占用会随系统、主题和使用状态变化。诊断模式 `--measure` 不执行壁纸应用或定位请求，不能代表完整后台占用。
