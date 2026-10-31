# Third-party dependencies (version lock)

All are permissively licensed, vendored unmodified, and redistributable in the ZIP.

| Component | Version | Licence | Source | SHA-256 of vendored file |
|---|---|---|---|---|
| cpp-httplib (`httplib.h`) | 0.18.3 | MIT | https://github.com/yhirose/cpp-httplib/releases/tag/v0.18.3 | a0a0c13dc086663863dbe6730a19716f7d3744904b6205496e8301750814dbd5 |
| nlohmann/json (`json.hpp`) | 3.11.3 | MIT | https://github.com/nlohmann/json/releases/tag/v3.11.3 | 9bea4c8066ef4a1c206b2be5a36302f8926f7fdc6087af5d20b417d0cf103ea6 |

Front-end libraries (bundled locally under `web/vendor/`, no CDN, so the app works offline) are listed in `web/vendor/VENDOR.md`.

Build tools (not redistributed): CMake >= 3.16, a C++17 compiler (MSVC 2022, GCC 13, Apple Clang).
