# -*- coding: utf-8 -*-
"""Fill the untranslated zh_CN UI strings for QGroundControl.

Pipeline:  extract -> translate (DeepSeek) -> apply -> verify

Only entries whose translation is still marked `type="unfinished"` are touched, and
only in the requested .ts file. Everything else is left byte-for-byte untouched.

Usage:
    python translate-ts.py extract   <ts> <work.json>
    python translate-ts.py run       <ts> <work.json> [--batches N]
    python translate-ts.py apply     <work.json>
    python translate-ts.py verify    <ts>
"""

import io
import json
import os
import re
import sys
import time
import urllib.request

API_URL = "https://api.deepseek.com/v1/chat/completions"
API_KEY = os.environ.get("DEEPSEEK_API_KEY", "")
BATCH_SIZE = 25

# Terms that must stay identical across the whole UI. Mined by hand from the
# already-translated half of qgc_source_zh_CN.ts so new strings match old ones.
GLOSSARY = """
Vehicle => 飞行器
vehicle => 飞行器
Armed / armed => 已解锁
Disarmed / disarmed => 未解锁
Arm => 解锁
Disarm => 解锁关闭
Flight Mode => 飞行模式
Guided => 引导
Loiter => 悬停
Stabilize => 增稳
Altitude Hold => 定高
Failsafe => 失效保护
Waypoint => 航点
Waypoints => 航点
Mission => 任务
Survey => 勘测
Geofence => 地理围栏
Rally Point => 集结
Rally => 集结
Telemetry => 遥测
Link => 链路
Battery => 电池
Charge => 充电
Voltage => 电压
Current => 电流
Sensor => 传感器
Sensors => 传感器
Calibration => 校准
Calibrate => 校准
Accelerometer => 加速度计
Gyroscope => 陀螺仪
Magnetometer / Compass => 磁罗盘
Barometer => 气压计
GPS => GPS
Satellites => 卫星
Parameter => 参数
Parameters => 参数
Firmware => 固件
Log / Logs => 日志
Settings => 设置
General => 通用
Application Settings => 应用设置
Security => 安全
Safety => 安全
Power => 电源
Motors => 电机
Motor => 电机
Frame => 机架
Airframe => 机架
Propeller => 螺旋桨
Takeoff => 起飞
Land => 降落
Return to Launch => 返航
Hold => 悬停
Altitude => 高度
Distance => 距离
Speed => 速度
Heading => 航向
Attitude => 姿态
Roll => 横滚
Pitch => 俯仰
Yaw => 偏航
Throttle => 油门
Map => 地图
Offline Maps => 离线地图
Camera => 相机
Gimbal => 云台
Video => 视频
Stream => 流
Recording => 录制
Survey => 测绘
Corridor Scan => 走廊扫描
Structure Scan => 结构扫描
Photo => 照片
Image => 图像
Geotagging => 地理位置标记
Console => 控制台
Inspector => 检测
Vibration => 振动
Analyze => 分析
Configure => 配置
Configuration => 配置
Enabled => 启用
Disabled => 禁用
Connect => 连接
Disconnect => 断开连接
Delete => 删除
Remove => 移除
Add => 添加
Edit => 编辑
Save => 保存
Load => 加载
Reset => 重置
Restore => 恢复
Cancel => 取消
Apply => 应用
Close => 关闭
Name => 名称
Type => 类型
Value => 数值
Units => 单位
Error => 错误
Warning => 警告
Failed => 失败
Success => 成功
"""

RULES = """You translate user-interface strings for QGroundControl, an open source
ground control station for drones, from English into Simplified Chinese.

Rules:
1. Translate only the UI text. Keep it concise; this is a GUI, space is limited.
2. NEVER translate or alter these, copy them EXACTLY as given:
   - placeholders %1 %2 %3 %n ${name}
   - HTML/XML entities such as &apos; &quot; &amp; &lt; &gt;
   - identifiers: MAVLink, PX4, ArduPilot, ArduCopter, ArduPlane, UDP, TCP, RTSP, JSON,
     EKF, GPS, RTK, HIL, SD, USB, ESC, IMU, ROI, KML, SHP, CSV, API
   - unit symbols (%, °, m, cm, km, s, ms, Hz, V, A, mAh, m/s, °/s) and file extensions
   - keyboard shortcuts such as Ctrl+Q or Shift+Tab
   - pure numbers and symbols such as "--.--"
3. If a string has no sensible translation (a bare symbol, a number, a unit, a
   protocol name, an empty string), keep it unchanged.
4. Use the glossary below verbatim so new strings match the already-translated UI.
5. Do not add punctuation that is not in the source. Do not add explanations.
6. Return machine-readable JSON only.
"""


