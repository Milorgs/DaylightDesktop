# Third-party components and references

- **miniz 3.1.2** — ZIP reading and decompression, statically compiled into the EXE. MIT license in `vendor/miniz/LICENSE`. Source: https://github.com/richgel999/miniz/tree/3.1.2. Compression and archive-writing APIs are disabled. `miniz_export.h` supplies the empty export macro for this static build.
- **jsmn** — small JSON tokenizer. MIT license in `vendor/jsmn/LICENSE`. Source: https://github.com/zserge/jsmn. Used with parent links and a bounded token array.
- **NOAA** — sunrise/sunset equations: https://gml.noaa.gov/grad/solcalc/solareqns.PDF.
- **Microsoft Windows Runtime metadata** — geolocation interface definitions checked against https://github.com/microsoft/windows-rs/tree/master/crates/libs/windows/src/Windows/Devices/Geolocation. The app calls the Windows system implementation directly.
- **BigDataCloud free client-side reverse geocoding** — current coordinates, rounded to two decimals, are sent directly from the user's device to obtain city/region names. No API key is embedded. Documentation: https://www.bigdatacloud.com/docs/article/why-is-reverse-geocoding-api-free. This is a network service, not a bundled code dependency.
- **WinDynamicDesktop** — reference for interoperable theme fields and the theme-selection workflow: https://github.com/t1m0thyj/WinDynamicDesktop/wiki/Creating-custom-themes. Its C# implementation is not included or copied into this app.

No Apple or WinDynamicDesktop wallpaper images are distributed in this repository. Local compatibility checks use installed themes read-only; fixtures are kept in `_local` and excluded from Git.
