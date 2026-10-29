<script setup>
import { onMounted, reactive, ref } from 'vue';
import SecretField from '@/components/SecretField.vue';
import { fetchNetworkStatus, fetchWifiConfig, saveWifiConfig } from '@/api/deskquad';
import { usePolling } from '@/composables/usePolling';

const form = reactive({
  ssid: '',
  pass: '',
});
const saving = ref(false);
const status = ref({ tone: 'info', text: '' });
const network = reactive({
  loaded: false,
  connected: false,
  ssid: '',
  ip: '',
  apIp: '192.168.4.1',
  rssi: 0,
});

function setStatus(tone, text) {
  status.value = { tone, text };
}

async function loadNetworkStatus() {
  try {
    const data = await fetchNetworkStatus();
    network.connected = Boolean(data.connected);
    network.ssid = data.ssid || '';
    network.ip = data.ip || '';
    network.apIp = data.apIp || network.apIp;
    network.rssi = data.rssi || 0;
  } finally {
    // 首次请求失败时也要结束「读取中」，避免一直停留在加载态
    network.loaded = true;
  }
}

// 回显已保存的配置；读取失败时保留空表单，但要提示是「读不到」而不是「没保存」
async function loadSavedConfig() {
  try {
    const data = await fetchWifiConfig();
    if (!data.configured) return;
    form.ssid = data.ssid || '';
    form.pass = data.pass || '';
  } catch {
    setStatus('warning', '无法读取设备上已保存的 WiFi 配置，请确认设备固件为最新版本。');
  }
}

async function submit() {
  if (!form.ssid.trim()) {
    setStatus('error', '请填写 WiFi 名称。');
    return;
  }

  saving.value = true;
  setStatus('info', '正在保存配置并连接 WiFi…');

  try {
    const data = await saveWifiConfig({ ssid: form.ssid.trim(), pass: form.pass });
    if (data.status === 'ok') {
      setStatus('success', 'WiFi 配置已保存，设备正在连接。若页面因切换网络中断，请重新连接设备热点后验证。');
    } else {
      setStatus('error', `保存失败：${data.error || '未知错误'}`);
    }
  } catch {
    setStatus(
      'warning',
      '请求可能因设备切换网络而中断。配置通常已经写入设备，请重新连接设备热点后访问 192.168.4.1 验证。',
    );
  } finally {
    saving.value = false;
  }
}

onMounted(loadSavedConfig);

// 连接结果由后台异步产生，轮询让地址和状态自动更新
usePolling(loadNetworkStatus, 5000);
</script>

<template>
  <div class="stack">
    <section class="card">
      <h2 class="card-title">当前连接</h2>
      <dl class="status-list">
        <div class="status-row">
          <dt>WiFi</dt>
          <dd v-if="!network.loaded" class="muted">读取中…</dd>
          <dd v-else-if="network.connected">
            {{ network.ssid || '已连接' }}
            <span v-if="network.rssi" class="rssi">{{ network.rssi }} dBm</span>
          </dd>
          <dd v-else class="muted">未连接</dd>
        </div>
        <div class="status-row">
          <dt>局域网地址</dt>
          <dd v-if="network.connected && network.ip">
            <a :href="`http://${network.ip}`" target="_blank" rel="noopener">http://{{ network.ip }}</a>
          </dd>
          <dd v-else class="muted">连接 WiFi 后可用</dd>
        </div>
        <div class="status-row">
          <dt>设备热点</dt>
          <dd>
            <a :href="`http://${network.apIp}`" target="_blank" rel="noopener">http://{{ network.apIp }}</a>
          </dd>
        </div>
      </dl>
      <p class="status-hint">手机或电脑连接同一路由器后，打开局域网地址即可远程访问设备。</p>
    </section>

    <section class="card">
      <h2 class="card-title">WiFi 设置</h2>
      <p class="card-note">连接家庭路由器后即可使用天气与 AI；DeskQuad 热点始终可用。</p>

      <form @submit.prevent="submit">
        <div class="form-grid">
          <div class="field">
            <label for="ssid">WiFi 名称</label>
            <input id="ssid" v-model="form.ssid" class="input" type="text" placeholder="输入 WiFi 名称"
              autocomplete="off" />
          </div>
          <div class="field">
            <label for="pass">WiFi 密码</label>
            <SecretField id="pass" v-model="form.pass" placeholder="输入 WiFi 密码" autocomplete="new-password" />
            <span class="hint">默认隐藏，点右侧眼睛查看；开放网络可以留空。</span>
          </div>
        </div>

        <button class="btn btn-primary submit" type="submit" :disabled="saving">
          {{ saving ? '正在保存…' : '保存并连接' }}
        </button>
      </form>

      <div v-if="status.text" class="status" :class="`status-${status.tone}`" role="status">
        {{ status.text }}
      </div>
    </section>

    <p class="note">配置保存在设备 NVS 中，重新烧录应用或文件系统都不会清空；擦除整片 Flash 才会丢失。</p>
  </div>
</template>

<style scoped>
.status-list {
  margin: 0;
}

.status-row {
  display: flex;
  align-items: baseline;
  justify-content: space-between;
  gap: 16px;
  padding: 9px 0;
  border-bottom: 1px solid var(--border);
}

.status-row:last-child {
  border-bottom: none;
}

.status-row dt {
  flex: none;
  font-size: 13px;
  color: var(--muted);
}

.status-row dd {
  margin: 0;
  font-size: 13px;
  font-weight: 600;
  text-align: right;
  word-break: break-all;
}

.status-row a {
  color: var(--primary-strong);
}

.muted {
  font-weight: 400;
  color: var(--muted);
}

.rssi {
  margin-left: 6px;
  font-weight: 400;
  color: var(--muted);
}

.status-hint {
  margin-top: 12px;
  font-size: 12px;
  line-height: 1.5;
  color: var(--muted);
}
</style>