def read_ts(path):
    with io.open(path, "rb") as handle:
        return handle.read().decode("utf-8")


def write_ts(path, text):
    with io.open(path, "wb") as handle:
        handle.write(text.encode("utf-8"))


MESSAGE_RE = re.compile(r"<message>(.*?)</message>", re.S)
SOURCE_RE = re.compile(r"<source>(.*?)</source>", re.S)
CONTEXT_RE = re.compile(r"<context>\s*<name>(.*?)</name>", re.S)
TRANSLATION_RE = re.compile(
    r"<translation(?P<attrs>[^>]*)>(?P<body>.*?)</translation>", re.S)


def contexts_with_spans(raw):
    """Yield (context_name, start, end) spans so each message knows its context."""
    for match in CONTEXT_RE.finditer(raw):
        name = match.group(1).strip()
        start = match.start()
        end = raw.find("</context>", start)
        yield name, start, end if end >= 0 else len(raw)


def extract(path, out_path):
    raw = read_ts(path)
    spans = list(contexts_with_spans(raw))

    def context_at(pos):
        for name, start, end in spans:
            if start <= pos < end:
                return name
        return ""

    items = []
    for match in MESSAGE_RE.finditer(raw):
        body = match.group(1)
        if 'type="unfinished"' not in body:
            continue
        source_match = SOURCE_RE.search(body)
        if not source_match:
            continue
        translation_match = TRANSLATION_RE.search(body)
        if not translation_match:
            continue
        items.append({
            "id": len(items),
            "context": context_at(match.start()),
            "source": source_match.group(1).strip(),
            "start": match.start(),
            "end": match.end(),
        })

    payload = {
        "tsPath": os.path.abspath(path),
        "items": items,
        "done": {},
    }
    with io.open(out_path, "w", encoding="utf-8") as handle:
        json.dump(payload, handle, ensure_ascii=False, indent=1)
    print("extracted %d unfinished strings -> %s" % (len(items), out_path))


def call_api(batch):
    numbered = "\n".join(
        '%d\t%s' % (i, item["source"].replace("\n", "\\n"))
        for i, item in enumerate(batch)
    )
    user = (
        RULES
        + "\nGlossary (English => Chinese):\n" + GLOSSARY
        + "\n\nTranslate these lines. Each line is `<index>\\t<english>`. "
          "A literal \\n inside a line means a line break in the source; keep it as \\n.\n\n"
        + numbered
        + '\n\nReturn JSON: {"items":[{"i":0,"t":"译文"}, ...]} with only an "i" and "t" key '
          "per item, and exactly one entry per input index."
    )
    body = {
        "model": "deepseek-chat",
        "messages": [
            {"role": "system", "content": "You are a precise software localisation engine. "
                                          "You output JSON only, never commentary."},
            {"role": "user", "content": user},
        ],
        "temperature": 0.1,
        "stream": False,
        "response_format": {"type": "json_object"},
    }
    request = urllib.request.Request(
        API_URL, data=json.dumps(body).encode("utf-8"), method="POST")
    request.add_header("Content-Type", "application/json")
    request.add_header("Authorization", "Bearer " + API_KEY)
    with urllib.request.urlopen(request, timeout=180) as response:
        data = json.loads(response.read().decode("utf-8"))
    content = data["choices"][0]["message"]["content"]
    parsed = json.loads(content)
    return parsed.get("items", [])


ENTITY_MAP = {
    "&lt;": "<",
    "&gt;": ">",
    "&amp;": "&",
    "&apos;": "'",
    "&quot;": '"',
}


def unescape(text):
    """Decode the entities a .ts file uses, so validation can compare real text."""
    for entity, char in ENTITY_MAP.items():
        text = text.replace(entity, char)
    return re.sub(r"&#(\d+);", lambda m: chr(int(m.group(1))), text)


def escape_ts(text):
    """Make text safe for a .ts element without double-escaping entities already there."""
    placeholders = {}

    def protect(match):
        key = "\x00%d\x00" % len(placeholders)
        placeholders[key] = match.group(0)
        return key

    text = re.sub(r"&(?:lt|gt|amp|apos|quot|#\d+);", protect, text)
    text = text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
    for key, original in placeholders.items():
        text = text.replace(key, original)
    return text


