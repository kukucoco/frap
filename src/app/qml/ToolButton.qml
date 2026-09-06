// ToolButton.qml —— 可复用的圆角图标按钮（hover/press/active/禁用态）
import QtQuick
import QtQuick.Controls
import Frap

Item {
    id: root

    property string icon: ""
    property string label: ""
    property string hint: ""
    property bool toggled: false
    property bool accent: false
    property int size: 40
    signal clicked()

    width: size
    height: size
    implicitWidth: size
    implicitHeight: size

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: Theme.radiusSm
        color: {
            if (!root.enabled) return Qt.rgba(1, 1, 1, 0.04)
            if (root.toggled) return Theme.accent
            if (root.accent) return Theme.accentSoft
            if (pressArea.pressed) return Theme.surfacePress
            if (hoverArea.hovered) return Theme.surfaceHover
            return Qt.rgba(1, 1, 1, 0.02)
        }
        border.color: {
            if (!root.enabled) return Qt.rgba(1, 1, 1, 0.04)
            if (root.toggled) return Qt.rgba(1, 1, 1, 0.18)
            return Theme.border
        }
        border.width: 1
        scale: pressArea.pressed ? 0.94 : (hoverArea.hovered ? 1.04 : 1.0)
        Behavior on scale { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
        Behavior on color { ColorAnimation { duration: 110 } }
    }

    Image {
        anchors.centerIn: parent
        width: size * 0.52
        height: width
        source: "image://icons/" + root.icon
        sourceSize: Qt.size(width * 2, height * 2)
        opacity: root.enabled ? 1.0 : 0.35
        mipmap: true
    }

    MouseArea {
        id: hoverArea
        anchors.fill: parent
        hoverEnabled: true
        enabled: root.enabled
        cursorShape: Qt.PointingHandCursor
        onPressed: (mouse) => pressArea.pressed = true
        onReleased: (mouse) => { pressArea.pressed = false; if (containsMouse) root.clicked() }
        onExited: pressArea.pressed = false
    }
    Item { id: pressArea; property bool pressed: false }

    // 禁用态浅灰
    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusSm
        visible: !root.enabled
        color: Qt.rgba(0, 0, 0, 0.18)
    }

    ToolTip.visible: hoverArea.hovered && root.label.length
    ToolTip.text: root.hint.length ? root.hint : root.label
    ToolTip.delay: 420
    ToolTip.timeout: 2500
}
