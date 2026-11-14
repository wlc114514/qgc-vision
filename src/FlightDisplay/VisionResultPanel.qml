/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick          2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts  1.12

import QGroundControl               1.0
import QGroundControl.Controls      1.0
import QGroundControl.ScreenTools   1.0
import QGroundControl.Vision        1.0

/// Shows progress and the result of a vision recognition (VisionController)
Rectangle {
    id:             panel
    width:          column.width + _margins * 2
    height:         column.height + _margins * 2
    radius:         ScreenTools.defaultFontPixelWidth
    color:          "#e61b1f23"
    border.color:   _ok ? "#3ddc5a" : (_busy ? "#9aa4ad" : "#ff5252")
    border.width:   2
    visible:        _busy || VisionController.resultVisible

    property bool   _busy:      VisionController.busy
    property bool   _ok:        !_busy && VisionController.resultOk
    property real   _margins:   ScreenTools.defaultFontPixelWidth * 1.5
    property real   _imageMax:  Math.min(ScreenTools.defaultFontPixelWidth * 55, parent ? parent.width * 0.5 : 600)

    MouseArea {
        anchors.fill:   parent
        onClicked:      VisionController.dismiss()
    }

    ColumnLayout {
        id:                 column
        anchors.centerIn:   parent
        spacing:            ScreenTools.defaultFontPixelHeight * 0.4

        // ---- in progress
        RowLayout {
            visible:    _busy
            spacing:    ScreenTools.defaultFontPixelWidth

            BusyIndicator {
                running:                _busy
                Layout.preferredWidth:  ScreenTools.defaultFontPixelHeight * 2
                Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 2
            }
            QGCLabel {
                text:           VisionController.status
                color:          "white"
                font.pointSize: ScreenTools.largeFontPointSize
            }
        }

        // ---- result
        RowLayout {
            visible:            _ok
            spacing:            ScreenTools.defaultFontPixelWidth
            Layout.alignment:   Qt.AlignHCenter

            QGCLabel {
                text:           VisionController.count
                color:          "#3ddc5a"
                font.pointSize: ScreenTools.largeFontPointSize * 3
                font.bold:      true
            }
            ColumnLayout {
                spacing: 0
                QGCLabel {
                    text:           VisionController.label
                    color:          "white"
                    font.pointSize: ScreenTools.largeFontPointSize
                    font.bold:      true
                }
                QGCLabel {
                    text:           VisionController.detail
                    color:          "#9aa4ad"
                }
            }
        }

        Image {
            visible:                _ok && source != ""
            source:                 _ok ? VisionController.imageUrl : ""
            fillMode:               Image.PreserveAspectFit
            cache:                  false
            Layout.preferredWidth:  Math.min(_imageMax, sourceSize.width)
            Layout.preferredHeight: sourceSize.width > 0 ? Layout.preferredWidth * sourceSize.height / sourceSize.width : 0
            Layout.alignment:       Qt.AlignHCenter
        }

        // ---- error
        QGCLabel {
            visible:                !_busy && !VisionController.resultOk
            text:                   VisionController.error
            color:                  "#ff8a80"
            wrapMode:               Text.WordWrap
            Layout.maximumWidth:    _imageMax
        }

        QGCLabel {
            visible:            !_busy
            text:               qsTr("点击关闭")
            color:              "#9aa4ad"
            font.pointSize:     ScreenTools.smallFontPointSize
            Layout.alignment:   Qt.AlignHCenter
        }
    }
}
