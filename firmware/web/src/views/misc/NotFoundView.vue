<script setup>
import Icon from '@/components/Icon.vue';
</script>

<template>
  <div class="not-found">
    <section class="card">
      <span class="icon">
        <Icon name="alert" :size="26" />
      </span>
      <h1>页面不存在</h1>
      <p>这个地址没有对应的页面。</p>
      <RouterLink to="/" class="btn btn-primary">
        <Icon name="home" :size="18" />
        返回遥控台
      </RouterLink>
    </section>
  </div>
</template>

<style scoped>
.not-found {
  display: flex;
  flex: 1;
  align-items: center;
  justify-content: center;
  min-height: 420px;
  padding: 24px 16px;
}

.card {
  width: 100%;
  max-width: 380px;
  padding: 32px 26px;
  text-align: center;
}

.icon {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 52px;
  height: 52px;
  margin-bottom: 14px;
  border-radius: 16px;
  background: var(--warning-soft);
  color: var(--warning);
}

h1 {
  font-size: 20px;
  font-weight: 700;
}

p {
  margin: 8px 0 22px;
  font-size: 13px;
  color: var(--muted);
}
</style>
