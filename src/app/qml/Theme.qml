// Theme.qml —— 设计令牌（颜色/尺寸/字体/色板），全局单例
pragma Singleton
import QtQuick

QtObject {
    // ---- 色彩 ----
    readonly property color accent:       "#19c37d"          // 主强调绿
    readonly property color accentStrong: "#0ea66b"
    readonly property color accentSoft:   Qt.rgba(0.10, 0.76, 0.49, 0.22)
    readonly property color selection:    "#5ab0ff"          // 标注选中蓝
    readonly property color danger:       "#ef4444"
    readonly property color textPrimary:  "#f5f5f7"
    readonly property color textSecondary:Qt.rgba(1, 1, 1, 0.55)
    readonly property color textDim:      Qt.rgba(1, 1, 1, 0.34)
    readonly property color border:       Qt.rgba(1, 1, 1, 0.14)
    readonly property color borderStrong: Qt.rgba(1, 1, 1, 0.28)
    readonly property color maskDim:      Qt.rgba(0, 0, 0, 0.40)

    // ---- 材质 ----
    readonly property color surface:      Qt.rgba(0.09, 0.09, 0.12, 0.86)
    readonly property color surfaceSolid: "#17171c"
    readonly property color surfaceHover: Qt.rgba(1, 1, 1, 0.10)
    readonly property color surfacePress: Qt.rgba(1, 1, 1, 0.16)
    readonly property color surfaceActive:Qt.rgba(0.10, 0.76, 0.49, 0.9)

    // ---- 圆角 / 尺寸 ----
    readonly property int radiusLg:   18
    readonly property int radius:     12
    readonly property int radiusSm:   8
    readonly property int buttonSize: 40
    readonly property int barHeight:  52
    readonly property int spacing:    6
    readonly property int shadowBlur: 28

    // ---- 字体 ----
    readonly property font fontBase:   Qt.font({ family: "Sans Serif", pointSize: 9.5 })
    readonly property font fontLabel:  Qt.font({ family: "Sans Serif", pointSize: 8.5, weight: Font.DemiBold })
    readonly property font fontHint:   Qt.font({ family: "Sans Serif", pointSize: 8.5 })
    readonly property font fontMono:   Qt.font({ family: "Monospace", pointSize: 9 })

    // ---- 工具色板 ----
    readonly property var palette: [
        "#e11d48", "#f59e0b", "#10b981", "#3b82f6",
        "#8b5cf6", "#ec4899", "#ffffff", "#111111"
    ]
}
