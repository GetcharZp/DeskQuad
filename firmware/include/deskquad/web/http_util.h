// http_util.h 提供静态资源服务所需的纯函数

#pragma once

namespace deskquad
{

    // IsSafeAssetPath 判断 URL 是否是可以公开提供的静态资源
    //
    // 只允许 /assets/ 前缀，拒绝路径穿越、反斜杠、百分号、控制字符和过长的
    // 路径，避免把 /assets/ 之外的文件暴露出去。
    //
    // # Params:
    //
    //	path: 已经过 URL 解码的请求路径
    bool IsSafeAssetPath(const char *path);

    // AcceptsGzip 判断 Accept-Encoding 是否接受 gzip
    //
    // 支持逗号分隔的编码列表、q 权重和 * 通配符。
    bool AcceptsGzip(const char *acceptEncoding);

    // MimeTypeForPath 根据文件扩展名返回 Content-Type
    const char *MimeTypeForPath(const char *path);

} // namespace deskquad
