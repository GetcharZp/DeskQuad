#include <deskquad/web/static_files.h>

#include <LittleFS.h>
#include <deskquad/web/http_util.h>

namespace
{

    // 构建产物位于 LittleFS 根目录，入口和资源都由这里提供
    const char *kIndexPath = "/index.html";
    const char *kNoStore   = "no-store, no-cache, must-revalidate, max-age=0";
    const char *kImmutable = "public, max-age=31536000, immutable";

    bool requestAcceptsGzip(AsyncWebServerRequest *request)
    {
        if (!request->hasHeader("Accept-Encoding"))
            return false;
        return deskquad::AcceptsGzip(request->getHeader("Accept-Encoding")->value().c_str());
    }

    void sendNotFound(AsyncWebServerRequest *request)
    {
        AsyncWebServerResponse *response = request->beginResponse(404, "text/plain; charset=utf-8", "Not Found");
        response->addHeader("Cache-Control", kNoStore, true);
        request->send(response);
    }

    // sendFile 以 path 决定 MIME 与缓存策略，file 可以是它的 .gz 版本
    void sendFile(AsyncWebServerRequest *request, fs::File file, const char *path, const char *cacheControl)
    {
        const String mime                = deskquad::MimeTypeForPath(path);
        AsyncWebServerResponse *response = request->beginResponse(file, path, mime);
        response->addHeader("Cache-Control", cacheControl, true);
        response->addHeader("Vary", "Accept-Encoding", true);
        request->send(response);
    }

    void sendFromLittleFs(AsyncWebServerRequest *request, const char *path, const char *cacheControl)
    {
        if (requestAcceptsGzip(request))
        {
            const String gzipPath = String(path) + ".gz";
            if (LittleFS.exists(gzipPath))
            {
                fs::File file = LittleFS.open(gzipPath, "r");
                if (file)
                {
                    sendFile(request, file, path, cacheControl);
                    return;
                }
            }
        }

        fs::File file = LittleFS.open(path, "r");
        if (!file)
        {
            sendNotFound(request);
            return;
        }
        sendFile(request, file, path, cacheControl);
    }

} // namespace

void RegisterStaticFiles(AsyncWebServer &server)
{
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) { sendFromLittleFs(request, kIndexPath, kNoStore); });

    server.onNotFound([](AsyncWebServerRequest *request) {
        if (request->method() != HTTP_GET)
        {
            sendNotFound(request);
            return;
        }

        const String &url = request->url();
        if (deskquad::IsSafeAssetPath(url.c_str()))
        {
            sendFromLittleFs(request, url.c_str(), kImmutable);
            return;
        }

        sendNotFound(request);
    });
}
