/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include "SettingsGroup.h"

/// Settings for the local vision recognition service (see VisionController)
class VisionSettings : public SettingsGroup
{
    Q_OBJECT
public:
    VisionSettings(QObject* parent = nullptr);
    DEFINE_SETTING_NAME_GROUP()
    DEFINE_SETTINGFACT(enabled)
    DEFINE_SETTINGFACT(serverUrl)
    DEFINE_SETTINGFACT(serverCommand)
    DEFINE_SETTINGFACT(autoStartServer)
    DEFINE_SETTINGFACT(captureSeconds)
    DEFINE_SETTINGFACT(captureFps)
};
