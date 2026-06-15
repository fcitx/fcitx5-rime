## Why

`ithuan-ubuntu24.04` branch 的目標是為 Ubuntu 24.04 LTS（fcitx5 5.1.7）用戶提供台文輸入法 .deb 套件；目前 branch 只有基礎 fcitx5-rime 程式碼與一個未完成的 `.travis.yml`，缺少參數化建置系統（`schemes.cmake`、`src/scheme_*.in*`）、submodule 設定（`.gitmodules`）、以及 Docker 打包流程（`packaging/`），無法產出任何 .deb。

## What Changes

- 修改 `src/rimeengine.cpp`：在最後的 `FCITX_ADDON_FACTORY` 加 `#ifndef HAVE_SCHEME_CONFIG` guard，避免與參數化建置的 `scheme_factory.cpp` 重複定義
- 修改 `CMakeLists.txt`：加入 `if(SCHEME_ID)` 分支，有 `SCHEME_ID` 時 `include(schemes.cmake)`，無則維持原始 rime 建置
- 新增 `src/scheme_config.h.in`、`src/scheme_factory.cpp.in`、`src/scheme.conf.in`、`src/scheme-addon.conf.in.in`：CMake configure_file 樣板，產生方案專屬的 .h、.cpp、.conf
- 新增 `schemes.cmake`：參數化建置邏輯，從 submodule 安裝資料檔與圖示
- 新增 `.gitmodules`：5 個台文方案 submodule（Rime-HanLo / POJ / POJHan / TOJ / TsuanLo）+ Rime-Logo
- 新增 `packaging/Dockerfile-24.04`：5-stage multi-stage Dockerfile（deps / builder / packager / verifier / exporter），base image `ubuntu:24.04`
- 新增 `packaging/build-all.sh`：一次 build 全部 5 個方案 .deb，版本號加 `~ubuntu24.04` 後綴
- 新增 `packaging/schemes.conf`、`packaging/PKGBUILD`、`packaging/fcitx5-scheme.spec`
- 更新 `.travis.yml`：加入 `ithuan-ubuntu24.04` branch，改描述為 Ubuntu 24.04，CI 執行 `./packaging/build-all.sh`

## Capabilities

### New Capabilities

- `ubuntu-24-04-taibun-packaging`：定義 `ithuan-ubuntu24.04` branch 的完整打包能力——參數化 CMake 建置、5 個台文方案 submodule、Dockerfile-24.04 multi-stage build、Travis CI 自動化，產出 5 個 .deb 套件供 Ubuntu 24.04 用戶安裝

### Modified Capabilities

（無。現有 `src/rimeengine.cpp` 的修改只加 `#ifdef` guard，不影響無 SCHEME_ID 時的上游行為。）

## Impact

- **修改檔案**：`src/rimeengine.cpp`、`CMakeLists.txt`、`.travis.yml`
- **新增檔案**：`src/scheme_config.h.in`、`src/scheme_factory.cpp.in`、`src/scheme.conf.in`、`src/scheme-addon.conf.in.in`、`schemes.cmake`、`.gitmodules`、`packaging/Dockerfile-24.04`、`packaging/build-all.sh`、`packaging/schemes.conf`、`packaging/PKGBUILD`、`packaging/fcitx5-scheme.spec`
- **無 breaking changes**：未加 SCHEME_ID 的一般建置行為完全不變
- **相依**：需要 Docker（build 環境）；submodule 需要 GitHub 網路存取；fcitx5 5.1.7 API（`add_library(MODULE)` 而非 `add_fcitx5_addon()`）
