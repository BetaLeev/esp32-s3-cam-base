<template>
  <div class="wifi-view">
    <el-card class="status-card">
      <template #header>
        <span>WiFi 状态</span>
        <el-button :icon="Refresh" circle size="small" @click="fetchStatus" :loading="loading" />
      </template>
      <el-descriptions :column="2" border>
        <el-descriptions-item label="连接状态">
          <el-tag :type="wifiStatus.connected ? 'success' : 'danger'">
            {{ wifiStatus.connected ? '已连接' : '未连接' }}
          </el-tag>
        </el-descriptions-item>
        <el-descriptions-item label="信号强度">
          <span :class="getSignalClass(wifiStatus.rssi)">
            {{ wifiStatus.rssi }} dBm ({{ getSignalLevel(wifiStatus.rssi) }})
          </span>
        </el-descriptions-item>
        <el-descriptions-item label="当前网络">
          {{ wifiStatus.ssid || '--' }}
        </el-descriptions-item>
        <el-descriptions-item label="IP地址">
          {{ wifiStatus.ip || '--' }}
        </el-descriptions-item>
        <el-descriptions-item label="自动连接">
          <el-switch v-model="wifiStatus.auto_connect" @change="handleAutoConnectChange" :disabled="saving" />
        </el-descriptions-item>
        <el-descriptions-item label="操作">
          <el-button type="danger" size="small" @click="handleDisconnect" :disabled="!wifiStatus.connected || saving">
            断开连接
          </el-button>
        </el-descriptions-item>
      </el-descriptions>
    </el-card>

    <el-card class="config-card">
      <template #header>
        <span>WiFi 配置</span>
      </template>
      <el-form :model="wifiForm" label-width="80px">
        <el-form-item label="SSID">
          <el-input v-model="wifiForm.ssid" placeholder="请输入 WiFi 名称" />
        </el-form-item>
        <el-form-item label="密码">
          <el-input v-model="wifiForm.password" type="password" placeholder="请输入 WiFi 密码" show-password />
        </el-form-item>
        <el-form-item>
          <el-button type="primary" @click="handleConnect" :loading="connecting">
            保存并连接
          </el-button>
          <el-button @click="handleScan" :loading="scanning">
            <el-icon>
              <Search />
            </el-icon>
            扫描网络
          </el-button>
        </el-form-item>
      </el-form>
    </el-card>

    <el-card v-if="networks.length > 0" class="scan-card">
      <template #header>
        <span>可用网络 ({{ networks.length }})</span>
        <el-button text type="primary" @click="networks = []">关闭</el-button>
      </template>
      <el-table :data="networks" size="small" max-height="300">
        <el-table-column prop="ssid" label="网络名称" min-width="150" />
        <el-table-column prop="rssi" label="信号" width="100" align="center">
          <template #default="{ row }">
            <el-tag :type="getSignalTagType(row.rssi)" size="small">
              {{ getSignalLevel(row.rssi) }} ({{ row.rssi }}dBm)
            </el-tag>
          </template>
        </el-table-column>
        <el-table-column prop="authmode" label="加密" width="100" align="center">
          <template #default="{ row }">
            {{ getAuthModeName(row.authmode) }}
          </template>
        </el-table-column>
        <el-table-column label="操作" width="80" align="center">
          <template #default="{ row }">
            <el-button type="primary" size="small" text @click="selectNetwork(row)">
              选择
            </el-button>
          </template>
        </el-table-column>
      </el-table>
    </el-card>
  </div>
</template>

<script setup>
import { ref, reactive, onMounted, onUnmounted } from 'vue'
import { ElMessage } from 'element-plus'
import { Refresh, Search } from '@element-plus/icons-vue'

const loading = ref(false)
const saving = ref(false)
const connecting = ref(false)
const scanning = ref(false)
const networks = ref([])

const wifiStatus = reactive({
  connected: false,
  ssid: '',
  rssi: 0,
  ip: '',
  auto_connect: true
})

const wifiForm = reactive({
  ssid: '',
  password: ''
})

let statusTimer = null

const getSignalLevel = (rssi) => {
  if (rssi >= -50) return '极强'
  if (rssi >= -60) return '强'
  if (rssi >= -70) return '中等'
  if (rssi >= -80) return '弱'
  return '极弱'
}

const getSignalClass = (rssi) => {
  if (rssi >= -50) return 'signal-excellent'
  if (rssi >= -60) return 'signal-good'
  if (rssi >= -70) return 'signal-fair'
  return 'signal-weak'
}

