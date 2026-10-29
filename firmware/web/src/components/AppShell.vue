<script setup>
import AppFooter from '@/components/AppFooter.vue';
import AppNav from '@/components/AppNav.vue';
</script>

<template>
  <div class="shell">
    <AppNav />
    <main class="page">
      <slot />
    </main>
    <AppFooter />
  </div>
</template>

<style scoped>
.shell {
  display: flex;
  flex-direction: column;
  min-height: 100vh;
  min-height: 100dvh;
}

.page {
  display: flex;
  flex: 1;
  flex-direction: column;
  width: 100%;
  padding: 16px 20px;
}
</style>
