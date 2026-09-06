// FloatingToolBar.qml —— 主工具条（玻璃圆角浮条）
import QtQuick
import Frap

Rectangle {
    id: root

    property var controller: null
    property var items: backend ? backend.toolbar : []

    width: row.width + 2 * Theme.spacing + 18
    height: Theme.barHeight
    radius: Theme.radiusLg
    color: Theme.surface
    border.color: Theme.border
    border.width: 1

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 4

        Repeater {
            model: root.items

            delegate: Item {
                id: cell
                required property var modelData
                width: modelData.type === "sep" ? 1 : 40
                height: 40

                // 分隔线
                Rectangle {
                    visible: modelData.type === "sep"
                    width: 1; height: 26
                    color: Theme.border
                    anchors.verticalCenter: parent.verticalCenter
                }

                // 按钮
                Item {
                    visible: modelData.type !== "sep"
                    anchors.fill: parent

                    Rectangle {
                        id: bg
                        anchors.fill: parent
                        radius: Theme.radiusSm
                        color: {
                            if (modelData.toggled) return Theme.accent
                            if (modelData.accent) return Theme.accentSoft
                            if (hover.hovered) return Theme.surfaceHover
                            if (press.pressed) return Theme.surfacePress
                            return Qt.rgba(1, 1, 1, 0.05)
                        }
                        border.color: modelData.toggled ? Qt.rgba(1, 1, 1, 0.2) : Theme.border
                        border.width: 1
                        scale: press.pressed ? 0.94 : (hover.hovered ? 1.05 : 1.0)
                        Behavior on scale { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
                    }

                    Image {
                        anchors.centerIn: parent
                        width: 20
                        height: 20
                        sourceSize: Qt.size(40, 40)
                        source: "image://icons/" + modelData.icon
                        mipmap: true
                        opacity: modelData.enabled === false ? 0.35 : 1.0
                    }

                    MouseArea {
                        id: hover
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        enabled: modelData.enabled !== false
                    }
                    Item { id: press; property bool pressed: false }
                    MouseArea {
                        anchors.fill: parent
                        enabled: modelData.enabled !== false
                        onPressed: press.pressed = true
                        onReleased: { press.pressed = false; if (containsMouse) controller.onToolbar(modelData.id) }
                        onExited: press.pressed = false
                    }

                    Rectangle {
                        anchors.fill: parent; radius: Theme.radiusSm
                        color: Qt.rgba(0, 0, 0, 0.2)
                        visible: modelData.enabled === false
                    }
                }
            }
        }
    }
}