def validate(source, translated):
    """Return an error string when the translation clearly broke the source contract."""
    if translated is None:
        return "missing"
    if not isinstance(translated, str):
        return "not a string"
    if translated.strip() == "" and source.strip() != "":
        return "empty translation"

    for token in set(re.findall(r"%\d|%n", source)):
        if token not in translated:
            return "lost placeholder " + token

    # Compare decoded text: the model may legitimately write a bare < or & character,
    # which escape_ts() turns back into an entity when writing the .ts file.
    plain_source = unescape(source)
    plain_translation = unescape(translated)
    for char in "<>":
        if plain_source.count(char) != plain_translation.count(char):
            return "changed <%s> count" % char

    return None


def run(work_path, max_batches=None):
    with io.open(work_path, encoding="utf-8") as handle:
        payload = json.load(handle)

    pending = [item for item in payload["items"] if str(item["id"]) not in payload["done"]]
    print("pending: %d (already done: %d)" % (len(pending), len(payload["done"])))

    batches = 0
    index = 0
    while index < len(pending):
        batch = pending[index:index + BATCH_SIZE]
        batch_id = batches + 1
        print("batch %d: %d strings (%d/%d)" % (batch_id, len(batch), index + len(batch), len(pending)))
        try:
            results = call_api(batch)
        except Exception as exc:                                    # noqa: BLE001
            print("  API error: %s (retry once after 5s)" % exc)
            time.sleep(5)
            try:
                results = call_api(batch)
            except Exception as exc2:                               # noqa: BLE001
                print("  API error again: %s -> skipping batch" % exc2)
                index += len(batch)
                batches += 1
                if max_batches and batches >= max_batches:
                    break
                continue

        by_index = {}
        for entry in results:
            try:
                by_index[int(entry["i"])] = entry["t"]
            except (KeyError, TypeError, ValueError):
                continue

        accepted = 0
        rejected = []
        for local, item in enumerate(batch):
            translated = by_index.get(local)
            problem = validate(item["source"], translated)
            if problem:
                rejected.append((item["source"][:60], problem))
                continue
            payload["done"][str(item["id"])] = translated
            accepted += 1
        print("  accepted %d, rejected %d" % (accepted, len(rejected)))
        for source, problem in rejected[:5]:
            print("    REJECT %-60s %s" % (source, problem))

        index += len(batch)
        batches += 1
        with io.open(work_path, "w", encoding="utf-8") as handle:
            json.dump(payload, handle, ensure_ascii=False, indent=1)

        if max_batches and batches >= max_batches:
            print("reached --batches limit, stopping")
            break

    print("done: %d/%d translated" % (len(payload["done"]), len(payload["items"])))


def apply(work_path):
    with io.open(work_path, encoding="utf-8") as handle:
        payload = json.load(handle)

    ts_path = payload["tsPath"]
    raw = read_ts(ts_path)
    items = sorted(payload["items"], key=lambda i: i["start"], reverse=True)

    applied = 0
    for item in items:
        translated = payload["done"].get(str(item["id"]))
        if translated is None:
            continue
        segment = raw[item["start"]:item["end"]]
        safe = escape_ts(translated)
        new_segment = TRANSLATION_RE.sub(
            lambda _m: "<translation>%s</translation>" % safe, segment, count=1)
        if new_segment == segment:
            continue
        raw = raw[:item["start"]] + new_segment + raw[item["end"]:]
        applied += 1

    write_ts(ts_path, raw)
    print("applied %d translations to %s" % (applied, ts_path))


def verify(path):
    raw = read_ts(path)
    total = len(MESSAGE_RE.findall(raw))
    unfinished = len(re.findall(r'<translation type="unfinished"', raw))
    empty = len(re.findall(r"<translation></translation>", raw))
    print("%-40s total=%d unfinished=%d empty=%d translated=%d"
          % (os.path.basename(path), total, unfinished, empty,
             total - unfinished - empty))

    import xml.etree.ElementTree as ET
    try:
        ET.fromstring(raw.encode("utf-8"))
        print("XML: OK")
    except ET.ParseError as exc:
        print("XML: BROKEN -> %s" % exc)
        return 1
    return 0


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    command = argv[1]

    if command == "extract":
        extract(argv[2], argv[3])
    elif command == "run":
        limit = None
        if "--batches" in argv:
            limit = int(argv[argv.index("--batches") + 1])
        run(argv[3], limit)
    elif command == "apply":
        apply(argv[2])
    elif command == "verify":
        return verify(argv[2])
    else:
        print("unknown command %s" % command)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
