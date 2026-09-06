# 插件开发指南（SDK）

插件是独立编译的共享库（`.so` / `.dll` / `.dylib`），**只依赖 `sdk/` 下的头文件**，
不需要链接 Qt、也不需要安装本项目的任何库。这保证了：

- 插件可用任何能导出 C 符号的语言编写（C / C++ / Rust 等）。
- 宿主升级 Qt、编译器后，已发布的插件无需重新编译（ABI 由 `plugin_api.h` 锁定）。
- 插件崩溃被隔离在宿主进程中。

## 目录

插件按 `.so` 文件分发，放在以下任一目录即可被加载：

1. 用户配置目录：`~/.config/frap/plugins/`
2. 环境变量：`MY_FRAP_PLUGIN_PATH`（`:` 分隔多个目录）
3. 可执行文件旁：`<安装目录>/plugins/`

## 最小插件（Qt 无关）

任何插件只需导出**一个符号** `frap_plugin_entry`：

```cpp
#include "plugin_api.h"        // sdk/
#include "json_min.h"          // sdk/，零依赖 JSON

static const char* g_result = "";   // 返回缓冲：下次调用前有效

extern "C" FRAP_PLUGIN_EXPORT FrapPluginVtable* frap_plugin_entry(void)
{
    static FrapPluginVtable vt = {
        FRAP_PLUGIN_API_VERSION,
        []() -> const char* { return "tool.rect"; },   // id
        []() -> const char* { return "矩形"; },         // name
        []() -> const char* { return "1.0.0"; },        // version
        []() -> FrapPluginKind { return SC_PLUGIN_KIND_TOOL; },
        []() -> const char* { return "[]"; },           // configSchema
        []() -> const char* { return R"([{"type":"tool","data":{"id":"tool.rect","label":"矩形","order":10}}])"; },
        [](const char* req) -> const char* {           // handle
            static std::string out;
            out = /* 处理 req，返回 JSON 结果 */;
            return out.c_str();
        },
        []() {}                                        // shutdown
    };
    return &vt;
}
```

`handle` 的输入输出都是 JSON 字符串，约定返回指针在下一次调用前有效。
消息格式见 `docs/protocol.md`。

## 用 C++ 便利层编写

更省事的写法是继承 `sc::PluginBase` 并使用 `FRAP_PLUGIN_DEFINE`：

```cpp
#include "plugin_helper.h"
#include "json_min.h"

class MyTool : public sc::PluginBase {
public:
    const std::string& id() const override { return m_id; }
    const std::string& name() const override { return m_name; }
    sc::PluginKind kind() const override { return sc::PluginKind::Tool; }
    std::string configSchemaJson() const override { return R"([...])"; }
    std::string contributionsJson() const override { return R"([...])"; }
    std::string handle(const std::string& request) override { return ...; }
private:
    const std::string m_id = "tool.mine";
    const std::string m_name = "我的工具";
};
FRAP_PLUGIN_DEFINE(MyTool)
```

## 插件类型

| 类型 | `kind` | 用途 | request |
|---|---|---|---|
| Tool | `SC_PLUGIN_KIND_TOOL` | 标注工具（矩形/箭头/文字/马赛克…） | `apply` |
| Export | `SC_PLUGIN_KIND_EXPORT` | 导出动作（保存/剪贴板/上传/OCR…） | `execute` |
| Capture | `SC_PLUGIN_KIND_CAPTURE` | 截图来源（全屏/窗口/延时/滚动…） | 见协议 |
| Service | `SC_PLUGIN_KIND_SERVICE` | 通用服务（翻译/步骤记录/公式识别…） | 自定 |

### Tool 插件

工具在工具条上的贡献点声明 `interaction`（交互方式）与 `shortcut`（快捷键），
主程序据此驱动鼠标交互与键盘切换：

```json
{"type":"tool","data":{"id":"tool.rect","label":"矩形","order":10,"interaction":"drag-rect","shortcut":"r"}}
```

| interaction | 交互 | 提交给插件的 payload 几何 |
|---|---|---|
| `drag-rect` | 拖拽画矩形/椭圆 | `rect:{x,y,w,h}` |
| `drag-line` | 拖拽画箭头/直线（保方向） | `start:{x,y}`, `end:{x,y}` |
| `drag-region` | 拖拽选择作用区域（马赛克/模糊/荧光笔） | `rect:{x,y,w,h}` |
| `click-point` | 点击定位（文字输入 / 编号自动标注） | `point:{x,y}` + `params.text` |

