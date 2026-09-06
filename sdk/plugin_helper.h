/*
 * plugin_helper.h —— C++ 插件作者便利层（可选）
 *
 * 插件并不需要 Qt、也不需要 frap_core。只需包含本头文件，
 * 继承 sc::PluginBase，并调用 FRAP_PLUGIN_DEFINE(ClassName)。
 *
 * 示例：
 *   class MyTool : public sc::PluginBase {
 *   public:
 *       const std::string& id() const override { return m_id; }
 *       sc::PluginKind kind() const override { return sc::PluginKind::Tool; }
 *       std::string handle(const std::string& req) override { ... }
 *   private:
 *       std::string m_id = "tool.mine";
 *   };
 *   FRAP_PLUGIN_DEFINE(MyTool)
 */
#ifndef FRAP_PLUGIN_HELPER_H
#define FRAP_PLUGIN_HELPER_H

#include "plugin_api.h"

#include <string>

namespace sc {

enum class PluginKind : int {
    Tool = SC_PLUGIN_KIND_TOOL,
    Export = SC_PLUGIN_KIND_EXPORT,
    Capture = SC_PLUGIN_KIND_CAPTURE,
    Service = SC_PLUGIN_KIND_SERVICE,
};

class PluginBase {
public:
    virtual ~PluginBase() = default;

    virtual const std::string& id() const = 0;
    virtual const std::string& name() const = 0;
    virtual std::string version() const { static std::string v = "1.0.0"; return v; }
    virtual PluginKind kind() const = 0;
    virtual std::string configSchemaJson() const { return "[]"; }
    virtual std::string contributionsJson() const { return "[]"; }
    virtual std::string handle(const std::string& /*request*/) { return R"({"status":"ok"})"; }
    virtual void shutdown() {}
};

/*
 * 生成宿主所需的 vtable。插件 API 的调用约定：
 *   - 返回的 const char* 在下次调用前有效（内部 thread_local 缓冲）
 *   - 宿主单线程调用 handle
 */
template <typename T>
FrapPluginVtable* makeVtable(T* plugin)
{
    static thread_local T* current = plugin;
    static thread_local std::string requestBuffer;
    static thread_local std::string resultBuffer;

    static FrapPluginVtable vt = {
        FRAP_PLUGIN_API_VERSION,
        []() -> const char* { return current->id().c_str(); },
        []() -> const char* { return current->name().c_str(); },
        []() -> const char* {
            static thread_local std::string v = current->version();
            return v.c_str();
        },
        []() -> FrapPluginKind { return static_cast<FrapPluginKind>(current->kind()); },
        []() -> const char* {
            static thread_local std::string s = current->configSchemaJson();
            return s.c_str();
        },
        []() -> const char* {
            static thread_local std::string s = current->contributionsJson();
            return s.c_str();
        },
        [](const char* req) -> const char* {
            requestBuffer = req ? req : "";
            resultBuffer = current->handle(requestBuffer);
            return resultBuffer.c_str();
        },
        []() { current->shutdown(); },
    };
    return &vt;
}

} // namespace sc

#define FRAP_PLUGIN_DEFINE(ClassName)                                \
    namespace {                                                            \
    ClassName sc_plugin_instance;                                          \
    }                                                                      \
    extern "C" FRAP_PLUGIN_EXPORT FrapPluginVtable*            \
    frap_plugin_entry(void)                                          \
    {                                                                      \
        return ::sc::makeVtable(&sc_plugin_instance);                      \
    }

#endif /* FRAP_PLUGIN_HELPER_H */
