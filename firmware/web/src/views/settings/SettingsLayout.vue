<template>
  <div class="settings-layout">
    <aside class="settings-nav">
      <p class="settings-nav-title">系统设置</p>
      <nav class="settings-links" aria-label="设置导航">
        <RouterLink to="/settings/wifi" class="settings-link" exact-active-class="active">
          WiFi 设置
        </RouterLink>
        <RouterLink to="/settings/weather" class="settings-link" exact-active-class="active">
          天气设置
        </RouterLink>
        <RouterLink to="/settings/ai" class="settings-link" exact-active-class="active">
          AI 设置
        </RouterLink>
      </nav>
    </aside>

    <section class="settings-content">
      <RouterView />
    </section>
  </div>
</template>

<style scoped>
.settings-layout {
  display: grid;
  grid-template-columns: 220px minmax(0, 1fr);
  gap: 16px;
  align-items: start;
}

.settings-nav {
  position: sticky;
  top: 76px;
  padding: 12px;
  border: 1px solid var(--border);
  border-radius: var(--radius);
  background: var(--surface);
  box-shadow: var(--shadow-soft);
}

.settings-nav-title {
  margin: 4px 8px 10px;
  font-size: 12px;
  font-weight: 700;
  letter-spacing: 0.08em;
  color: var(--muted);
}

.settings-links {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.settings-link {
  padding: 10px 12px;
  border-radius: 9px;
  color: var(--text-soft);
  font-size: 14px;
  font-weight: 600;
}

.settings-link:hover {
  background: var(--primary-soft);
  color: var(--primary-strong);
}

.settings-link.active {
  background: var(--primary);
  color: #fff;
  box-shadow: 0 6px 14px rgba(79, 110, 247, 0.22);
}

.settings-content {
  min-width: 0;
}

@media (max-width: 899px) {
  .settings-layout {
    grid-template-columns: 1fr;
  }

  .settings-nav {
    position: static;
    padding: 8px;
  }

  .settings-nav-title {
    display: none;
  }

  .settings-links {
    flex-direction: row;
    overflow-x: auto;
  }

  .settings-link {
    flex: none;
    padding: 8px 14px;
  }
}
</style>
