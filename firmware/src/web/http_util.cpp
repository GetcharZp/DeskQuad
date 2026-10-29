#include <deskquad/web/http_util.h>

#include <stdlib.h>
#include <string.h>

namespace
{

    // 与前端构建脚本保持一致的保守路径上限
    constexpr size_t kMaxPathLength     = 31;
    constexpr const char *kAssetPrefix  = "/assets/";
    constexpr size_t kAssetPrefixLength = 8;

    bool equalsIgnoreCase(const char *text, size_t length, const char *token)
    {
        size_t index = 0;
        for (; index < length; ++index)
        {
            if (token[index] == '\0')
                return false;
            char left  = text[index];
            char right = token[index];
            if (left >= 'A' && left <= 'Z')
                left = static_cast<char>(left - 'A' + 'a');
            if (right >= 'A' && right <= 'Z')
                right = static_cast<char>(right - 'A' + 'a');
            if (left != right)
                return false;
        }
        return token[index] == '\0';
    }

    bool tokenMatches(const char *text, size_t length, const char *token)
    {
        if (length == 1 && text[0] == '*')
            return true;
        return equalsIgnoreCase(text, length, token);
    }

    // qualityFor 读取分号参数里的 q= 权重，缺省为 1.0
    double qualityFor(const char *start, const char *end)
    {
        const char *cursor = start;
        while (cursor < end && *cursor != ';')
            ++cursor;
        while (cursor < end)
        {
            if ((*cursor == 'q' || *cursor == 'Q') && cursor + 1 < end && cursor[1] == '=')
            {
                return strtod(cursor + 2, nullptr);
            }
            ++cursor;
        }
        return 1.0;
    }

} // namespace

namespace deskquad
{

    bool IsSafeAssetPath(const char *path)
    {
        if (path == nullptr)
            return false;

        const size_t length = strlen(path);
        if (length <= kAssetPrefixLength || length > kMaxPathLength)
            return false;
        if (strncmp(path, kAssetPrefix, kAssetPrefixLength) != 0)
            return false;
        if (path[length - 1] == '/')
            return false;

        for (size_t index = 1; index < length; ++index)
        {
            const char value = path[index];

            if (value == '.')
            {
                if (index + 1 < length && path[index + 1] == '.')
                    return false;
                continue;
            }

            const bool alpha = (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
            const bool digit = value >= '0' && value <= '9';
            if (!alpha && !digit && value != '/' && value != '-' && value != '_')
            {
                return false;
            }
        }

        return true;
    }

    bool AcceptsGzip(const char *acceptEncoding)
    {
        if (acceptEncoding == nullptr)
            return false;

        const char *cursor = acceptEncoding;
        while (*cursor != '\0')
        {
            while (*cursor == ' ' || *cursor == '\t' || *cursor == ',')
                ++cursor;
            if (*cursor == '\0')
                break;

            const char *start = cursor;
            while (*cursor != '\0' && *cursor != ',')
                ++cursor;
            const char *end = cursor;

            while (end > start && (end[-1] == ' ' || end[-1] == '\t'))
                --end;

            const char *tokenEnd = start;
            while (tokenEnd < end && *tokenEnd != ';')
                ++tokenEnd;
            while (tokenEnd > start && (tokenEnd[-1] == ' ' || tokenEnd[-1] == '\t'))
                --tokenEnd;

            const size_t tokenLength = static_cast<size_t>(tokenEnd - start);
            if (tokenMatches(start, tokenLength, "gzip") && qualityFor(start, end) > 0.0)
            {
                return true;
            }

            if (*cursor == ',')
                ++cursor;
        }

        return false;
    }

    const char *MimeTypeForPath(const char *path)
    {
        if (path == nullptr)
            return "application/octet-stream";

        const char *dot = strrchr(path, '.');
        if (dot == nullptr)
            return "application/octet-stream";

        if (strcmp(dot, ".html") == 0)
            return "text/html; charset=utf-8";
        if (strcmp(dot, ".js") == 0)
            return "text/javascript; charset=utf-8";
        if (strcmp(dot, ".css") == 0)
            return "text/css; charset=utf-8";
        if (strcmp(dot, ".json") == 0)
            return "application/json; charset=utf-8";
        if (strcmp(dot, ".svg") == 0)
            return "image/svg+xml";
        if (strcmp(dot, ".png") == 0)
            return "image/png";
        if (strcmp(dot, ".ico") == 0)
            return "image/x-icon";
        if (strcmp(dot, ".woff2") == 0)
            return "font/woff2";
        if (strcmp(dot, ".txt") == 0)
            return "text/plain; charset=utf-8";
        return "application/octet-stream";
    }

} // namespace deskquad
