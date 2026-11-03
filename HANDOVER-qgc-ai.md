# QGC AI 诊断助手 —— 项目交接文档

> 交接对象：基于官方 QGroundControl **v5.0.8** + AI 飞行诊断功能的二次开发工程
> 最后更新：2026-10-30
> **当前状态：编译通过、可运行**；AI 诊断页已进菜单，尚未做功能验收，尚未接入自有模型

---

## 1. 仓库信息

| 项 | 值 |
|---|---|
| 本地路径 | `C:\Users\amov_pometheus\Desktop\qgc-ai` |
| 远程仓库 | `https://github.com/2333zy/qgc-ai`（public） |
| 基线 | 官方 tag **`v5.0.8`** = commit `e0816c957602789200ae5ba0af45217f0f2f1db4`（2025-10-08） |
| 当前 HEAD | `main` = `5f0ed0357` |
| 构建版本号 | `v5.0.8.1-ai.1-3-g5f0ed0357`（CMake 自动生成） |
| 许可证 | Apache-2.0。**分发时必须保留 `COPYING.md` 与各文件头版权声明** |
| AI 代码来源 | 参考 `amovlgf/qgroundcontrol` 的 `ai/mavlink-console-assistant-v5.0.8` 分支（基点同为官方 v5.0.8，作者 lvguofei） |

远端配置：

```
origin    → 2333zy/qgc-ai              自己的仓库，日常 push
upstream  → mavlink/qgroundcontrol      官方仓库，按需 cherry-pick 修复
amov      → amovlgf/qgroundcontrol      AI 代码来源，可选（跟进原作者更新）
```

### 现有提交（相对官方 v5.0.8 的 7 条）

| sha | 说明 | 类别 |
|---|---|---|
| `753a0b9a1` | Add MAVLink console AI assistant（初版，Console 内嵌，+2968 行） | 功能 |
| `9dba9acea` | Prepare AI assistant Windows test build 5.0.8.1 | 打包 |
| `ebea0e7db` | Add Codex auth and PX4 AI diagnostics（Codex 登录 + PX4 诊断工具） | 功能 |
| `e56105713` | Handle prerelease tags in CMake version | 构建 |
| `823585e0e` | Add PX4 AI flight diagnostics analyze page（重构为独立 Analyze 页） | 功能 |
| `5f869595d` | fix(tools): preserve desktop input method when launching QGC | Windows 修复 |
| `5f0ed0357` | Add ChatGPT proxy configuration and refresh AI assistant documentation | 功能 |

> 这 7 条的 git author 仍是原作者。**我们自己的提交请统一加 `feat(AI):` / `fix(AI):` 前缀**，方便和来源代码区分。

---

## 2. 本机开发环境（已实测可用，不要随意换版本）

| 组件 | 版本 | 路径 |
|---|---|---|
| Visual Studio **Build Tools** 2022 | 17.14.37827.9 | `D:\VS\BuildTools` |
| MSVC 编译器 | 14.44.35207 (19.44) | `D:\VS\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe` |
| Windows SDK | 10.0.26100.0 | |
| **Qt** | **6.8.3** `win64_msvc2022_64` | `D:\Qt\6.8.3\msvc2022_64` |
| CMake | 4.4.4 | winget 安装 |
| Ninja | 1.13.2 | winget 安装 |
| Python | 3.13.14 | 用于 `aqtinstall` 装 Qt |

### ⚠️ 机器上另有 `D:\Qt\6.12.0`，本项目绝对不能用

原因（两条都是硬性阻断）：

1. `CMakeLists.txt` 第 172~178 行把 Qt 版本锁死为 `6.8.3...6.8.3`（最小 = 最大），6.12.0 会被 CMake 直接拒绝；
2. 6.12.0 缺三个**必需**模块：`Charts`（6.12 已改名 `Graphs`）、`Core5Compat`、`ImageFormats`。

**纪律**：永远用绝对路径调用 `D:\Qt\6.8.3\msvc2022_64\bin\qt-cmake.bat`，不要把 6.12 的 `bin` 放进 PATH。

### Qt 6.8.3 必须包含的 13 个模块（与官方 CI 一致，见 `.github/workflows/windows.yml` 第 92 行）

