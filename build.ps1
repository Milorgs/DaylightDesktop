param([string]$Compiler = 'g++.exe')
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Path "$PSScriptRoot/dist" -Force | Out-Null
$compilerPath = (Get-Command $Compiler).Source
$windres = Join-Path (Split-Path $compilerPath) 'windres.exe'
$gcc = Join-Path (Split-Path $compilerPath) 'gcc.exe'
foreach ($source in @('miniz','miniz_zip','miniz_tinfl')) {
    & $gcc -Os -DMINIZ_NO_DEFLATE_APIS -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES -c "$PSScriptRoot/vendor/miniz/$source.c" -o "$PSScriptRoot/dist/$source.o"
    if ($LASTEXITCODE -ne 0) { throw 'ZIP reader compilation failed.' }
}
& $windres -I "$PSScriptRoot/src" -i "$PSScriptRoot/src/app.rc" -o "$PSScriptRoot/dist/app.o"
if ($LASTEXITCODE -ne 0) { throw 'Resource compilation failed.' }
& $Compiler -Os -s -std=c++17 -fno-exceptions -fno-rtti -static -static-libstdc++ -static-libgcc -ffunction-sections -fdata-sections '-Wl,--gc-sections' -Wall -Wextra -Werror -municode -mwindows -DMINIZ_NO_DEFLATE_APIS -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES `
    "$PSScriptRoot/src/main.cpp" "$PSScriptRoot/src/drawing.cpp" "$PSScriptRoot/dist/app.o" "$PSScriptRoot/dist/miniz.o" "$PSScriptRoot/dist/miniz_zip.o" "$PSScriptRoot/dist/miniz_tinfl.o" -o "$PSScriptRoot/dist/DaylightDesktop.exe" `
    -luser32 -lgdi32 -lcomdlg32 -lshell32 -lole32 -luuid -ladvapi32 -ldwmapi -lmsimg32 -lshlwapi -lruntimeobject -lwindowscodecs -lwinhttp -liphlpapi -luxtheme -lcomctl32
if ($LASTEXITCODE -ne 0) { throw "Build failed: $LASTEXITCODE" }
Get-FileHash "$PSScriptRoot/dist/DaylightDesktop.exe"
