// json_util.h: minimal JSON reader shared by the media/stream clients
// (plex, jellyfin, xcloud, tmdb). Not a real parser. It pulls fields off
// the flat responses those APIs return: find a key, read a string/int/
// float/bool value, split an array into its object strings, decode string
// escapes. One copy so all four clients read JSON the same way.

#pragma once

#include <string>
#include <vector>
#include <cstdlib>
#include <cstdio>

// Offset just past `"key":` (leading whitespace skipped), or npos. `from`
// lets callers scope the search to a sub-object they already located.
inline size_t Json_FindKey(const std::string& s, const char* key, size_t from = 0)
{
    std::string needle = "\"";
    needle += key;
    needle += "\"";
    size_t k = s.find(needle, from);
    if (k == std::string::npos) return std::string::npos;
    size_t c = s.find(':', k + needle.size());
    if (c == std::string::npos) return std::string::npos;
    c++;
    while (c < s.size() && (s[c] == ' ' || s[c] == '\t')) c++;
    return c;
}

// String value at key, escapes decoded (\n \r \t \b \f \uXXXX and literal
// \" \\ \/). \u handles the BMP only, enough for these responses. Empty if
// the key is missing or the value is not a string.
inline std::string Json_GetString(const std::string& s, const char* key, size_t from = 0)
{
    size_t v = Json_FindKey(s, key, from);
    if (v == std::string::npos || v >= s.size() || s[v] != '"') return "";
    v++;
    size_t end = v;
    while (end < s.size()) {
        if (s[end] == '\\' && end + 1 < s.size()) { end += 2; continue; }
        if (s[end] == '"') break;
        end++;
    }
    if (end >= s.size()) return "";
    std::string out;
    for (size_t i = v; i < end; i++) {
        char c = s[i];
        if (c == '\\' && i + 1 < end) {
            char n = s[++i];
            switch (n) {
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'u': {
                    if (i + 4 < end) {
                        char hex[5] = { s[i+1], s[i+2], s[i+3], s[i+4], 0 };
                        unsigned cp = (unsigned)strtoul(hex, nullptr, 16);
                        i += 4;
                        if (cp < 0x80) out.push_back((char)cp);
                        else if (cp < 0x800) {
                            out.push_back((char)(0xC0 | (cp >> 6)));
                            out.push_back((char)(0x80 | (cp & 0x3F)));
                        } else {
                            out.push_back((char)(0xE0 | (cp >> 12)));
                            out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
                            out.push_back((char)(0x80 | (cp & 0x3F)));
                        }
                    }
                    break;
                }
                default: out.push_back(n); break;   // \"  \\  \/  etc
            }
            continue;
        }
        out.push_back(c);
    }
    return out;
}

inline int Json_GetInt(const std::string& s, const char* key, size_t from = 0)
{
    size_t v = Json_FindKey(s, key, from);
    if (v == std::string::npos) return 0;
    return (int)strtol(s.c_str() + v, NULL, 10);
}

inline float Json_GetFloat(const std::string& s, const char* key, size_t from = 0)
{
    size_t v = Json_FindKey(s, key, from);
    if (v == std::string::npos) return 0.0f;
    return (float)strtod(s.c_str() + v, NULL);
}

inline bool Json_GetBool(const std::string& s, const char* key, size_t from = 0)
{
    size_t v = Json_FindKey(s, key, from);
    return v != std::string::npos && s.compare(v, 4, "true") == 0;
}

// True if the value at key is a non-empty JSON array (not []).
inline bool Json_HasNonEmptyArray(const std::string& s, const char* key, size_t from = 0)
{
    size_t v = Json_FindKey(s, key, from);
    if (v == std::string::npos || v >= s.size() || s[v] != '[') return false;
    size_t i = v + 1;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) i++;
    return i < s.size() && s[i] != ']';
}

// Offset of the named array's '[', or npos.
inline size_t Json_FindArray(const std::string& s, const char* key)
{
    size_t v = Json_FindKey(s, key);
    if (v == std::string::npos) return std::string::npos;
    while (v < s.size() && (s[v] == ' ' || s[v] == '\t')) v++;
    if (v >= s.size() || s[v] != '[') return std::string::npos;
    return v;
}

// Split the array at arrStart ('[') into its top-level object strings, each
// running '{' .. matching '}'. Non-object elements are skipped.
inline std::vector<std::string> Json_SplitArray(const std::string& s, size_t arrStart)
{
    std::vector<std::string> out;
    if (arrStart >= s.size() || s[arrStart] != '[') return out;
    size_t i = arrStart + 1;
    while (i < s.size()) {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\t' || s[i] == ',')) i++;
        if (i >= s.size() || s[i] == ']') break;
        if (s[i] != '{') { i++; continue; }
        size_t start = i;
        int depth = 0;
        bool inStr = false;
        for (; i < s.size(); i++) {
            char c = s[i];
            if (inStr) {
                if (c == '\\' && i + 1 < s.size()) { i++; continue; }
                if (c == '"') inStr = false;
            } else {
                if (c == '"') inStr = true;
                else if (c == '{') depth++;
                else if (c == '}') { depth--; if (depth == 0) { i++; break; } }
            }
        }
        out.push_back(s.substr(start, i - start));
    }
    return out;
}

// Split the object at objStart ('{') into its member VALUE object strings.
// For maps keyed by id (e.g. the xCloud catalog's Products), where each
// value is itself an object, this pulls out each value block.
inline std::vector<std::string> Json_SplitObjectValues(const std::string& s, size_t objStart)
{
    std::vector<std::string> out;
    if (objStart >= s.size() || s[objStart] != '{') return out;
    size_t i = objStart + 1;
    bool inStr = false;
    while (i < s.size()) {
        char c = s[i];
        if (inStr) {
            if (c == '\\' && i + 1 < s.size()) { i += 2; continue; }
            if (c == '"') inStr = false;
            i++; continue;
        }
        if (c == '"') { inStr = true; i++; continue; }
        if (c == '}') break;
        if (c == '{') {
            size_t start = i;
            int depth = 0;
            for (; i < s.size(); i++) {
                char cc = s[i];
                if (inStr) {
                    if (cc == '\\' && i + 1 < s.size()) { i++; continue; }
                    if (cc == '"') inStr = false;
                } else {
                    if (cc == '"') inStr = true;
                    else if (cc == '{') depth++;
                    else if (cc == '}') { depth--; if (depth == 0) { i++; break; } }
                }
            }
            out.push_back(s.substr(start, i - start));
            continue;
        }
        i++;
    }
    return out;
}

// Encode a string as a JSON string literal, quotes included. SDP blobs carry
// newlines, so they have to be escaped before going into a request body.
inline std::string JsonQuote(const std::string& s)
{
    std::string o = "\"";
    for (char c : s) {
        switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n";  break;
            case '\r': o += "\\r";  break;
            case '\t': o += "\\t";  break;
            default:
                if ((unsigned char)c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
                else o += c;
        }
    }
    o += "\"";
    return o;
}
