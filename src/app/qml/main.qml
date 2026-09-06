// main.qml —— 截图工作台覆盖层（QtQuick）
import QtQuick
import QtQuick.Controls
import Frap

Item {
    id: screen
    visible: true

    CaptureController { id: controller }

    focus: true

    // 渲染帧
    FrameImage {
        anchors.fill: parent
        source: backend ? backend.frame : null
    }

    // 裁剪遮罩 / 边框 / 手柄
    CropOverlay {
        anchors.fill: parent
        controller: controller
        z: 5
    }

    // 选中标注外框
    AnnotationOutline {
        anchors.fill: parent
        controller: controller
        z: 6
    }

    // 主输入区（最底层交互）
    MouseArea {
        id: input
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        z: 100
        cursorShape: controller ? controller.cursorShape : Qt.ArrowCursor

        onPressed: (mouse) => { if (screen.focus === false) screen.forceActiveFocus(); controller.press(mouse.x, mouse.y, mouse.button) }
        onPositionChanged: (mouse) => controller.move(mouse.x, mouse.y)
        onReleased: (mouse) => controller.release()
    }

    // 放大镜
    Magnifier {
        controller: controller
        screen: screen
        z: 120
    }

    // 主工具条
    FloatingToolBar {
        id: toolbar
        anchors.horizontalCenter: parent.horizontalCenter
        y: 14
        z: 200
        controller: controller
    }

    // 选项条（活动工具）
    OptionsBar {
        id: options
        anchors.horizontalCenter: parent.horizontalCenter
        y: toolbar.y + toolbar.height + 8
        z: 200
        controller: controller
    }

    // 提示
    Text {
        id: hint
        anchors.horizontalCenter: parent.horizontalCenter
        y: (options.visible ? options.y + options.height + 8 : toolbar.y + toolbar.height + 8)
        z: 150
        text: controller.toolId.length ? "拖拽绘制标注 · 右键/ESC 取消"
             : controller.cropTool ? "拖拽裁剪框选 · 可移动/调整/重裁剪 · ESC 清除"
             : controller.hasEffectiveCrop ? "选择工具全屏标注 · 裁剪已框选 · 右键/ESC 取消"
             : "拖拽以裁剪框选区域 · 选择工具全屏标注 · 右键/ESC 取消"
        color: "#ffffff"
        font: Theme.fontHint
        style: Text.Outline
        styleColor: Qt.rgba(0, 0, 0, 0.55)
        visible: controller.mode === controller.modeIdle
        opacity: 0.9
    }

    // 坐标 / 颜色读数
    Readout {
        z: 150
        controller: controller
    }

    // Toast
    Toast {
        id: toast
        anchors.horizontalCenter: parent.horizontalCenter
        y: 14
        z: 300
    }

    // 翻译结果面板
    TranslationPanel {
        id: transPanel
        anchors.horizontalCenter: parent.horizontalCenter
        y: options.visible ? options.y + options.height + 8 : toolbar.y + toolbar.height + 8
        z: 250
        src: backend ? backend.translationSrc : ""
        dst: backend ? backend.translationDst : ""
        visible: backend ? backend.translationVisible : false
    }

    Connections {
        target: backend
        function onMessage(level, text) { toast.show(text, level) }
    }

    // 键盘
    Keys.onPressed: (event) => {
        if (controller.onKey(event.key, event.text, event.modifiers))
            event.accepted = true
    }
}
