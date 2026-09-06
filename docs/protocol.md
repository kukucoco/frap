# 插件协议规范（Protocol v1）

主程序（core）与插件宿主（frap-host）之间、以及未来社区插件宿主实现之间，
通过本文档定义的线协议通信。**这是插件生态的稳定契约**：只要协议不变，
宿主内部如何加载插件、用什么语言/框架实现都不影响兼容性。

## 传输与分帧

- 通道：进程 stdio（stdin / stdout），主程序是写端，宿主是读端。
- 分帧：换行符（`\n`）分隔，一行一个完整 JSON 对象。JSON 使用紧凑序列化，
  base64 载荷不含换行符，因此按行分帧安全。
- 语义：JSON-RPC 2.0 风格（request / response / notification）。

消息一律带 `"jsonrpc":"2.0"` 字段。

```json
{"jsonrpc":"2.0","method":"...","params":{...}}          // notification
{"jsonrpc":"2.0","id":1,"method":"...","params":{...}}    // request
{"jsonrpc":"2.0","id":1,"result":{...}}                   // response
{"jsonrpc":"2.0","id":1,"error":{"code":-32000,"message":"..."}}  // 错误
```

## 版本握手

宿主启动后必须**首先**发送 `initialize` notification：

```json
{
  "jsonrpc": "2.0",
  "method": "initialize",
  "params": {
    "apiVersion": 1,
    "protocolVersion": 1,
    "hostInfo": { "pid": 1234, "qtVersion": "6.11.2", "hostVersion": "0.1.0" }
  }
}
```

- `apiVersion`：C ABI 版本（见 `plugin_api.h` 的 `FRAP_PLUGIN_API_VERSION`）。
- `protocolVersion`：线协议版本（`FRAP_PROTOCOL_VERSION`）。
  主程序检测到版本不匹配时应拒绝后续消息并关闭宿主。

主程序随后回发 `initialize.result` notification：

```json
{
  "jsonrpc": "2.0",
  "method": "initialize.result",
  "params": {
    "settings": { "tool.rect.color": "#e11d48", ... }
  }
}
```

`settings` 是全部**有效设置**（默认值 + 用户覆盖合并后的展平结果），
宿主可将其中属于各插件的键切分后注入插件 `config`。

## 插件发现

宿主加载插件后，为每个插件发送一条 `plugin.manifest` notification：

```json
{
  "jsonrpc": "2.0",
  "method": "plugin.manifest",
  "params": {
    "manifest": {
      "id": "tool.rect",
      "name": "矩形",
      "version": "1.0.0",
      "kind": 1,
      "configSchema": [
        {"key":"tool.rect.color","type":"color","label":"颜色","default":"#e11d48"},
        {"key":"tool.rect.width","type":"int","label":"线宽","default":3,"min":1,"max":20}
      ],
      "contributions": [
        {"type":"tool","data":{"id":"tool.rect","label":"矩形","order":10}}
      ]
    }
  }
}
```

- `kind`：`1=Tool` `2=Export` `3=Capture` `4=Service`（与 `plugin_api.h` 一致）。
- `configSchema`：插件声明其设置项，主程序据此渲染设置 UI 并做类型校验。
- `contributions`：UI 贡献点（工具条按钮、导出动作、命令等）。

所有插件上报完毕后，宿主发送一条**完成标记**：

```json
{"jsonrpc":"2.0","method":"plugin.manifest","params":{"complete":true}}
```

主程序收到完成标记即认为插件系统就绪。

## 插件调用

主程序 → 宿主，request：

```json
{
  "jsonrpc": "2.0",
  "id": 7,
  "method": "plugin.invoke",
  "params": {
    "pluginId": "tool.rect",
    "request": "apply",
    "payload": { ... },
    "config": { "tool.rect.color": "#ff0000", "tool.rect.width": 5 }
  }
}
```

- `request`：插件自解释的操作名（工具通常用 `apply`，导出用 `execute`）。
- `payload`：主程序传入的业务数据（选区、图像引用等）。
- `config`：该插件声明键的有效设置切片。

宿主将 `request/payload/config` 转交插件，插件返回 JSON 作为 response result：

```json
{
  "jsonrpc": "2.0",
  "id": 7,
  "result": {
    "status": "ok",
    "message": "已保存到 /home/u/Pictures/screenshot_20260901_000000.png",
    "operations": [ { "type": "rect", "rect": {...}, "color": "#ff0000", "width": 5 } ],
    "actions": [ { "method": "app.open.file", "params": {"path": "..."} } ]
  }
}
```

### 结果字段

| 字段 | 含义 |
|---|---|
| `status` | `"ok"` 或 `"error"` |
| `message` | 人类可读说明（可选） |
| `operations` | 声明式编辑操作数组（Tool 插件返回；见下节） |
| `actions` | 回传主程序的桌面集成动作数组（可选） |

### 动作（actions）

宿主不直接触碰桌面能力（剪贴板、打开文件、通知），而是通过动作请求主程序执行：

| method | params | 说明 |
|---|---|---|
| `app.clipboard.set` | `{format:"png", data:"<base64>"}` | 设置剪贴板图像 |
| `app.open.file` | `{path:"..."}` | 用默认程序打开文件 |
| `app.notify` | `{summary,body}` | 系统通知 |

## 编辑操作（operations）

Tool 插件返回**声明式操作**，像素渲染由主程序内置渲染器完成。
坐标一律为**图像物理像素**。主程序维护 原图+操作栈，实现非破坏编辑与撤销/重做。

内置操作类型：

| type | 关键字段 |
|---|---|
| `rect` | `rect:{x,y,w,h}`, `color`, `width`, `fill?` |
| `ellipse` | 同上 |
| `line` | `start:{x,y}`, `end:{x,y}`, `color`, `width` |
| `arrow` | `start`, `end`, `color`, `width`, `headSize?` |
| `pen` | `points:[{x,y},...]`, `color`, `width` |
| `text` | `point:{x,y}`, `text`, `color`, `size?`, `fontFamily?`, `bold?` |
| `highlight` | `rect`, `color`, `alpha?` |
| `mosaic` | `rect`, `cell?` |
| `blur` | `rect`, `radius?` |
| `number` | `point`, `text`, `color`, `size?` |

## 服务 / 采集类插件

主程序通过同样的 `plugin.invoke` 与 `service` / `capture` 类插件交互（`request` 自定）。

**翻译**（`service.translate`）：
```json
{"request":"translate","payload":{"imageBase64":"...","imageSize":{"w":800,"h":600}}}
▸ {"status":"ok","src":"...","dst":"...","paste_img":"<base64 译文图>"}
```

**长截图拼接**（`capture.longshot`）：
```json
{"request":"stitch","payload":{"sections":["<b64>","<b64>"]}}
▸ {"status":"ok","imageBase64":"<b64>","width":1600,"height":2000}
```

## 生命周期

- 主程序退出前发送 `host.shutdown` notification，宿主应优雅退出（调用插件 `shutdown`）。
- 宿主异常退出时主程序会自动重启宿主（退避重试）。
- 插件崩溃被限制在宿主进程内，不影响主程序。

## 日志

宿主可用 `log` notification 上报日志：

```json
{"jsonrpc":"2.0","method":"log","params":{"level":"warn","message":"..."}}
```

## 扩展点（Roadmap）

- `Capture`（kind=3）：滚动截图、延时截图、窗口录制等采集来源（`capture.longshot` 已落地）。
- `Service`（kind=4）：翻译、步骤记录、OCR/公式/表格识别等通用服务
  （`service.translate` 已落地），通过 `app.notify` 等动作与用户交互。