```
qtcharts qtlocation qtpositioning qtspeech qt5compat qtmultimedia qtserialport
qtimageformats qtshadertools qtconnectivity qtquick3d qtsensors
```

需要重装时：

```powershell
python -m aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 --outputdir D:\Qt -m qtcharts qtlocation qtpositioning qtspeech qt5compat qtmultimedia qtserialport qtimageformats qtshadertools qtconnectivity qtquick3d qtsensors
```

---

## 3. 编译（每次新开窗口都要走这 4 步）

```powershell
# 1) 加载 MSVC 环境（Build Tools 版没有"Native Tools 命令提示符"快捷方式，必须手动加载）
& "D:\VS\BuildTools\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -SkipAutomaticLocation

# 2) 确认编译器可见（必须能看到 cl.exe，看不到就别往下走）
where.exe cl

# 3) 配置（首次，或改动了 CMakeLists/依赖时执行）
cd C:\Users\amov_pometheus\Desktop\qgc-ai
D:\Qt\6.8.3\msvc2022_64\bin\qt-cmake.bat -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DQGC_ENABLE_GST_VIDEOSTREAMING=OFF -DQGC_ENABLE_QT_VIDEOSTREAMING=ON

# 4) 编译（首次约 20~40 分钟）
cmake --build build --parallel
```

产物：`build\Debug\QGroundControl.exe`

**两个参数不能省**：

- `-DQGC_ENABLE_GST_VIDEOSTREAMING=OFF` —— 关掉 GStreamer 视频后端（它要求精确 `1.22.12` 版本 + pkg-config，本地极难满足）
- `-DQGC_ENABLE_QT_VIDEOSTREAMING=ON` —— 改用 QtMultimedia（内置 FFmpeg）视频后端，官方 CI 注释也认可这条路径

---

## 4. 运行

**开发时（最快，改完代码只需重编译）**：

```powershell
$env:PATH = "D:\Qt\6.8.3\msvc2022_64\bin;" + $env:PATH
.\build\Debug\QGroundControl.exe
```

**要"双击就能跑"（给人演示、交付测试）**：

```powershell
cmake --install build
# 产物：build\staging\QGroundControl.exe，可整体拷贝到别的机器
```

**为什么不能直接双击 `build\Debug\QGroundControl.exe`**：Debug 构建只产出 exe，缺 Qt 运行时 DLL（报错里的 `Qt6Chartsd.dll`、`Qt6QuickControls2d.dll` 等，结尾的 `d` 表示 Debug 版）。官方安装包之所以"装了就能用"，是因为安装程序执行了 windeployqt 部署 —— 上面两个方案就是在补这一步。

---

## 5. 代码地图（AI 部分合计约 5000 行）

```
src/AnalyzeView/
├── AIDiagnosticController.cc/.h       2661 + 273 行  ★核心：会话循环、LLM 请求、工具调用、答案渲染
├── AIDiagnosticContextBuilder.cc/.h    286 +  38 行   把 Vehicle 打包成 JSON 快照
├── AIDiagnosticProvider.h               24 行         工具/上下文抽象接口
├── PX4DiagnosticProvider.cc/.h         304 +  39 行    PX4 专用诊断（工具注册与执行都在这）
├── PX4DiagnosticEvidence.cc/.h         291 +  48 行    诊断证据结构（"结论附原始数据"）
├── CodexAppServerClient.cc/.h          521 + 102 行    外部 codex CLI 子进程 + OAuth【待替换】
└── AIDiagnosticPage.qml                337 行          独立 Analyze 页界面

src/Settings/
├── AIAssistant.SettingsGroup.json       79 行          设置项定义
└── AIAssistantSettings.cc/.h            26 + 27 行     设置组注册
```

### 注册点（改 AI 功能只需要关心这几处）

| 位置 | 作用 |
|---|---|
| `src/AnalyzeView/CMakeLists.txt` 第 15~21、51 行 | 源文件与 QML 登记 |
| `src/API/QGCCorePlugin.cc` 第 108 行 | 把 `AIDiagnosticPage.qml` 挂进 Analyze 菜单 |
| `src/Settings/SettingsManager.cc/.h` | 注册 `aiAssistantSettings()` |
| `src/Settings/AIAssistantSettings.cc` | `DECLARE_SETTINGGROUP` + `qmlRegisterUncreatableType` |

### 现有设置项（`AIAssistant.SettingsGroup.json`）

