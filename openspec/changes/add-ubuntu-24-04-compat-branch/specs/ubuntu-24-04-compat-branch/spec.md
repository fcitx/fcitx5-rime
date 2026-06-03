## ADDED Requirements

### Requirement: long-lived branch 存在
專案 SHALL 維護一條 long-lived git branch 名為 `compat/ubuntu-24.04`，專門為 Ubuntu 24.04 LTS 用戶提供 fcitx5-rime 台文方案的 .deb 套件。

該分支不需要追平 main 的所有功能，可以缺漏 fcitx5 5.1.8+ 才提供的 feature。

#### Scenario: 分支存在於 remote
- **WHEN** 執行 `git ls-remote origin compat/ubuntu-24.04`
- **THEN** SHALL 回傳該 branch 的 commit hash

#### Scenario: 分支命名
- **WHEN** 列出所有 long-lived branches
- **THEN** SHALL 看到 `compat/ubuntu-24.04`，且名稱不為 `24.04-compat`、`ubuntu-24.04`、`legacy-24.04` 等其他形式

### Requirement: 分支起點
`compat/ubuntu-24.04` SHALL 從 main 分支的 `88d3e39^` 為起點切出。`88d3e39^` 是 main 上最後一個 REQUIRED_FCITX_VERSION 未被設成 5.1.12+ 的 commit，對應 project version 5.1.9，目標 fcitx5 5.1.7-era。

#### Scenario: 起點驗證
- **WHEN** 執行 `git merge-base compat/ubuntu-24.04 main`
- **THEN** SHALL 回傳 `88d3e39^` 或其祖先

#### Scenario: 起點理由
- **WHEN** 開發者欲了解為何選擇 `88d3e39^`
- **THEN** 設計文件 SHALL 說明：該 commit 之前 main 尚未引入 `FCITX_ADDON_FACTORY_V2` 與 `StandardPaths` API，是最後一个能 build 於 fcitx5 5.1.7 environment 的歷史點

### Requirement: 起手 cherry-pick 範圍
分支建立後 SHALL 從 main 上 archive 過的 `add-parameterized-scheme-build` change 對應的 commits 中，cherry-pick 台文 parameterization 的 **source/CMake 部份**：`schemes.cmake`、`src/scheme_factory.cpp.in`、`src/scheme-addon.conf.in.in`、`src/scheme.conf.in`、`src/scheme_config.h.in`、`src/CMakeLists.txt` 相關改動，以及 `.gitmodules`、`schemes/` submodule 設定、`Rime-Logo/` submodule 設定。

SHALL NOT cherry-pick 主分支後續其他 commits（API-neutral fix、Dockerfile 演進等）；該些工作未來如有需要，另外發 change 處理。

#### Scenario: 完成初期 cherry-pick
- **WHEN** 分支建立完成
- **THEN** SHALL 包含 hanlo、poj、pojhan、toj、tsuanlo 五个 scheme 的 parameterization 資料

#### Scenario: 不帶 Dockerfile/packaging 相關 commits
- **WHEN** 列舉 compat 分支的 commit 紀錄
- **THEN** SHALL NOT 有 cherry-pick from main 的 `packaging/Dockerfile-25.10`、`packaging/Dockerfile-26.04` 相關 commits；packaging 整套於 compat 分支新建

#### Scenario: V2 macro 衝突解法
- **WHEN** cherry-pick 觸發 `FCITX_ADDON_FACTORY_V2(@SCHEME_ID@, fcitx::rime::RimeEngineFactory)` 衝突
- **THEN** SHALL 改寫為 `FCITX_ADDON_FACTORY(fcitx::rime::RimeEngineFactory)`

### Requirement: CMakeLists.txt 版本下調
`compat/ubuntu-24.04` 分支上 `CMakeLists.txt` SHALL 設定 `REQUIRED_FCITX_VERSION` 為 5.1.7。

#### Scenario: 版本設定
- **WHEN** 讀取 compat 分支的 `CMakeLists.txt`
- **THEN** SHALL 含 `set(REQUIRED_FCITX_VERSION 5.1.7)`

### Requirement: Dockerfile-24.04 multistage 結構
`packaging/Dockerfile-24.04` SHALL 包含 5 個 stage：`deps`、`builder`、`packager`、`verifier`、`exporter`，仿照 main 上 `Dockerfile-26.04` 的結構，但 base image 改為 `ubuntu:24.04`。

#### Scenario: Stage 名稱存在
- **WHEN** 讀取 `packaging/Dockerfile-24.04`
- **THEN** SHALL 包含 `AS deps`、`AS builder`、`AS packager`、`AS verifier`、`AS exporter` 共 5 个 stage 宣告

#### Scenario: Base image
- **WHEN** 讀取 `packaging/Dockerfile-24.04`
- **THEN** 第一個與 verifier 的 `FROM` 宣告 SHALL 為 `FROM ubuntu:24.04`

