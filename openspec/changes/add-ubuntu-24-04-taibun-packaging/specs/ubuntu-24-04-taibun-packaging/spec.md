## ADDED Requirements

### Requirement: 參數化 CMake 建置（SCHEME_ID 模式）
當以 `-DSCHEME_ID=<id>` 執行 cmake 時，建置系統 SHALL 透過 `schemes.cmake` 產出以該方案命名的 fcitx5 addon，而非預設的 `rime` addon。未設定 `SCHEME_ID` 時，行為 SHALL 與上游 fcitx5-rime 完全一致。

#### Scenario: 有 SCHEME_ID 時走 schemes.cmake
- **WHEN** 以 `-DSCHEME_ID=hanlo` 執行 cmake
- **THEN** `CMakeLists.txt` SHALL include `schemes.cmake` 而非 `add_subdirectory(src)`

#### Scenario: 無 SCHEME_ID 時維持上游行為
- **WHEN** 不帶 `-DSCHEME_ID` 執行 cmake
- **THEN** 建置結果 SHALL 與上游 fcitx5-rime 一致（library 名稱為 `rime`）

### Requirement: rimeengine.cpp ADDON_FACTORY guard
`src/rimeengine.cpp` 末尾的 `FCITX_ADDON_FACTORY` SHALL 以 `#ifndef HAVE_SCHEME_CONFIG` 包覆，確保參數化建置時不產生重複的 `fcitx_module_init` symbol。

#### Scenario: 有 HAVE_SCHEME_CONFIG 時 guard 生效
- **WHEN** 以 `-DHAVE_SCHEME_CONFIG` 編譯 `rimeengine.cpp`
- **THEN** `FCITX_ADDON_FACTORY` 呼叫 SHALL 不出現在編譯單元中

#### Scenario: 無 HAVE_SCHEME_CONFIG 時正常運作
- **WHEN** 不定義 `HAVE_SCHEME_CONFIG`（一般建置）
- **THEN** `FCITX_ADDON_FACTORY(fcitx::RimeEngineFactory)` SHALL 正常產生 `fcitx_module_init`

### Requirement: 方案樣板檔案（src/*.in）
專案 SHALL 提供以下 CMake configure_file 樣板：
- `src/scheme_config.h.in`：產生 `SCHEME_ADDON_NAME`、`SCHEME_ICON_PREFIX`、`SCHEME_CONF_PREFIX`、`SCHEME_DISPLAY_NAME` 巨集
- `src/scheme_factory.cpp.in`：include `rimeengine.h`，實作 `RimeEngineFactory::create`，呼叫 `FCITX_ADDON_FACTORY(fcitx::RimeEngineFactory)`
- `src/scheme.conf.in`：InputMethod 設定樣板（Name、Icon、Label、LangCode、Addon）
- `src/scheme-addon.conf.in.in`：Addon 設定樣板

#### Scenario: scheme_factory.cpp.in 產生正確的 factory
- **WHEN** 以 `-DSCHEME_ID=hanlo` 建置
- **THEN** 產生的 `scheme_factory.cpp` SHALL include `rimeengine.h` 並呼叫 `FCITX_ADDON_FACTORY(fcitx::RimeEngineFactory)`

#### Scenario: scheme_config.h.in 產生正確的巨集
- **WHEN** 以 `-DSCHEME_ID=hanlo -DSCHEME_NAME=意傳教育部漢羅` 執行 cmake configure
- **THEN** 產生的 `scheme_config.h` SHALL 包含 `#define SCHEME_ADDON_NAME "hanlo"`

### Requirement: schemes.cmake 參數化建置邏輯
`schemes.cmake` SHALL 以 `add_library(${SCHEME_ID} MODULE ...)` 建置方案 addon（不使用 fcitx5 5.1.7 不存在的 `add_fcitx5_addon()` macro），SCHEME_SOURCES SHALL 不含 `rimeaction.cpp`。

