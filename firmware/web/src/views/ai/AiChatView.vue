<script setup>
import { computed, nextTick, onBeforeUnmount, ref, watch } from 'vue';
import Icon from '@/components/Icon.vue';
import {
  awaitAiReply,
  clearAiHistory,
  fetchAiStatus,
  submitAiMessage,
} from '@/api/deskquad';
import { usePolling } from '@/composables/usePolling';
import { useToast } from '@/composables/useToast';

const GREETING = '你好！我是 DeskQuad 机器狗，有什么想聊的吗？';

const AI_TEXT = {
  checking: '检查中',
  online: 'AI 在线',
  busy: 'AI 思考中',
  offline: '设备未联网',
  disabled: 'AI 未配置',
  error: '状态未知',
};

const { toast } = useToast();
const messages = ref([{ id: 1, role: 'robot', text: GREETING }]);
const input = ref('');
const waiting = ref(false);
const status = ref({ enabled: false, wifi: false, busy: false });
const checked = ref(false);
const unreachable = ref(false);
const chatBox = ref(null);

let nextId = 2;
let pending = null; // 当前请求的 AbortController

const aiState = computed(() => {
  if (unreachable.value) return 'error';
  if (!checked.value) return 'checking';
  if (!status.value.wifi) return 'offline';
  if (!status.value.enabled) return 'disabled';
  if (waiting.value || status.value.busy) return 'busy';
  return 'online';
});

const aiText = computed(() => AI_TEXT[aiState.value]);

async function checkStatus() {
  try {
    status.value = await fetchAiStatus();
    unreachable.value = false;
  } catch {
    unreachable.value = true;
  } finally {
    checked.value = true;
  }
}

usePolling(checkStatus, 3000);

function scrollToBottom() {
  nextTick(() => {
    if (chatBox.value) chatBox.value.scrollTop = chatBox.value.scrollHeight;
  });
}

function addMessage(role, text) {
  messages.value.push({ id: nextId++, role, text });
  scrollToBottom();
}

function removeTyping() {
  messages.value = messages.value.filter((message) => message.role !== 'typing');
}

async function send() {
  const text = input.value.trim();
  if (!text || waiting.value) return;

  input.value = '';
  waiting.value = true;
  addMessage('user', text);
  addMessage('typing', '思考中');

  const controller = new AbortController();
  pending = controller;

  try {
    const { request_id: requestId } = await submitAiMessage(text);
    const result = await awaitAiReply(requestId, { signal: controller.signal });
    if (pending !== controller) return;

    removeTyping();
    if (result.status === 'completed' && result.reply) {
      addMessage('robot', result.reply);
    } else {
      addMessage('system', result.error || 'AI 没有返回内容');
    }
  } catch (error) {
    if (error.name === 'AbortError' || pending !== controller) return;
    removeTyping();
    addMessage('system', error.message || '请求失败，请重试');
  } finally {
    if (pending === controller) {
      pending = null;
      waiting.value = false;
      checkStatus();
    }
  }
}

async function clearHistory() {
  pending?.abort();
  pending = null;
  waiting.value = false;
  messages.value = [{ id: nextId++, role: 'robot', text: GREETING }];
  try {
    await clearAiHistory();
    toast('对话已清除', 'success');
  } catch {
    toast('清除失败，请重试', 'error');
  }
}

watch(
  () => messages.value.length,
  () => scrollToBottom(),
);

onBeforeUnmount(() => {
  pending?.abort();
  pending = null;
});
</script>

<template>
  <div class="chat-page">
    <section class="chat-card">
      <header class="chat-status">
        <span class="state" :class="aiState">
          <span class="dot" />
          {{ aiText }}
        </span>
        <button type="button" class="clear" @click="clearHistory">
          <Icon name="trash" :size="14" />
          清除对话
        </button>
      </header>

      <div ref="chatBox" class="chat-box">
        <div v-for="message in messages" :key="message.id" class="row" :class="message.role">
          <span v-if="message.role === 'robot'" class="avatar">
            <Icon name="paw" :size="18" />
          </span>
          <div class="bubble" :class="message.role">
            <template v-if="message.role === 'typing'">
              {{ message.text }}
              <span class="typing-dots" aria-hidden="true"><i /><i /><i /></span>
            </template>
            <template v-else>{{ message.text }}</template>
          </div>
        </div>
      </div>

      <form class="composer" @submit.prevent="send">
        <input v-model="input" class="input" type="text" placeholder="输入消息…" autocomplete="off" :disabled="waiting" />
        <button class="send" type="submit" :disabled="waiting || !input.trim()" aria-label="发送">
          <Icon name="send" :size="18" />
        </button>
      </form>
    </section>
  </div>
