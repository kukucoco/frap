/*
 * tool_arrow —— 箭头标注工具（Qt 无关插件示例）
 */
#include "plugin_helper.h"
#include "json_min.h"

class ArrowTool : public sc::PluginBase {
public:
    const std::string& id() const override { return m_id; }
    const std::string& name() const override { return m_name; }
    sc::PluginKind kind() const override { return sc::PluginKind::Tool; }

    std::string configSchemaJson() const override
    {
        return R"([
            {"key":"tool.arrow.color","type":"color","label":"颜色","default":"#f59e0b"},
            {"key":"tool.arrow.width","type":"int","label":"线宽","default":4,"min":1,"max":20}
        ])";
    }

    std::string contributionsJson() const override
    {
        return R"([
            {"type":"tool","data":{"id":"tool.arrow","label":"箭头","order":20,"interaction":"drag-line","shortcut":"a"}}
        ])";
    }

    std::string handle(const std::string& request) override
    {
        try {
            const jmin::Json req = jmin::parse(request);
            const jmin::Json& config = req.get("config");
            const std::string color = config.get("tool.arrow.color").asString("#f59e0b");
            const int width = config.get("tool.arrow.width").asInt(4);

            const jmin::Json& payload = req.get("payload");
            /* 交互式绘制传入 payload.start/end；兼容旧协议按选区默认生成 */
            const jmin::Json& sel = payload.get("selection");
            auto point = [](int px, int py) {
                jmin::Json p = jmin::Json::makeObject();
                p.set("x", jmin::Json::makeNumber(px));
                p.set("y", jmin::Json::makeNumber(py));
                return p;
            };
            jmin::Json start = payload.get("start");
            jmin::Json end = payload.get("end");
            if (start.isNull() && end.isNull()) {
                const int x = sel.get("x").asInt(0);
                const int y = sel.get("y").asInt(0);
                const int w = sel.get("w").asInt(0);
                const int h = sel.get("h").asInt(0);
                const int cx = x + w / 2;
                start = point(cx, y + h * 3 / 4);
                end = point(cx, y + h / 4);
            }

            jmin::Json op = jmin::Json::makeObject();
            op.set("type", jmin::Json::makeString("arrow"));
            op.set("start", start);
            op.set("end", end);
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
    const std::string m_id = "tool.arrow";
    const std::string m_name = "箭头";
};

FRAP_PLUGIN_DEFINE(ArrowTool)
