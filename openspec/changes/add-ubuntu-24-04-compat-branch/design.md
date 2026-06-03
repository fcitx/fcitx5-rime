## Context

main 分支 HEAD 要求 fcitx5 >= 5.1.13；Ubuntu 24.04 LTS（Noble Numbat）官方 repo 內 fcitx5 為 5.1.7-1build3。差距源自三個 5.1.8+ API 的引入：

1. `<fcitx-utils/standardpaths.h>` 與 `StandardPaths::global()` API（commit `4c620df`，May 2025）
2. `FCITX_ADDON_FACTORY_V2` macro（commit `88d3e39`，Nov 2024）
3. `<fcitx-utils/eventloopinterface.h>` 等其他 headers（需驗證）

社群 PPA 全部失敗（見 proposal）：fcitx-team 兩個 PPA 已超過十年無維護、hosxy PPA 只支援 20.04、ikuya-fruitsbasket PPA 只到 5.1.11 且為個人 untrusted PPA。

之前討論過兩條路：
- **A1 (revert path)**：從 HEAD 切分支，往後扣 5.1.8+ API。風險：archaeology 推測難免漏，首次 build 通過率受影響。
- **A4 (rebase path)**：從歷史的 5.1.7-era ancestor 切分支，往前疊台文 parameterization。優點：起點本身就是 known-good 5.1.7 設計。

選 **A4**：cherry-pick 範圍縮限於台文 parameterization 的 source/CMake 部份，目的單純——讓 24.04 用戶儘快有可用的 .deb。後續 main 上的 API-neutral fix、其他改進，若有需要，另外提 change 處理；本 change 不背這份維護承諾。

相關 stakeholders：Ubuntu 24.04 LTS 用戶（目標到 2029-04 維護期間）、台文輸入法社群。

## Goals / Non-Goals

**Goals:**

- 為 Ubuntu 24.04 LTS 用戶提供 hanlo、poj、pojhan、toj、tsuanlo 五個方案的 .deb 套件
- 起手 build 即可通過，無 archaeology guess 風險
- .deb 內部版本與檔名與 26.04 build 區分清楚，apt 排序語意正確
- 兩條 release pipeline 完全獨立，互不影響

**Non-Goals:**

- 不追求 main 與 compat 分支功能對等。compat 分支可缺漏 5.1.8+ 功能（例：notification list mode、新候選詞 BulkCursor 改進、StandardPaths 帶來的路徑處理升級）
- 不嘗試讓一份 source 同時 build 兩個版本（無 `#ifdef` 策略）
- 不引入 PPA 依賴。compat 分支的用戶安裝 .deb 後，runtime 完全依靠 Ubuntu 24.04 官方 fcitx5 5.1.7 與 librime ~1.8.5
- 不維護 `compat/ubuntu-22.04` 或其他更舊版本。22.04 用戶請自行從 source 編譯或升級
- 不形式化 cherry-pick policy 文件。本 change 預期 compat 分支發版後即「凍結」，後續若有需要 backport，另外提 change 討論並執行
- 不在 main 加 `#ifdef`、無意把 compat 分支的限制傳染回 main

## Decisions

### D1：採 A4 而非 A1 revert 或 A4-hybrid

**Decision**：從 `88d3e39^` checkout 新分支，只 cherry-pick 台文 parameterization commits。不 cherry-pick main 後續 API-neutral fixes（如 `bc60a06`、`d76a3c3`）。

**Why**：
- A1 倚賴對「5.1.13 → 5.1.7 要 revert 啥」的盤點，目前只有 70% 把握（StandardPath、FACTORY_V2 已確認；eventloopinterface、BulkCursor、notification 改動需 compile 驗證）
- A4 起點本身就是設計給 fcitx5 5.1.7-ish 環境的 known-good code，首次 build 通過率高
- A4-hybrid 雖然能拿回 main 的 API-neutral fix，但會引入維護承諾（每个 main 新 commit 都要判斷是否帶過去）；本 change 的定位係「24.04 用戶有 .deb 可用」即可，後續若真有需要 backport 再另外提 change，避免本 change 範圍蔓延

