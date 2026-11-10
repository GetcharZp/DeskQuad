<script setup>
import { onBeforeUnmount, ref } from 'vue';
import Icon from '@/components/Icon.vue';
import { capturePhoto } from '@/api/deskquad';
import { useToast } from '@/composables/useToast';

const { toast } = useToast();

const busy = ref(false);
const imageUrl = ref('');
const downloadName = ref('deskquad.jpg');
const capturedAt = ref('');
const error = ref('');

function releaseImage() {
  if (imageUrl.value) URL.revokeObjectURL(imageUrl.value);
  imageUrl.value = '';
}

function timestamp(date) {
  const pad = (value) => String(value).padStart(2, '0');
  return `${date.getFullYear()}${pad(date.getMonth() + 1)}${pad(date.getDate())}-${pad(
    date.getHours(),
  )}${pad(date.getMinutes())}${pad(date.getSeconds())}`;
}

async function takePhoto() {
  if (busy.value) return;

  busy.value = true;
  error.value = '';

  try {
    const blob = await capturePhoto();
    if (!blob.size) throw new Error('设备返回了空画面');

    releaseImage();
    imageUrl.value = URL.createObjectURL(blob);

    const now = new Date();
    capturedAt.value = now.toLocaleTimeString('zh-CN', { hour12: false });
    downloadName.value = `deskquad-${timestamp(now)}.jpg`;
    toast('拍照成功', 'success');
  } catch (err) {
    error.value = err?.message || '拍照失败，请检查设备连接';
    toast(error.value, 'error');
  } finally {
    busy.value = false;
  }
}

onBeforeUnmount(releaseImage);
</script>

<template>
  <div class="camera">
    <div class="preview" :class="{ 'has-image': imageUrl }">
      <img v-if="imageUrl" :src="imageUrl" alt="摄像头拍摄的画面" />
      <div v-else class="placeholder">
        <Icon name="camera" :size="30" />
        <span>点击「拍照」抓拍一张画面</span>
      </div>
      <span v-if="busy" class="overlay">正在拍摄…</span>
    </div>

    <div class="panel">
      <div class="actions">
        <button type="button" class="btn btn-primary" :disabled="busy" @click="takePhoto">
          <Icon name="camera" :size="18" />
          {{ busy ? '拍摄中…' : '拍照' }}
        </button>
        <a v-if="imageUrl" class="btn btn-secondary" :href="imageUrl" :download="downloadName">
          <Icon name="save" :size="18" />
          保存图片
        </a>
      </div>

      <p v-if="error" class="error">{{ error }}</p>
      <p v-else-if="capturedAt" class="hint">拍摄于 {{ capturedAt }} · 640×480 JPEG</p>
      <p v-else class="hint">单次抓拍，不会持续传输视频流。</p>
    </div>
  </div>
</template>

<style scoped>
.camera {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.panel {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.preview {
  position: relative;
  display: flex;
  align-items: center;
  justify-content: center;
  aspect-ratio: 4 / 3;
  overflow: hidden;
  border: 1px dashed var(--border);
  border-radius: 12px;
  background: var(--surface-muted);
}

.preview.has-image {
  border-style: solid;
  background: #0e1320;
}

.preview img {
  width: 100%;
  height: 100%;
  object-fit: contain;
}

.placeholder {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 8px;
  padding: 12px;
  color: var(--muted);
  font-size: 13px;
  text-align: center;
}

.overlay {
  position: absolute;
  padding: 6px 12px;
  border-radius: 999px;
  background: rgba(14, 19, 32, 0.68);
  color: #fff;
  font-size: 12px;
}

.actions {
  display: flex;
  flex-wrap: wrap;
  gap: 10px;
}

.actions .btn {
  flex: 1 1 0;
  min-width: 120px;
}

.hint {
  font-size: 12px;
  color: var(--muted);
}

.error {
  font-size: 12px;
  color: var(--danger);
}
</style>
