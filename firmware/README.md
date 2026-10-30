# firmware

DeskQuad 的 ESP32-S3 固件与设备端 Web 控制台。

目标板为 ESP32-S3-N16R8（16MB QIO Flash + 8MB OPI PSRAM），依赖 PlatformIO 与 Node.js 18+。

## 前端

```bash
cd web
npm ci
npm run dev      # 本地开发，接口默认代理到 192.168.4.1
npm run build    # 构建并生成 .gz，输出到 web/dist
```

开发时可指定设备地址：

```bash
VITE_DEVICE_URL=http://192.168.4.1 npm run dev
```

## 构建与测试

```bash
pio test -e native
pio run -e esp32-s3
```

## 烧录

```bash
cd web && npm ci && cd ..
pio run -e esp32-s3 -t uploadfs   # 上传前端文件系统
pio run -e esp32-s3 -t upload     # 上传固件
```

`uploadfs` 会检查 `web/dist` 是否落后于前端源码，过期时自动执行 `npm run build`。
设置 `DESKQUAD_SKIP_WEB_BUILD=1` 可跳过自动构建。

WiFi、天气与 AI 凭据保存在 NVS，`uploadfs` 只重写文件系统，不会清空配置；
只有 `pio run -t erase` 擦除整片 Flash 时才会丢失。

## 访问

设备启动后开启热点 `DeskQuad`，密码 `deskquad1234`，浏览器打开 <http://192.168.4.1>
即可进入控制台，完成 WiFi、天气与 AI 设置。
