<script setup>
import { ref } from 'vue';
import Icon from '@/components/Icon.vue';

const props = defineProps({
  id: { type: String, required: true },
  placeholder: { type: String, default: '' },
  autocomplete: { type: String, default: 'off' },
});

const model = defineModel({ type: String, default: '' });

// 默认掩码显示，点右侧眼睛临时展开，方便核对密钥
const revealed = ref(false);
</script>

<template>
  <div class="secret-field">
    <input :id="props.id" v-model="model" class="input secret-input" :type="revealed ? 'text' : 'password'"
      :placeholder="props.placeholder" :autocomplete="props.autocomplete" spellcheck="false" />
    <button class="secret-toggle" type="button" :aria-label="revealed ? '隐藏内容' : '显示内容'"
      :aria-pressed="revealed" :title="revealed ? '隐藏' : '显示'" @click="revealed = !revealed">
      <Icon :name="revealed ? 'eye-off' : 'eye'" :size="18" />
    </button>
  </div>
</template>

<style scoped>
.secret-field {
  position: relative;
}

.secret-input {
  padding-right: 46px;
}

.secret-toggle {
  position: absolute;
  top: 0;
  right: 3px;
  display: flex;
  align-items: center;
  justify-content: center;
  width: 38px;
  height: 44px;
  border: none;
  border-radius: 9px;
  background: transparent;
  color: var(--muted);
  transition:
    color 0.18s,
    background 0.18s;
}

.secret-toggle:hover {
  color: var(--text-soft);
  background: var(--surface-muted);
}
</style>
