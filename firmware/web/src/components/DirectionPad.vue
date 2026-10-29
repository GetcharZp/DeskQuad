<script setup>
import Icon from '@/components/Icon.vue';

const emit = defineEmits(['command']);

const directions = [
  { id: 'front', label: '前进', icon: 'arrow-up', area: 'up' },
  { id: 'left', label: '左转', icon: 'arrow-left', area: 'left' },
  { id: 'right', label: '右转', icon: 'arrow-right', area: 'right' },
  { id: 'back', label: '后退', icon: 'arrow-down', area: 'down' },
];
</script>

<template>
  <div class="pad">
    <button v-for="direction in directions" :key="direction.id" type="button" class="key" :class="direction.area"
      :aria-label="direction.label" @click="emit('command', direction)">
      <Icon :name="direction.icon" :size="22" />
      <span>{{ direction.label }}</span>
    </button>
    <div class="center" aria-hidden="true">
      <Icon name="paw" :size="22" />
    </div>
  </div>
</template>

<style scoped>
.pad {
  display: grid;
  grid-template-columns: repeat(3, minmax(0, 1fr));
  grid-template-rows: repeat(3, 62px);
  gap: 8px;
  max-width: 330px;
  margin: 0 auto;
}

.key {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 4px;
  border: 1px solid var(--border);
  border-radius: 14px;
  background: var(--surface-muted);
  color: var(--text);
  font-size: 12px;
  font-weight: 600;
  transition:
    background 0.16s,
    border-color 0.16s,
    transform 0.12s;
}

.key:hover {
  background: var(--primary-soft);
  border-color: #cdd8ff;
  color: var(--primary-strong);
}

.key:active {
  transform: translateY(1px);
}

.up {
  grid-area: 1 / 2;
}

.left {
  grid-area: 2 / 1;
}

.right {
  grid-area: 2 / 3;
}

.down {
  grid-area: 3 / 2;
}

.center {
  display: flex;
  align-items: center;
  justify-content: center;
  grid-area: 2 / 2;
  color: #b7c0d4;
}
</style>
