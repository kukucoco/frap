/*
 * tool_ellipse —— 椭圆标注工具（Qt 无关插件示例）
 */
#include "plugin_helper.h"
#include "json_min.h"

class EllipseTool : public sc::PluginBase {
public:
    const std::string& id() const override { return m_id; }
    const std::string& name() const override { return m_name; }
    sc::PluginKind kind() const override { return sc::PluginKind::Tool; }

    std::string configSchemaJson() const override
    {
        return R"([
            {"key":"tool.ellipse.color","type":"color","label":"颜色","default":"#10b981"},
            {"key":"tool.ellipse.width","type":"int","label":"线宽","default":3,"min":1,"max":20}
        ])";
    }

    std::string contributionsJson() const override
    {
        return R"([
            {"type":"tool","data":{"id":"tool.ellipse","label":"椭圆","order":15,"interaction":"drag-rect","shortcut":"e"}}
        ])";
    }

    std::string handle(const std::string& request) override
    {
        try {
            const jmin::Json req = jmin::parse(request);
            const jmin::Json& config = req.get("config");
            const std::string color = config.get("tool.ellipse.color").asString("#10b981");
            const int width = config.get("tool.ellipse.width").asInt(3);

            const jmin::Json& payload = req.get("payload");
            const jmin::Json& rect = payload.get("rect").isNull()
                                         ? payload.get("selection")
                                         : payload.get("rect");

            jmin::Json op = jmin::Json::makeObject();
            op.set("type", jmin::Json::makeString("ellipse"));
            op.set("rect", rect);
            op.set("color", jmin::Json::makeString(color));
            op.set("width", jmin::Json::makeNumber(width));

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
    const std::string m_id = "tool.ellipse";
    const std::string m_name = "椭圆";
};

FRAP_PLUGIN_DEFINE(EllipseTool)
