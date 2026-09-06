/*
 * tool_mosaic —— 马赛克工具（Qt 无关插件示例）
 */
#include "plugin_helper.h"
#include "json_min.h"

class MosaicTool : public sc::PluginBase {
public:
    const std::string& id() const override { return m_id; }
    const std::string& name() const override { return m_name; }
    sc::PluginKind kind() const override { return sc::PluginKind::Tool; }

    std::string configSchemaJson() const override
    {
        return R"([
            {"key":"tool.mosaic.cell","type":"int","label":"粒度","default":12,"min":4,"max":40}
        ])";
    }

    std::string contributionsJson() const override
    {
        return R"([
            {"type":"tool","data":{"id":"tool.mosaic","label":"马赛克","order":40,"interaction":"drag-region","shortcut":"m"}}
        ])";
    }

    std::string handle(const std::string& request) override
    {
        try {
            const jmin::Json req = jmin::parse(request);
            const jmin::Json& config = req.get("config");
            const int cell = config.get("tool.mosaic.cell").asInt(12);

            const jmin::Json& payload = req.get("payload");
            const jmin::Json& rect = payload.get("rect").isNull()
                                         ? payload.get("selection")
                                         : payload.get("rect");

            jmin::Json op = jmin::Json::makeObject();
            op.set("type", jmin::Json::makeString("mosaic"));
            op.set("rect", rect);
            op.set("cell", jmin::Json::makeNumber(cell));

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
    const std::string m_id = "tool.mosaic";
    const std::string m_name = "马赛克";
};

FRAP_PLUGIN_DEFINE(MosaicTool)
