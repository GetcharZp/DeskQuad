// agent_config.h 定义 AI Agent 的上游端点、超时与内存限制

#pragma once

#define DEEPSEEK_BASE_URL      "https://api.deepseek.com"
#define DEEPSEEK_API_PATH      "/chat/completions"
#define DEEPSEEK_DEFAULT_MODEL "deepseek-flash"

#define AGENT_HTTP_TIMEOUT 8000 // 单次 HTTP 请求超时 (ms)
#define AGENT_COOLDOWN_MS  500  // 两次请求的最小间隔 (ms)
#define AGENT_MAX_RETRIES  1    // 仅对网络错误与 5xx 重试；4xx 是配置问题，重试无意义
#define AGENT_RETRY_DELAY  500  // 重试前的等待时间 (ms)

#define AGENT_MAX_TOKENS    512  // 回复 token 上限
#define AGENT_MAX_HISTORY   3    // 保留的对话轮数
#define AGENT_MAX_USER_LEN  100  // 历史中 user 文本的截断长度
#define AGENT_MAX_REPLY_LEN 200  // 历史中 reply_text 的截断长度
#define AGENT_REQ_BUF_SIZE  8192 // 请求 JSON 缓冲区
