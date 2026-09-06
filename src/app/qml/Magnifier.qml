// Magnifier.qml —— 拖拽跟随放大镜（圆形取景）
import QtQuick
import Frap

Item {
    id: root

    property var controller: null
    property var screen: null
    property real zoom: 3.2

    visible: controller && (controller.mode === controller.modeDrawing
                            || controller.mode === controller.modeCropDraw)
    width: 148
    height: 148
    z: 120

    function place() {
        var x = controller.mousePos.x - width - 30
        var y = controller.mousePos.y - height - 30
        x = Math.max(8, Math.min(screen.width - width - 8, x))
        y = Math.max(8, Math.min(screen.height - height - 8, y))
        root.x = x
        root.y = y
    }
    onVisibleChanged: if (visible) place()
    onXChanged: if (visible) place()

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: "#0e0e12"
        border.color: Qt.rgba(1, 1, 1, 0.4)
        border.width: 2
        clip: true

        FrameImage {
            id: mag
            source: backend ? backend.frame : null
            width: root.screen.width * root.zoom
            height: root.screen.height * root.zoom
            x: root.width / 2 - controller.mousePos.x * root.zoom
            y: root.height / 2 - controller.mousePos.y * root.zoom
        }

        // 中心十字
        Rectangle { width: 1; height: 12; color: Qt.rgba(1, 1, 1, 0.8); x: parent.width / 2 - 0.5; y: parent.height / 2 - 6 }
        Rectangle { width: 12; height: 1; color: Qt.rgba(1, 1, 1, 0.8); x: parent.width / 2 - 6; y: parent.height / 2 - 0.5 }
    }
}
