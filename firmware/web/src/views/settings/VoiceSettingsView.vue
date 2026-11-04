<script setup>
import { onMounted, reactive, ref } from 'vue';
import Icon from '@/components/Icon.vue';
import SecretField from '@/components/SecretField.vue';
import { fetchVoiceConfig, saveVoiceConfig, testVoiceConfig } from '@/api/deskquad';

const form = reactive({
  apiKey: '',
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
    setStatus('error', '请填写豆包 API Key。');
    return false;
  }
  return true;
}

async function load() {
  try {
    const data = await fetchVoiceConfig();
    form.apiKey = data.apiKey || '';
    setStatus('info', data.configured ? '豆包语音已配置。' : '尚未配置豆包语音。');
  } catch {
    setStatus('error', '无法读取当前语音配置。');
  } finally {
    loading.value = false;
  }
}

async function save() {
  if (!valid()) return false;

  saving.value = true;
  setStatus('info', '正在保存配置…');
  try {
    const data = await saveVoiceConfig({ apiKey: form.apiKey.trim() });
    if (data.status === 'ok') {
      setStatus('success', '语音配置已保存，唤醒词对话立即生效。');
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

// 测试不写入 NVS，确认可用后再点保存
async function testConnection() {
  if (!valid()) return;

  testing.value = true;
  setStatus('info', '正在连接豆包语音服务…');
  try {
    const data = await testVoiceConfig(form.apiKey.trim());
    setStatus('success', data.message || '豆包语音服务连接正常，可点击保存配置。');
  } catch (error) {
    setStatus('error', `测试失败：${error.message || '请检查设备网络'}`);
  } finally {
    testing.value = false;
  }
}

onMounted(load);
</script>

<template>
  <div class="stack">
    <section class="card">
      <h2 class="card-title">语音设置</h2>
      <p class="card-note">豆包实时语音（ASR / TTS）</p>

      <div class="info-box">
        <strong>如何获取？</strong>
        <ol>
          <li>登录火山引擎控制台（console.volcengine.com）</li>
          <li>开通豆包语音服务，并在 API Key 管理中创建密钥</li>
          <li>把密钥填入下方，先测试连接再保存</li>
        </ol>
      </div>

      <form @submit.prevent="save">
        <div class="form-grid">
          <div class="field span-2">
            <label for="apiKey">豆包 API Key</label>
            <SecretField id="apiKey" v-model="form.apiKey" placeholder="请输入豆包 API Key" />
            <span class="hint">密钥保存在设备 NVS 中，不会写入固件；默认隐藏，点右侧眼睛查看。</span>
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

      <p class="note">
        测试只校验密钥与豆包语音服务的握手，不会播放声音；语音对话需要设备已连接 WiFi。
      </p>
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
