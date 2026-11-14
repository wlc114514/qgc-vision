# QGroundControl Vision

QGroundControl 4.4.4 的魔改版，加了一个接视觉识别模型的接口：飞行界面左侧工具栏多一个 **识别** 按钮，点一下就从 QGC 自己的视频流里取 3 秒画面，交给本机的识别服务，结果（数量、说明、标框图）显示在画面上方。

- 下载：[Releases](https://github.com/wlc114514/qgc-vision/releases) 里的 `QGroundControl-Vision-4.4.4-installer.exe`
- 装在单独的「QGroundControl Vision」目录，和官方 QGroundControl 并存，不覆盖也不卸载官方版；两者共用同一份设置（飞控连接、视频源等）
- 识别模型不在 QGC 里，是单独的本地 HTTP 服务，接口见 [VISION.md](VISION.md)。现成的模型：[mate-crab-counter](https://github.com/wlc114514/mate-crab-counter)（MATE 2026 数青蟹），它的安装脚本会把服务登记好，QGC Vision 启动时自动拉起
- 改动、编译方法也在 [VISION.md](VISION.md)

基于官方 [mavlink/qgroundcontrol](https://github.com/mavlink/qgroundcontrol) v4.4.4，许可证不变（Apache 2.0 / GPLv3，见 [COPYING.md](COPYING.md)）。以下是官方原 README。

---

# QGroundControl Ground Control Station

[![Releases](https://img.shields.io/github/release/mavlink/QGroundControl.svg)](https://github.com/mavlink/QGroundControl/releases)

*QGroundControl* (QGC) is an intuitive and powerful ground control station (GCS) for UAVs.

The primary goal of QGC is ease of use for both first time and professional users.
It provides full flight control and mission planning for any MAVLink enabled drone, and vehicle setup for both PX4 and ArduPilot powered UAVs. Instructions for *using QGroundControl* are provided in the [User Manual](https://docs.qgroundcontrol.com/en/) (you may not need them because the UI is very intuitive!)

All the code is open-source, so you can contribute and evolve it as you want.
The [Developer Guide](https://dev.qgroundcontrol.com/en/) explains how to [build](https://dev.qgroundcontrol.com/en/getting_started/) and extend QGC.


Key Links:
* [Website](http://qgroundcontrol.com) (qgroundcontrol.com)
* [User Manual](https://docs.qgroundcontrol.com/en/)
* [Developer Guide](https://dev.qgroundcontrol.com/en/)
* [Discussion/Support](https://docs.qgroundcontrol.com/en/Support/Support.html)
* [Contributing](https://dev.qgroundcontrol.com/en/contribute/)
* [License](https://github.com/mavlink/qgroundcontrol/blob/master/COPYING.md)
