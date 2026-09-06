// Toast.qml —— 顶部消息提示（导出结果/错误/插件通知）
import QtQuick
import Frap

Item {
    id: root

    property string message: ""
    property string level: "info"
    visible: message.length > 0

    width: pill.width
    height: pill.height

    function show(msg, lvl) {
        level = lvl
        message = msg
        timer.restart()
    }

    opacity: message.length ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: 200 } }

    Rectangle {
        id: pill
        width: label.width + 40
        height: 38
        radius: 19
        color: level === "error" ? Qt.rgba(0.85, 0.22, 0.22, 0.94)
                                 : Qt.rgba(0.09, 0.09, 0.12, 0.92)
        border.color: Theme.border
        border.width: 1

        Text {
            id: label
            anchors.centerIn: parent
            text: root.message
            color: Theme.textPrimary
            font: Theme.fontBase
        }
    }

    Timer {
        id: timer
        interval: 2200
        onTriggered: root.message = ""
    }
}
