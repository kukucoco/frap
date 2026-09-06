/*
 * tool_text —— 文字标注工具（Qt 无关插件示例）
 *
 * 演示：文本类工具的"瞬态参数"（params.text）与持久设置（config）的区分。
 * 文字内容通过 params 传入（不落盘），颜色/字号通过 config 持久化。
 */
#include "plugin_helper.h"
#include "json_min.h"

class TextTool : public sc::PluginBase {
public:
    const std::string& id() const override { return m_id; }
    const std::string& name() const override { return m_name; }
    sc::PluginKind kind() const override { return sc::PluginKind::Tool; }

    std::string configSchemaJson() const override
    {
        return R"([
            {"key":"tool.text.color","type":"color","label":"颜色","default":"#ffffff"},
            {"key":"tool.text.size","type":"int","label":"字号","default":28,"min":12,"max":72},
            {"key":"tool.text.text","type":"string","label":"文字","default":"","hidden":true}
        ])";
    }

    std::string contributionsJson() const override
    {
        return R"([
            {"type":"tool","data":{"id":"tool.text","label":"文字","order":30,"interaction":"click-point","shortcut":"t"}}
        ])";
    }

    std::string handle(const std::string& request) override
    {
        try {
            const jmin::Json req = jmin::parse(request);
            const jmin::Json& config = req.get("config");
            const std::string color = config.get("tool.text.color").asString("#ffffff");
            const int size = config.get("tool.text.size").asInt(28);
            const std::string text = req.get("payload").get("params").get("text").asString();
            if (text.empty())
                return jmin::Json::objectOf("message",
                                            jmin::Json::makeString("文字为空"))
                           .toString();

            /* 交互式绘制传入 payload.point；兼容旧协议按选区左上角 */
            const jmin::Json& payload = req.get("payload");
            jmin::Json point = payload.get("point");
            if (point.isNull()) {
                const jmin::Json& sel = payload.get("selection");
                point = jmin::Json::makeObject();
                point.set("x", jmin::Json::makeNumber(sel.get("x").asInt(0) + 10));
                point.set("y", jmin::Json::makeNumber(sel.get("y").asInt(0) + size));
            }

            jmin::Json op = jmin::Json::makeObject();
            op.set("type", jmin::Json::makeString("text"));
            op.set("point", point);
            op.set("text", jmin::Json::makeString(text));
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
    const std::string m_id = "tool.text";
    const std::string m_name = "文字";
};

FRAP_PLUGIN_DEFINE(TextTool)
