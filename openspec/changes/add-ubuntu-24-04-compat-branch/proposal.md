## Why

Ubuntu 24.04 LTS（Noble Numbat）支援到 2029-04，是真濟保守用戶長期所在的版本。但是 main 分支現此時 `REQUIRED_FCITX_VERSION=5.1.13`，而 24.04 內建 fcitx5 只有 5.1.7-1build3。

研究過社群 PPA（`ppa:fcitx-team/nightly`、`ppa:hosxy/fcitx5`、`ppa:ikuya-fruitsbasket/fcitx5` 等），無一个同時滿足「信用、安全、現用、版本到 5.1.13、有 librime backport」這幾項條件。即便 ikuya 的有 fcitx5 5.1.11，仍未到 5.1.13，且為個人標明「testing purpose」的 untrusted PPA。

因此選擇 A4 路線：**從 88d3e39^（5.1.7-era ancestor）開分支 `compat/ubuntu-24.04`，cherry-pick 台文 parameterization 相關 commits，新建 Dockerfile-24.04**。比起「從 HEAD 往後扣 5.1.8+ API」的 A1 revert 路線，A4 的起點本身就是為 fcitx5 5.1.7 era 設計的 known-good code，首次 build 通過率較高。本 change 不做 main 後續 API-neutral fix 的回填——等發版後若有需要，另外發 change 處理。

## What Changes

- 新增 long-lived branch `compat/ubuntu-24.04`，從 main 的 `88d3e39^`（fcitx5 5.1.7-era ancestor，Nov 2024）切出
- 在該分支上 cherry-pick 台文 parameterization 相關 commits（對應已 archive 的 `add-parameterized-scheme-build` change），但**只取 source/CMake 部份**（`schemes.cmake`、`src/scheme_*.in*`、`src/CMakeLists.txt` 改動），**不帶 packaging/Dockerfile** 相關 commits（在 88d3e39^ 起點本來無 `packaging/` 目錄）
- 初始化 `schemes/` 與 `Rime-Logo/` submodule（5 個 scheme：hanlo、poj、pojhan、toj、tsuanlo）
- 在 compat 分支上**新建** `packaging/Dockerfile-24.04`（參考 main 的 `Dockerfile-26.04` 五 stage 設計：`deps`、`builder`、`packager`、`verifier`、`exporter`，base image 用 `ubuntu:24.04`）
- 在 compat 分支上**新建** `packaging/build-all.sh`、`packaging/schemes.conf`、`packaging/PKGBUILD`、`packaging/fcitx5-scheme.spec`：
  - `UBUNTU_VER=24.04` 寫死
  - 內部 .deb Version 加 `~ubuntu24.04` 後綴（依 Debian 版號慣例，`~` 排序在 `.` 之前，表示「2.0.0 的降版變種」）
  - 輸出檔名為 `fcitx5-{scheme}_{APP_VERSION}~ubuntu24.04_ubuntu24.04.deb`
- cherry-pick 過程處理 V2 macro 衝突：`FCITX_ADDON_FACTORY_V2(@SCHEME_ID@, fcitx::rime::RimeEngineFactory)` → `FCITX_ADDON_FACTORY(fcitx::rime::RimeEngineFactory)`
- 新增 `.github/workflows/build-24.04.yml`：trigger 為 push 到 `compat/ubuntu-24.04` 或 PR 對它；build 5 个 .deb，上傳做 release artifact
- Release 策略：兩條 pipeline 各自 release。tag `v2.0.0` 對 main 出 5 个 26.04 .deb；tag `v2.0.0-ubuntu24.04` 對 compat 分支出 5 个 24.04 .deb
- 5 个 scheme（hanlo、poj、pojhan、toj、tsuanlo）攏要做 24.04 包

## Capabilities

### New Capabilities

- `ubuntu-24-04-compat-branch`：定義 `compat/ubuntu-24.04` long-lived branch 的存在、起點、cherry-pick policy、.deb 命名與版號慣例、Dockerfile-24.04 multistage 結構、CI/Release 策略，以及主分支對該 branch 的維護責任

### Modified Capabilities

無。本 change 不改動既有 main 分支的 spec。`linux-packaging`、`multistage-deb-build`、`parameterized-build`、`scheme-addon-identity`、`scheme-submodules` 攏維持現狀，僅在 compat 分支上有對應的衍生實作。

## Impact

- **新增 git branch**：`compat/ubuntu-24.04`（long-lived，從 `88d3e39^` 起）
- **影響檔案（compat 分支上）**：
  - 新增：`packaging/` 整個目錄（含 `Dockerfile-24.04`、`build-all.sh`、`schemes.conf` 等）、`.github/workflows/build-24.04.yml`、`schemes.cmake`、`src/scheme_*.in*`
  - 修改：`CMakeLists.txt`（加 `REQUIRED_FCITX_VERSION 5.1.7`）、cherry-pick 過程處理 V2 macro 衝突
- **影響檔案（main 分支上）**：僅本 change 的 openspec 檔案
- **無 API 變更，無 breaking changes** 對既有 26.04 build path
- **CI/Release 加成本**：兩條 pipeline 各自 build/release，運算費約加倍
- **長期維護成本**：低；compat 分支發版後即固定，後續 fix 視需要再另外提 change（不在本範圍）
