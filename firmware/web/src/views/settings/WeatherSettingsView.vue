<script setup>
import { onMounted, reactive, ref } from 'vue';
import SecretField from '@/components/SecretField.vue';
import { fetchWeatherConfig, saveWeatherConfig } from '@/api/deskquad';

const form = reactive({
  city: '',
  api: '',
});
const saving = ref(false);
const status = ref({ tone: 'info', text: '' });

function setStatus(tone, text) {
  status.value = { tone, text };
}

// 回显已保存的配置；读取失败时保留空表单，用户仍可重新填写
async function loadSavedConfig() {
  try {
    const data = await fetchWeatherConfig();
    if (!data.configured) return;
    form.city = data.city || '';
    form.api = data.apiKey || '';
  } catch {
    // 忽略错误，表单保持可编辑
  }
}

async function submit() {
  if (!form.city.trim()) {
    setStatus('error', '请填写天气城市。');
    return;
  }
  if (!form.api.trim()) {
    setStatus('error', '请填写心知天气 API 密钥。');
    return;
  }

  saving.value = true;
  setStatus('info', '正在保存天气配置…');

  try {
    const data = await saveWeatherConfig({ city: form.city.trim(), api: form.api.trim() });
    if (data.status === 'ok') {
      setStatus('success', '天气配置已保存，返回遥控台的天气页即可查看。');
    } else {
      setStatus('error', `保存失败：${data.error || '未知错误'}`);
    }
  } catch {
    setStatus('error', '保存失败，请检查设备连接。');
  } finally {
    saving.value = false;
  }
}

onMounted(loadSavedConfig);
</script>

<template>
  <div class="stack">
    <section class="card">
      <h2 class="card-title">天气设置</h2>
      <p class="card-note">通过心知天气获取遥控台天气页的数据。</p>

      <div class="info-box">
        <strong>如何获取密钥？</strong>
        <ol>
          <li>访问心知天气（seniverse.com）并注册账号</li>
          <li>在产品控制台创建免费版 API 密钥</li>
          <li>把密钥和城市拼音填入下方</li>
        </ol>
      </div>

      <form @submit.prevent="submit">
        <div class="form-grid">
          <div class="field">
            <label for="city">天气城市</label>
            <input id="city" v-model="form.city" class="input" type="text" placeholder="城市拼音小写，如 shanghai"
              autocomplete="off" />
            <span class="hint">使用心知天气支持的拼音小写名称。</span>
          </div>
          <div class="field">
            <label for="api">心知天气 API 密钥</label>
            <SecretField id="api" v-model="form.api" placeholder="输入心知天气 API 密钥" />
            <span class="hint">默认隐藏，点右侧眼睛查看或核对。</span>
          </div>
        </div>

        <button class="btn btn-primary submit" type="submit" :disabled="saving">
          {{ saving ? '正在保存…' : '保存配置' }}
        </button>
      </form>

      <div v-if="status.text" class="status" :class="`status-${status.tone}`" role="status">
        {{ status.text }}
      </div>
    </section>

    <p class="note">配置保存在设备 NVS 中，重新烧录应用或文件系统都不会清空；擦除整片 Flash 才会丢失。</p>
  </div>
</template>
