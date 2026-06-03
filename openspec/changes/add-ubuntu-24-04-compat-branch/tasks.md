## 約定

實作本 change 時，working tree 跟 `git checkout 88d3e39^` 後一致——即 main HEAD 之前約半年的歷史狀態，packaging/、schemes.cmake、src/scheme_*.in* 等檔案攏不存在。

要參考 main 的「現在狀態」（例：Dockerfile-26.04 結構、build-all.sh 邏輯、schemes.conf 內容、CI workflow 模板），SHALL 從 `other/fcitx5-rime/` 直接讀檔案。SHALL NOT 用 `git show <hash>:path` 或 `git log` 翻歷史——working tree 直接看效率高且無 hash 漂移風險。

## 1. 確認起點 + 開分支

- [ ] 1.1 從 main 找出 `88d3e39^` 的實際 commit hash，記錄於 commit message 或 PR description
- [ ] 1.2 `git checkout -b compat/ubuntu-24.04 88d3e39^`
- [ ] 1.3 驗證起點 src/ 內容：`CMakeLists.txt` 應無 `REQUIRED_FCITX_VERSION` 行；`src/rimefactory.cpp` 應用 `FCITX_ADDON_FACTORY`（無 V2）；`src/rimeengine.cpp` 應 include `<fcitx-utils/standardpath.h>`（單數）
- [ ] 1.4 確認 88d3e39^ 起點本來無 `packaging/` 目錄
- [ ] 1.5 `git push -u origin compat/ubuntu-24.04`

## 2. Cherry-pick 台文 parameterization（source/CMake 部份）

- [ ] 2.1 找出 main 上 archive 過的 `add-parameterized-scheme-build` change 對應的 commits（可看 `other/fcitx5-rime/openspec/changes/archive/2026-05-25-add-parameterized-scheme-build/tasks.md` 對照 git log）
- [ ] 2.2 依時序 cherry-pick **只取 source/CMake 部份**：`schemes.cmake` 引入、`src/scheme_factory.cpp.in`、`src/scheme-addon.conf.in.in`、`src/scheme.conf.in`、`src/scheme_config.h.in`、`src/CMakeLists.txt` 相關改動
- [ ] 2.3 跳過 packaging 相關 commits（`packaging/Dockerfile-22.04`、`Dockerfile-25.10` 等都不帶過，因為本分支要 build 24.04）
- [ ] 2.4 處理 V2 macro 衝突：`scheme_factory.cpp.in` 內 `FCITX_ADDON_FACTORY_V2(@SCHEME_ID@, fcitx::rime::RimeEngineFactory)` 改為 `FCITX_ADDON_FACTORY(fcitx::rime::RimeEngineFactory)`
- [ ] 2.5 處理 namespace 衝突：若 88d3e39^ 是 `namespace fcitx { }` 而非 `namespace fcitx::rime { }`，cherry-pick 的 commit 引入的 `namespace fcitx::rime` 寫法**可保留**（C++17 nested namespace 不需 fcitx5 5.1.8+ 支援），但須確保 ADDON_FACTORY 註冊路徑相符
- [ ] 2.6 確保 `.gitmodules` 有帶到 5 個 scheme submodule（Rime-HanLo、Rime-POJ、Rime-POJHan、Rime-TOJ、Rime-TsuanLo）與 Rime-Logo
- [ ] 2.7 `git submodule update --init --recursive`

## 3. CMakeLists.txt 設定 REQUIRED_FCITX_VERSION

- [ ] 3.1 在 `CMakeLists.txt` 加 `set(REQUIRED_FCITX_VERSION 5.1.7)`（位置仿 main 風格，靠近 `project()` 之後）
- [ ] 3.2 改 `find_package(Fcitx5Core ...)` 帶 version 參數：`find_package(Fcitx5Core ${REQUIRED_FCITX_VERSION} REQUIRED)`
- [ ] 3.3 Commit：「Pin REQUIRED_FCITX_VERSION to 5.1.7 for Ubuntu 24.04」

## 4. 新建 Dockerfile-24.04（仿 main 的 Dockerfile-26.04 結構）

- [ ] 4.1 參考 `other/fcitx5-rime/packaging/Dockerfile-26.04` 結構，建立 `packaging/Dockerfile-24.04`
- [ ] 4.2 Stage 1 `deps`：`FROM ubuntu:24.04`，`apt-get install` 所有 build deps（`build-essential`、`cmake`、`extra-cmake-modules`、`gettext`、`git`、`pkg-config`、`fcitx5-modules-dev`、`libfcitx5core-dev`、`librime-dev`）
- [ ] 4.3 Stage 2 `builder`：`FROM deps`，宣告 SCHEME_* 與 APP_VERSION ARGs，COPY 全部 source，`cmake` + `make` + `make install DESTDIR=/staging`
- [ ] 4.4 Stage 3 `packager`：`FROM builder`，產生 `DEBIAN/control`（Version 用 `${APP_VERSION}`，APP_VERSION 由 build-all.sh 傳入完整 `2.0.0~ubuntu24.04`），`dpkg-deb --build`
- [ ] 4.5 Stage 4 `verifier`：`FROM ubuntu:24.04`，`apt-get install fcitx5 librime1`，`dpkg -i /tmp/deb/*.deb`，全部 `RUN ls/test` 驗證關鍵檔案存在
- [ ] 4.6 Stage 5 `exporter`：`FROM verifier`，`USER nobody`，`CMD ["sh", "-c", "cat /output/*.deb"]`

