import { reactive } from 'vue';

const toasts = reactive([]);
let sequence = 0;

export function toast(message, type = 'info') {
  const id = ++sequence;
  toasts.push({ id, message, type });
  if (toasts.length > 3) {
    toasts.splice(0, toasts.length - 3);
  }
  window.setTimeout(() => dismiss(id), 2600);
}

export function dismiss(id) {
  const index = toasts.findIndex((item) => item.id === id);
  if (index >= 0) toasts.splice(index, 1);
}

export function useToast() {
  return { toasts, toast, dismiss };
}
