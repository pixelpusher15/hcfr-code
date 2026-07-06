// MiniJson.h : a small, dependency-free JSON parser.
//
// Just enough JSON to read the GitHub Releases API response: objects, arrays,
// strings (with \uXXXX -> UTF-8), numbers, booleans and null. Not a general
// purpose serializer - it only parses. All string values are returned as UTF-8
// encoded std::string (callers convert to the app's ANSI code page as needed).
//
#ifndef _MINIJSON_H
#define _MINIJSON_H

#include <string>
#include <vector>
#include <map>
#include <stdlib.h>   // atof

namespace minijson
{
    class Value
    {
    public:
        enum Type { T_NULL, T_BOOL, T_NUMBER, T_STRING, T_ARRAY, T_OBJECT };

        Value() : m_type(T_NULL), m_bool(false), m_number(0.0) {}

        Type type() const { return m_type; }
        bool isNull()   const { return m_type == T_NULL; }
        bool isBool()   const { return m_type == T_BOOL; }
        bool isNumber() const { return m_type == T_NUMBER; }
        bool isString() const { return m_type == T_STRING; }
        bool isArray()  const { return m_type == T_ARRAY; }
        bool isObject() const { return m_type == T_OBJECT; }

        bool               asBool()   const { return m_bool; }
        double             asNumber() const { return m_number; }
        const std::string& asString() const { return m_string; }

        // Array access
        size_t       size() const { return m_array.size(); }
        const Value& at(size_t i) const { return m_array[i]; }

        // Object access. Returns a static null Value when the key is absent, so
        // chained lookups on a missing key are safe (they just yield null).
        bool has(const std::string& key) const { return m_object.find(key) != m_object.end(); }
        const Value& get(const std::string& key) const
        {
            std::map<std::string, Value>::const_iterator it = m_object.find(key);
            return it == m_object.end() ? nullValue() : it->second;
        }

        // Convenience typed getters with defaults.
        std::string getString(const std::string& key, const char* def = "") const
        {
            const Value& v = get(key);
            return v.isString() ? v.asString() : std::string(def);
        }
        bool getBool(const std::string& key, bool def = false) const
        {
            const Value& v = get(key);
            return v.isBool() ? v.asBool() : def;
        }

    private:
        static const Value& nullValue() { static const Value s; return s; }

        Type                          m_type;
        bool                          m_bool;
        double                        m_number;
        std::string                   m_string;
        std::vector<Value>            m_array;
        std::map<std::string, Value>  m_object;

        friend class Parser;
    };

    class Parser
    {
    public:
        // Returns true on success. On failure returns false and sets err.
        static bool parse(const std::string& text, Value& out, std::string& err)
        {
            Parser p(text);
            p.skipWs();
            bool ok = p.parseValue(out);
            if (ok)
            {
                p.skipWs();
                if (p.m_pos != p.m_text.size())
                {
                    err = "trailing characters after JSON value";
                    ok = false;
                }
            }
            if (!ok && err.empty())
                err = p.m_err.empty() ? "parse error" : p.m_err;
            return ok;
        }

    private:
        explicit Parser(const std::string& text) : m_text(text), m_pos(0), m_depth(0) {}

        // Cap nesting depth so adversarial deeply-nested input (the JSON comes
        // off the network) can't overflow the stack via mutual recursion.
        static const size_t kMaxDepth = 200;

        const std::string& m_text;
        size_t             m_pos;
        size_t             m_depth;
        std::string        m_err;

        bool eof() const { return m_pos >= m_text.size(); }
        char cur() const { return m_text[m_pos]; }

        void skipWs()
        {
            while (!eof())
            {
                char c = cur();
                if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
                    ++m_pos;
                else
                    break;
            }
        }

        bool fail(const char* msg) { if (m_err.empty()) m_err = msg; return false; }

        bool parseValue(Value& out)
        {
            skipWs();
            if (eof()) return fail("unexpected end of input");
            if (m_depth >= kMaxDepth) return fail("JSON nesting too deep");
            ++m_depth;
            bool ok = parseValueDispatch(out);
            --m_depth;
            return ok;
        }

        bool parseValueDispatch(Value& out)
        {
            char c = cur();
            switch (c)
            {
            case '{': return parseObject(out);
            case '[': return parseArray(out);
            case '"': return parseStringValue(out);
            case 't': case 'f': return parseBool(out);
            case 'n': return parseNull(out);
            default:
                if (c == '-' || (c >= '0' && c <= '9'))
                    return parseNumber(out);
                return fail("unexpected character");
            }
        }

        bool parseObject(Value& out)
        {
            out.m_type = Value::T_OBJECT;
            ++m_pos; // consume '{'
            skipWs();
            if (!eof() && cur() == '}') { ++m_pos; return true; }
            for (;;)
            {
                skipWs();
                if (eof() || cur() != '"') return fail("expected string key");
                std::string key;
                if (!parseRawString(key)) return false;
                skipWs();
                if (eof() || cur() != ':') return fail("expected ':'");
                ++m_pos;
                Value v;
                if (!parseValue(v)) return false;
                out.m_object[key] = v;
                skipWs();
                if (eof()) return fail("unterminated object");
                if (cur() == ',') { ++m_pos; continue; }
                if (cur() == '}') { ++m_pos; return true; }
                return fail("expected ',' or '}'");
            }
        }

