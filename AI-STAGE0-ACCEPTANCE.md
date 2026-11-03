# AI 飞行诊断 —— 阶段 0 验收报告

> 验收日期：2026-11-02
> 基线：`v5.0.8` + 7 条社区 AI 提交，HEAD `7eaaf1712`
> 结论：**通过**。DeepSeek 真实端到端跑通，结论带原始数据证据。
> 本报告同时勘误 `HANDOVER-qgc-ai.md` 中已被实测证明过时的 3 处描述。

---

## 1. 一句话结论

代码里 **Codex 不是唯一通路**。`authMethod = 0`（API Key）走的是标准 OpenAI 兼容 HTTP，
实测把端点指向 DeepSeek 即可工作，**不需要改一行代码**。阶段 0 的全部验收项均已通过，
证据见第 3 节。

---

## 2. 验收项逐条结果

| # | 验收项 | 结果 | 证据 |
|---|---|---|---|
| 1 | 编译产出 `build\Debug\QGroundControl.exe` | ✅ | `ninja: no work to do`，exit 0；exe 114 MB |
| 2 | 程序启动，界面正常 | ✅ | Fly 视图、离线地图、工具栏、中文界面均正常 |
| 3 | Analyze 菜单里有 AI 页面 | ✅ | 主菜单 → Analyze Tools → **AI 飞行诊断** |
| 4 | 设置页有 AI 配置项 | ✅ | 设置 → **AI 助手**（身份验证 / 端点 URL / 模型 / API 密钥） |
| 5 | MockLink 连模拟机能出快照 | ✅ | 页面状态行：`PX4 飞行器 128 已连接 — 未解锁，链路可用。` |
| 6 | 提问能得到带证据的结论 | ✅ | 见第 3 节，引用 20+ 个原始字段 |

---

## 3. 真实问答证据（DeepSeek `deepseek-chat`）

提问：**「这架飞机现在处于什么飞行模式？是否已经满足起飞条件？请引用快照里的具体数值作为依据。」**

回答要点（节选，均为界面原文）：

- 飞行模式：`core.flightMode = "Manual"`，`armed = false`，`flying = false`，`landing = false`
- 可起飞依据：`healthAndArmingCheckReport` 的 `canArm`/`canTakeoff`/`canStartMission` 均为 `true`，
  `hasWarningsOrErrors = false`，`prearmError` 为空字符串
- GPS：`gps.lock = 3`（3D 锁定），`count = 8` 颗星，`hdop = 3.0`，`vdop = 3.0`
- 位置：`core.coordinate` = 47.397 / 8.5455 / 488.056 m，`distanceToHome = 0.0 m`
- 风险 1 链路：`mavlinkLossPercent = 22.80%`（2591 / 11364），`messagesLost = 2581`
- 风险 2 电池：`battery1.percentRemaining = 1%`，`voltage = 12.60 V`，`timeRemaining = 9 s`，
  `chargeState = 4`（Emergency）；`battery2` 25%，`chargeState = 3`（Critical）
- 风险 3 振动：`vibration.xAxis = 50.5`，`yAxis = 105`，`zAxis = 60.0`，`clipCount1/2/3 = 1/2/3`
- 工具调用痕迹：`EKF2_EV_CTRL 在已加载参数中不可用（state: unavailable）`、
  「输入探测与融合控制探测均未执行（`unknown`）」→ 说明模型确实发起了工具调用并消费了工具结果
- 语义边界：明确说明 `sysStatusSensorInfo` 里 GPS `Disabled` 只是 `SYS_STATUS` 位，
  与 `gps.lock = 3` 不矛盾；外部视觉不可达时**拒绝推断**
- 结论：「模式为 Manual；系统自检报告允许起飞，但依据快照中的电池（1% / 9s）、
  链路丢包（22.8%）和振动削波数值，当前不建议起飞。」

**这一条同时验证了**：中文提问 → 中文作答（语言策略生效）、真实快照、工具层、
「确认事实 vs 推测原因」分层、以及只读边界（模型没有任何写参数/解锁动作）。

---

## 4. 交接文档勘误

### 4.1 工具不是 2 个，是 10 个

`AIDiagnosticController::_buildToolDefinitions()` 已注册 10 个工具：

| 类别 | 工具 | 权限 |
|---|---|---|
| 本地即时（6） | `get_vehicle_status`、`get_fact_group`、`get_parameter`、`search_parameters`、`get_health_report`、`get_link_status` | 自动执行 |
| PX4 控制台只读（2） | `query_sensor_status`、`query_px4_param` | 自动执行，命令白名单校验 |
| MAVLink 数据（2） | `request_mavlink_message`、`set_message_interval` | **需用户确认后发送** |

因此 P2「把工具集从 2 个扩到 10 个」这个任务描述已经不成立；
真实缺口是**工具定义与执行都硬编码在 2945 行的 Controller 里**（加一个工具要改 4 处），
以及缺日志/任务校验类工具。

### 4.2 P1「新增 OpenAiCompatibleProvider」不需要新建传输层

`_postChatRequest()` 已经是通用 OpenAI 兼容实现：

| 环节 | 实现 | 是否通用 |
|---|---|---|
| 请求 | `endpointUrl` 原样作 POST URL（**不拼路径**） | ✅ |
| 鉴权 | `Authorization: Bearer <apiKey>`，留空则不发 | ✅ |
| 请求体 | `model` / `messages` / `temperature: 0.2` / `stream: false` / `tools` | ✅ |
| 响应 | `choices[0].message.content` + `choices[0].message.tool_calls` | ✅ |

