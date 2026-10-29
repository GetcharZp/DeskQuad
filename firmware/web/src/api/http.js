const DEFAULT_TIMEOUT = 8000;

// 后端接口统一前缀，避免与前端路由冲突
const API_PREFIX = '/api';

export class RequestError extends Error {
  constructor(message, status) {
    super(message);
    this.name = 'RequestError';
    this.status = status;
  }
}

export async function request(path, options = {}) {
  const {
    method = 'GET',
    form = null,
    timeout = DEFAULT_TIMEOUT,
    responseType = 'text',
  } = options;

  const controller = new AbortController();
  const timer = window.setTimeout(() => controller.abort(), timeout);
  const init = {
    method,
    signal: controller.signal,
    cache: 'no-store',
  };

  if (form) {
    init.headers = { 'Content-Type': 'application/x-www-form-urlencoded' };
    init.body = new URLSearchParams(form).toString();
  }

  try {
    const response = await fetch(API_PREFIX + path, init);
    if (!response.ok) {
      // 后端错误体统一为 {status, error}，优先把这句话展示给用户
      let message = '';
      try {
        const payload = await response.json();
        message = payload?.error || payload?.message || '';
      } catch {
        // 错误体不是 JSON 时退回到状态码
      }
      throw new RequestError(message || `HTTP ${response.status}`, response.status);
    }
    return responseType === 'json' ? await response.json() : await response.text();
  } catch (error) {
    if (error.name === 'AbortError') {
      throw new RequestError('请求超时', 0);
    }
    throw error;
  } finally {
    window.clearTimeout(timer);
  }
}
