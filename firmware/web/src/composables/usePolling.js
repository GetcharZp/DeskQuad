import { onBeforeUnmount, onMounted } from 'vue';

// 用递归 setTimeout 代替 setInterval，避免慢响应时请求堆积
export function usePolling(task, interval, options = {}) {
  const immediate = options.immediate !== false;
  let timer = null;
  let stopped = false;
  let running = false;

  async function tick() {
    if (stopped || running) return;
    running = true;
    try {
      await task();
    } catch {
      // 轮询失败由调用方决定如何展示，不在这里中断循环
    } finally {
      running = false;
      if (!stopped) {
        timer = window.setTimeout(tick, interval);
      }
    }
  }

  function start() {
    if (stopped || timer) return;
    if (immediate) {
      tick();
    } else {
      timer = window.setTimeout(tick, interval);
    }
  }

  function stop() {
    stopped = true;
    if (timer) {
      window.clearTimeout(timer);
      timer = null;
    }
  }

  onMounted(start);
  onBeforeUnmount(stop);

  return { start, stop };
}