DeepSeek 实测返回的 `tool_calls` 结构（`id` / `type` / `function.name` /
`function.arguments` 为 **JSON 字符串**）与上述解析完全一致，多轮 `role=tool` 回填也通过。

**P1 的真实内容是**：删 Codex 分支（约 1100 行）、把端点做成供应商预设 + 自动补路径、
加流式输出、错误文案中文化。不是「新建一个 Provider」。

### 4.3 代码地图有遗漏，且有一个误认

- 缺 `src/UI/AppSettings/AIAssistantSettings.qml`（197 行，其中 ChatGPT 登录块 122 行）
  与 `src/UI/AppSettings/SettingsPagesModel.qml`（菜单挂载）
- `src/AnalyzeView/PX4LogParser.cc`（85 行）**不是 AI 代码**，是上游的地标图像解析器，
  被同一目录放置容易误判

---

## 5. 实测发现的问题（按影响排序）

| # | 问题 | 影响 | 建议 |
|---|---|---|---|
| 1 | `--unittest` 启动即崩：`QGCLogging::log` 处 0xC0000005（`QTestLog::enterTestFunction` → `msgHandler`） | 单元测试不可用，后续重构没有安全网 | 放进 P1 一并修，先修日志处理器在测试期的空指针 |
| 2 | QSettings 用 `SHGetKnownFolderPath` 取 AppData，**忽略 `APPDATA` 环境变量** | 无法做隔离配置，测试必须写真机 `%APPDATA%` | 文档记录，验证脚本用真机配置 |
| 3 | `endpointUrl` 原样当 URL，不补 `/v1/chat/completions` | 用户必须手打全路径，是接入失败的头号原因 | P1 供应商预设里解决 |
| 4 | `stream: false` | 长答案期间界面只有转圈 | P1 加 SSE |
| 5 | 错误文案全英文硬编码（`_formatNetworkErrorText` 等） | 中文用户看不懂失败原因 | P1 |

未覆盖项（需后续补验）：控制台类工具在 MockLink 上的**成功路径**（本次只观察到
`state: unavailable` 的降级返回，符合预期但非成功样本）；MAVLink 工具的确认弹窗未触发。

---

## 6. 环境事实补充（交接文档未写）

| 项 | 值 |
|---|---|
| 设置文件 | `%APPDATA%\QGroundControl\QGroundControl Daily.ini` |
| AI 设置组名 | `[AIConsole]`（**遗留名**，`DECLARE_SETTINGGROUP(AIAssistant, "AIConsole")`，不是改名后的新组） |
| 通道标识 | 本机构建带 `-DQGC_DAILY_BUILD`，窗口标题为 `QGroundControl Daily` |
| 界面语言 | 跟随 `qLocaleLanguage`，当前为中文，故菜单标签是 `AI 飞行诊断` / `AI 助手`（交接文档写的是英文标签） |
| DeepSeek 实测 | `model=deepseek-chat` 请求，服务端回报 `model=deepseek-flash` |

---

## 7. 提交信息规范

官方仓库 **不使用 conventional commits**（没有 `feat:` / `fix(scope):`），
主流格式是：`[Scope: ]祈使句`，首字母大写、结尾不加句号；40 条官方提交里仅 15 条带 body。

**本 fork 改为中文提交信息**（官方风格仅作参考，供将来需要向上游提 PR 时改写）：

```
AI: <中文祈使句>      AI 相关功能/修复（保留 AI: 前缀便于与上游提交区分、便于过滤）
<中文祈使句>          通用缺陷与构建修复
<中文祈使句>          文档
```

规则：

- 标题用中文祈使句，不以 `Fix` / `Add` 这类英文动词开头
- `AI:` 前缀保留英文：它同时是范围标记和 grep 锚点，不翻译
- 正文用中文说明「为什么改」；函数名、字段名、MAVLink/参数标识符保持原文
- 正文为 UTF-8。Windows 下建议写入文件后 `git commit -F <file>`，
  避免 `-m` 传中文时的编码问题

示例：

```
AI: 使用 SSE 流式输出对话补全结果

控制器原先要等一整份 JSON 响应回来才处理，模型工作期间页面只有一个转圈……
```

> ⚠️ 若将来向上游提 PR，官方只接受英文提交信息。届时另开英文分支，
> 不要把中文历史直接推给上游。

---

## 8. 复现步骤

```powershell
# 1) 编译（首次约 20~40 分钟）
& "D:\VS\BuildTools\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
D:\Qt\6.8.3\msvc2022_64\bin\qt-cmake.bat -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug `
    -DQGC_ENABLE_GST_VIDEOSTREAMING=OFF -DQGC_ENABLE_QT_VIDEOSTREAMING=ON
cmake --build build --parallel

# 2) 配置 DeepSeek（写 %APPDATA% 的真实 profile）
#    [AIConsole]
#    authMethod=0
#    endpointUrl=https://api.deepseek.com/v1/chat/completions
#    modelName=deepseek-chat
#    apiKey=sk-...

# 3) 运行
$env:PATH = "D:\Qt\6.8.3\msvc2022_64\bin;" + $env:PATH
.\build\Debug\QGroundControl.exe
```

界面操作：主菜单 → Application Settings → Mock Link → **PX4 Vehicle** →
退出设置 → 主菜单 → Analyze Tools → **AI 飞行诊断** → 输入问题 → 提问。

GUI 自动化验证脚本放在 `.dev/`（已 gitignore，不进仓库）。
