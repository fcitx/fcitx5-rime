## Context

目前 `ithuan-ubuntu24.04` branch 停在 commit `92a4410`，這是 main 線上最後一個 fcitx5 5.1.7-compatible 的 commit（下一個 commit `395500a` 要求 5.1.9）。

此起點的 src/ 特性：
- `FCITX_ADDON_FACTORY(fcitx::RimeEngineFactory)` 在 `rimeengine.cpp` 最後一行
- `RimeEngineFactory::create` 在 `rimeengine.h` 以 **inline body** 定義（非 declaration-only）
- 無 `rimefactory.cpp/h`（在 `8f80c36` 才分離，比我們的起點新 31 個 commit）
- 無 `rimeaction.cpp/h`（同上）
- `namespace fcitx`（無 `::rime`）
- `fcitx-utils/standardpath.h`（單數），`StandardPath::global()`
- `src/CMakeLists.txt` 已使用 `add_library(rime MODULE ...)`（fcitx5 5.1.7 無 `add_fcitx5_addon()` macro）

參考來源：
- `other/fcitx5-rime/`：**ithuan-taibun** branch（Ubuntu 26.04，fcitx5 5.1.13+，移植品質較好）
- `compat/ubuntu-24.04` git branch：從 `8f80c36` 起點建立，已完整處理 5.1.7 API 適配

## Goals / Non-Goals

**Goals:**
- 在 `ithuan-ubuntu24.04` branch 建立完整的台文 .deb 打包流程
- 5 個方案（hanlo、poj、pojhan、toj、tsuanlo）各產出獨立 .deb
- Travis CI 自動 build + verify（`ubuntu:24.04` Docker 環境）
- 盡量不改 92a4410 原有檔案；必要修改以最小侵入為原則

**Non-Goals:**
- 不加 GitHub Actions（只用 Travis CI）
- 不支援 Ubuntu 26.04 或其他版本
- 不修改任何 C++ 邏輯（只加 `#ifdef` guard 與 CMake 參數化）

## 每個檔案的來源與修改性質

| 檔案 | 來源 | 說明 |
|------|------|------|
| `src/rimeengine.cpp` | **修改 92a4410**（最小改動） | 最後一行加 `#ifndef HAVE_SCHEME_CONFIG` guard（詳見下方說明） |
| `CMakeLists.txt` | **修改 92a4410** | 加 `if(SCHEME_ID) include(schemes.cmake) else ...` 分支 |
| `.travis.yml` | **修改現有** | 加 `ithuan-ubuntu24.04` 到 `branches.only`，改 job 描述為 Ubuntu 24.04 |
| `src/scheme_config.h.in` | **從 compat 複製，不需調整** | 四個巨集定義，compat 與 ithuan-taibun 內容相同 |
| `src/scheme_factory.cpp.in` | **全新撰寫（2 行）** | compat 版不可用（見決定 2）；只需 include + FCITX_ADDON_FACTORY |
| `src/scheme.conf.in` | **從 compat 複製，不需調整** | InputMethod 設定樣板 |
| `src/scheme-addon.conf.in.in` | **從 compat 複製，不需調整** | Addon 設定樣板 |
| `schemes.cmake` | **從 compat 複製，移除一行** | 移除 `rimeaction.cpp`（92a4410 無此檔）；`add_library(MODULE)` 已是 compat 版，照用 |
| `.gitmodules` | **從 compat 複製，不需調整** | 5 個方案 + Rime-Logo |
| `packaging/Dockerfile-24.04` | **從 compat 複製，不需調整** | base image 已是 `ubuntu:24.04` |
| `packaging/build-all.sh` | **從 compat 複製，不需調整** | `UBUNTU_VER=24.04`，版本號加 `~ubuntu24.04` |
| `packaging/schemes.conf` | **從 compat 複製，不需調整** | 5 個方案參數 |
| `packaging/PKGBUILD` | **從 compat 複製，不需調整** | Arch Linux 打包 |
| `packaging/fcitx5-scheme.spec` | **從 compat 複製，不需調整** | RPM spec |

## Decisions

### 決定 1：rimeengine.cpp 必須加 `#ifndef HAVE_SCHEME_CONFIG` guard

**問題根源**：`rimeengine.cpp` 最後一行呼叫 `FCITX_ADDON_FACTORY`，而新增的 `scheme_factory.cpp`（從 `scheme_factory.cpp.in` generate）也呼叫它。兩者都被 link 進同一個 `lib${SCHEME_ID}.so`，linker 看到兩個 `fcitx_module_init` → 錯誤。

