/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "VisionSettings.h"

#include <QQmlEngine>
#include <QtQml>

DECLARE_SETTINGGROUP(Vision, "Vision")
{
    qmlRegisterUncreatableType<VisionSettings>("QGroundControl.SettingsManager", 1, 0, "VisionSettings", "Reference only");
}

DECLARE_SETTINGSFACT(VisionSettings, enabled)
DECLARE_SETTINGSFACT(VisionSettings, serverUrl)
DECLARE_SETTINGSFACT(VisionSettings, serverCommand)
DECLARE_SETTINGSFACT(VisionSettings, autoStartServer)
DECLARE_SETTINGSFACT(VisionSettings, captureSeconds)
DECLARE_SETTINGSFACT(VisionSettings, captureFps)
