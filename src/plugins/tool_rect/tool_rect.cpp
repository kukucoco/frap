/*
 * tool_rect —— 矩形标注工具（Qt 无关插件示例）
 *
 * 演示：工具类插件 = 声明配置 schema + 工具条贡献 + 输出声明式操作。
 * 渲染由主程序内置渲染器完成（非破坏、可撤销）。
 */
#include "plugin_helper.h"
#include "json_min.h"

class RectTool : public sc::PluginBase {
public:
    const std::string& id() const override { return m_id; }
    const std::string& name() const override { return m_name; }
    sc::PluginKind kind() const override { return sc::PluginKind::Tool; }

    std::string configSchemaJson() const override
    {
        return R"([
            {"key":"tool.rect.color","type":"color","label":"颜色","default":"#e11d48"},
            {"key":"tool.rect.width","type":"int","label":"线宽","default":3,"min":1,"max":20},
            {"key":"tool.rect.fill","type":"bool","label":"填充","default":false}
        ])";
    }

    std::string contributionsJson() const override
    {
        return R"([
            {"type":"tool","data":{"id":"tool.rect","label":"矩形","order":10,"interaction":"drag-rect","shortcut":"r"}}
        ])";
    }

    std::string handle(const std::string& request) override
    {
        try {
            const jmin::Json req = jmin::parse(request);
            const jmin::Json& config = req.get("config");
            const std::string color = config.get("tool.rect.color").asString("#e11d48");
            const int width = config.get("tool.rect.width").asInt(3);
            const bool fill = config.get("tool.rect.fill").asBool(false);

            /* 交互式绘制传入 payload.rect；兼容旧协议回退 payload.selection */
            const jmin::Json& payload = req.get("payload");
            const jmin::Json& rect = payload.get("rect").isNull()
                                         ? payload.get("selection")
                                         : payload.get("rect");

            jmin::Json op = jmin::Json::makeObject();
            op.set("type", jmin::Json::makeString("rect"));
            op.set("rect", rect);
            op.set("color", jmin::Json::makeString(color));
            op.set("width", jmin::Json::makeNumber(width));
            op.set("fill", jmin::Json::makeBool(fill));

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
    const std::string m_id = "tool.rect";
    const std::string m_name = "矩形";
};

FRAP_PLUGIN_DEFINE(RectTool)
