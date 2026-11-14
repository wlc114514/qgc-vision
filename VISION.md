# QGroundControl Vision（QGC 4.4.4 魔改版）

在 QGC 4.4.4 上加了一个「视觉识别」接口：飞行界面左侧工具栏多一个 **识别** 按钮，点一下，QGC 从自己的视频管线取几秒画面，发给本机的识别服务，再把结果（数量、说明、结果图）显示在画面上方。

识别模型不编进 QGC，而是作为一个本地 HTTP 服务单独运行。任何模型只要实现下面两个接口，就能接到 QGC 上。

## 接口（JSON，UTF-8）

`GET /health`

```json
{"ok": true, "ready": true, "name": "青蟹计数", "api": 1}
```

`ready: false` 表示服务在、模型还在加载。

`POST /analyze`

请求：
```json
{"frames": ["<base64 JPEG>", "..."]}
```
帧按时间顺序排列，默认 3 秒 × 5 fps = 15 帧，是视频原始分辨率（解码后的帧，不是界面截图）。

返回：
```json
{"ok": true,
 "label": "青蟹", "count": 4,
 "text": "青蟹 4 只",
 "detail": "15 帧投票：4 只 ×15",
 "image": "<base64 JPEG，可选>",
 "boxes": [{"x": 0.38, "y": 0.50, "w": 0.10, "h": 0.11, "label": "green", "score": 0.96}]}
```
出错时返回 `{"ok": false, "error": "原因"}`，QGC 会把原因显示出来。

参考实现：[mate-crab-counter](https://github.com/wlc114514/mate-crab-counter) 的 `scripts/vision_server.py`。

## 设置（应用设置 → 通用 → 视觉识别）

| 设置 | 默认 | 说明 |
|---|---|---|
| 显示「识别」按钮 | 开 | |
| 服务地址 | `http://127.0.0.1:8765` | |
| 启动命令 | 空 | 空时用识别模型安装时登记在注册表 `HKCU\Software\QGC-Vision\ServerCommand` 的命令 |
| QGC 启动时自动启动服务 | 开 | 服务没在跑就启动它；QGC 退出时关掉标准输入，服务（`--exit-with-parent`）随之退出 |
| 每次取画面 | 3 秒 | |
| 每秒取几帧 | 5 | |

服务日志：`%TEMP%\qgc-vision\server.log`。

## 实现

- `src/VideoReceiver/GstVideoReceiver.cc`：`grabFrame()` 在视频 sink 的 pad probe 上取下一帧解码后的画面，转换成 RGB（`gst_video_convert_sample`，在工作线程里做），通过 `frameGrabbed(QImage)` 发出。QGC 4.4.4 自带的 `takeScreenshot` 在 GStreamer 里没有实现，所以另加了这个。
- `src/Vision/VisionController.*`：QML 单例 `QGroundControl.Vision / VisionController`，负责取帧、HTTP 请求、结果、服务进程管理。
- `src/Settings/VisionSettings.*`、`Vision.SettingsGroup.json`：设置项。
- `src/FlightDisplay/VisionResultPanel.qml`、`FlyView.qml`、`FlyViewToolStripActionList.qml`、`src/ui/preferences/GeneralSettings.qml`：界面。
- `deploy/windows/nullsoft_installer.nsi`：产品名改成 QGroundControl Vision，装在单独目录，不会卸载或覆盖官方 QGC；卸载时不删除和官方 QGC 共用的设置。

测试钩子：环境变量 `QGC_VISION_AUTOTEST=1` 时，视频和服务就绪后自动识别一次，结果写到 `%TEMP%\qgc-vision\autotest.json`。

## 编译（Windows）

和官方 4.4 一样：Qt 5.15.2 MSVC2019 64-bit + Qt Charts、GStreamer 1.18.1 MSVC（runtime + devel，装到 `C:\gstreamer`）、VS 2019/2022 C++ 工具、jom、NSIS。

```
qmake -r qgroundcontrol.pro CONFIG+=release CONFIG+=installer CONFIG+=StableBuild
jom -j6
```
安装包在 `staging\QGroundControl-installer.exe`。系统代码页是中文（936）时 MSVC 需要 `/utf-8`，已经写进 `qgroundcontrol.pro`。