| 字段 | 说明 |
|---|---|
| `authMethod` | `0` = API Key，`1` = Sign in with ChatGPT |
| `endpointUrl` | OpenAI 兼容端点，默认空 |
| `modelName` | 端点侧模型名 |
| `chatGptModelName` | Codex App Server 提供的模型 id |
| `chatGptProxyUrl` | 支持 http / https / socks5 / socks5h，改完需重启 QGC |
| `apiKey` | Bearer token，本地端点可留空 |
| `oauthDeviceAuthorizationUrl` / `oauthTokenUrl` / `oauthClientId` / `oauthScope` | **已废弃**，仅为设置兼容保留，不参与逻辑 |

### 现有工具能力（`PX4DiagnosticProvider.cc`）

- 工具名：`request_mavlink_message`、`set_message_interval`
- 子系统 → 飞控 shell 只读命令映射，例如：
  `sensors status`、`listener sensor_accel 1`、`listener sensor_mag 1`、`listener estimator_status 1`、`listener battery_status 1`、`commander status`、`mavlink status`、`ver all`
- **已内置中文关键词**：`参数`、`解锁`、`起飞检查`、`数传`、`陀螺`、`版本`、`固件` 等（作者已做了一部分中文本地化）

---

## 6. 一次提问发生了什么（数据流）

```
用户在 AIDiagnosticPage.qml 输入问题
  → AIDiagnosticController::ask()
  → AIDiagnosticContextBuilder 生成飞机状态 JSON
       （armed / flightMode / prearmError / readyToFly / allSensorsHealthy /
         requiresGpsFix / 坐标 / 电池 / 各 FactGroup）
  → 按 authMethod 选通道
       API Key  → 直接 POST endpointUrl（OpenAI 兼容 /v1/chat/completions）
       ChatGPT  → CodexAppServerClient 起子进程 + OAuth 登录，走 Codex
  → 模型返回 tool_call → PX4DiagnosticProvider 执行
       （读参数 / 发 MAVLink 请求 / 跑只读 nsh 命令）
  → 工具结果回填模型，多轮迭代直到得出结论
  → 回答 + PX4DiagnosticEvidence（引用原始数值）渲染到页面
```

---

## 7. 已知问题与既定决策（接手必读）

| # | 问题 | 现状 / 决策 |
|---|---|---|
| 1 | `libevents` 依赖跟 `main` 分支，导致编译报 `mavlink_msg_target_field` 找不到 | **已修**：`src/MAVLink/LibEvents/CMakeLists.txt` 锁到 `9ef591c447fe0386d698bf6fb9a6d27e43988ee4`（v5.0.8 发布时的版本）。**不要改回 `main`** |
| 2 | GStreamer 视频后端要精确 1.22.12 + pkg-config | **已决策**：编译时关闭，用 QtMultimedia 后端。将来做低延迟 FPV 图传再补装 |
| 3 | AI 强依赖外部 `codex` CLI + OAuth 登录 | **待改造（P1 首要任务）**：保留 `AIDiagnosticProvider` 抽象，用通用 HTTP Provider 替换 `CodexAppServerClient` |
| 4 | 只实现了 PX4 诊断（`PX4DiagnosticProvider`） | ArduPilot 未覆盖；QGC 两边都支持，只做 PX4 会丢一半用户 |
| 5 | 分支名 `ai/mavlink-console-assistant-*` 名不副实 | 功能早已迁到独立 Analyze 页，名字是历史遗留，**不要被误导** |
| 6 | `ulog_cpp` 等其它依赖同样跟 `main` | 若后续编译报 C++ API 错误，用同样方法锁到 2025-10-08 之前的提交 |
| 7 | CMake 4.4.4 较新 | 当前未触发兼容问题；若报 `Compatibility with CMake < 3.5 has been removed`，配置时追加 `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` |
| 8 | Debug 构建双击缺 DLL | 见第 4 节，开发用 PATH，交付用 `cmake --install` |

---

## 8. 后续开发计划

