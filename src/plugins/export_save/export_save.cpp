/*
 * export_save —— 保存到目录的导出插件（Qt 无关插件示例）
 *
 * 演示：导出类插件 = 在宿主进程中执行副作用（文件 IO），
 *       并可选通过 actions 请求主程序完成桌面集成（打开文件等）。
 */
#include "plugin_helper.h"
#include "json_min.h"

#include <cstdio>
#include <ctime>
#include <fstream>
#include <filesystem>
#include <string>

namespace {

std::string homeDir()
{
#ifdef _WIN32
    if (const char* p = getenv("USERPROFILE"))
        return p;
    return ".";
#else
    if (const char* p = getenv("HOME"))
        return p;
    return ".";
#endif
}

bool copyFile(const std::string& from, const std::string& to)
{
    std::ifstream in(from, std::ios::binary);
    std::ofstream out(to, std::ios::binary);
    if (!in || !out)
        return false;
    out << in.rdbuf();
    return out.good();
}

} // namespace

class ExportSave : public sc::PluginBase {
public:
    const std::string& id() const override { return m_id; }
    const std::string& name() const override { return m_name; }
    sc::PluginKind kind() const override { return sc::PluginKind::Export; }

    std::string configSchemaJson() const override
    {
        return R"([
            {"key":"export.save.directory","type":"string","label":"保存目录","default":"","description":"留空使用图片目录"}
        ])";
    }

    std::string contributionsJson() const override
    {
        return R"([
            {"type":"export","data":{"id":"export.save","label":"保存","order":100}}
        ])";
    }

    std::string handle(const std::string& request) override
    {
        try {
            const jmin::Json req = jmin::parse(request);
            const jmin::Json& config = req.get("config");
            const std::string imageRef = req.get("payload").get("imageRef").asString();
            std::string dir = config.get("export.save.directory").asString();
            if (dir.empty())
                dir = homeDir() + "/Pictures";

            try {
                std::filesystem::create_directories(dir);
            } catch (...) {}

            char stamp[32];
            const std::time_t now = std::time(nullptr);
            std::strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", std::localtime(&now));

            const std::string target = dir + "/frap_" + stamp + ".png";
            if (!copyFile(imageRef, target))
                throw std::runtime_error("文件复制失败");

            jmin::Json result = jmin::Json::makeObject();
            result.set("status", jmin::Json::makeString("ok"));
            result.set("message", jmin::Json::makeString("已保存到 " + target));
            return result.toString();
        } catch (const std::exception& e) {
            return jmin::Json::objectOf("message", jmin::Json::makeString(e.what())).toString();
        }
    }

private:
    const std::string m_id = "export.save";
    const std::string m_name = "保存到目录";
};

FRAP_PLUGIN_DEFINE(ExportSave)
