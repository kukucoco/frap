/*
 * tool_number —— 编号圆点工具（Qt 无关插件示例）
 *
 * 点击即自动编号（1、2、3...）。编号由主程序根据已有点数传入
 * params.number，插件无需维护状态（保持无状态、可重放）。
 */
#include "plugin_helper.h"
#include "json_min.h"

class NumberTool : public sc::PluginBase {
public:
    const std::string& id() const override { return m_id; }
    const std::string& name() const override { return m_name; }
    sc::PluginKind kind() const override { return sc::PluginKind::Tool; }

    std::string configSchemaJson() const override
    {
        return R"([
            {"key":"tool.number.color","type":"color","label":"颜色","default":"#e11d48"},
            {"key":"tool.number.size","type":"int","label":"字号","default":24,"min":12,"max":64}
        ])";
    }

    std::string contributionsJson() const override
    {
        return R"([
            {"type":"tool","data":{"id":"tool.number","label":"编号","order":35,"interaction":"click-point","shortcut":"n","auto":"number"}}
        ])";
    }

    std::string handle(const std::string& request) override
    {
        try {
            const jmin::Json req = jmin::parse(request);
            const jmin::Json& config = req.get("config");
            const std::string color = config.get("tool.number.color").asString("#e11d48");
            const int size = config.get("tool.number.size").asInt(24);
            const int number = req.get("payload").get("params").get("number").asInt(1);

            const jmin::Json& payload = req.get("payload");
            jmin::Json point = payload.get("point");
            if (point.isNull()) {
                point = jmin::Json::makeObject();
                point.set("x", jmin::Json::makeNumber(0));
                point.set("y", jmin::Json::makeNumber(0));
            }

            jmin::Json op = jmin::Json::makeObject();
            op.set("type", jmin::Json::makeString("number"));
            op.set("point", point);
            op.set("text", jmin::Json::makeString(std::to_string(number)));
            op.set("color", jmin::Json::makeString(color));
            op.set("size", jmin::Json::makeNumber(size));

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
    const std::string m_id = "tool.number";
    const std::string m_name = "编号";
};

FRAP_PLUGIN_DEFINE(NumberTool)