> 标注工具在全屏绘制（工作台模型），坐标一律为图像物理像素。
> 主程序还提供 **crop 工具**（内置，非插件）用于定义输出裁剪区。

`apply` 请求的 `payload` 结构：

```json
{
  "imageSize": {"w":1920,"h":1080},
  "rect": {...} | "start"/"end" | "point": {...},   // 本次绘制的几何
  "params": {}                                     // 瞬态参数
}
```

**编号工具（auto=number）**：贡献点声明 `"auto":"number"`，主程序点击即调用
`apply`，并传入 `params.number`（基于会话中已有点数自动递增）。插件保持无状态：

```json
{"type":"tool","data":{"id":"tool.number","label":"编号","order":35,
  "interaction":"click-point","shortcut":"n","auto":"number"}}
```

### Export 插件

`execute` 请求的 `payload` 包含：

```json
{
  "imageRef": "/tmp/myscr_....png",   // 主程序渲染结果（选区）临时 PNG 路径
  "imageBase64": "...",               // 同图 base64（无需读文件）
  "imageSize": {"w":800,"h":600}
}
```

导出插件在宿主进程中执行副作用（写文件、网络请求等），
需要桌面集成时返回 `actions`（如 `app.clipboard.set`），由主程序代为执行。

### Service 插件（通用服务）

贡献点类型 `service`，在工具条上显示为一个按钮（如"翻译"）。主程序调用时
把当前选区/全屏图像的 base64 传入，插件返回结果。

`service.translate`（示例）—— `translate` 请求：

```json
{
  "request": "translate",
  "payload": { "imageBase64": "...", "imageSize": {"w":800,"h":600} }
}
```

返回：

```json
{"status":"ok","src":"...","dst":"...","paste_img":"<base64 译文图>"}
```

> 可选用 Qt（本例用 QtNetwork 调百度图片翻译），以"能跑"优先；
> Qt 无关插件仍是首选形态。

### Capture 插件（采集来源 / 长截图）

贡献点类型 `service` 亦可承载采集能力（主程序做分屏/滚动交互，插件做图像处理）。

`capture.longshot`（示例）—— `stitch` 请求：

```json
{
  "request": "stitch",
  "payload": { "sections": ["<base64 PNG>", "<base64 PNG>"] }
}
```

返回：

```json
{"status":"ok","imageBase64":"<拼接后 PNG>","width":1600,"height":2000}
```

插件内置**垂直重叠检测**：自动找出相邻两图重复的行数，去掉后纵向拼接，
因此即使滚动有少量位移也能得到无缝长图。

## 配置（configSchema）

插件通过 `configSchema` 声明设置项，主程序负责合并默认值、校验类型、持久化。
支持的 `type`：

| type | 字段 | 渲染 |
|---|---|---|
| `string` | `default` | 文本 |
| `int` | `default`,`min`,`max` | 步进器 |
| `double` | `default`,`min`,`max` | 步进器 |
| `bool` | `default` | 开关 |
| `enum` | `default`,`enum:[...]` | 下拉 |
| `color` | `default` | 色板 |

每个键以 `插件id.` 开头（如 `tool.rect.color`）。主程序调用插件时，
会把这些键的有效值切片到 `config` 传入。

> 用户在主程序中调整选项（如点击色板、步进线宽）会写入设置并持久化，
> 这正是"深度用户自定义"的一部分。

## 用户自定义

- 设置文件：`~/.config/frap/settings.json`，可手改或由 UI 修改。
- 插件启停：`"plugins": {"disabled": ["tool.mosaic"]}`。
- 插件目录：见上文。

## 模板工程

复制 `sdk/plugin-template/` 目录即可开始：
插件只用 CMake 的 `add_library(... SHARED)` 构建，不依赖本项目安装。

## 社区要点

1. **协议优先**：只要遵循 `docs/protocol.md`，甚至可以不写 C++——
   未来可提供 Python/Lua/进程型宿主，直接把脚本暴露成"插件"。
2. **声明式操作**：能输出内置操作类型的工具不需要碰像素，天然支持撤销/重做
   与步骤记录。
3. **actions 约定**：桌面能力统一回传主程序执行，权限集中、避免滥用。
