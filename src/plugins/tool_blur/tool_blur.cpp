/*
 * tool_blur —— 模糊工具（Qt 无关插件示例）
 */
#include "plugin_helper.h"
#include "json_min.h"

class BlurTool : public sc::PluginBase {
public:
    const std::string& id() const override { return m_id; }
    const std::string& name() const override { return m_name; }
    sc::PluginKind kind() const override { return sc::PluginKind::Tool; }

    std::string configSchemaJson() const override
    {
        return R"([
            {"key":"tool.blur.radius","type":"int","label":"模糊度","default":6,"min":1,"max":20}
        ])";
    }

    std::string contributionsJson() const override
    {
        return R"([
            {"type":"tool","data":{"id":"tool.blur","label":"模糊","order":47,"interaction":"drag-region","shortcut":"b"}}
        ])";
    }

    std::string handle(const std::string& request) override
    {
        try {
            const jmin::Json req = jmin::parse(request);
            const jmin::Json& config = req.get("config");
            const int radius = config.get("tool.blur.radius").asInt(6);

            const jmin::Json& payload = req.get("payload");
            const jmin::Json& rect = payload.get("rect").isNull()
                                         ? payload.get("selection")
                                         : payload.get("rect");

            jmin::Json op = jmin::Json::makeObject();
            op.set("type", jmin::Json::makeString("blur"));
            op.set("rect", rect);
            op.set("radius", jmin::Json::makeNumber(radius));

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
    const std::string m_id = "tool.blur";
    const std::string m_name = "模糊";
};

FRAP_PLUGIN_DEFINE(BlurTool)
