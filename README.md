# frap — 跨平台截图编辑与标注应用（类 VSCode 扩展体系）

frap 是一款以 **类 VSCode 扩展体系** 为设计范式的跨平台截图工具：
原生核心能力完备（截图、选区、标注、撤销/重做、导出），
能力扩展通过**插件**实现，且用户可**深度自定义**（配置、启停、自行编译插件）。

- 交互层基于 **QtQuick/QML 自写**（借鉴 Spectacle 视觉语言，不依赖其代码），玻璃质感悬浮工具条。
- 插件运行在**独立宿主进程**，通过稳定协议与主程序通信；ABI 由 `sdk/` 锁定，
  插件可用任意语言编写、无需链接 Qt（可选）。
- 支持**翻译**、**长截图**等能力型插件（Service / Capture）。

> 由对 KDE Spectacle 的学习而来：不只做一个截图工具，更探索"如何让社区用插件为应用续写功能"。

---

## 特性

- **工作台交互**：截图后直接在**全屏**标注；crop 是一把随时可用的工具（可移动/调整/重裁剪）。
- **标注可编辑**：画好的矩形/箭头/文字/编号→ 点击选中、拖动移动、`Delete` 删除；
  快照栈实现 增/移/删/撤销/重做 统一。
- **丰富工具**：矩形、椭圆、箭头、文字、编号、马赛克、荧光笔、模糊（均为插件）。
- **导出**：保存到目录 / 复制到剪贴板（插件）。
- **翻译**：选区翻译（OCR + 百度图片翻译，译文复制到剪贴板并展示）。
- **长截图**：滚动拼接（自动重叠检测，纵向拼成一张长图）。
- **健壮性**：宿主崩溃自动重启、崩溃报告查看器、非破坏编辑、多显示器（规划）。
- **UI**：放大镜、像素坐标/颜色读数、Toast、色板/线宽选项条、快捷键。

---

## 快速开始

```bash
cmake -B build -G Ninja .
cmake --build build
./build/frap            # 触发系统截图（freedesktop portal）
ctest --test-dir build  # 无头框架 + QML 交互测试
```

**交互**：截图后覆盖层即工作台。拖拽以裁剪框选；选择工具在全屏标注；
点画好的标注可选中/移动/删除；拖拽时有放大镜与坐标/颜色读数。

**快捷键**：`R`矩形 `A`箭头 `T`文字 `M`马赛克 `E`椭圆 `H`荧光笔 `B`模糊 `N`编号
`C`裁剪 `Y`翻译 `G`长截图；`Ctrl+Z/Y` 撤销重做，`Ctrl+C` 复制，`Ctrl+S` 保存，
`Ctrl+D` 完成；`ESC`/右键先取消当前操作再退出。

**验收工具**：`./build/tests/ui_preview <插件目录> <输出.png>` 离线渲染覆盖层 UI；
`./build/frap-host` 是插件宿主；`./build/crash_viewer` 查看崩溃报告。

---

## 架构

```
frap（主程序 / QML 工作台）
 ├─ FrapBackend       C++ 原语层：帧渲染、op 命中/移动/删除、裁剪、工具条与选项模型、导出
 ├─ CaptureController QML 交互状态机（全 QML 交互）
 ├─ OptionsBar / FloatingToolBar / CropOverlay / Magnifier / Readout / Toast / TranslationPanel
 └─ plugin manager    管理宿主生命周期（崩溃自动重启/退避）
         │ JSON-RPC over stdio（换行分帧，版本握手）
         ▼
 frap-host（插件宿主，独立进程，崩溃隔离）
         └─ 原生插件（稳定 C ABI：frap_plugin_entry → FrapPluginVtable）
```

**插件边界 = 协议，不是 C++ ABI**。主程序与宿主唯一的契约是 JSON-RPC 线协议（`docs/protocol.md`）。

| 目录 | 说明 |
|---|---|
| `sdk/` | 插件 SDK（仅头文件，零依赖）：`plugin_api.h`（稳定 C ABI）、`plugin_helper.h`、`json_min.h` |
| `src/core/` | `frap_core`：会话/渲染/设置/插件管理/崩溃采集 |
| `src/app/` | `frap_ui` + 主程序：QML 后端 + 覆盖层 |
| `src/host/` | `frap-host`：插件宿主 |
| `src/plugins/` | 内置插件（同时作为 SDK 示例） |
| `src/crash/` | `crash_viewer` |
| `tests/` | 框架 + QML 交互测试、UI 预览 |

---

## 写一个插件

任何插件 = 一个 `.so` + 一个导出符号 `frap_plugin_entry`（stable C ABI），**零依赖**：

1. 复制 `sdk/plugin-template/`，改类名与 JSON；
2. 构建，把 `.so` 放到 `~/.config/frap/plugins/`；
3. 重启应用，工具条上出现你的按钮。

插件类型：**Tool**（标注）、**Export**（导出）、**Capture**（采集来源）、**Service**（通用服务）。

详见 `docs/plugin-sdk.md`。

### 内置插件

**标注工具**：`tool_rect` 矩形、`tool_ellipse` 椭圆、`tool_arrow` 箭头、`tool_text` 文字、
`tool_number` 编号（自动递增）、`tool_mosaic` 马赛克、`tool_highlight` 荧光笔、`tool_blur` 模糊。

**导出**：`export.save`（保存到目录）、`export.clipboard`（复制到剪贴板）。

**服务 / 采集**：
- `service.translate` —— **翻译**：把选区图像交给百度图片翻译 API（配置读取
  `~/.config/screenshot-translator/config.json`，含 `appid`/`api_key`），
  返回 原文/译文，译文复制到剪贴板并显示。参照 `~/.local/bin/screenshot-translator.py`。
- `capture.longshot` —— **长截图**：分屏截取后自动检测垂直重叠并纵向拼接成一张长图。

---

## 用户配置

`~/.config/frap/settings.json`：

```json
{
  "settings": {
    "tool.rect.color": "#e11d48",
    "export.save.directory": "/home/u/Pictures",
    "app.export.directory": ""
  },
  "plugins": { "enabled": [], "disabled": [] }
}
```

- 插件目录：`~/.config/frap/plugins/`、`$FRAP_PLUGIN_PATH`、可执行文件旁 `plugins/`。
- 崩溃报告：`~/.local/share/frap/crashes/`，用 `crash_viewer list|show|clean` 查看。
- 翻译配置（外部工具共享）：`~/.config/screenshot-translator/config.json`。

---

## 测试

```bash
ctest --test-dir build --output-on-failure
```
- `core_framework_test`：宿主握手、插件调用链、非破坏渲染、undo、崩溃重启。
- `qml_controller_test`：QML 交互状态机（工具条/绘制/裁剪/标注移动删除/快捷键/文字/编号/完成导出）。

---

## 技术栈

C++20 · Qt 6（Widgets + Quick/QML + QuickControls2）· CMake · Wayland（LayerShellQt）·
freedesktop portal · JSON-RPC over stdio

## Roadmap

见 `docs/architecture.md`（多显示器、插件市场、进程外脚本宿主、二进制图像通道、设置面板、更多 Service 插件）。
