# frap 项目状态报告

> 生成时间: 2026-09-06

## 项目概述

frap 是一个跨平台、高度可定制的截图工具，采用类 VSCode 的插件架构设计。已完成从 QPainter 到 QML 的架构迁移，具备现代 UI 框架和动画系统；项目由 `screenshot` 更名为 **frap**。

## 最新进展

- ✅ 项目更名为 **frap**（目录/产物/模块/配置路径全面更新）。
- ✅ 新增 **翻译插件** `service.translate`：参照 `~/.local/bin/screenshot-translator.py`，
  调用百度图片翻译 API，返回原文/译文，译文复制到剪贴板并展示。
- ✅ 新增 **长截图插件** `capture.longshot`：分屏截取后自动检测垂直重叠并纵向拼接。
- ✅ 新增 Service（翻译/长截图）插件类型接入工具条，翻译结果面板（TranslationPanel）。
- ✅ 协议/架构/SDK 文档同步更新，README 全面重写。

---

## 一、已完成的改进 ✅

| 类别 | 改进项 | 状态 |
|------|--------|------|
| **架构** | QPainter → QML 迁移 | ✅ 完成 |
| **架构** | C++ 后端 + QML 状态机分离 | ✅ 完成 |
| **主题** | Theme.qml 设计令牌系统（颜色/字体/圆角/尺寸） | ✅ 完成 |
| **图标** | IconProvider（主题图标 + 彩色圆回退） | ✅ 完成 |
| **按钮** | hover/press/active 动画（缩放 + 颜色过渡） | ✅ 完成 |
| **工具栏** | FloatingToolBar（玻璃浮条 + 圆角 + 边框） | ✅ 完成 |
| **选项栏** | OptionsBar（色板/线宽步进/文字输入/清除裁剪） | ✅ 完成 |
| **放大镜** | Magnifier（圆形取景 + 3.2x 缩放 + 十字线） | ✅ 完成 |
| **读数** | Readout（像素坐标 + 颜色色块） | ✅ 完成 |
| **裁剪遮罩** | CropOverlay（四块遮罩 + 虚线 + 手柄 + 尺寸标签） | ✅ 完成 |
| **标注外框** | AnnotationOutline（虚线 + 角点） | ✅ 完成 |
| **Toast** | 消息提示（成功/错误颜色区分） | ✅ 完成 |
| **新插件** | blur, ellipse, highlight, number | ✅ 完成 |

---

## 二、剩余问题和改进清单

### 2.1 性能问题

| 问题 | 文件位置 | 难度 | 说明 |
|------|----------|------|------|
| **马赛克逐像素读取** | `src/core/render_ops.cpp:65` | 🟡 中 | `img.pixel(i, j)` 每次调用有格式转换开销。改用 `constScanLine()` 可提速 5-10x |
| **全量操作重放** | `src/core/session.cpp:190` | 🔴 高 | 每次 `render()` 从头重放所有操作，无缓存机制。需实现增量渲染缓存 |
| **拖拽预览无节流** | `src/app/qml/CaptureController.qml:183` | 🟢 低 | 每次 `move` 都调用 `setToolPreview`，高频触发渲染。可加 30fps 节流 |

### 2.2 平台兼容

| 问题 | 文件位置 | 难度 | 说明 |
|------|----------|------|------|
| **X11 无覆盖层** | `src/app/main.cpp:159` | 🔴 高 | LayerShellQt 仅 Wayland，X11 需要 XShape/compositing 回退 |
| **仅主屏幕** | `src/app/main.cpp:162` | 🟡 中 | 未支持多显示器覆盖 |

### 2.3 功能缺失（对比 Spectacle）

| 功能 | Spectacle 实现 | 难度 | 说明 |
|------|---------------|------|------|
| **自由手绘工具** | draw-freehand | 🟡 中 | `render_ops.cpp` 已支持 "pen" 类型，缺插件 |
| **直线工具** | draw-line | 🟡 中 | `render_ops.cpp` 已支持 "line" 类型，缺插件 |
| **Shift 键 45° 锁定** | 线/箭头/矩形 | 🟢 低 | `CaptureController.qml` 未实现 shift 修饰 |
| **工具栏可拖拽** | DragHandler | 🟡 中 | 当前工具栏固定位置 |
| **工具栏自动避让** | 屏幕边缘检测 | 🟡 中 | 当前无翻转逻辑 |
| **撤销/重做禁用态** | 按钮灰显 | 🟢 低 | `FloatingToolBar.qml` 未检查 `canUndo/canRedo` |
| **快捷键提示** | 按钮 tooltip 显示 | 🟢 低 | `ToolButton.qml` 已有 tooltip，但 toolbar 未传递 shortcut |
| **设置 GUI** | SettingsDialog | 🔴 高 | 当前仅 JSON 配置文件，无图形界面 |

### 2.4 UI 细节

| 问题 | 文件位置 | 难度 | 说明 |
|------|----------|------|------|
| **选区手柄样式** | `src/app/qml/CropOverlay.qml:45` | 🟡 中 | 当前方块手柄（11x11 圆角矩形），Spectacle 用 270° 弧形 |
| **虚线动画** | `src/app/qml/CropOverlay.qml:31` | 🟢 低 | 无 dash offset 动画，可加流动效果 |
| **文本预览位置** | `src/app/qml/OptionsBar.qml:97` | 🟡 中 | 文本输入在工具栏内，不在锚点位置实时预览 |
| **颜色选择器** | `src/app/qml/OptionsBar.qml:45` | 🟢 低 | 当前内联色板，可改为弹出式调色板 |