const getSignalTagType = (rssi) => {
  if (rssi >= -50) return 'success'
  if (rssi >= -60) return 'success'
  if (rssi >= -70) return 'warning'
  return 'info'
}

const getAuthModeName = (mode) => {
  const modes = {
    0: '开放',
    1: 'WEP',
    2: 'WPA',
    3: 'WPA2',
    4: 'WPA/WPA2',
    5: 'WPA2/WPA3',
    6: 'WPA3'
  }
  return modes[mode] || '未知'
}

const fetchStatus = async () => {
  loading.value = true
  try {
    const baseURL = import.meta.env.VITE_API_BASE_URL || ''
    const response = await fetch(`${baseURL}/api/wifi/status`)
    const result = await response.json()
    const data = result.data
    wifiStatus.connected = data.connected
    wifiStatus.ssid = data.ssid
    wifiStatus.rssi = data.rssi
    wifiStatus.ip = data.ip
    wifiStatus.auto_connect = data.auto_connect
  } catch (error) {
    console.error('获取 WiFi 状态失败:', error)
  } finally {
    loading.value = false
  }
}

const fetchConfig = async () => {
  try {
    const baseURL = import.meta.env.VITE_API_BASE_URL || ''
    const response = await fetch(`${baseURL}/api/wifi/config`)
    const result = await response.json()
    const data = result.data
    wifiForm.ssid = data.ssid || ''
    wifiForm.password = data.password || ''
  } catch (error) {
    console.error('获取 WiFi 配置失败:', error)
  }
}

const handleConnect = async () => {
  if (!wifiForm.ssid) {
    ElMessage.warning('请输入 WiFi 名称')
    return
  }

  connecting.value = true
  try {
    const baseURL = import.meta.env.VITE_API_BASE_URL || ''
    const params = new URLSearchParams({
      ssid: wifiForm.ssid,
      password: wifiForm.password,
      auto_connect: wifiStatus.auto_connect ? 'true' : 'false'
    })
    const response = await fetch(`${baseURL}/api/wifi/set?${params}`)
    const result = await response.json()
    const data = result.data

    if (data && data.success) {
      ElMessage.success(data.message || '正在连接...')
      setTimeout(fetchStatus, 2000)
    } else {
      ElMessage.error(result.message || '连接失败')
    }
  } catch (error) {
    ElMessage.error('连接失败')
    console.error(error)
  } finally {
    connecting.value = false
  }
}

const handleDisconnect = async () => {
  saving.value = true
  try {
    const baseURL = import.meta.env.VITE_API_BASE_URL || ''
    const response = await fetch(`${baseURL}/api/wifi/disconnect`)
    const result = await response.json()
    const data = result.data

    if (data && data.success) {
      ElMessage.success('已断开连接')
      fetchStatus()
    } else {
      ElMessage.error(result.message || '断开失败')
    }
  } catch (error) {
    ElMessage.error('断开失败')
  } finally {
    saving.value = false
  }
}

const handleAutoConnectChange = async (value) => {
  wifiStatus.auto_connect = value
  await handleConnect()
}

const handleScan = async () => {
  scanning.value = true
  networks.value = []
  try {
    const baseURL = import.meta.env.VITE_API_BASE_URL || ''
    const response = await fetch(`${baseURL}/api/wifi/scan`)
    const result = await response.json()
    console.log(result)

    // 统一响应格式：数据在 result.data 中
    const data = result.data
    if (result.status === 'success' && data && data.success && data.networks) {
      networks.value = data.networks.filter(n => n.ssid)
        .sort((a, b) => b.rssi - a.rssi)
      ElMessage.success(`找到 ${networks.value.length} 个网络`)
    } else {
      // ElMessage.error(result.message || '扫描失败')
    }
  } catch (error) {
    ElMessage.error('扫描失败')
    console.error(error)
  } finally {
    scanning.value = false
  }
}

const selectNetwork = (network) => {
  wifiForm.ssid = network.ssid
  wifiForm.password = ''
  ElMessage.info(`已选择: ${network.ssid}`)
}

onMounted(() => {
  fetchStatus()
  fetchConfig()
  statusTimer = setInterval(fetchStatus, 10000)
})

onUnmounted(() => {
  if (statusTimer) clearInterval(statusTimer)
})
</script>

<style scoped>
.wifi-view {
  max-width: 600px;
  margin: 0 auto;
}

.status-card,
.config-card,
.scan-card {
  margin-bottom: 20px;
}

.signal-excellent {
  color: #67c23a;
  font-weight: bold;
}

.signal-good {
  color: #85ce61;
}

.signal-fair {
  color: #e6a23c;
}

.signal-weak {
  color: #f56c6c;
}
</style>