#### Scenario: 建置產出正確的 .so
- **WHEN** 以 `-DSCHEME_ID=hanlo` 執行 cmake + make
- **THEN** SHALL 產出 `libhanlo.so`，安裝至 `${CMAKE_INSTALL_LIBDIR}/fcitx5/`

#### Scenario: SCHEME_ID target 定義 HAVE_SCHEME_CONFIG
- **WHEN** `schemes.cmake` 建立 `${SCHEME_ID}` target
- **THEN** target compile definitions SHALL 包含 `HAVE_SCHEME_CONFIG`

### Requirement: 五個台文方案 submodule
專案 SHALL 在 `schemes/` 下以 git submodule 管理 5 個台文方案，`Rime-Logo/` 以 git submodule 管理圖示。`.gitmodules` SHALL 包含：
- `schemes/Rime-HanLo` → `git@github.com:i3thuan5/Rime-HanLo.git`
- `schemes/Rime-POJ` → `git@github.com:i3thuan5/Rime-POJ.git`
- `schemes/Rime-POJHan` → `git@github.com:i3thuan5/Rime-POJHan.git`
- `schemes/Rime-TOJ` → `git@github.com:i3thuan5/Rime-TOJ.git`
- `schemes/Rime-TsuanLo` → `git@github.com:i3thuan5/Rime-TsuanLo.git`
- `Rime-Logo/` → `git@github.com:i3thuan5/Rime-Logo.git`

#### Scenario: clone 後可初始化 submodule
- **WHEN** 執行 `git submodule update --init --recursive`
- **THEN** `schemes/` 下 5 個目錄與 `Rime-Logo/` SHALL 各自 checkout 對應 repo 內容

### Requirement: Ubuntu 24.04 multi-stage Dockerfile
`packaging/Dockerfile-24.04` SHALL 包含 5 個 stage（deps / builder / packager / verifier / exporter），base image SHALL 為 `ubuntu:24.04`。verifier stage SHALL 在乾淨的 `ubuntu:24.04` 中安裝 .deb 並驗證關鍵檔案存在。exporter stage SHALL 繼承 verifier，使 `docker build` 預設執行驗證。

#### Scenario: Dockerfile base image
- **WHEN** 讀取 `packaging/Dockerfile-24.04`
- **THEN** 第一個 `FROM` SHALL 為 `FROM ubuntu:24.04 AS deps`

#### Scenario: 5 個 stage 存在
- **WHEN** 讀取 `packaging/Dockerfile-24.04`
- **THEN** SHALL 包含 `AS deps`、`AS builder`、`AS packager`、`AS verifier`、`AS exporter`

#### Scenario: verifier 驗證關鍵檔案
- **WHEN** `docker build` 執行到 verifier stage
- **THEN** `dpkg -i /tmp/deb/*.deb` SHALL 成功，且 `/usr/lib/*/fcitx5/lib${SCHEME_ID}.so` 等路徑 SHALL 存在

### Requirement: build-all.sh 一次 build 5 個方案
`packaging/build-all.sh` SHALL 讀取 `packaging/schemes.conf`，依序 build 5 個方案的 .deb，版本號 SHALL 使用 `${APP_VERSION}~ubuntu24.04` 格式，輸出至 `build/deb/`，以 `docker run --rm TAG > output.deb` 方式提取。

#### Scenario: 執行 build-all.sh
- **WHEN** 執行 `./packaging/build-all.sh 2.0.0`
- **THEN** SHALL 在 `build/deb/` 產出 5 個 .deb，檔名格式為 `fcitx5-{id}_2.0.0~ubuntu24.04_ubuntu24.04.deb`

### Requirement: Travis CI 監聽 ithuan-ubuntu24.04 branch
`.travis.yml` SHALL 在 `branches.only` 包含 `ithuan-ubuntu24.04`，CI job SHALL 執行 `./packaging/build-all.sh`，並使用 `services: docker`。

#### Scenario: Travis CI 設定正確
- **WHEN** 讀取 `.travis.yml`
- **THEN** `branches.only` SHALL 包含 `ithuan-ubuntu24.04`，job script SHALL 為 `./packaging/build-all.sh`
