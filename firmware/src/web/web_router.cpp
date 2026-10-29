#include <deskquad/web/web_router.h>

#include <deskquad/web/routes_agent.h>
#include <deskquad/web/routes_control.h>
#include <deskquad/web/routes_network.h>
#include <deskquad/web/routes_weather.h>
#include <deskquad/web/static_files.h>

void RegisterAllRoutes(AsyncWebServer &server, AppContext &context)
{
    // 页面与静态资源挂在根路径，所有后端接口统一使用 /api 前缀，
    // 避免以后新增的界面路由与接口路径冲突。
    RegisterStaticFiles(server);
    RegisterControlRoutes(server, context);
    RegisterAgentRoutes(server, context);
    RegisterNetworkRoute(server, context);
    RegisterWeatherRoutes(server, context);

    server.begin();
}