---

## 三、插件清单

### 3.1 已有插件（11 个）

| 插件 | 文件 | 状态 | 备注 |
|------|------|------|------|
| tool_rect | `src/plugins/tool_rect/` | ✅ | 矩形标注 |
| tool_arrow | `src/plugins/tool_arrow/` | ✅ | 箭头标注 |
| tool_text | `src/plugins/tool_text/` | ✅ | 文字标注 |
| tool_mosaic | `src/plugins/tool_mosaic/` | ⚠️ | 马赛克（性能问题） |
| tool_blur | `src/plugins/tool_blur/` | ✅ | 模糊 |
| tool_ellipse | `src/plugins/tool_ellipse/` | ✅ | 椭圆标注 |
| tool_highlight | `src/plugins/tool_highlight/` | ✅ | 高亮标注 |
| tool_number | `src/plugins/tool_number/` | ✅ | 数字标记 |
| export_save | `src/plugins/export_save/` | ✅ | 保存文件 |
| export_clipboard | `src/plugins/export_clipboard/` | ✅ | 复制剪贴板 |

### 3.2 缺失插件（render_ops 已支持，待实现）

| 插件 | render_ops 类型 | 优先级 | 说明 |
|------|----------------|--------|------|
| tool_pen | `pen` | 高 | 自由手绘，需收集 points 数组 |
| tool_line | `line` | 高 | 直线，需 startX/startY/endX/endY |

---

## 四、总体评估

### 4.1 完成度

| 维度 | 完成度 | 说明 |
|------|--------|------|
| UI 架构 | ██████████░ 95% | QML + 主题系统 + 动画 |
| 视觉效果 | ████████░░░ 80% | 玻璃浮条 + hover 动画 + 放大镜 |
| 交互体验 | ████████░░░ 75% | 状态机完整，缺 shift 修饰、拖拽工具栏 |
| 功能完整度 | ███████░░░░ 70% | 11 插件，缺 pen/line |
| 性能 | ██████░░░░░ 60% | 马赛克卡顿，无缓存 |
| 跨平台 | ███░░░░░░░░ 30% | 仅 Wayland |

### 4.2 建议优先修复顺序

| 优先级 | 任务 | 预估时间 |
|--------|------|----------|
| P0 | 马赛克性能优化（`constScanLine` 替换 `pixel()`） | 2-3 小时 |
| P0 | 补全 pen 插件 | 2-3 小时 |
| P0 | 补全 line 插件 | 1-2 小时 |
| P1 | Shift 键 45° 锁定 | 1-2 小时 |
| P1 | 撤销/重做禁用态 | 1 小时 |
| P1 | 快捷键提示显示 | 1 小时 |
| P2 | 虚线流动动画 | 1 小时 |
| P2 | 工具栏可拖拽 | 2-3 小时 |
| P2 | 文本预览在锚点位置 | 3-4 小时 |
| P3 | 设置 GUI | 2-3 天 |
| P3 | X11 覆盖层回退 | 3-5 天 |
| P3 | 多显示器支持 | 2-3 天 |

---

## 五、架构参考

### 关键文件

```
src/
├── app/
│   ├── main.cpp                    # 入口，LayerShell + Portal 截图
│   ├── qml_backend.h/.cpp          # C++ 后端（给 QML 暴露原语）
│   ├── frame_image.h/.cpp          # QML Image source wrapper
│   ├── icon_provider.h/.cpp        # 主题图标提供器
│   └── qml/
│       ├── main.qml                # 根组件
│       ├── Theme.qml               # 设计令牌
│       ├── CaptureController.qml   # 交互状态机
│       ├── FloatingToolBar.qml     # 主工具条
│       ├── OptionsBar.qml          # 选项条
│       ├── CropOverlay.qml         # 裁剪遮罩
│       ├── AnnotationOutline.qml   # 标注外框
│       ├── Magnifier.qml           # 放大镜
│       ├── Readout.qml             # 坐标/颜色读数
│       ├── ToolButton.qml          # 按钮组件
│       └── Toast.qml               # 消息提示
├── core/
│   ├── session.h/.cpp              # 截图会话（编辑栈 + 渲染）
│   ├── render_ops.h/.cpp           # 操作渲染引擎
│   ├── plugin_manager.h/.cpp       # 插件宿主
│   ├── settings.h/.cpp             # 设置注册表
│   └── rpc_peer.h                  # JSON-RPC 通信
├── host/                           # 插件宿主进程
└── plugins/                        # 原生插件
    ├── tool_rect/
    ├── tool_arrow/
    ├── tool_text/
    ├── tool_mosaic/
    ├── tool_blur/
    ├── tool_ellipse/
    ├── tool_highlight/
    ├── tool_number/
    ├── export_save/
    └── export_clipboard/
```

### 渲染流程

```
用户拖拽 → CaptureController.move()
         → backend.setToolPreview()     [实时预览]
         → backend.applyTool()          [松手提交]
         → PluginManager.invoke()       [插件宿主]
         → 返回 operations JSON
         → Session.addOperation()       [压栈]
         → rebuildFrame()               [重渲染]
         → QML 显示 FrameImage
```
