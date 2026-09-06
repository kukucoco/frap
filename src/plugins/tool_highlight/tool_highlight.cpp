/*
 * tool_highlight —— 荧光笔工具（Qt 无关插件示例）
 */
#include "plugin_helper.h"
#include "json_min.h"

class HighlightTool : public sc::PluginBase {
public:
    const std::string& id() const override { return m_id; }
    const std::string& name() const override { return m_name; }
    sc::PluginKind kind() const override { return sc::PluginKind::Tool; }

    std::string configSchemaJson() const override
    {
        return R"([
            {"key":"tool.highlight.color","type":"color","label":"颜色","default":"#fde047"}
        ])";
    }

    std::string contributionsJson() const override
    {
        return R"([
            {"type":"tool","data":{"id":"tool.highlight","label":"荧光笔","order":45,"interaction":"drag-region","shortcut":"h"}}
        ])";
    }

    std::string handle(const std::string& request) override
    {
        try {
            const jmin::Json req = jmin::parse(request);
            const jmin::Json& config = req.get("config");
            const std::string color = config.get("tool.highlight.color").asString("#fde047");

            const jmin::Json& payload = req.get("payload");
            const jmin::Json& rect = payload.get("rect").isNull()
                                         ? payload.get("selection")
                                         : payload.get("rect");

            jmin::Json op = jmin::Json::makeObject();
            op.set("type", jmin::Json::makeString("highlight"));
            op.set("rect", rect);
            op.set("color", jmin::Json::makeString(color));
            op.set("alpha", jmin::Json::makeNumber(110));

            jmin::Json ops = jmin::Json::makeArray();
            ops.push(op);

            jmin::Json result = jmin::Json::makeObject();
            result.set("status", jmin::Json::makeString("ok"));
            result.set("operations", ops);
            return result.toString();
        } catch (const std::exception& e) {
            return jmin::Json::objectOf("message", jmin::Json::makeString(e.what())).toString();
        }
    }

private:
    const std::string m_id = "tool.highlight";
    const std::string m_name = "荧光笔";
};

FRAP_PLUGIN_DEFINE(HighlightTool)
