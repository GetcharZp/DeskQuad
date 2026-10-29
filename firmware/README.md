# firmware

DeskQuad 的 ESP32-S3 固件与设备端 Web 控制台。

## 目录结构

```text
firmware/
  include/deskquad/     固件头文件
  src/                  固件源码
  test/                 native 单元测试
  scripts/              系统脚本，包含：PlatformIO 构建钩子（构建文件系统前自动跑前端）
  web/                  Vue 3 + Vite 前端源码
    src/views/          按领域分组：control、settings、ai、misc
  web/dist/             前端构建产物，同时作为 LittleFS 根目录
```

`platformio.ini` 中的 `data_dir = web/dist` 把 LittleFS 根指向前端构建产物，因此不再需要把 HTML 放在 `data/` 里。

## 前端

```bash
cd web
npm ci
npm run dev      # 本地开发，默认把接口代理到 192.168.4.1
npm run build    # 构建并生成 .gz、校验体积与路径
```

开发时可用 `VITE_DEVICE_URL` 覆盖设备地址：

```bash
VITE_DEVICE_URL=http://192.168.4.1 npm run dev
```

构建产物只有 `index.html`、一个 JS、一个 CSS 及对应的 `.gz`。`postbuild.mjs` 会检查路径长度上限、缺失的 gzip 文件、敏感文件名、source map、emoji 和体积预算。

## 固件

```bash
pio test -e native
pio run -e esp32-s3
```

目标板为 ESP32-S3-N16R8（16MB QIO Flash + 8MB OPI PSRAM），串口监控默认走 UART0。

源码按依赖方向分层：

- `core/`：纯逻辑与接口，不调用 Arduino，可被 `env:native` 直接测试
- `config/`：运行参数与引脚定义
- `hal/`：OLED、按键、电池、舵机、扬声器、SNTP、NVS 配置存储
- `motion/`、`emotion/`、`perception/`、`render/`：动作、情感、输入外设、页面
- `agent/`：LLM 协议与 HTTP 客户端
- `app/`：装配与编排（`AppContext`、`AgentService`、`InteractionController`、天气服务）
- `web/`：HTTP 路由与静态资源

## 烧录

`uploadfs` 会先比较 `web/dist` 与前端源码的时间戳，过期时自动执行
`npm run build`。首次使用需先安装依赖；设置 `DESKQUAD_SKIP_WEB_BUILD=1`
可跳过自动构建。

```bash
cd web && npm ci && cd ..
pio run -e esp32-s3 -t uploadfs
pio run -e esp32-s3 -t upload
```

WiFi、天气与 AI 凭据保存在 NVS（`Preferences`，命名空间 `deskquad`），与 LittleFS 分区分离：
`uploadfs` 只重写文件系统，不会清空这些配置。只有 `pio run -t erase` 擦除整片 Flash
时才会丢失，重新烧录时需要重新配置。

NVS 分区只有 20KB，写满后 Arduino 核会擦除整个分区，配置会一起丢失。因此固件启动时调用
`WiFi.persistent(false)`，不让 WiFi 驱动把凭据和射频校准数据也塞进 NVS。另外 `uploadfs`
只更新页面资源、不更新固件：从「配置存在 LittleFS」的旧版本升级时，必须先执行一次
`pio run -e esp32-s3 -t upload`，否则旧固件读不到已刷掉的配置文件，页面会显示为空白。

## 路由

设备端是 hash 路由的单页应用：

- `/` 遥控台（默认，包含运动、表情、时间天气与电量）
- `/#/ai-chat`
- `/#/settings` 系统设置，左侧导航包含「WiFi 设置」（`/#/settings/wifi`）、「天气设置」（`/#/settings/weather`）与「AI 设置」（`/#/settings/ai`）

静态资源只允许 `/assets/` 前缀，其余路径一律 404；配置保存在 NVS，不经过 Web 静态资源服务。