**這是新加的程式碼造成的問題**，92a4410 原本沒有這個問題。

**為何不能只改 scheme_factory.cpp.in？** 若 `scheme_factory.cpp` 不呼叫 `FCITX_ADDON_FACTORY`，整個 `.so` 沒有 `fcitx_module_init` 入口點，fcitx5 無法載入。兩個編譯單元都必須各守其職，只能讓 `rimeengine.cpp` 在 scheme build 時讓位。

**修改極小**：只在現有最後一行上下各加一行 guard，不動任何邏輯：
```cpp
#ifndef HAVE_SCHEME_CONFIG
FCITX_ADDON_FACTORY(fcitx::RimeEngineFactory)
#endif
```
`schemes.cmake` 對 scheme target 設定 `HAVE_SCHEME_CONFIG` define，一般 rime build 不受影響。

### 決定 2：scheme_factory.cpp.in 全新撰寫，不從 compat 複製

compat 版的 `scheme_factory.cpp.in` 除了 `#include "rimefactory.h"` 外，還重新定義了 `RimeEngineFactory::create`（改用 `"fcitx5-@SCHEME_ID@"` i18n domain）。

這在 92a4410 **無法使用**，原因有二：
1. `rimefactory.h` 不存在（在 `8f80c36` 才分離）
2. `rimeengine.h` 的 `RimeEngineFactory::create` 已有 **inline body**；C++ 不允許再提供 out-of-line 定義，編譯會直接報 redefinition error

因此 `scheme_factory.cpp.in` 只需做一件事：提供 `FCITX_ADDON_FACTORY` 入口。`create` 的實作沿用 `rimeengine.h` 的 inline 版本（i18n domain 為 `"fcitx5-rime"`，台文用途可接受）：

```cpp
#include "rimeengine.h"

FCITX_ADDON_FACTORY(fcitx::RimeEngineFactory)
```

這是 92a4410 可能的最小實作，完全不需修改任何 header。

### 決定 3：schemes.cmake 的 SCHEME_SOURCES 不含 rimeaction.cpp

compat 的 `SCHEME_SOURCES` 含 `rimeaction.cpp`，因為 compat 起點 `8f80c36` 已有它。92a4410 無此檔，從 compat 複製 `schemes.cmake` 時直接移除那一行——不是刪除功能，只是不加入不存在的檔案。

### 決定 4：schemes.cmake 使用 add_library(MODULE)

92a4410 的 `src/CMakeLists.txt` 已是 `add_library(rime MODULE ...)`，因為 fcitx5 5.1.7 沒有 `add_fcitx5_addon()` macro。新增的 `schemes.cmake` 照同樣寫法：`add_library(${SCHEME_ID} MODULE ...)`，與 compat 版相同（compat 已適配 5.1.7）。ithuan-taibun 使用 `add_fcitx5_addon()`，那是 5.1.13+ 的寫法，不適用。

### 決定 5：Travis CI 只監聽 ithuan-ubuntu24.04（不加 GitHub Actions）

使用者確認 CI 只走 Travis，不需 `.github/workflows/build-24.04.yml`。

## Risks / Trade-offs

- **submodule 需 GitHub 存取** → Travis CI 有網路，本地離線時需注意
- **verifier stage 需 apt-get install fcitx5 librime1** → 需要網路，Ubuntu 24.04 apt 鏡像在 EOL（2029）前可用
- **rimeengine.cpp 的最小修改** → 若未來 upstream rebase 有 conflict，只有這一處三行需要注意

## Migration Plan

按以下順序執行（相依性由上而下）：
1. 修改 `src/rimeengine.cpp`（加 guard）
2. 修改 `CMakeLists.txt`（加 `if(SCHEME_ID)` 分支）
3. 修改 `.travis.yml`（加 branch，改描述）
4. 新增 4 個 `src/*.in` 樣板檔案
5. 新增 `schemes.cmake`
6. 新增 `.gitmodules` 並初始化 submodule
7. 新增 `packaging/` 目錄全部檔案
8. 手動執行 `./packaging/build-all.sh` 驗證

Rollback：`git revert` 各 commit 即可；`.gitmodules` 移除對應項目再 deinit submodule。

## Open Questions

（無）
