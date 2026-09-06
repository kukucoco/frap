# 架构与设计决策

frap 以"类 VSCode 扩展体系"为设计范式：核心提供完备的原生截图编辑能力，
能力扩展通过插件实现，且用户可深度自定义。

## 核心理念

**插件边界是协议，不是 C++ ABI。** 主程序与插件宿主之间的唯一契约是
`docs/protocol.md` 描述的 JSON-RPC 线协议。由此：

- 插件可用任意语言实现（原生插件用 C 导出符号即可，甚至可做脚本宿主）。
- 宿主升级 Qt / 编译器后，已发布插件无需重编译。
- 插件崩溃被隔离在宿主进程中，主程序可自动重启宿主。

## 进程模型

```
┌─────────────────────────────┐
│  frap（主程序）     │
│  ├─ CaptureSession           │  原图 + 选区 + 操作栈（非破坏编辑）
│  ├─ OverlayWindow            │  覆盖层选区 + 插件贡献的浮动工具条
│  ├─ Settings                 │  配置注册表（schema + 默认 + 用户覆盖）
│  └─ PluginManager            │  管理宿主生命周期，转发协议消息
└─────────────┬───────────────┘
              │ JSON-RPC over stdio（换行分帧）
┌─────────────▼───────────────┐
│  frap-host（插件宿主）  │
│  ├─ 原生插件加载器（QLibrary）│  稳定 C ABI（plugin_api.h）
│  └─ 插件实例（Tool/Export…） │  运行在独立进程，崩溃隔离
└─────────────┬───────────────┘
              │ 桌面集成动作回传（app.clipboard.set / app.open.file…）
```

## 目录结构

```
sdk/              插件 SDK（仅头文件，无任何依赖）
  plugin_api.h     稳定 C ABI（vtable + 宏）
  plugin_helper.h  C++ 便利基类
  json_min.h       零依赖 JSON
  plugin-template/ 独立可编译的插件模板工程
src/core/         frap_core 静态库
  protocol.h       消息构造 + 版本常量
  rpc_peer.h       JSON-RPC 线协议客户端/服务端通用实现
  settings.*       配置注册表
  plugin_manager.* 宿主进程生命周期管理
  session.*        截图会话 + 非破坏编辑栈
  render_ops.*     声明式操作的内置渲染器
  logging.*        控制台日志
src/host/         插件宿主进程
src/app/          主程序（portal 截图 + 覆盖层 + 工具条）
src/plugins/      内置插件（同样走插件通道，作为 SDK 示例）
tests/            无头集成测试
config/           默认设置
docs/             协议 / SDK / 架构文档
```

## 关键设计点

### 0. 覆盖层即工作台（Spectacle 设计理念）

不强制"先框选再编辑"：截图后覆盖层直接可用全部工具，标注在全屏任意处绘制；
**crop 裁剪**是一把随时可调用的工具，定义输出范围（无裁剪 = 全屏输出）。
画好的标注可点选、拖动移动、Delete 删除（操作带稳定 `id`，`CaptureSession`
用操作列表快照栈统一实现 增/移/删/撤销/重做）。

**UI 层为 QtQuick/QML 自写**（不复用 Spectacle 源码，仅借鉴视觉语言）：
- 交互状态机在 QML（`CaptureController.qml`，全 QML 交互），C++ 只提供原语
  （`FrapBackend`：frame 渲染、op 命中/移动/删除、裁剪、工具条与选项模型）。
- 组件：玻璃悬浮工具条/选项条（色板、线宽）、裁剪遮罩+手柄、标注外框、
  放大镜、坐标/颜色读数、Toast。仅依赖 Qt Quick + QuickControls2。
- 依赖的 C++ 类型 `FrameImage`（QQuickPaintedItem）与 `IconProvider`（主题图标）
  运行时注册；QML 模块用 `qmldir` 驱动。

### 1. 非破坏编辑模型

Tool 插件返回**声明式操作**（如 `{type:"rect", rect, color, width}`），
主程序内置渲染器负责像素绘制。原图 + 操作栈可：

- 撤销/重做（栈指针移动）
- 步骤序列化（为"步骤记录"等 Service 插件预留）
- 保持像素无损（直到最终导出）

```cpp
session.addOperation(op);  // 推入栈，丢弃已撤销分支
session.render();          // 原图 + 栈中所有操作
session.undo(); session.redo();
```

### 2. 配置注册表（仿 VSCode settings）

插件通过 manifest 的 `configSchema` 声明设置项，主程序负责：
默认值、类型校验、用户覆盖、持久化到 `~/.config/frap/settings.json`。
调用插件时，有效设置按插件 id 切片注入 `config`，插件无需自己读文件。

### 3. 贡献点（contribution points）

插件 manifest 中的 `contributions` 声明 UI 贡献：

```json
{"type":"tool","data":{"id":"tool.rect","label":"矩形","order":10}}
{"type":"export","data":{"id":"export.save","label":"保存到目录","order":100}}
```

主程序聚合后填充浮动工具条；`order` 控制顺序。未来可扩展
`command`（快捷键）、`menu`（右键菜单）、`panel`（侧栏）等贡献点。

### 4. 桌面能力集中（actions）

插件不直接访问剪贴板/文件打开/通知，而是返回 `actions` 数组，
由主程序统一执行。权限集中、行为可控，也便于审计。

### 5. 宿主健壮性

- 宿主异常退出 → 主程序自动重启（指数退避，上限 6 次）。
- 插件 `handle` 返回非法 JSON → 记错误不崩溃。
- 协议版本不匹配 → 拒绝加载。

### 6. 崩溃进程查看器（验收工具）

主程序与宿主安装崩溃信号处理器：崩溃时把 backtrace + 最近日志落盘到
`~/.local/share/frap/crashes/`，然后保留 core 继续崩溃。
`crash_viewer`（`list`/`show`/`clean`）用于验收期排查崩溃。

## 版本与演进

- `FRAP_PROTOCOL_VERSION`：线协议版本（语义兼容位）。
- `FRAP_PLUGIN_API_VERSION`：C ABI 版本（内存布局位）。
- 向后兼容策略：新增消息类型不 bump 版本；破坏性变更才 bump。

## Roadmap

1. **Capture 插件**：滚动截图已落地（`capture.longshot` 拼接）；延时/窗口录制规划中。
2. **Service 插件**：翻译已落地（`service.translate`）；步骤记录、OCR/公式/表格识别
   （借助 actions 与用户交互）规划中。
3. **脚本宿主**：协议是语言无关的，可做 Python/Lua 宿主，脚本即成插件。
4. **性能**：图像传输从 base64 升级为共享内存/专用 socket 二进制通道。
5. **多显示器**：覆盖层扩展到全部输出。
6. **包管理**：`--install-plugin`、插件市场目录。