| 阶段 | 内容 | 预估 |
|---|---|---|
| **P0** | 验收现有能力：MockLink 连模拟飞机 → 问"为什么飞不了" → 看是否给出带证据的结论 | 0.5 人日 |
| **P1** | 替换 Codex 依赖：新增 `OpenAiCompatibleProvider`（Ollama / DeepSeek / 通义 / OpenAI 一套通吃），设置页去掉 ChatGPT 登录项 | 2 人日 |
| **P2** | 扩充工具集：参数检索（按名匹配，禁止全量塞入）、传感器健康位、日志列表与分析、航线校验 | 3~5 人日 |
| **P3** | 全工具强制带证据（`PX4DiagnosticEvidence` 已有骨架）、中文提示词与输出 | 2 人日 |
| **P4** | 代码收拢到 `src/AI/`，减少与上游文件的耦合，为提 PR 做准备 | 2 人日 |
| **P5** | NSIS 安装包 + GitHub Actions CI | 2 人日 |

### 上游 PR 策略（如果需要）

**不要整体提交"AI 面板"** —— 官方对 PR 极其保守（最近一个飞控板 PR 被要求从 8 个文件砍到 2 个文件才合并）。拆两步：

1. 先提**只读诊断数据接口**（工具注册表 + 参数/传感器查询），不含任何模型调用，diff 小、对上游有独立价值；
2. 再提**可插拔 LLM 接口 + 可选 UI**，构建开关默认 OFF。

---

## 9. Git 工作流

```
main              官方 v5.0.8 基线 + 我们的 AI 改动，永远保持可编译
feature/xxx       功能分支，完成后合回 main
tag v5.0.8-ai.N   每个可交付版本
```

提交信息前缀：`feat(AI):` / `fix(AI):` / `chore(deps):` / `docs:`

**按需捞官方修复（不要整体升级）**：

```powershell
git fetch upstream --tags
git cherry-pick <某个修复的 sha>
```

**跟进原作者的 AI 更新（可选）**：

```powershell
git fetch amov
git log --oneline main..amov/ai
git merge amov/ai
```

**开工前先确认环境干净**：

```powershell
git status
git log --oneline -3
git merge-base v5.0.8 main      # 应输出 e0816c95…，证明基线未漂移
```

---

## 10. 接手第一天 checklist

- [ ] 按第 3 节编译一次，产出 `build\Debug\QGroundControl.exe`
- [ ] 按第 4 节运行，界面正常
- [ ] 确认 **Analyze 菜单里有 `AI Flight Diagnostics (PX4)`**
- [ ] 确认 Application Settings 里有 AI 配置页（端点 URL / 模型 / API key）
- [ ] MockLink（Application Settings → Mock Link）连模拟飞机，点开 AI 页确认不崩、能出快照
- [ ] 精读 `AIDiagnosticController.cc` 的 `ask()` 与工具调用循环（2661 行里最关键的是前 300 行）
- [ ] 决定第一个接入的模型（Ollama / DeepSeek / OpenAI），开始 P1

---

## 附：一句话总结

> 这是一份**官方 v5.0.8 + 社区 AI 诊断原型**的工程。编译和运行问题已全部踩平（见第 7 节 8 条）。
> 接下来的核心工作是：**把 Codex 依赖换成通用模型接口 → 把工具集从 2 个扩到 10 个 → 让所有结论带原始数据证据**。
> 地基不是模型，是工具层。

---

## 附二：勘误与阶段 0 验收（2026-11-02 补）

本文件第 5、6 节的以下描述已被实测证伪/过时，**以 `AI-STAGE0-ACCEPTANCE.md` 为准**：

1. **工具不是 2 个，是 10 个**（6 个本地即时 + 2 个控制台只读 + 2 个需确认的 MAVLink 数据请求）。
2. **P1 不需要「新增 OpenAiCompatibleProvider」**：`authMethod=0` 的通用 OpenAI 兼容 HTTP 通路已存在，
   实测把端点指向 DeepSeek 即可工作，无需改代码。P1 的真实内容是删 Codex 分支 + 做供应商预设 + 流式 + 中文化。
3. 代码地图漏了 `src/UI/AppSettings/AIAssistantSettings.qml`（197 行）；`PX4LogParser.cc` **不是 AI 代码**。

另新增两条硬事实：设置组名是遗留的 `[AIConsole]`；`--unittest` 在本 fork 启动即崩（`QGCLogging::log` 0xC0000005），
重构前需要先修测试设施。

阶段 0 六项验收**全部通过**，DeepSeek 端到端产出带原始数值证据的中文结论。

