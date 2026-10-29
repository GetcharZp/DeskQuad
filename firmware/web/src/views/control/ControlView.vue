<script setup>
import { computed, ref } from 'vue';
import CommandGrid from '@/components/CommandGrid.vue';
import DirectionPad from '@/components/DirectionPad.vue';
import Icon from '@/components/Icon.vue';
import { fetchPercentage, fetchVoltage, sendCommand } from '@/api/deskquad';
import {
  expressionActions,
  freeModeActions,
  networkActions,
  poseActions,
} from '@/constants/actions';
import { usePolling } from '@/composables/usePolling';
import { useToast } from '@/composables/useToast';

const { toast } = useToast();
const voltage = ref('--');
const percentage = ref('--');
const batteryError = ref(false);

const voltageText = computed(() => (voltage.value === '--' ? '--' : `${voltage.value} V`));
const percentageText = computed(() => (percentage.value === '--' ? '--' : `${percentage.value}%`));

async function refreshBattery() {
  const [voltageResult, percentageResult] = await Promise.allSettled([
    fetchVoltage(),
    fetchPercentage(),
  ]);
  if (voltageResult.status === 'fulfilled') voltage.value = voltageResult.value.trim();
  if (percentageResult.status === 'fulfilled') percentage.value = percentageResult.value.trim();
  batteryError.value =
    voltageResult.status !== 'fulfilled' || percentageResult.status !== 'fulfilled';
}

usePolling(refreshBattery, 1000);

async function run(action) {
  try {
    await sendCommand(action.id);
    toast(`已发送：${action.label}`, 'success');
  } catch {
    toast('发送失败，请检查设备连接', 'error');
  }
}
</script>

<template>
  <div class="control-grid">
    <section class="battery-section">
      <div class="battery-row">
        <div class="battery-card">
          <span class="battery-icon">
            <Icon name="battery" :size="20" />
          </span>
          <div>
            <small>电压</small>
            <strong>{{ voltageText }}</strong>
          </div>
        </div>
        <div class="battery-card">
          <span class="battery-icon">
            <Icon name="motion" :size="20" />
          </span>
          <div>
            <small>电量</small>
            <strong>{{ percentageText }}</strong>
          </div>
        </div>
      </div>
      <p v-if="batteryError" class="battery-warning">暂时无法读取电量，正在重试。</p>
    </section>

    <section class="card">
      <h2 class="card-title">方向</h2>
      <DirectionPad @command="run" />
    </section>

    <section class="card">
      <h2 class="card-title">姿态动作</h2>
      <CommandGrid :items="poseActions" :columns="3" @command="run" />
    </section>

    <section class="card">
      <h2 class="card-title">自由模式</h2>
      <CommandGrid :items="freeModeActions" :columns="2" @command="run" />
    </section>

    <section class="card">
      <h2 class="card-title">表情</h2>
      <CommandGrid :items="expressionActions" :columns="4" @command="run" />
    </section>

    <section class="card wide">
      <h2 class="card-title">时间与天气</h2>
      <CommandGrid :items="networkActions" :columns="2" @command="run" />
    </section>
  </div>
</template>

<style scoped>
.control-grid {
  display: grid;
  grid-template-columns: 1fr;
  gap: 14px;
}

.battery-section {
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.battery-row {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 10px;
}

.battery-card {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 14px 16px;
  border: 1px solid var(--border);
  border-radius: var(--radius);
  background: var(--surface);
  box-shadow: var(--shadow-soft);
}

.battery-icon {
  display: flex;
  flex: none;
  align-items: center;
  justify-content: center;
  width: 40px;
  height: 40px;
  border-radius: 12px;
  background: var(--primary-soft);
  color: var(--primary-strong);
}

.battery-card div {
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.battery-card small {
  font-size: 12px;
  color: var(--muted);
}

.battery-card strong {
  font-size: 17px;
  font-variant-numeric: tabular-nums;
}

.battery-warning {
  padding: 0 4px;
  font-size: 12px;
  color: var(--warning);
}

.card-title {
  margin-bottom: 14px;
  font-size: 15px;
  font-weight: 700;
}

@media (min-width: 900px) {
  .control-grid {
    grid-template-columns: repeat(2, minmax(0, 1fr));
  }

  .battery-section,
  .wide {
    grid-column: 1 / -1;
  }
}

@media (min-width: 1400px) {
  .control-grid {
    grid-template-columns: repeat(3, minmax(0, 1fr));
  }

  .wide {
    grid-column: span 2;
  }
}
</style>