        bool parseArray(Value& out)
        {
            out.m_type = Value::T_ARRAY;
            ++m_pos; // consume '['
            skipWs();
            if (!eof() && cur() == ']') { ++m_pos; return true; }
            for (;;)
            {
                Value v;
                if (!parseValue(v)) return false;
                out.m_array.push_back(v);
                skipWs();
                if (eof()) return fail("unterminated array");
                if (cur() == ',') { ++m_pos; continue; }
                if (cur() == ']') { ++m_pos; return true; }
                return fail("expected ',' or ']'");
            }
        }

        bool parseStringValue(Value& out)
        {
            std::string s;
            if (!parseRawString(s)) return false;
            out.m_type = Value::T_STRING;
            out.m_string = s;
            return true;
        }

        bool parseRawString(std::string& out)
        {
            ++m_pos; // consume opening quote
            out.clear();
            while (!eof())
            {
                char c = m_text[m_pos++];
                if (c == '"')
                    return true;
                if (c == '\\')
                {
                    if (eof()) return fail("bad escape");
                    char e = m_text[m_pos++];
                    switch (e)
                    {
                    case '"':  out += '"';  break;
                    case '\\': out += '\\'; break;
                    case '/':  out += '/';  break;
                    case 'b':  out += '\b'; break;
                    case 'f':  out += '\f'; break;
                    case 'n':  out += '\n'; break;
                    case 'r':  out += '\r'; break;
                    case 't':  out += '\t'; break;
                    case 'u':
                    {
                        unsigned int cp;
                        if (!parseHex4(cp)) return false;
                        // Handle UTF-16 surrogate pairs. Any unpaired surrogate
                        // (lone high, high not followed by a valid low, or a
                        // lone low) becomes U+FFFD rather than being emitted as
                        // raw surrogate bytes, which would be invalid UTF-8.
                        if (cp >= 0xD800 && cp <= 0xDBFF)
                        {
                            unsigned int cp2 = 0xFFFD;
                            if (m_pos + 1 < m_text.size() &&
                                m_text[m_pos] == '\\' && m_text[m_pos + 1] == 'u')
                            {
                                m_pos += 2;
                                unsigned int lo;
                                if (!parseHex4(lo)) return false;
                                if (lo >= 0xDC00 && lo <= 0xDFFF)
                                    cp2 = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                            }
                            cp = cp2;
                        }
                        else if (cp >= 0xDC00 && cp <= 0xDFFF)
                        {
                            cp = 0xFFFD;   // lone low surrogate
                        }
                        appendUtf8(out, cp);
                        break;
                    }
                    default:
                        return fail("unknown escape");
                    }
                }
                else
                {
                    out += c;
                }
            }
            return fail("unterminated string");
        }

        bool parseHex4(unsigned int& out)
        {
            if (m_pos + 4 > m_text.size()) return fail("bad \\u escape");
            out = 0;
            for (int i = 0; i < 4; ++i)
            {
                char c = m_text[m_pos++];
                out <<= 4;
                if (c >= '0' && c <= '9') out |= (c - '0');
                else if (c >= 'a' && c <= 'f') out |= (c - 'a' + 10);
                else if (c >= 'A' && c <= 'F') out |= (c - 'A' + 10);
                else return fail("bad hex digit");
            }
            return true;
        }

        static void appendUtf8(std::string& out, unsigned int cp)
        {
            if (cp < 0x80)
                out += (char)cp;
            else if (cp < 0x800)
            {
                out += (char)(0xC0 | (cp >> 6));
                out += (char)(0x80 | (cp & 0x3F));
            }
            else if (cp < 0x10000)
            {
                out += (char)(0xE0 | (cp >> 12));
                out += (char)(0x80 | ((cp >> 6) & 0x3F));
                out += (char)(0x80 | (cp & 0x3F));
            }
            else
            {
                out += (char)(0xF0 | (cp >> 18));
                out += (char)(0x80 | ((cp >> 12) & 0x3F));
                out += (char)(0x80 | ((cp >> 6) & 0x3F));
                out += (char)(0x80 | (cp & 0x3F));
            }
        }

        bool parseNumber(Value& out)
        {
            size_t start = m_pos;
            if (!eof() && cur() == '-') ++m_pos;
            while (!eof())
            {
                char c = cur();
                if ((c >= '0' && c <= '9') || c == '.' || c == 'e' ||
                    c == 'E' || c == '+' || c == '-')
                    ++m_pos;
                else
                    break;
            }
            std::string num = m_text.substr(start, m_pos - start);
            out.m_type = Value::T_NUMBER;
            out.m_number = atof(num.c_str());
            return true;
        }

        bool parseBool(Value& out)
        {
            if (m_text.compare(m_pos, 4, "true") == 0)
            {
                out.m_type = Value::T_BOOL; out.m_bool = true; m_pos += 4; return true;
            }
            if (m_text.compare(m_pos, 5, "false") == 0)
            {
                out.m_type = Value::T_BOOL; out.m_bool = false; m_pos += 5; return true;
            }
            return fail("invalid literal");
        }

        bool parseNull(Value& out)
        {
            if (m_text.compare(m_pos, 4, "null") == 0)
            {
                out.m_type = Value::T_NULL; m_pos += 4; return true;
            }
            return fail("invalid literal");
        }
    };

} // namespace minijson

#endif // _MINIJSON_H