#### Scenario: Verifier 驗證 .deb
- **WHEN** `docker build` 執行到 verifier stage
- **THEN** `dpkg -i /tmp/deb/*.deb` 成功，且所有關鍵檔案存在的 `RUN ls/test` 通過

#### Scenario: Exporter 繼承 verifier
- **WHEN** 執行 `docker build -f Dockerfile-24.04 ...`（不指定 `--target`）
- **THEN** verifier stage 的所有 RUN 指令皆被執行

### Requirement: build-all.sh 寫死 24.04
`compat/ubuntu-24.04` 分支的 `packaging/build-all.sh` SHALL 將 `UBUNTU_VER` 寫死為 `24.04`，並使用 `packaging/Dockerfile-24.04`。

#### Scenario: UBUNTU_VER 寫死
- **WHEN** 讀取 compat 分支的 `packaging/build-all.sh`
- **THEN** SHALL 看到 `UBUNTU_VER=24.04` 或相當的寫死宣告
- **THEN** SHALL NOT 看到 `UBUNTU_VER=26.04` 或可由 arg 改變的彈性

### Requirement: .deb 內部 Version 加 ~ubuntu24.04 後綴
24.04 build 出的 .deb 套件 DEBIAN/control SHALL 將 `Version` 設為 `${APP_VERSION}~ubuntu24.04`，使用 `~` 符號（Debian 版號慣例，排序在 `.` 之前），語意為「2.0.0 的降版變種」。

#### Scenario: Version 後綴
- **WHEN** 對 24.04 build 出的 .deb 執行 `dpkg-deb -f <file>.deb Version`
- **THEN** SHALL 回傳 `2.0.0~ubuntu24.04`（假設 APP_VERSION=2.0.0）

#### Scenario: 與 26.04 build 相對排序
- **WHEN** 將 24.04 build 的 .deb 跟 26.04 build 的 .deb 放做伙比較版本
- **THEN** `dpkg --compare-versions "2.0.0~ubuntu24.04" lt "2.0.0"` SHALL 為真

### Requirement: 五個 scheme 攏要做包
`compat/ubuntu-24.04` 分支 `packaging/build-all.sh` 執行完 SHALL 為以下五个 scheme 各產生一个 .deb 套件：hanlo、poj、pojhan、toj、tsuanlo。

#### Scenario: 五个包完成
- **WHEN** 執行 `./packaging/build-all.sh`
- **THEN** `build/deb/` 目錄下 SHALL 出現 5 个 .deb 檔，scheme ID 完整覆蓋 hanlo、poj、pojhan、toj、tsuanlo

### Requirement: .deb 輸出檔名
24.04 build 出的 .deb 檔名 SHALL 為 `fcitx5-{SCHEME_ID}_{APP_VERSION}~ubuntu24.04_ubuntu24.04.deb` 格式。

#### Scenario: hanlo .deb 檔名
- **WHEN** 執行 `./packaging/build-all.sh` 完成
- **THEN** SHALL 出現 `fcitx5-hanlo_2.0.0~ubuntu24.04_ubuntu24.04.deb`

#### Scenario: 與 26.04 build 並存
- **WHEN** 兩條 pipeline 各自輸出 .deb 到合併目錄
- **THEN** 26.04 build 為 `fcitx5-hanlo_2.0.0_ubuntu26.04.deb`，24.04 build 為 `fcitx5-hanlo_2.0.0~ubuntu24.04_ubuntu24.04.deb`，兩者檔名與內部版本攏可區分

### Requirement: CI workflow
專案 SHALL 在 main 分支上加 `.github/workflows/build-24.04.yml`，trigger 為 push 到 `compat/ubuntu-24.04` 或 PR 對它；workflow 執行 `./packaging/build-all.sh`，產出的 .deb 上傳做 release artifact。

#### Scenario: workflow 存在
- **WHEN** 查看 main 分支 `.github/workflows/`
- **THEN** SHALL 看到 `build-24.04.yml`

#### Scenario: trigger 設定
- **WHEN** 讀取 `build-24.04.yml`
- **THEN** SHALL 看到 `on: { push: { branches: [compat/ubuntu-24.04] }, pull_request: { branches: [compat/ubuntu-24.04] } }` 或相當設定

### Requirement: Release 策略
24.04 build 與 26.04 build SHALL 各自有獨立 git tag 與 GitHub release。

#### Scenario: 26.04 release
- **WHEN** main 分支發版
- **THEN** SHALL 用 tag `v{APP_VERSION}`（例 `v2.0.0`），release 內含 5 个 ubuntu26.04 .deb

#### Scenario: 24.04 release
- **WHEN** compat 分支發版
- **THEN** SHALL 用 tag `v{APP_VERSION}-ubuntu24.04`（例 `v2.0.0-ubuntu24.04`），release 內含 5 个 ubuntu24.04 .deb

