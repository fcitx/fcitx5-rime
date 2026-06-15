## 1. 修改 src/rimeengine.cpp（加 HAVE_SCHEME_CONFIG guard）

- [x] 1.1 在 `src/rimeengine.cpp` 最後的 `FCITX_ADDON_FACTORY(fcitx::RimeEngineFactory)` 上下加 `#ifndef HAVE_SCHEME_CONFIG` / `#endif` guard

## 2. 修改 CMakeLists.txt（加 SCHEME_ID 分支）

- [x] 2.1 在 `CMakeLists.txt` 的 `add_subdirectory(po)` 之後加入：若 `SCHEME_ID` 有設定則 `include(schemes.cmake)`，否則執行原有的 `add_subdirectory(src)`、`add_subdirectory(data)` 等

## 3. 新增 src/ 樣板檔案

- [x] 3.1 新增 `src/scheme_config.h.in`：定義 `SCHEME_ADDON_NAME`、`SCHEME_ICON_PREFIX`、`SCHEME_CONF_PREFIX`、`SCHEME_DISPLAY_NAME` 四個巨集（從 compat/ubuntu-24.04 複製）
- [x] 3.2 新增 `src/scheme_factory.cpp.in`：全新撰寫，只有 2 行——`#include "rimeengine.h"` 和 `FCITX_ADDON_FACTORY(fcitx::RimeEngineFactory)`（不重新定義 `create`：92a4410 的 `rimeengine.h` 已有 inline body，無法 redefine）
- [x] 3.3 新增 `src/scheme.conf.in`：InputMethod 設定樣板（從 compat/ubuntu-24.04 複製）
- [x] 3.4 新增 `src/scheme-addon.conf.in.in`：Addon 設定樣板（從 compat/ubuntu-24.04 複製）

## 4. 新增 schemes.cmake

- [x] 4.1 新增 `schemes.cmake`，以 compat/ubuntu-24.04 為基礎，只移除 SCHEME_SOURCES 裡的 `rimeaction.cpp` 一行（92a4410 無此檔案）；`add_library(${SCHEME_ID} MODULE ...)` compat 已是此寫法，照用不改

## 5. 新增 .gitmodules 與初始化 submodule

- [x] 5.1 新增 `.gitmodules`（從 compat/ubuntu-24.04 複製，包含 5 個 scheme + Rime-Logo）
- [x] 5.2 確認 `schemes/` 下 5 個子目錄與 `Rime-Logo/` 的 gitlink 已正確設定（視需要執行 `git submodule update --init --recursive`）

## 6. 新增 packaging/ 目錄

- [x] 6.1 新增 `packaging/Dockerfile-24.04`（從 compat/ubuntu-24.04 複製，base image 已是 `ubuntu:24.04`）
- [x] 6.2 新增 `packaging/build-all.sh`（從 compat/ubuntu-24.04 複製，`UBUNTU_VER=24.04`，版本號加 `~ubuntu24.04`）
- [x] 6.3 新增 `packaging/schemes.conf`（從 compat/ubuntu-24.04 複製，5 個方案參數）
- [x] 6.4 新增 `packaging/PKGBUILD`（從 compat/ubuntu-24.04 複製）
- [x] 6.5 新增 `packaging/fcitx5-scheme.spec`（從 compat/ubuntu-24.04 複製）

## 7. 更新 .travis.yml

- [x] 7.1 在 `branches.only` 加入 `ithuan-ubuntu24.04`
- [x] 7.2 job 名稱改為「Build and verify .deb packages (Ubuntu 24.04)」
- [x] 7.3 確認 script 為 `./packaging/build-all.sh`

## 8. 手動驗證（需有 Docker 的機器）

- [ ] 8.1 執行 `./packaging/build-all.sh 2.0.0`，確認 `build/deb/` 出現 5 個 .deb（`fcitx5-{hanlo,poj,pojhan,toj,tsuanlo}_2.0.0~ubuntu24.04_ubuntu24.04.deb`）
- [ ] 8.2 若 cmake/make 失敗，逐 error 排查（可能是缺少的 API 或 include 路徑問題）
- [ ] 8.3 在 Ubuntu 24.04 環境安裝 .deb，確認 fcitx5 輸入法清單出現台文方案
