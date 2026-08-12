<template>
  <el-card class="board-control-card">
    <template #header>
      <span>系统控制</span>
    </template>

    <el-descriptions :column="2" border size="small" class="board-info" v-if="boardInfo">
      <el-descriptions-item label="芯片型号">
        {{ boardInfo.chip_model || '—' }}
      </el-descriptions-item>
      <el-descriptions-item label="固件版本">
        {{ boardInfo.firmware_version || '—' }}
      </el-descriptions-item>
      <el-descriptions-item label="板子名称">
        {{ boardInfo.board_name || '—' }}
      </el-descriptions-item>
      <el-descriptions-item label="运行时间">
        {{ boardInfo.uptime || '—' }}
      </el-descriptions-item>
      <el-descriptions-item label="编译时间">
        {{ boardInfo.build_time || '—' }}
      </el-descriptions-item>
      <el-descriptions-item label="可用内存">
        {{ boardInfo.free_heap || '—' }}
      </el-descriptions-item>
    </el-descriptions>

    <el-divider content-position="left">操作</el-divider>

    <div class="control-buttons">
      <el-button type="primary" @click="handleReboot" :loading="actionLoading === 'reboot'">
        <el-icon><RefreshRight /></el-icon>
        重启系统
      </el-button>

      <!-- 睡眠按钮 -->
      <el-popconfirm
        :title="`确定要进入${sleepMode === 'light' ? '轻度睡眠' : '深度睡眠'}吗？设备将断开网络，可通过复位键唤醒。`"
        confirm-button-text="确定"
        cancel-button-text="取消"
        @confirm="handleShutdown"
      >
        <template #reference>
          <el-button type="danger" :loading="actionLoading === 'sleep'">
            <el-icon><VideoPause /></el-icon>
            {{ sleepMode === 'light' ? '轻度睡眠' : '深度睡眠' }}
          </el-button>
        </template>
      </el-popconfirm>

      <!-- 睡眠模式选择 -->
      <el-dropdown @command="handleSleepModeChange">
        <el-button type="info">
          睡眠模式 <el-icon class="el-icon--right"><ArrowDown /></el-icon>
        </el-button>
        <template #dropdown>
          <el-dropdown-menu>
            <el-dropdown-item :command="'light'" :class="{ 'active-mode': sleepMode === 'light' }">
              <el-icon><Moon /></el-icon>
              轻度睡眠
              <span class="mode-desc">Wi-Fi断开，保持程序运行</span>
            </el-dropdown-item>
            <el-dropdown-item :command="'deep'" :class="{ 'active-mode': sleepMode === 'deep' }">
              <el-icon><MoreFilled /></el-icon>
              深度睡眠
              <span class="mode-desc">Wi-Fi断开，CPU停止，仅定时器唤醒</span>
            </el-dropdown-item>
          </el-dropdown-menu>
        </template>
      </el-dropdown>
    </div>

    <div class="control-tip">
      <el-alert type="info" :closable="false" show-icon>
        <template #title>
          <span>提示：{{ sleepMode === 'light' ? '轻度睡眠' : '深度睡眠' }}模式 - {{ sleepMode === 'light' ? 'Wi-Fi断开但程序保持运行，可快速唤醒' : 'CPU停止运行，可通过复位键唤醒' }}</span>
        </template>
      </el-alert>
    </div>
  </el-card>
</template>

<script setup>
import { ref } from 'vue'
import { ElMessage } from 'element-plus'
import { RefreshRight, VideoPause, ArrowDown, Moon, MoreFilled } from '@element-plus/icons-vue'
import { rebootSystem, shutdownSystem } from '@/api/esp32'

const props = defineProps({
  boardInfo: {
    type: Object,
    default: () => ({})
  }
})

const emit = defineEmits(['reboot', 'sleep'])

const actionLoading = ref('')
const sleepMode = ref('light')  // 'light' 轻度睡眠, 'deep' 深度睡眠

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

const handleShutdown = async () => {
  actionLoading.value = 'sleep'
  try {
    // 轻度睡眠使用 sleep_mode = light
    const params = {
      wakeup_pin: -1,
      wakeup_level: 0,
      sleep_mode: sleepMode.value
    }
    await shutdownSystem(params)
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

<style scoped>
.board-control-card {
  margin-bottom: 20px;
}

.board-info {
  margin-bottom: 8px;
}

.control-buttons {
  display: flex;
  gap: 12px;
  flex-wrap: wrap;
  align-items: center;
}

.control-tip {
  margin-top: 16px;
}

.mode-desc {
  display: block;
  font-size: 11px;
  color: #909399;
  margin-top: 2px;
}

.active-mode {
  background-color: #ecf5ff;
  font-weight: 600;
}

:deep(.el-dropdown-menu__item) {
  display: flex;
  flex-direction: column;
  align-items: flex-start;
  padding: 10px 16px;
  line-height: 1.5;
}

:deep(.el-dropdown-menu__item .el-icon) {
  margin-right: 8px;
  vertical-align: middle;
}
</style>