**Alternatives considered**：
- A1 (revert)：上面已分析，盤點不確定性高
- A2 (#ifdef)：殺雞用牛刀；main 為了 24.04 永久背負舊 API 包袱，使用者既已決定「24.04 不需要維護 26.04 功能」就不適用
- A3 (patches)：technically 可行但 patch 隨 main 漂移會頻繁失效，比 branch 多一層間接
- A4-hybrid：好處實在，但對應的維護承諾超出本 change 範圍。延後到「真有需要 backport 某條 fix」時再評估

### D2：起點選 `88d3e39^` 而非更前

**Decision**：選 `88d3e39^`（commit "Move code to namespace fcitx::rime and use FCITX_ADDON_FACTORY_V2" 的父 commit）作為分支起點。

**Why**：
- 該 commit 是 main 上**第一次**設定 `REQUIRED_FCITX_VERSION=5.1.12` 的 commit。其父 commit `88d3e39^` 為「最後一個未限定 fcitx5 5.1.12+ 的狀態」，是自然的 5.1.7-era 邊界
- 在 `88d3e39^` 這个時間點，source/CMake 有三項特徵可佐證它是「為舊 fcitx5 設計的 known-good code」：
  - **CMakeLists.txt 的 `project(fcitx5-rime VERSION 5.1.9)`**：專案家己彼時的 release 號碼（毋是要求 fcitx5 版本），表示這時點是 5.1.9 release 一陣，上線過、穩定
  - **`src/rimefactory.cpp` 用 `FCITX_ADDON_FACTORY`（無 V2 後綴的舊 macro）**：證明 source 編譯對的 fcitx5 還未要求 V2 macro，對應 5.1.7~5.1.9 範圍
  - **`src/*.cpp` 用 `namespace fcitx`（無 `::rime` 子 namespace）**：證明 source 結構猶未做過後續的 namespace 整理（這項本身是 C++ 語法問題，不會直接擋編譯，純做時間佐證）
- 太早會錯過其他重要功能（如 librime 1.7.0 適配）；太晚會踩到 5.1.8+ API

**Alternatives considered**：
- 從 main fork 直接 revert：見 D1
- 找上游 fcitx5-rime 對應 fcitx5 5.1.7 的 release tag：本 repo 是 fork，跟上游版本對位需要額外確認，反而引入不確定性

### D3：.deb 版號用 `2.0.0~ubuntu24.04`

**Decision**：control 檔的 `Version` 欄位寫做 `${APP_VERSION}~ubuntu24.04`（例：`2.0.0~ubuntu24.04`）。

**Why**：
- Debian 版號慣例：`~` 排序在 `.` 之前。`2.0.0~ubuntu24.04` < `2.0.0`，語意「24.04 build 是 2.0.0 的降版變種」
- 確保 apt 不會誤判 24.04 .deb 比 26.04 .deb 新；若用戶手動切換或混用 repo 也安全
- 比「兩個都 `2.0.0`」更清楚：dpkg/apt 工具可直接從 Version 區分

**Alternatives considered**：
- 兩個 .deb 都 `2.0.0`：apt 不能從 Version 區分，只能從檔名。安裝後 `dpkg -l` 看不出來
- `2.0.0+ubuntu24.04`：`+` 排序在 `.` 之後，語意「24.04 build 是 2.0.0 之後的衍生」，跟「降版」直覺相反。雖然兩個 .deb 不會同時安裝，但 metadata 看起來有違常識
- `2.0.0-ubuntu24.04`：`-` 在 Debian 版號是 revision separator（upstream-revision），可能被工具誤解析

### D4：CI/Release 兩條 pipeline 各自 release

**Decision**：main 分支用 tag `v{X.Y.Z}` + 26.04 release；compat 分支用 tag `v{X.Y.Z}-ubuntu24.04` + 24.04 release。

**Why**：
- Release notes 可各自書寫，內容反映各自支援功能（24.04 release 可說明「無 notification list mode、無 BulkCursor 改進」等限制）
- 用戶下載頁面顯然清晰：26.04 用戶只見 26.04 .deb，反之亦然
- CI workflow 可獨立調整 timeout、build matrix、cache 策略

**Alternatives considered**：
- 兩條 pipeline 合一 release：使用者可能誤抓錯版。release notes 也難寫
- 共用 tag `v{X.Y.Z}` 含 10 個 .deb：tag 字串無法表達「24.04 build 用 5.1.7-era source」

## Risks / Trade-offs

**[R1] 24.04 用戶誤裝 26.04 build** → Version 後綴 `~ubuntu24.04` + 檔名 `_ubuntu24.04.deb` 雙重區分；release 頁分開避免下載錯

**[R2] librime 在 24.04 是 1.8.5（已 >= 1.7.0 最低要求）** → 已有 `FCITX_RIME_NO_HIGHLIGHT_CANDIDATE`（< 1.10）、`FCITX_RIME_NO_DELETE_CANDIDATE`（< 1.8）的 #ifdef，cmake 會自動處理。風險低

**[R3] 起手 cherry-pick V2 macro 等衝突解錯** → 衝突解法明文寫入 spec scenario；compile 通過是最後驗證

**[R4] `88d3e39^` 之前可能有未驗證的 5.1.8+ API 用法** → compat 分支在 24.04 容器中實際 build 一次即可驗證；不通過再修

**[R5] 用戶若同時有 main 跟 compat 兩個 repo 註冊** → `~ubuntu24.04` 版號保證 apt 自動偏好新版（26.04 build）。但 24.04 系統根本無法安裝 26.04 build（fcitx5 dep 不滿足），實務上不會混淆

**[R6] main 後續若有對 src/ 的重要 fix，compat 分支不會自動帶過去** → 接受。compat 分支發版即固定；若 24.04 用戶遇到 main 後續修掉的 bug，他們可開 issue，再評估是否另起 change backport

## Migration Plan

本 change 並非取代既有功能，係新增。無 backward compatibility 問題。

部署順序建議：

1. 在 main 分支先把本 openspec change 與相關 docs（compat-branches.md）merge 入
2. 切 compat/ubuntu-24.04 分支並做 cherry-pick 與初次 build 驗證
3. 在本機 Ubuntu 24.04 環境驗證 5 個 .deb 安裝、fcitx5 啟動、輸入法正常運作
4. 切 GitHub release 公開
5. Rollback：若發現問題，從 GitHub release 撤下 .deb 即可。compat 分支 git history 保留供後續修補

## Open Questions

- [Q1] 維護政策文件放 `CONTRIBUTING.md` 抑或單獨 `docs/compat-branches.md`？建議單獨檔案以利未來加 `compat/ubuntu-XX.XX` 同類分支時擴充
- [Q2] CI workflow 是否啟用 docker layer cache（如 actions/cache）？因 build-all.sh 跑 5 次 docker build 共用 deps stage，cache 有大幅加速空間。但本 change 預設先求綠燈，cache optimization 可後續另外提 change
- [Q3] 24.04 build 出的 .deb 是否上傳到自家 apt repo？若有則用戶可 `apt install fcitx5-hanlo`；本 change 暫不處理，先以 GitHub release 為唯一發布管道
