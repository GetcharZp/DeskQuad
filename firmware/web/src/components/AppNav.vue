<script setup>
import { useRoute } from 'vue-router';
import Icon from '@/components/Icon.vue';

const route = useRoute();

const items = [
  { to: '/', label: '遥控台' },
  { to: '/ai-chat', label: 'AI 聊天' },
  { to: '/settings', label: '系统设置' },
];

function isActive(item) {
  if (item.to === '/settings') {
    return route.path === '/settings' || route.path.startsWith('/settings/');
  }
  return route.path === item.to;
}
</script>

<template>
  <header class="topnav">
    <div class="topnav-inner">
      <RouterLink to="/" class="brand" aria-label="DeskQuad 遥控台">
        <span class="brand-mark"><Icon name="paw" :size="20" /></span>
        <span class="brand-name">DeskQuad</span>
      </RouterLink>

      <nav class="links" aria-label="主导航">
        <RouterLink
          v-for="item in items"
          :key="item.to"
          :to="item.to"
          class="link"
          :class="{ active: isActive(item) }"
        >
          {{ item.label }}
        </RouterLink>
      </nav>
    </div>
  </header>
</template>

<style scoped>
.topnav {
  position: sticky;
  top: 0;
  z-index: 30;
  border-bottom: 1px solid rgba(226, 231, 241, 0.8);
  background: rgba(244, 246, 251, 0.86);
  backdrop-filter: blur(12px);
}

.topnav-inner {
  display: flex;
  align-items: center;
  gap: 16px;
  width: 100%;
  padding: 9px 20px;
}

.brand {
  display: inline-flex;
  flex: none;
  align-items: center;
  gap: 9px;
  color: var(--text);
  font-weight: 700;
}

.brand-mark {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 34px;
  height: 34px;
  border-radius: 11px;
  background: var(--primary);
  color: #fff;
  box-shadow: 0 6px 14px rgba(79, 110, 247, 0.28);
}

.brand-name {
  font-size: 16px;
  letter-spacing: 0.01em;
}

.links {
  display: flex;
  flex: 1 1 0;
  min-width: 0;
  gap: 10px;
  overflow-x: auto;
  scrollbar-width: none;
}

.links::-webkit-scrollbar {
  display: none;
}

.link {
  flex: none;
  padding: 8px;
  border-radius: 9px;
  color: var(--text-soft);
  font-size: 13px;
  font-weight: 600;
  white-space: nowrap;
  transition:
    background 0.16s,
    color 0.16s;
}

.link:hover {
  background: rgba(79, 110, 247, 0.08);
  color: var(--primary-strong);
}

.link.active {
  background: var(--primary);
  color: #fff;
  box-shadow: 0 6px 14px rgba(79, 110, 247, 0.22);
}

@media (max-width: 420px) {
  .brand-name {
    display: none;
  }
}
</style>
