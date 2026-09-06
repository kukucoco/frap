// CropOverlay.qml —— 裁剪遮罩 + 虚线边框 + 手柄
import QtQuick
import QtQuick.Shapes
import Frap

Item {
    id: root

    property var controller: null

    readonly property rect crop: controller ? controller.effectiveCrop : Qt.rect(0, 0, 0, 0)
    readonly property bool show: controller ? controller.hasEffectiveCrop : false
    readonly property bool interactive: controller ? controller.cropTool : false

    // 裁剪区外遮罩（四块拼合）
    Rectangle { color: Theme.maskDim; visible: root.show; x: 0; y: 0; width: root.width; height: Math.max(0, crop.y) }
    Rectangle { color: Theme.maskDim; visible: root.show; x: 0; y: crop.y + crop.height; width: root.width; height: Math.max(0, root.height - crop.y - crop.height) }
    Rectangle { color: Theme.maskDim; visible: root.show; x: 0; y: crop.y; width: Math.max(0, crop.x); height: crop.height }
    Rectangle { color: Theme.maskDim; visible: root.show; x: crop.x + crop.width; y: crop.y; width: Math.max(0, root.width - crop.x - crop.width); height: crop.height }

    // 虚线边框
    Shape {
        visible: root.show
        anchors.fill: parent
        antialiasing: true
        containsMode: Shape.FillContains

        ShapePath {
            strokeColor: Theme.accent
            strokeWidth: 1.6
            strokeStyle: ShapePath.DashLine
            dashPattern: [5, 4]
            fillColor: "transparent"
            startX: root.crop.x; startY: root.crop.y
            PathLine { x: root.crop.x + root.crop.width; y: root.crop.y }
            PathLine { x: root.crop.x + root.crop.width; y: root.crop.y + root.crop.height }
            PathLine { x: root.crop.x; y: root.crop.y + root.crop.height }
            PathLine { x: root.crop.x; y: root.crop.y }
        }
    }

    // 手柄
    Repeater {
        model: root.interactive ? 8 : 0
        delegate: Rectangle {
            width: 11; height: 11; radius: 4
            color: "#ffffff"
            border.color: Theme.accent
            border.width: 2
            x: root.controller.handlePoint(root.crop, index).x - 5.5
            y: root.controller.handlePoint(root.crop, index).y - 5.5
        }
    }

    // 尺寸标签（裁剪框内左上角）
    Rectangle {
        visible: root.show
        x: root.crop.x + 4
        y: root.crop.y + 4
        width: label.width + 12
        height: label.height + 6
        radius: 6
        color: Qt.rgba(0, 0, 0, 0.55)
        Text {
            id: label
            anchors.centerIn: parent
            text: root.crop.width + " × " + root.crop.height
            color: Theme.textPrimary
            font: Theme.fontMono
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
