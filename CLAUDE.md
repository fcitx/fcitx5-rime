# CLAUDE.md

## 操作規則

- **不可** 執行 `git commit` 或 `git add`（會影響 staged/working tree 狀態）
- **可以** 執行 `git stash`、`git diff`、`git log`、`git show` 等唯讀或暫存指令
- commit 一律由使用者自己執行

## 專案背景

- 這是 fcitx5-rime 的台文版本（ithuan-taibun），目前工作在 `ithuan-ubuntu24.04` branch
- 目標：為 Ubuntu 24.04 LTS（fcitx5 5.1.7）打包 5 個台文輸入法方案的 .deb

## 參考資料位置

- `other/fcitx5-rime/`：**ithuan-taibun** branch 的完整快照（Ubuntu 26.04，fcitx5 5.1.13+）
- `compat/ubuntu-24.04` git branch：Ubuntu 24.04 compat 實作（從 `8f80c36` 起點，有 rimeaction、rimefactory 分離）
- 兩者都可參考，**ithuan-taibun 移植品質較好**，但 compat 對 5.1.7 API 的適配更完整

## 當前 branch 的關鍵特性（92a4410 起點）

- `find_package(Fcitx5Core 5.1.7 REQUIRED)` 已正確，不需降版
- `namespace fcitx`（無 `::rime`），`FCITX_ADDON_FACTORY`（無 V2）
- **無** `rimefactory.cpp/h`、`rimeaction.cpp/h`（這兩個在更新的 commit 才引入）
- `FCITX_ADDON_FACTORY(fcitx::RimeEngineFactory)` 在 `src/rimeengine.cpp` 最後一行
- `add_fcitx5_addon()` macro 在 fcitx5 5.1.7 **不存在**，要用 `add_library(MODULE)`

## 移植時的關鍵差異（vs compat/ubuntu-24.04）

| 項目 | compat 分支 | 我們的 92a4410 | 處理方式 |
|------|------------|---------------|----------|
| ADDON_FACTORY 位置 | rimefactory.cpp（分離） | rimeengine.cpp 最後一行 | 加 `#ifndef HAVE_SCHEME_CONFIG` guard |
| scheme_factory.cpp.in include | `rimefactory.h` | `rimeengine.h` | 改用 `rimeengine.h` |
| namespace | `fcitx` | `fcitx` | 一致，直接用 |
| ADDON_FACTORY macro | `FCITX_ADDON_FACTORY` | `FCITX_ADDON_FACTORY` | 一致，直接用 |
| SCHEME_SOURCES | 含 rimeaction.cpp | 無此檔案 | 移除 rimeaction.cpp |
| add_library | `add_library(MODULE)` | 需要 MODULE | 與 compat 相同 |

## 需要新增/修改的檔案清單

### 修改既有檔案
- `src/rimeengine.cpp`：最後的 `FCITX_ADDON_FACTORY` 加 `#ifndef HAVE_SCHEME_CONFIG` guard
- `CMakeLists.txt`：加 `if(SCHEME_ID)` 分支（include schemes.cmake）
- `.travis.yml`：改 branch name 為 `ithuan-ubuntu24.04`，改描述為 Ubuntu 24.04

### 新增檔案（從 compat 移植，注意差異）
- `src/scheme_config.h.in`：compat 版直接用
- `src/scheme_factory.cpp.in`：改 include `rimeengine.h`，其餘同 compat
- `src/scheme.conf.in`：compat 版直接用
- `src/scheme-addon.conf.in.in`：compat 版直接用
- `schemes.cmake`：compat 版，但移除 SCHEME_SOURCES 裡的 `rimeaction.cpp`
- `.gitmodules`：compat 版直接用
- `packaging/Dockerfile-24.04`：compat 版直接用
- `packaging/build-all.sh`：compat 版直接用
- `packaging/schemes.conf`：compat 版直接用
- `packaging/PKGBUILD`：compat 版直接用
- `packaging/fcitx5-scheme.spec`：compat 版直接用

### 不需要
- `.github/workflows/build-24.04.yml`（使用者確認只用 Travis CI）