## 5. 新建 build-all.sh 與 packaging 周邊

- [ ] 5.1 參考 `other/fcitx5-rime/packaging/build-all.sh`，建立 compat 分支版本
- [ ] 5.2 將 `UBUNTU_VER` 寫死為 `24.04`（移除 from-arg 彈性）
- [ ] 5.3 將 `DOCKERFILE` 路徑寫做 `Dockerfile-24.04`
- [ ] 5.4 將 build-arg `APP_VERSION` 包成 `${APP_VERSION}~ubuntu24.04`（例：當原 APP_VERSION=2.0.0 時，傳入 docker 的 APP_VERSION 為 `2.0.0~ubuntu24.04`）
- [ ] 5.5 將 `DEB_FILE` 命名改為 `fcitx5-${ID}_${APP_VERSION}~ubuntu24.04_ubuntu24.04.deb`
- [ ] 5.6 增加 echo 訊息明確顯示 build target 為 Ubuntu 24.04
- [ ] 5.7 建立 `packaging/schemes.conf`，內容對照 `other/fcitx5-rime/packaging/schemes.conf`（5 行：hanlo、poj、pojhan、toj、tsuanlo）
- [ ] 5.8 視需要建立 `packaging/PKGBUILD`、`packaging/fcitx5-scheme.spec`，內容對照 `other/fcitx5-rime/packaging/` 對應檔案（Fedora/Arch 參考）

## 6. 首次 build 試驗

- [ ] 6.1 在本機（有 docker）執行 `./packaging/build-all.sh 2.0.0`
- [ ] 6.2 確認 `build/deb/` 出現 5 个 .deb：`fcitx5-{hanlo,poj,pojhan,toj,tsuanlo}_2.0.0~ubuntu24.04_ubuntu24.04.deb`
- [ ] 6.3 若 compile 失敗，逐 error 排查：可能漏 revert 的 API（eventloopinterface、BulkCursor、notification 改動），手動補修
- [ ] 6.4 每修一次提一個 commit，message 註明「revert <api> for fcitx5 5.1.7 compat」

## 7. 安裝驗證（在 Ubuntu 24.04 環境）

- [ ] 7.1 起 ubuntu:24.04 container 或 LXC，`apt install fcitx5`
- [ ] 7.2 `dpkg -i fcitx5-hanlo_2.0.0~ubuntu24.04_ubuntu24.04.deb`
- [ ] 7.3 確認 `dpkg -l fcitx5-hanlo` 顯示 Version 為 `2.0.0~ubuntu24.04`
- [ ] 7.4 啟動 fcitx5 daemon，確認 `fcitx5-configtool` 出現「漢羅輸入法」候選
- [ ] 7.5 重複 7.2-7.4 於剩餘 4 個 scheme
- [ ] 7.6 驗證五個方案能同時安裝（並存）

## 8. CI workflow（在 main 分支上加）

- [ ] 8.1 參考 `other/fcitx5-rime/.github/workflows/` 內現有的 workflow 結構（若無對應 GitHub Actions 模板，依 `other/fcitx5-rime/.travis.yml` 改寫）
- [ ] 8.2 在 main 分支新增 `.github/workflows/build-24.04.yml`
- [ ] 8.3 trigger：`on: { push: { branches: [compat/ubuntu-24.04] }, pull_request: { branches: [compat/ubuntu-24.04] } }`
- [ ] 8.4 jobs：checkout、`docker --version` 確認、執行 `./packaging/build-all.sh 2.0.0`
- [ ] 8.5 上傳產出的 5 个 .deb 為 workflow artifact
- [ ] 8.6 在 main 分支 PR + merge

## 9. 首次 release

- [ ] 9.1 在 compat 分支打 tag：`git tag v2.0.0-ubuntu24.04`、`git push origin v2.0.0-ubuntu24.04`
- [ ] 9.2 在 GitHub 開 release：title `v2.0.0-ubuntu24.04`，附 5 个 .deb 為 release asset
- [ ] 9.3 Release notes 寫明：「為 Ubuntu 24.04 LTS 用戶建置；無 5.1.8+ 功能；fcitx5 系統需求為 >= 5.1.7」

## 10. Archive 本 change

- [ ] 10.1 全部 tasks 完成、verify 通過、release 發出後
- [ ] 10.2 在 main 分支執行 `openspec archive add-ubuntu-24-04-compat-branch`
- [ ] 10.3 確認 archive 進 `openspec/changes/archive/` 並 update `openspec/specs/ubuntu-24-04-compat-branch/spec.md`