</template>

<style scoped>
.chat-page {
  display: flex;
  flex: 1;
  min-height: 420px;
}

.chat-card {
  display: flex;
  flex: 1;
  flex-direction: column;
  overflow: hidden;
  border: 1px solid var(--border);
  border-radius: var(--radius);
  background: var(--surface);
  box-shadow: var(--shadow-soft);
}

.chat-status {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  padding: 10px 14px;
  border-bottom: 1px solid var(--border);
  background: var(--surface-muted);
}

.state {
  display: inline-flex;
  align-items: center;
  gap: 7px;
  font-size: 12px;
  font-weight: 600;
  color: var(--muted);
}

.dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background: #b7c0d4;
}

.state.online .dot {
  background: var(--success);
}

.state.busy .dot {
  background: var(--warning);
}

.state.offline .dot,
.state.disabled .dot,
.state.error .dot {
  background: var(--danger);
}

.state.online,
.state.busy {
  color: var(--text-soft);
}

.clear {
  display: inline-flex;
  align-items: center;
  gap: 5px;
  padding: 5px 10px;
  border: 1px solid var(--border);
  border-radius: 9px;
  background: var(--surface);
  color: var(--muted);
  font-size: 12px;
}

.clear:hover {
  color: var(--danger);
  border-color: #f4c7cc;
}

.chat-box {
  display: flex;
  flex: 1;
  flex-direction: column;
  gap: 10px;
  padding: 16px;
  overflow-y: auto;
  background:
    radial-gradient(400px 200px at 100% 0%, #f6f8ff, transparent 70%),
    var(--surface);
}

.row {
  display: flex;
  align-items: flex-start;
  gap: 8px;
}

.row.user {
  justify-content: flex-end;
}

.row.system {
  justify-content: center;
}

.avatar {
  display: inline-flex;
  flex: none;
  align-items: center;
  justify-content: center;
  width: 30px;
  height: 30px;
  border-radius: 10px;
  background: var(--primary-soft);
  color: var(--primary-strong);
}

.bubble {
  max-width: min(82%, 720px);
  padding: 10px 13px;
  border-radius: 14px;
  font-size: 14px;
  line-height: 1.6;
  white-space: pre-wrap;
  word-break: break-word;
}

.bubble.robot {
  border: 1px solid var(--border);
  border-top-left-radius: 5px;
  background: var(--surface-muted);
  color: var(--text);
}

.bubble.user {
  border-top-right-radius: 5px;
  background: var(--primary);
  color: #fff;
}

.bubble.typing {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  border: 1px dashed var(--border);
  background: var(--surface);
  color: var(--muted);
  font-style: italic;
}

.bubble.system {
  max-width: 90%;
  padding: 6px 12px;
  border-radius: 9px;
  background: var(--warning-soft);
  color: var(--warning);
  font-size: 12px;
  text-align: center;
}

.typing-dots {
  display: inline-flex;
  gap: 3px;
}

.typing-dots i {
  width: 4px;
  height: 4px;
  border-radius: 50%;
  background: currentColor;
  animation: blink 1.2s infinite ease-in-out;
}

.typing-dots i:nth-child(2) {
  animation-delay: 0.15s;
}

.typing-dots i:nth-child(3) {
  animation-delay: 0.3s;
}

@keyframes blink {

  0%,
  80%,
  100% {
    opacity: 0.25;
  }

  40% {
    opacity: 1;
  }
}

.composer {
  display: flex;
  gap: 8px;
  padding: 12px;
  border-top: 1px solid var(--border);
  background: var(--surface);
}

.composer .input {
  flex: 1;
  border-radius: 22px;
}

.send {
  display: inline-flex;
  flex: none;
  align-items: center;
  justify-content: center;
  width: 44px;
  height: 44px;
  border: none;
  border-radius: 50%;
  background: var(--primary);
  color: #fff;
  transition: background 0.18s;
}

.send:hover:not(:disabled) {
  background: var(--primary-strong);
}

.send:disabled {
  background: #c7cede;
  cursor: not-allowed;
}
</style>
