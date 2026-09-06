/*
 * json_min.h —— 零依赖的极小 JSON 解析/序列化（供 Qt 无关插件使用）
 *
 * 完整、健壮的 JSON 库建议插件作者用 nlohmann/json 等；这里提供的
 * 迷你实现覆盖插件消息的常见子集（对象/数组/字符串/数字/布尔/null），
 * 足以读写协议消息，且不依赖任何第三方库。
 */
#ifndef FRAP_JSON_MIN_H
#define FRAP_JSON_MIN_H

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <stdexcept>

namespace jmin {

class Json {
public:
    enum Type { Null, Bool, Number, String, Array, Object };

    Json() : m_type(Null) {}
    static Json makeBool(bool v) { Json j; j.m_type = Bool; j.m_bool = v; return j; }
    static Json makeNumber(double v) { Json j; j.m_type = Number; j.m_number = v; return j; }
    static Json makeString(std::string v) { Json j; j.m_type = String; j.m_string = std::move(v); return j; }
    static Json makeArray() { Json j; j.m_type = Array; return j; }
    static Json makeObject() { Json j; j.m_type = Object; return j; }

    Type type() const { return m_type; }
    bool isObject() const { return m_type == Object; }
    bool isArray() const { return m_type == Array; }
    bool isString() const { return m_type == String; }
    bool isNumber() const { return m_type == Number; }
    bool isBool() const { return m_type == Bool; }
    bool isNull() const { return m_type == Null; }

    bool asBool(bool def = false) const { return m_type == Bool ? m_bool : def; }
    double asNumber(double def = 0.0) const { return m_type == Number ? m_number : def; }
    int asInt(int def = 0) const { return m_type == Number ? static_cast<int>(m_number) : def; }
    const std::string& asString(const std::string& def = {}) const
    {
        return m_type == String ? m_string : m_defString;
    }

    bool has(const std::string& key) const
    {
        return m_type == Object && m_object.find(key) != m_object.end();
    }
    const Json& get(const std::string& key) const
    {
        static const Json kNull;
        if (m_type != Object) return kNull;
        auto it = m_object.find(key);
        return it == m_object.end() ? kNull : it->second;
    }
    Json& operator[](const std::string& key)
    {
        if (m_type != Object) {
            m_type = Object;
            m_object.clear();
        }
        return m_object[key];
    }
    void set(const std::string& key, const Json& v) { (*this)[key] = v; }

    void push(const Json& v)
    {
        if (m_type != Array) {
            m_type = Array;
            m_array.clear();
        }
        m_array.push_back(v);
    }
    const Json& at(size_t i) const { return m_array.at(i); }
    size_t size() const
    {
        return m_type == Array ? m_array.size() : (m_type == Object ? m_object.size() : 0);
    }
    const std::vector<Json>& array() const { return m_array; }

    /* 便捷构造：{ "k": v } */
    static Json objectOf(const std::string& k, const Json& v)
    {
        Json j = makeObject();
        j.set(k, v);
        return j;
    }

    std::string toString(bool pretty = false) const
    {
        std::string out;
        int indent = 0;
        serialize(out, indent, pretty);
        return out;
    }

private:
    void serialize(std::string& out, int indent, bool pretty) const
    {
        auto newline = [&](int extra) {
            if (!pretty) return;
            out += '\n';
            out += std::string((size_t)(indent + extra) * 2, ' ');
        };
        switch (m_type) {
        case Null: out += "null"; break;
        case Bool: out += m_bool ? "true" : "false"; break;
        case Number: {
            char buf[32];
            if (m_number == static_cast<double>(static_cast<int64_t>(m_number)))
                snprintf(buf, sizeof(buf), "%lld", (long long)m_number);
            else
                snprintf(buf, sizeof(buf), "%.6f", m_number);
            out += buf;
            break;
        }
        case String: serializeString(out, m_string); break;
        case Array:
            if (m_array.empty()) { out += "[]"; break; }
            out += '[';
            indent++;
            for (size_t i = 0; i < m_array.size(); ++i) {
                newline(0);
                m_array[i].serialize(out, indent, pretty);
                if (i + 1 < m_array.size()) out += ',';
            }
            indent--;
            newline(-1);
            out += ']';
            break;
        case Object:
            if (m_object.empty()) { out += "{}"; break; }
            out += '{';
            indent++;
            {
                size_t i = 0;
                for (const auto& kv : m_object) {
                    newline(0);
                    serializeString(out, kv.first);
                    out += pretty ? ": " : ":";
                    kv.second.serialize(out, indent, pretty);
                    if (i + 1 < m_object.size()) out += ',';
                    ++i;
                }
            }
            indent--;
            newline(-1);
            out += '}';
            break;
        }
    }

