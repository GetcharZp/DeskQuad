<script setup>
import { onMounted, reactive, ref } from 'vue';
import Icon from '@/components/Icon.vue';
import SecretField from '@/components/SecretField.vue';
import {
  awaitAiReply,
  fetchAiConfig,
  saveAiConfig,
  submitAiMessage,
} from '@/api/deskquad';

const form = reactive({
  apiKey: '',
  model: 'deepseek-flash',
  baseUrl: '',
});
const loading = ref(true);
const saving = ref(false);
const testing = ref(false);
const status = ref({ tone: 'info', text: '' });

function setStatus(tone, text) {
  status.value = { tone, text };
}

function valid() {
  if (!form.apiKey.trim()) {
    setStatus('error', '请填写 API Key。');
    return false;
  }
  return true;
}

async function load() {
  try {
    const data = await fetchAiConfig();
    form.apiKey = data.apiKey || '';
    form.model = data.model || 'deepseek-flash';
    form.baseUrl = data.baseUrl || '';
    setStatus('info', data.configured ? 'AI 已配置。' : '尚未配置 AI。');
  } catch {
    setStatus('error', '无法读取当前 AI 配置。');
  } finally {
    loading.value = false;
  }
}

async function save() {
  if (!valid()) return false;

  saving.value = true;
  setStatus('info', '正在保存配置…');
  try {
    const data = await saveAiConfig({
      apiKey: form.apiKey.trim(),
      model: form.model.trim() || 'deepseek-flash',
      baseUrl: form.baseUrl.trim(),
    });
    if (data.status === 'ok') {
      setStatus('success', 'AI 配置保存成功，机器狗已连接上 AI 大脑。');
      return true;
    }
    setStatus('error', `保存失败：${data.error || '未知错误'}`);
    return false;
  } catch {
    setStatus('error', '保存失败，请检查设备连接。');
    return false;
  } finally {
    saving.value = false;
  }
}

async function testConnection() {
  if (!valid()) return;

  const saved = await save();
  if (!saved) return;

  testing.value = true;
  setStatus('info', '正在测试 AI 连接…');

  try {
    const { request_id: requestId } = await submitAiMessage('你好，请回复一句话');
    const result = await awaitAiReply(requestId);
    if (result.status === 'completed' && result.reply) {
      setStatus('success', `AI 回复：${result.reply}`);
    } else {
      setStatus('error', `连接失败：${result.error || 'AI 未返回内容'}`);
    }
  } catch (error) {
    setStatus('error', `测试失败：${error.message || '请检查设备连接'}`);
  } finally {
    testing.value = false;
  }
}

onMounted(load);
</script>

<template>
  <div class="stack">
    <section class="card">
      <h2 class="card-title">AI 设置</h2>
      <p class="card-note">DeepSeek 开放平台 API</p>

      <div class="info-box">
        <strong>如何获取？</strong>
        <ol>
          <li>访问 DeepSeek 开放平台（platform.deepseek.com）</li>
          <li>在 API Keys 页面创建密钥</li>
          <li>复制 API Key 填入下方</li>
        </ol>
      </div>

      <form @submit.prevent="save">
        <div class="form-grid">
          <div class="field">
            <label for="apiKey">API Key</label>
            <SecretField id="apiKey" v-model="form.apiKey" placeholder="请输入 API Key" />
            <span class="hint">DeepSeek 开放平台的 API 密钥，默认隐藏，点右侧眼睛查看。</span>
          </div>
          <div class="field">
            <label for="model">模型</label>
            <input id="model" v-model="form.model" class="input" type="text" placeholder="deepseek-flash"
              autocomplete="off" />
            <span class="hint">默认 deepseek-flash；该模型支持 JSON 输出。</span>
          </div>
          <div class="field span-2">
            <label for="baseUrl">AI 接口地址（可选）</label>
            <input id="baseUrl" v-model="form.baseUrl" class="input" type="url"
              placeholder="例如：https://api.deepseek.com 或 http://192.168.4.2:8000/v1" autocomplete="off" />
            <span class="hint">留空直连 DeepSeek；填根地址或 /v1 即可，会自动补全 /chat/completions。</span>
          </div>
        </div>

        <div class="actions">
          <button class="btn btn-primary" type="submit" :disabled="loading || saving || testing">
            <Icon name="save" :size="18" />
            {{ saving ? '正在保存…' : '保存配置' }}
          </button>
          <button class="btn btn-secondary" type="button" :disabled="loading || saving || testing"
            @click="testConnection">
            <Icon name="plug" :size="18" />
            {{ testing ? '正在测试…' : '测试连接' }}
          </button>
        </div>
      </form>

      <div v-if="status.text" class="status" :class="`status-${status.tone}`" role="status">
        {{ status.text }}
      </div>
    </section>
  </div>
</template>

<style scoped>
.actions {
  display: flex;
  flex-wrap: wrap;
  gap: 10px;
  margin-top: 4px;
}

@media (min-width: 900px) {
  .span-2 {
    grid-column: 1 / -1;
  }

  .actions .btn {
    min-width: 160px;
  }
}
</style>
