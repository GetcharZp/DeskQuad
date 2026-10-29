import { RequestError, request } from '@/api/http';

// AI 回复轮询参数：间隔压在 1s 内保证体感，超时覆盖设备端最长的重试耗时
const AI_POLL_INTERVAL = 600;
const AI_REPLY_TIMEOUT = 30000;

export function sendCommand(action) {
  return request('/' + action);
}

export function fetchVoltage() {
  return request('/batteryVoltage');
}

export function fetchPercentage() {
  return request('/batteryPercentage');
}

export function fetchNetworkStatus() {
  return request('/networkStatus', { responseType: 'json' });
}

export function fetchWifiConfig() {
  return request('/wifiConfig', { responseType: 'json' });
}

export function saveWifiConfig(payload) {
  return request('/wifiConfig', { method: 'POST', form: payload, responseType: 'json' });
}

export function fetchWeatherConfig() {
  return request('/weatherConfig', { responseType: 'json' });
}

export function saveWeatherConfig(payload) {
  return request('/weatherConfig', { method: 'POST', form: payload, responseType: 'json' });
}

export function fetchAiStatus() {
  return request('/aiStatus', { responseType: 'json' });
}

export function submitAiMessage(text) {
  return request('/aitalk', { method: 'POST', form: { text }, responseType: 'json' });
}

export function clearAiHistory() {
  return request('/aiClearHistory', { method: 'POST' });
}

// 轮询 /aiStatus 直到该 request_id 进入终态。
// 单次失败不立刻放弃，连续三次失败才认为设备失联。
export async function awaitAiReply(requestId, { timeout = AI_REPLY_TIMEOUT, signal } = {}) {
  const deadline = Date.now() + timeout;
  let failures = 0;

  for (;;) {
    if (signal?.aborted) throw new DOMException('请求已取消', 'AbortError');

    try {
      const data = await fetchAiStatus();
      failures = 0;
      if (data.request_id === requestId && (data.status === 'completed' || data.status === 'failed')) {
        return data;
      }
    } catch (error) {
      if (error.name === 'AbortError') throw error;
      failures += 1;
      if (failures >= 3) throw error;
    }

    if (Date.now() >= deadline) throw new RequestError('等待 AI 回复超时', 0);
    await new Promise((resolve) => setTimeout(resolve, AI_POLL_INTERVAL));
  }
}

export function fetchAiConfig() {
  return request('/aiConfig', { responseType: 'json' });
}

export function saveAiConfig(payload) {
  return request('/aiConfig', { method: 'POST', form: payload, responseType: 'json' });
}
