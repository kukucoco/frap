/*
 * export_clipboard —— 复制到剪贴板的导出插件（Qt 无关插件示例）
 *
 * 演示：剪贴板属主程序桌面能力。插件不直接访问剪贴板，
 *       而是通过 actions 把图像数据回传主程序，由主程序执行
 *       app.clipboard.set。这保持了"桌面集成权限集中在主程序"的设计。
 */
#include "plugin_helper.h"
#include "json_min.h"

class ExportClipboard : public sc::PluginBase {
public:
    const std::string& id() const override { return m_id; }
    const std::string& name() const override { return m_name; }
    sc::PluginKind kind() const override { return sc::PluginKind::Export; }

    std::string contributionsJson() const override
    {
        return R"([
            {"type":"export","data":{"id":"export.clipboard","label":"复制","order":110}}
        ])";
    }

    std::string handle(const std::string& request) override
    {
        const jmin::Json req = jmin::parse(request);
        const std::string data = req.get("payload").get("imageBase64").asString();

        jmin::Json params = jmin::Json::makeObject();
        params.set("format", jmin::Json::makeString("png"));
        params.set("data", jmin::Json::makeString(data));

        jmin::Json action = jmin::Json::makeObject();
        action.set("method", jmin::Json::makeString("app.clipboard.set"));
        action.set("params", params);

        jmin::Json actions = jmin::Json::makeArray();
        actions.push(action);

        jmin::Json result = jmin::Json::makeObject();
        result.set("status", jmin::Json::makeString("ok"));
        result.set("message", jmin::Json::makeString("已复制到剪贴板"));
        result.set("actions", actions);
        return result.toString();
    }

private:
    const std::string m_id = "export.clipboard";
    const std::string m_name = "复制到剪贴板";
};

FRAP_PLUGIN_DEFINE(ExportClipboard)
