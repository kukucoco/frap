// AnnotationOutline.qml —— 选中标注的虚线外框 + 角点
import QtQuick
import QtQuick.Shapes
import Frap

Item {
    id: root

    property var controller: null

    readonly property rect r: controller && controller.opGhostBounds.width > 0
                              ? controller.opGhostBounds : Qt.rect(0, 0, 0, 0)
    visible: r.width > 0 && r.height > 0

    Shape {
        anchors.fill: parent
        antialiasing: true
        ShapePath {
            strokeColor: Theme.selection
            strokeWidth: 1.6
            strokeStyle: ShapePath.DashLine
            dashPattern: [4, 3]
            fillColor: "transparent"
            startX: root.r.x; startY: root.r.y
            PathLine { x: root.r.x + root.r.width; y: root.r.y }
            PathLine { x: root.r.x + root.r.width; y: root.r.y + root.r.height }
            PathLine { x: root.r.x; y: root.r.y + root.r.height }
            PathLine { x: root.r.x; y: root.r.y }
        }
    }

    Repeater {
        model: 4
        delegate: Rectangle {
            width: 9; height: 9; radius: 2
            color: "#ffffff"
            border.color: Theme.selection
            border.width: 2
            x: [root.r.x, root.r.x + root.r.width,
                root.r.x + root.r.width, root.r.x][index] - 4.5
            y: [root.r.y, root.r.y,
                root.r.y + root.r.height, root.r.y + root.r.height][index] - 4.5
        }
    }
}
