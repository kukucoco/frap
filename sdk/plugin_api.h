/*
 * plugin_api.h —— frap 插件稳定 C ABI（ABI 契约）
 *
 * 这是插件与宿主之间唯一的内存级契约。它刻意不使用任何 C++ 库
 * （不含 Qt、不含 STL 对象），从而保证：
 *
 *   - 插件可用任何语言编写（C/C++/Rust 等能导出 C 符号的语言）
 *   - 宿主升级 Qt / 编译器后，已发布的插件无需重新编译
 *   - 插件崩溃被隔离在宿主进程中，不影响主程序
 *
 * 约定：
 *   - 所有返回的 const char* 指向插件内部内存，在该插件下一次调用前有效
 *     （单线程调用，无需多线程安全）
 *   - handle() 的输入/输出均为 JSON 字符串（协议见 docs/protocol.md）
 *   - 插件只需导出单个符号 frap_plugin_entry
 *
 * 版本：
 *   FRAP_PLUGIN_API_VERSION 是 C ABI 版本（内存布局）
 *   FRAP_PROTOCOL_VERSION 是 JSON 协议版本（消息语义）
 */
#ifndef FRAP_PLUGIN_API_H
#define FRAP_PLUGIN_API_H

#ifdef __cplusplus
extern "C" {
#endif

#define FRAP_PROTOCOL_VERSION   1
#define FRAP_PLUGIN_API_VERSION 1

#if defined(_WIN32)
#  define FRAP_PLUGIN_EXPORT __declspec(dllexport)
#else
#  define FRAP_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

/* 插件类型 */
typedef enum FrapPluginKind {
    SC_PLUGIN_KIND_TOOL    = 1, /* 标注工具：矩形/箭头/文字/马赛克... */
    SC_PLUGIN_KIND_EXPORT  = 2, /* 导出动作：保存文件/剪贴板/上传/OCR... */
    SC_PLUGIN_KIND_CAPTURE = 3, /* 截图来源：全屏/窗口/延时/滚动... */
    SC_PLUGIN_KIND_SERVICE = 4  /* 通用服务：翻译/步骤记录/公式识别... */
} FrapPluginKind;

/*
 * 插件 vtable。宿主通过 QLibrary::resolve("frap_plugin_entry")
 * 取得该结构体指针，再通过函数指针调用。
 */
typedef struct FrapPluginVtable {
    /* 必须为 FRAP_PLUGIN_API_VERSION，宿主校验，不匹配则拒绝加载 */
    unsigned int apiVersion;

    const char* (*id)(void);                 /* 全局唯一，如 "tool.rect" */
    const char* (*name)(void);               /* 显示名，如 "矩形" */
    const char* (*version)(void);            /* 插件版本，如 "1.0.0" */
    FrapPluginKind (*kind)(void);      /* 插件类型 */
    const char* (*configSchemaJson)(void);   /* JSON 数组：设置项 schema（见 docs） */
    const char* (*contributionsJson)(void);  /* JSON 数组：UI 贡献点（工具条/命令） */
    /*
     * 处理一次调用。jsonRequest 是宿主转发的主程序请求（JSON 字符串）。
     * 返回 JSON 结果字符串，指向插件内部缓冲，下次调用前有效。
     */
    const char* (*handle)(const char* jsonRequest);
    void (*shutdown)(void);                  /* 宿主退出前调用，插件做清理 */
} FrapPluginVtable;

/*
 * 插件唯一入口。宿主加载 .so/.dll/.dylib 后解析该符号。
 * 返回指向静态/全局 vtable 的指针，宿主不负责释放。
 */
typedef FrapPluginVtable* (*FrapPluginEntry)(void);

#ifdef __cplusplus
}
#endif

#endif /* FRAP_PLUGIN_API_H */
