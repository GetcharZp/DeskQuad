import { createRouter, createWebHashHistory } from 'vue-router';

import AiChatView from '@/views/ai/AiChatView.vue';
import ControlView from '@/views/control/ControlView.vue';
import NotFoundView from '@/views/misc/NotFoundView.vue';
import AiSettingsView from '@/views/settings/AiSettingsView.vue';
import SettingsLayout from '@/views/settings/SettingsLayout.vue';
import WeatherSettingsView from '@/views/settings/WeatherSettingsView.vue';
import WifiSettingsView from '@/views/settings/WifiSettingsView.vue';

const routes = [
  { path: '/', component: ControlView, meta: { title: '遥控台' } },
  { path: '/ai-chat', component: AiChatView, meta: { title: 'AI 聊天' } },
  {
    path: '/settings',
    component: SettingsLayout,
    meta: { title: '系统设置' },
    children: [
      { path: '', redirect: '/settings/wifi' },
      { path: 'wifi', component: WifiSettingsView, meta: { title: 'WiFi 设置' } },
      { path: 'weather', component: WeatherSettingsView, meta: { title: '天气设置' } },
      { path: 'ai', component: AiSettingsView, meta: { title: 'AI 设置' } },
    ],
  },
  { path: '/ai-config', redirect: '/settings/ai' },
  { path: '/:pathMatch(.*)*', component: NotFoundView, meta: { title: '页面不存在' } },
];

const router = createRouter({
  history: createWebHashHistory(),
  routes,
  scrollBehavior: () => ({ top: 0 }),
});

router.afterEach((to) => {
  document.title = to.meta.title ? `${to.meta.title} · DeskQuad` : 'DeskQuad';
});

export default router;
