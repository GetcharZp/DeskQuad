<script setup>
import Icon from '@/components/Icon.vue';

defineProps({
  items: { type: Array, required: true },
  columns: { type: Number, default: 3 },
});

const emit = defineEmits(['command']);
</script>

<template>
  <div class="grid" :style="{ '--columns': columns }">
    <button v-for="item in items" :key="item.id" type="button" class="item" @click="emit('command', item)">
      <Icon v-if="item.icon" :name="item.icon" :size="18" />
      <span>{{ item.label }}</span>
    </button>
  </div>
</template>

<style scoped>
.grid {
  display: grid;
  grid-template-columns: repeat(var(--columns), minmax(0, 1fr));
  gap: 10px;
}

.item {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 8px;
  min-height: 48px;
  padding: 10px 12px;
  border: 1px solid var(--border);
  border-radius: 12px;
  background: var(--surface-muted);
  color: var(--text);
  font-size: 14px;
  font-weight: 600;
  transition:
    background 0.16s,
    border-color 0.16s,
    transform 0.12s;
}

.item:hover {
  background: var(--primary-soft);
  border-color: #cdd8ff;
  color: var(--primary-strong);
}

.item:active {
  transform: translateY(1px);
}

@media (max-width: 520px) {
  .grid {
    grid-template-columns: repeat(2, minmax(0, 1fr));
  }
}
</style>
