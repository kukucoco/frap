// Readout.qml —— 左下角像素坐标 + 颜色读数
import QtQuick
import Frap

Item {
    id: root

    property var controller: null
    readonly property string text: controller ? controller.readout : ""
    readonly property color swatch: {
        var m = /#([0-9a-fA-F]{6})/.exec(text)
        return m ? ("#" + m[1]) : "#000000"
    }

    visible: text.length > 0
    x: 14
    y: parent.height - height - 14
    width: pill.width
    height: pill.height

    Rectangle {
        id: pill
        width: row.width + 28
        height: 32
        radius: 16
        color: Qt.rgba(0, 0, 0, 0.62)
        border.color: Theme.border
        border.width: 1

        Row {
            id: row
            anchors.centerIn: parent
            spacing: 9
            Rectangle {
                width: 13; height: 13; radius: 7
                color: root.swatch
                border.color: Qt.rgba(1, 1, 1, 0.35)
                border.width: 1
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: root.text
                color: Theme.textPrimary
                font: Theme.fontMono
                verticalAlignment: Text.AlignVCenter
            }
        }
    }
}
