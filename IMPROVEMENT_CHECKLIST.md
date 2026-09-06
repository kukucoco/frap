# frap 剩余问题与改进清单

> 更新时间: 2026-09-02

---

## 一、性能问题

| 问题 | 文件位置 | 难度 | 改进建议 |
|------|----------|------|----------|
| 马赛克逐像素读取 | `src/core/render_ops.cpp:65` | 🟡 中 | 将 `img.pixel(i, j)` 替换为 `constScanLine()` 直接扫描行访问，避免逐像素格式转换开销。或参考 `drawBlur` 用 `QImage::scaled()` 缩放技巧实现快速马赛克 |
| 全量操作重放 | `src/core/session.cpp:190` | 🔴 高 | 在 `Session` 中增加 `m_cachedImage` 缓存，每次 `addOperation()` 后仅渲染新增操作，`undo/redo` 时 invalidate 缓存。或至少缓存最近一次渲染结果，仅在 `operationsChanged` 时重绘 |
| 拖拽预览无节流 | `src/app/qml/CaptureController.qml:183` | 🟢 低 | 添加 `property real lastPreviewTime: 0`，在 `move()` 中判断 `Date.now() - lastPreviewTime > 33`（30fps）才调用 `setToolPreview` |

---

## 二、平台兼容

| 问题 | 文件位置 | 难度 | 改进建议 |
|------|----------|------|----------|
| X11 无覆盖层 | `src/app/main.cpp:159` | 🔴 高 | 检测 `QGuiApplication::platformName()` 为 `xcb` 时，改用 `Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint | Qt::Tool` 创建透明窗口，或集成 `QRasterWindow` + XShape 裁剪。参考 Ksnip 的 X11 overlay 实现 |
| 仅主屏幕 | `src/app/main.cpp:162` | 🟡 中 | 遍历 `QGuiApplication::screens()`，为每个屏幕创建覆盖窗口，或使用 `QScreen::virtualGeometry()` 计算多屏合并区域 |

---

## 三、功能缺失

| 功能 | 难度 | 改进建议 |
|------|------|----------|
| 自由手绘工具 | 🟡 中 | 新建 `src/plugins/tool_pen/`，交互模式 `drag-region`，在 `handle()` 中收集拖拽路径点数组，返回 `{"type":"pen","points":[...],"color":"#xxx","width":4}`。`render_ops.cpp:143` 已有 `pen` 渲染实现 |
| 直线工具 | 🟡 中 | 新建 `src/plugins/tool_line/`，交互模式 `drag-line`，返回 `{"type":"line","start":{...},"end":{...}}`。`render_ops.cpp:119` 已有 `line` 渲染实现 |
| Shift 键 45° 锁定 | 🟢 低 | 在 `CaptureController.qml` 的 `move()` 中，当 `modifiers & Qt.ShiftModifier` 时，将 `dragCurrent` 约束到 0°/45°/90°/135°/180°/225°/270°/315° 方向。计算角度后取最近的 45° 倍数，用 `Math.cos/sin` 投影 |
| 工具栏可拖拽 | 🟡 中 | 在 `FloatingToolBar.qml` 根 `Rectangle` 上添加 `DragHandler`，记录 `activeChanged` 时的偏移量，更新 `x/y` 属性。添加边界约束防止拖出屏幕 |
| 工具栏自动避让 | 🟡 中 | 在 `FloatingToolBar.qml` 中检测 `y + height > parent.height` 时自动翻转到选区上方，或检测左右边界后水平偏移 |
| 撤销/重做禁用态 | 🟢 低 | 在 `FloatingToolBar.qml` 的 Repeater delegate 中，当 `modelData.id === "undo"` 时设置 `enabled: backend.canUndo`，`modelData.id === "redo"` 时设置 `enabled: backend.canRedo`。已有的禁用态遮罩（第88-92行）会自动生效 |
| 快捷键提示 | 🟢 低 | 在 `FloatingToolBar.qml` 的 `ToolTip.text` 中追加 `modelData.shortcut`，格式如 `"撤销\nCtrl+Z"`。`qml_backend.cpp` 的 `rebuildToolbar()` 需确保返回 `shortcut` 字段 |
| 设置 GUI | 🔴 高 | 新建 `SettingsDialog.qml`，从 `backend` 读取 `configSchema` 生成表单。使用 `Dialog` 组件 + `Repeater` 遍历 schema，按 `type` 渲染 `TextField/SpinBox/ComboBox/ColorPicker`。保存时调用 `backend.setSetting()` |

---

## 四、UI 细节

| 问题 | 文件位置 | 难度 | 改进建议 |
|------|----------|------|----------|
| 选区手柄样式 | `src/app/qml/CropOverlay.qml:45` | 🟡 中 | 用 `Shape` + `ShapePath.PathAngleArc` 绘制 270° 弧形手柄（参考 Spectacle 的 `Handle.qml`）。角点手柄用 270° 弧，边中点手柄用 180° 弧。填充 `Theme.accent`，描边 `Theme.textPrimary` |
| 虚线动画 | `src/app/qml/CropOverlay.qml:31` | 🟢 低 | 添加 `NumberAnimation on dashOffset` 从 0 到 `dashPattern[0] + dashPattern[1]`，`duration: 800`，`loops: Animation.Infinite` 实现蚂蚁线流动效果 |
| 文本预览位置 | `src/app/qml/OptionsBar.qml:97` | 🟡 中 | 在 `main.qml` 中新增 `TextPreview` 组件，绑定 `controller.textAnchor` 和 `controller.textBuffer`，使用与实际渲染相同的字体/颜色/大小，在锚点位置实时预览。OptionsBar 中的输入框改为仅输入，预览组件跟随鼠标位置 |
| 颜色选择器 | `src/app/qml/OptionsBar.qml:45` | 🟢 低 | 点击色块时弹出 `Popup` 组件，内含 4x2 色板网格 + 自定义颜色输入框。或用 `QtQuick.Dialogs.ColorDialog`（Qt 6） |

---

## 五、待创建插件

| 插件 | render_ops 已支持 | 交互模式 | 返回格式 |
|------|------------------|----------|----------|
| `tool_pen` | ✅ `pen` (L143) | `drag-region` | `{"type":"pen","points":[{"x":0,"y":0},...],"color":"#e11d48","width":4}` |
| `tool_line` | ✅ `line` (L119) | `drag-line` | `{"type":"line","start":{"x":0,"y":0},"end":{"x":100,"y":100}}` |
