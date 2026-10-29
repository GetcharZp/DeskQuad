<script setup>
import Icon from '@/components/Icon.vue';
import { useToast } from '@/composables/useToast';

const { toasts, dismiss } = useToast();
</script>

<template>
  <div class="toast-host" aria-live="polite" aria-atomic="false">
    <button v-for="item in toasts" :key="item.id" type="button" class="toast" :class="item.type"
      @click="dismiss(item.id)">
      <Icon :name="item.type === 'error' ? 'alert' : item.type === 'success' ? 'check' : 'info'" :size="16" />
      <span>{{ item.message }}</span>
    </button>
  </div>
</template>

<style scoped>
.toast-host {
  position: fixed;
  right: 16px;
  bottom: 16px;
  left: 16px;
  z-index: 100;
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 8px;
  pointer-events: none;
}

.toast {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  max-width: 420px;
  padding: 10px 14px;
  border: 1px solid var(--border);
  border-radius: 12px;
  background: var(--surface);
  color: var(--text);
  font-size: 13px;
  box-shadow: var(--shadow);
  pointer-events: auto;
}

.toast.success {
  border-color: #bfe8cd;
  background: var(--success-soft);
  color: var(--success);
}

.toast.error {
  border-color: #f4c7cc;
  background: var(--danger-soft);
  color: var(--danger);
}
</style>
