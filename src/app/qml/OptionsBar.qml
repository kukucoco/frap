// OptionsBar.qml —— 活动工具的选项条（色板 / 线宽步进 / 文字输入 / 清除裁剪）
import QtQuick
import QtQuick.Controls
import Frap

Rectangle {
    id: root

    property var controller: null

    width: row.width + 2 * Theme.spacing + 18
    height: 48
    radius: Theme.radiusLg
    color: Theme.surface
    border.color: Theme.border
    border.width: 1
    visible: options.length > 0

    property var options: backend ? backend.activeOptions : []

    Row {
        id: row
        anchors.centerIn: parent
        spacing: Theme.spacing

        Repeater {
            id: rep
            model: root.options

            delegate: Item {
                id: cell
                required property var modelData
                width: cell.implicitW
                height: 30

                property int implicitW: {
                    if (modelData.type === "color")
                        return 18 * modelData.enum.length + 5 * (modelData.enum.length - 1) + 2
                    if (modelData.type === "text") return 160
                    if (modelData.type === "clear-crop") return 120
                    return 96
                }

                // 色板
                Row {
                    visible: modelData.type === "color"
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 5
                    Repeater {
                        model: modelData.enum
                        delegate: Rectangle {
                            required property var modelData
                            width: 18; height: 18; radius: 9
                            color: modelData
                            border.color: modelData === cell.modelData.value ? Theme.textPrimary : Theme.border
                            border.width: modelData === cell.modelData.value ? 2 : 1
                            scale: dot.hovered ? 1.18 : 1.0
                            Behavior on scale { NumberAnimation { duration: 80 } }
                            MouseArea {
                                id: dot
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: backend.setSetting(cell.modelData.key, modelData)
                            }
                        }
                    }
                }

                // 线宽步进
                Row {
                    visible: modelData.type === "int"
                    spacing: 4
                    anchors.verticalCenter: parent.verticalCenter
                    Label { text: "线宽"; color: Theme.textSecondary; font: Theme.fontHint }
                    Item { width: 24; height: 24
                        Rectangle { anchors.fill: parent; radius: 6; color: Theme.surfaceHover
                            border.color: Theme.border; border.width: 1
                            Label { anchors.centerIn: parent; text: "−"; color: Theme.textPrimary } }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: if (cell.modelData.value > cell.modelData.min)
                                           backend.setSetting(cell.modelData.key, cell.modelData.value - 1) }
                    }
                    Label { text: cell.modelData.value; color: Theme.textPrimary; font: Theme.fontBase
                        width: 26; horizontalAlignment: Text.AlignHCenter }
                    Item { width: 24; height: 24
                        Rectangle { anchors.fill: parent; radius: 6; color: Theme.surfaceHover
                            border.color: Theme.border; border.width: 1
                            Label { anchors.centerIn: parent; text: "+"; color: Theme.textPrimary } }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: if (cell.modelData.value < cell.modelData.max)
                                           backend.setSetting(cell.modelData.key, cell.modelData.value + 1) }
                    }
                }

                // 文字输入
                TextField {
                    visible: modelData.type === "text"
                    width: 160; height: 30
                    placeholderText: "输入文字…"
                    placeholderTextColor: Theme.textDim
                    color: Theme.textPrimary
                    font: Theme.fontBase
                    text: controller ? controller.textBuffer : ""
                    background: Rectangle {
                        radius: 7
                        color: Qt.rgba(0, 0, 0, 0.25)
                        border.color: activeFocus ? Theme.accent : Theme.border
                        border.width: 1
                    }
                    onTextChanged: { if (controller) controller.textBuffer = text }
                    onAccepted: { if (controller) controller.commitText() }
                    onFocusChanged: if (focus && controller) controller.textFocused = true
                }

                // 清除裁剪
                Row {
                    visible: modelData.type === "clear-crop"
                    spacing: 6
                    anchors.verticalCenter: parent.verticalCenter
                    Label { text: "✓ 已裁剪"; color: Theme.accent; font: Theme.fontLabel }
                    Rectangle {
                        width: 66; height: 30; radius: 7
                        color: clr.hovered ? Theme.surfaceHover : Qt.rgba(1, 1, 1, 0.04)
                        border.color: Theme.border; border.width: 1
                        Label { anchors.centerIn: parent; text: "清除裁剪"; color: Theme.textPrimary
                            font: Theme.fontHint }
                        MouseArea {
                            id: clr
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: backend.clearCrop()
                        }
                    }
                }
            }
        }
    }
}
