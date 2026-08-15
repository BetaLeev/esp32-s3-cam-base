<template>
  <div class="board-info-actions">
    <el-divider content-position="left">操作</el-divider>

    <div class="action-buttons">
      <el-button type="primary" @click="handleReboot" :loading="actionLoading === 'reboot'">
        <el-icon><RefreshRight /></el-icon>
        重启系统
      </el-button>

      <el-popconfirm
        :title="`确定要进入${sleepMode === 'light' ? '轻度睡眠' : '深度睡眠'}吗？`"
        confirm-button-text="确定"
        cancel-button-text="取消"
        @confirm="handleSleep"
      >
        <template #reference>
          <el-button type="danger" :loading="actionLoading === 'sleep'">
            <el-icon><VideoPause /></el-icon>
            {{ sleepMode === 'light' ? '轻度睡眠' : '深度睡眠' }}
          </el-button>
        </template>
      </el-popconfirm>

      <el-dropdown @command="handleSleepModeChange">
        <el-button type="info">
          睡眠模式 <el-icon class="el-icon--right"><ArrowDown /></el-icon>
        </el-button>
        <template #dropdown>
          <el-dropdown-menu>
            <el-dropdown-item command="light" :class="{ 'is-active': sleepMode === 'light' }">
              <el-icon><Moon /></el-icon>
              轻度睡眠
              <span class="mode-desc">Wi-Fi断开，保持程序运行</span>
            </el-dropdown-item>
            <el-dropdown-item command="deep" :class="{ 'is-active': sleepMode === 'deep' }">
              <el-icon><MoreFilled /></el-icon>
              深度睡眠
              <span class="mode-desc">Wi-Fi断开，CPU停止</span>
            </el-dropdown-item>
          </el-dropdown-menu>
        </template>
      </el-dropdown>
    </div>

    <el-alert type="info" :closable="false" show-icon class="action-tip">
      <template #title>
        <span>提示：{{ sleepMode === 'light' ? '轻度睡眠' : '深度睡眠' }}模式 - {{ sleepTip }}</span>
      </template>
    </el-alert>
  </div>
</template>

<script setup>
import { ref, computed } from 'vue'
import { ElMessage } from 'element-plus'
import {
  RefreshRight,
  VideoPause,
  ArrowDown,
  Moon,
  MoreFilled
} from '@element-plus/icons-vue'
import { rebootSystem, shutdownSystem } from '@/api/esp32'

const emit = defineEmits(['reboot', 'sleep'])

const actionLoading = ref('')
const sleepMode = ref('light')

const sleepTip = computed(() => {
  return sleepMode.value === 'light'
    ? 'Wi-Fi断开但程序保持运行，可快速唤醒'
    : 'CPU停止运行，可通过复位键唤醒'
})

const handleReboot = async () => {
  actionLoading.value = 'reboot'
  try {
    await rebootSystem()
    ElMessage.success('系统正在重启...')
    emit('reboot')
  } catch (error) {
    ElMessage.error('重启失败')
  } finally {
    actionLoading.value = ''
  }
}

const handleSleep = async () => {
  actionLoading.value = 'sleep'
  try {
    await shutdownSystem({
      wakeup_pin: -1,
      wakeup_level: 0,
      sleep_mode: sleepMode.value
    })
    ElMessage.success(`系统正在进入${sleepMode.value === 'light' ? '轻度睡眠' : '深度睡眠'}...`)
    emit('sleep')
  } catch (error) {
    ElMessage.error('进入睡眠模式失败')
  } finally {
    actionLoading.value = ''
  }
}

const handleSleepModeChange = (mode) => {
  sleepMode.value = mode
}
</script>

<style scoped lang="scss">
@import '@/styles/variables';

.action-buttons {
  display: flex;
  gap: $spacing-base;
  flex-wrap: wrap;
  align-items: center;
  margin-bottom: $spacing-base;
}

.action-tip {
  margin-top: $spacing-base;
}

.mode-desc {
  display: block;
  font-size: $font-size-xs;
  color: $text-secondary;
  margin-top: 2px;
}

:deep(.el-dropdown-menu__item) {
  display: flex;
  flex-direction: column;
  align-items: flex-start;
  padding: $spacing-md $spacing-base;
  line-height: 1.5;
}

:deep(.el-dropdown-menu__item .el-icon) {
  margin-right: $spacing-sm;
}

:deep(.el-dropdown-menu__item.is-active) {
  background-color: #ecf5ff;
  font-weight: 600;
}
</style>