    static void serializeString(std::string& out, const std::string& s)
    {
        out += '"';
        for (unsigned char c : s) {
            switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += (char)c;
                }
            }
        }
        out += '"';
    }

    Type m_type = Null;
    bool m_bool = false;
    double m_number = 0.0;
    std::string m_string;
    std::string m_defString;
    std::vector<Json> m_array;
    std::map<std::string, Json> m_object;

    friend Json parse(const std::string& text);
};

/* ---- 解析器 ---- */

namespace detail {
struct Parser {
    const char* p;
    const char* end;
    explicit Parser(const std::string& s) : p(s.data()), end(s.data() + s.size()) {}

    void skipWs()
    {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
            ++p;
    }

    bool consume(char c)
    {
        skipWs();
        if (p < end && *p == c) { ++p; return true; }
        return false;
    }

    std::string parseString()
    {
        if (!consume('"')) throw std::runtime_error("json: 期望字符串");
        std::string out;
        while (p < end) {
            const char c = *p++;
            if (c == '"') return out;
            if (c == '\\') {
                if (p >= end) break;
                const char e = *p++;
                switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    /* 仅支持 BMP 内的 \uXXXX（协议消息以 ASCII/UTF-8 为主） */
                    if (end - p < 4) break;
                    unsigned int code = 0;
                    for (int i = 0; i < 4; ++i) {
                        const char h = p[i];
                        code <<= 4;
                        if (h >= '0' && h <= '9') code |= h - '0';
                        else if (h >= 'a' && h <= 'f') code |= h - 'a' + 10;
                        else if (h >= 'A' && h <= 'F') code |= h - 'A' + 10;
                    }
                    p += 4;
                    /* 转成 UTF-8 */
                    if (code < 0x80) out += (char)code;
                    else if (code < 0x800) {
                        out += (char)(0xC0 | (code >> 6));
                        out += (char)(0x80 | (code & 0x3F));
                    } else {
                        out += (char)(0xE0 | (code >> 12));
                        out += (char)(0x80 | ((code >> 6) & 0x3F));
                        out += (char)(0x80 | (code & 0x3F));
                    }
                    break;
                }
                default: out += e;
                }
            } else {
                out += c;
            }
        }
        throw std::runtime_error("json: 字符串未闭合");
    }

    Json parseNumber()
    {
        const char* start = p;
        bool isDouble = false;
        while (p < end) {
            const char c = *p;
            if (c == '-' || (c >= '0' && c <= '9')) { ++p; }
            else if (c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') {
                isDouble = true; ++p;
            } else break;
        }
        const std::string tok(start, p);
        if (tok.empty()) throw std::runtime_error("json: 数字无效");
        return Json::makeNumber(std::stod(tok));
    }

    Json parseValue()
    {
        skipWs();
        if (p >= end) throw std::runtime_error("json: 意外结束");
        switch (*p) {
        case '{': return parseObject();
        case '[': return parseArray();
        case '"': return Json::makeString(parseString());
        case 't':
            if (end - p >= 4 && std::string(p, p + 4) == "true") { p += 4; return Json::makeBool(true); }
            break;
        case 'f':
            if (end - p >= 5 && std::string(p, p + 5) == "false") { p += 5; return Json::makeBool(false); }
            break;
        case 'n':
            if (end - p >= 4 && std::string(p, p + 4) == "null") { p += 4; return Json(); }
            break;
        default:
            if (*p == '-' || (*p >= '0' && *p <= '9'))
                return parseNumber();
        }
        throw std::runtime_error("json: 无法解析的值");
    }

    Json parseObject()
    {
        consume('{');
        Json obj = Json::makeObject();
        skipWs();
        if (consume('}')) return obj;
        while (true) {
            skipWs();
            const std::string key = parseString();
            if (!consume(':')) throw std::runtime_error("json: 缺少冒号");
            obj.set(key, parseValue());
            skipWs();
            if (consume('}')) break;
            if (!consume(',')) throw std::runtime_error("json: 缺少逗号");
        }
        return obj;
    }

    Json parseArray()
    {
        consume('[');
        Json arr = Json::makeArray();
        skipWs();
        if (consume(']')) return arr;
        while (true) {
            arr.push(parseValue());
            skipWs();
            if (consume(']')) break;
            if (!consume(',')) throw std::runtime_error("json: 缺少逗号");
        }
        return arr;
    }
};
} // namespace detail

inline Json parse(const std::string& text)
{
    detail::Parser parser(text);
    Json value = parser.parseValue();
    parser.skipWs();
    if (parser.p != parser.end) throw std::runtime_error("json: 尾部多余内容");
    return value;
}

} // namespace jmin

#endif /* FRAP_JSON_MIN_H */
