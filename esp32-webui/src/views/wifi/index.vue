<template>
  <div class="wifi-view">
    <!-- 网络模式切换 -->
    <PageCard title="网络模式" icon="Connection">
      <div class="mode-selector">
        <el-radio-group v-model="wifiMode" @change="handleModeChange">
          <el-radio-button value="sta">Wi-Fi 连接</el-radio-button>
          <el-radio-button value="ap">热点模式 (AP)</el-radio-button>
          <el-radio-button value="ap-sta">混合模式</el-radio-button>
        </el-radio-group>
      </div>
    </PageCard>

    <!-- Wi-Fi 连接状态 -->
    <PageCard v-if="wifiMode !== 'ap'" title="Wi-Fi 连接状态" icon="Connection" :refreshable="true" :loading="loading" @refresh="fetchStatus">
      <div class="status-grid">
        <div class="status-item">
          <div class="status-icon" :class="wifiStatus.connected ? 'connected' : 'disconnected'">
            <el-icon :size="32"><Connection /></el-icon>
          </div>
          <div class="status-info">
            <div class="status-label">连接状态</div>
            <div class="status-value">
              <StatusBadge :status="wifiStatus.connected" :text="wifiStatus.connected ? '已连接' : '未连接'" />
            </div>
          </div>
        </div>

        <div class="status-item" v-if="wifiStatus.connected">
          <div class="status-icon">
            <el-icon :size="32"><Promotion /></el-icon>
          </div>
          <div class="status-info">
            <div class="status-label">信号强度</div>
            <div class="status-value">
              <span :class="getSignalClass(wifiStatus.rssi)">
                {{ wifiStatus.rssi }} dBm ({{ getSignalLevel(wifiStatus.rssi) }})
              </span>
            </div>
          </div>
        </div>

        <div class="status-item" v-if="wifiStatus.connected">
          <div class="status-icon">
            <el-icon :size="32"><Link /></el-icon>
          </div>
          <div class="status-info">
            <div class="status-label">IP地址</div>
            <div class="status-value">{{ wifiStatus.ip || '--' }}</div>
          </div>
        </div>

        <div class="status-item" v-if="wifiStatus.connected">
          <div class="status-icon">
            <el-icon :size="32"><OfficeBuilding /></el-icon>
          </div>
          <div class="status-info">
            <div class="status-label">当前网络</div>
            <div class="status-value">{{ wifiStatus.ssid || '--' }}</div>
          </div>
        </div>
      </div>

      <div class="action-buttons" v-if="wifiStatus.connected">
        <el-button type="danger" @click="handleDisconnect" :loading="saving">
          断开连接
        </el-button>
      </div>
    </PageCard>

    <!-- AP模式：热点状态 -->
    <PageCard v-if="wifiMode !== 'sta'" title="热点模式状态" icon="Share" :refreshable="true" :loading="loading" @refresh="fetchApStatus">
      <div class="status-grid">
        <div class="status-item">
          <div class="status-icon" :class="apStatus.running ? 'connected' : 'disconnected'">
            <el-icon :size="32"><Share /></el-icon>
          </div>
          <div class="status-info">
            <div class="status-label">热点状态</div>
            <div class="status-value">
              <StatusBadge :status="apStatus.running" :text="apStatus.running ? '运行中' : '未运行'" />
            </div>
          </div>
        </div>

        <div class="status-item" v-if="apStatus.running">
          <div class="status-icon">
            <el-icon :size="32"><User /></el-icon>
          </div>
          <div class="status-info">
            <div class="status-label">连接设备</div>
            <div class="status-value">{{ apStatus.connected_clients || 0 }} 台</div>
          </div>
        </div>

        <div class="status-item" v-if="apStatus.running">
          <div class="status-icon">
            <el-icon :size="32"><Cpu /></el-icon>
          </div>
          <div class="status-info">
            <div class="status-label">热点IP</div>
            <div class="status-value">{{ apStatus.ip || '192.168.4.1' }}</div>
          </div>
        </div>
      </div>

      <div class="action-buttons" v-if="apStatus.running">
        <el-button type="danger" @click="handleStopAp" :loading="saving">
          关闭热点
        </el-button>
      </div>
    </PageCard>

    <!-- Wi-Fi连接配置 -->
    <PageCard v-if="wifiMode !== 'ap'" title="Wi-Fi 连接配置" icon="Setting">
      <el-form :model="staConfig" label-width="100px">
        <el-form-item label="SSID">
          <el-input v-model="staConfig.ssid" placeholder="请输入 WiFi 名称" clearable />
        </el-form-item>
        <el-form-item label="密码">
          <el-input v-model="staConfig.password" type="password" placeholder="请输入 WiFi 密码" show-password clearable />
        </el-form-item>
        <el-form-item label="自动连接">
          <el-switch v-model="staConfig.auto_connect" />
        </el-form-item>
        <el-form-item>
          <el-button type="primary" @click="handleStaConnect" :loading="connecting">
            保存并连接
          </el-button>
          <el-button @click="handleScan" :loading="scanning">
            <el-icon><Search /></el-icon>
            扫描网络
          </el-button>
        </el-form-item>
      </el-form>
    </PageCard>

    <!-- AP热点配置 -->
    <PageCard v-if="wifiMode !== 'sta'" title="热点配置" icon="Share">
      <el-alert type="info" :closable="false" show-icon>
        <template #title>
          <span>热点模式下，设备将作为Wi-Fi热点，其他设备可连接到此热点</span>
        </template>
      </el-alert>

      <el-form :model="apConfig" label-width="100px" class="ap-form">
        <el-form-item label="热点SSID">
          <el-input v-model="apConfig.ssid" placeholder="热点名称" clearable />
        </el-form-item>
        <el-form-item label="密码">
          <el-input v-model="apConfig.password" type="password" placeholder="留空则不加密（不推荐）" show-password clearable />
        </el-form-item>
        <el-form-item label="频道">
          <el-select v-model="apConfig.channel" style="width: 100%">
            <el-option v-for="ch in 13" :key="ch" :label="`频道 ${ch}`" :value="ch" />
          </el-select>
        </el-form-item>
        <el-form-item>
          <el-button type="primary" @click="handleStartAp" :loading="saving">
            启动热点
          </el-button>
        </el-form-item>
      </el-form>
    </PageCard>

    <!-- 已扫描网络列表 -->
    <PageCard v-if="networks.length > 0" title="可用网络" :tag="`${networks.length} 个`" icon="FolderOpened">
      <template #header-extra>
        <el-button text type="primary" @click="networks = []">关闭</el-button>
      </template>
      <div class="network-list">
        <div
          v-for="network in networks"
          :key="network.ssid"
          class="network-item"
          @click="selectNetwork(network)"
        >
          <div class="network-info">
            <div class="network-ssid">
              <el-icon><Connection /></el-icon>
              {{ network.ssid }}
              <el-tag v-if="network.authmode === 0" type="info" size="small">开放</el-tag>
            </div>
            <div class="network-detail">
              <span class="signal-strength" :class="getSignalClass(network.rssi)">
                {{ getSignalLevel(network.rssi) }}
              </span>
              <span class="auth-mode">{{ getAuthModeName(network.authmode) }}</span>
            </div>
          </div>
          <el-icon class="select-icon"><ArrowRight /></el-icon>
        </div>
      </div>
    </PageCard>

    <!-- 网络物理层信息 -->
    <PageCard title="网络物理层信息" icon="InfoFilled">
      <el-descriptions :column="2" border size="small">
        <el-descriptions-item label="协议标准">
          IEEE 802.11 b/g/n (2.4GHz)
        </el-descriptions-item>
        <el-descriptions-item label="信道">
          {{ networkPhy.channel }}
        </el-descriptions-item>
        <el-descriptions-item label="频段">
          2.4 GHz (2400-2483.5 MHz)
        </el-descriptions-item>
        <el-descriptions-item label="带宽">
          {{ networkPhy.bandwidth }}
        </el-descriptions-item>
        <el-descriptions-item label="发射功率">
          {{ networkPhy.txPower }}
        </el-descriptions-item>
        <el-descriptions-item label="最大速率">
          {{ networkPhy.maxRate }}
        </el-descriptions-item>
      </el-descriptions>
    </PageCard>
  </div>
</template>

<script setup>
import { ref, reactive, onMounted, onUnmounted } from 'vue'
import { ElMessage } from 'element-plus'
import {
  Connection, Link, Share, User, Cpu, Search,
  Setting, FolderOpened, ArrowRight, OfficeBuilding, Promotion
} from '@element-plus/icons-vue'
import { PageCard, StatusBadge } from '@/components/common'
import { safeFetch } from '@/api/esp32'

// 加载状态
const loading = ref(false)
const saving = ref(false)
const connecting = ref(false)
const scanning = ref(false)

// 网络列表
const networks = ref([])

// 当前Wi-Fi模式: sta | ap | ap-sta
const wifiMode = ref('sta')

// STA模式状态
const wifiStatus = reactive({
  connected: false,
  ssid: '',
  rssi: 0,
  ip: '',
  auto_connect: true
})

// AP模式状态
const apStatus = reactive({
  running: false,
  ssid: '',
  ip: '192.168.4.1',
  connected_clients: 0
})

// STA配置
const staConfig = reactive({
  ssid: '',
  password: '',
  auto_connect: true
})

// AP配置
const apConfig = reactive({
  ssid: 'ESP32-AP',
  password: '',
  channel: 1
})

// 网络物理层信息
const networkPhy = reactive({
  channel: 6,
  bandwidth: '20 MHz',
  txPower: '20 dBm',
  maxRate: '72.2 Mbps'
})

let statusTimer = null

// ========================================
// 信号强度相关
// ========================================
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

// ========================================
// 数据获取
// ========================================
const fetchStatus = async () => {
  loading.value = true
  const baseURL = import.meta.env.VITE_API_BASE_URL || ''
  const { data: result } = await safeFetch(`${baseURL}/api/wifi/status`)

  if (result?.data) {
    const data = result.data
    wifiStatus.connected = data.connected || false
    wifiStatus.ssid = data.ssid || ''
    wifiStatus.rssi = data.rssi || 0
    wifiStatus.ip = data.ip || ''
    wifiStatus.auto_connect = data.auto_connect !== undefined ? data.auto_connect : true
  }
  loading.value = false
}

const fetchApStatus = async () => {
  loading.value = true
  const baseURL = import.meta.env.VITE_API_BASE_URL || ''
  const { data: result } = await safeFetch(`${baseURL}/api/wifi/ap/status`)

  if (result?.data) {
    const data = result.data
    apStatus.running = data.running || false
    apStatus.ssid = data.ssid || ''
    apStatus.ip = data.ip || '192.168.4.1'
    apStatus.connected_clients = data.connected_clients || 0
  }
  loading.value = false
}

const fetchConfig = async () => {
  const baseURL = import.meta.env.VITE_API_BASE_URL || ''
  const { data: result } = await safeFetch(`${baseURL}/api/wifi/config`)

  if (result?.data) {
    const data = result.data
    wifiMode.value = data.mode || 'sta'
    // STA配置
    if (data.sta) {
      staConfig.ssid = data.sta.ssid || ''
      staConfig.password = data.sta.password || ''
      staConfig.auto_connect = data.sta.auto_connect !== undefined ? data.sta.auto_connect : true
    }
    // AP配置
    if (data.ap) {
      apConfig.ssid = data.ap.ssid || 'ESP32-AP'
      apConfig.password = data.ap.password || ''
      apConfig.channel = data.ap.channel || 1
    }
  }
}

// ========================================
// 模式切换
// ========================================
const handleModeChange = async (mode) => {
  saving.value = true
  try {
    const baseURL = import.meta.env.VITE_API_BASE_URL || ''
    await fetch(`${baseURL}/api/wifi/mode`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ mode })
    })
    ElMessage.success(`已切换到${getModeName(mode)}`)
  } catch (error) {
    ElMessage.error('切换失败')
  }
  saving.value = false
}

const getModeName = (mode) => {
  const names = { sta: 'Wi-Fi连接模式', ap: '热点模式', 'ap-sta': '混合模式' }
  return names[mode] || mode
}

// ========================================
// STA操作
// ========================================
const handleStaConnect = async () => {
  if (!staConfig.ssid) {
    ElMessage.warning('请输入 WiFi 名称')
    return
  }

  connecting.value = true
  try {
    const baseURL = import.meta.env.VITE_API_BASE_URL || ''
    const response = await fetch(`${baseURL}/api/wifi/sta/connect`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        ssid: staConfig.ssid,
        password: staConfig.password,
        auto_connect: staConfig.auto_connect
      })
    })
    const result = await response.json()

    if (result.status === 'success') {
      ElMessage.success(result.message || '正在连接...')
      setTimeout(fetchStatus, 2000)
    } else {
      ElMessage.error(result.message || '连接失败')
    }
  } catch (error) {
    ElMessage.error('连接失败')
  }
  connecting.value = false
}

const handleDisconnect = async () => {
  saving.value = true
  try {
    const baseURL = import.meta.env.VITE_API_BASE_URL || ''
    const response = await fetch(`${baseURL}/api/wifi/sta/disconnect`, { method: 'POST' })
    const result = await response.json()

    if (result.status === 'success') {
      ElMessage.success('已断开连接')
      fetchStatus()
    } else {
      ElMessage.error(result.message || '断开失败')
    }
  } catch (error) {
    ElMessage.error('断开失败')
  }
  saving.value = false
}

const handleScan = async () => {
  scanning.value = true
  networks.value = []
  try {
    const baseURL = import.meta.env.VITE_API_BASE_URL || ''
    const response = await fetch(`${baseURL}/api/wifi/scan`)
    const result = await response.json()

    const data = result.data
    if (result.status === 'success' && data?.networks) {
      networks.value = data.networks.filter(n => n.ssid).sort((a, b) => b.rssi - a.rssi)
      ElMessage.success(`找到 ${networks.value.length} 个网络`)
    }
  } catch (error) {
    ElMessage.error('扫描失败')
  }
  scanning.value = false
}

const selectNetwork = (network) => {
  staConfig.ssid = network.ssid
  staConfig.password = ''
  ElMessage.info(`已选择: ${network.ssid}`)
}

// ========================================
// AP操作
// ========================================
const handleStartAp = async () => {
  if (!apConfig.ssid) {
    ElMessage.warning('请输入热点名称')
    return
  }

  saving.value = true
  try {
    const baseURL = import.meta.env.VITE_API_BASE_URL || ''
    const response = await fetch(`${baseURL}/api/wifi/ap/start`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        ssid: apConfig.ssid,
        password: apConfig.password,
        channel: apConfig.channel
      })
    })
    const result = await response.json()

    if (result.status === 'success') {
      ElMessage.success(result.message || '热点已启动')
      fetchApStatus()
    } else {
      ElMessage.error(result.message || '启动失败')
    }
  } catch (error) {
    ElMessage.error('启动失败')
  }
  saving.value = false
}

const handleStopAp = async () => {
  saving.value = true
  try {
    const baseURL = import.meta.env.VITE_API_BASE_URL || ''
    const response = await fetch(`${baseURL}/api/wifi/ap/stop`, { method: 'POST' })
    const result = await response.json()

    if (result.status === 'success') {
      ElMessage.success('热点已关闭')
      fetchApStatus()
    } else {
      ElMessage.error(result.message || '关闭失败')
    }
  } catch (error) {
    ElMessage.error('关闭失败')
  }
  saving.value = false
}

// ========================================
// 生命周期
// ========================================
onMounted(() => {
  fetchConfig()
  fetchStatus()
  fetchApStatus()
  statusTimer = setInterval(() => {
    fetchStatus()
    fetchApStatus()
  }, 10000)
})

onUnmounted(() => {
  if (statusTimer) clearInterval(statusTimer)
})
</script>

<style scoped lang="scss">
@import '@/styles/variables';

.wifi-view {
  max-width: 100%;
}

.mode-selector {
  display: flex;
  justify-content: center;
  padding: $spacing-md 0;
}

// 状态卡片网格
.status-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
  gap: $spacing-lg;
  margin-bottom: $spacing-lg;
}

.status-item {
  display: flex;
  align-items: center;
  gap: $spacing-md;
  padding: $spacing-md;
  background: $bg-color;
  border-radius: $border-radius-base;
}

.status-icon {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 48px;
  height: 48px;
  border-radius: $border-radius-base;
  background: $bg-color-white;
  color: $text-secondary;

  &.connected {
    color: $success-color;
    background: rgba($success-color, 0.1);
  }

  &.disconnected {
    color: $text-secondary;
  }
}

.status-info {
  flex: 1;
  min-width: 0;
}

.status-label {
  font-size: $font-size-xs;
  color: $text-secondary;
  margin-bottom: $spacing-xs;
}

.status-value {
  font-size: $font-size-md;
  font-weight: 600;
  color: $text-primary;
}

.action-buttons {
  display: flex;
  gap: $spacing-md;
  padding-top: $spacing-md;
  border-top: 1px solid $border-color-lighter;
}

.ap-form {
  margin-top: $spacing-lg;
}

// 网络列表
.network-list {
  display: flex;
  flex-direction: column;
  gap: $spacing-sm;
}

.network-item {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: $spacing-md;
  background: $bg-color;
  border-radius: $border-radius-base;
  cursor: pointer;
  transition: all 0.2s;

  &:hover {
    background: darken($bg-color, 3%);
    transform: translateX(4px);
  }
}

.network-info {
  flex: 1;
  min-width: 0;
}

.network-ssid {
  display: flex;
  align-items: center;
  gap: $spacing-sm;
  font-weight: 600;
  color: $text-primary;
  margin-bottom: $spacing-xs;
}

.network-detail {
  display: flex;
  gap: $spacing-md;
  font-size: $font-size-sm;
  color: $text-secondary;
}

.signal-strength {
  font-weight: 600;

  &.signal-excellent { color: $success-color; }
  &.signal-good { color: #85ce61; }
  &.signal-fair { color: $warning-color; }
  &.signal-weak { color: $danger-color; }
}

.select-icon {
  color: $text-placeholder;
  transition: color 0.2s;
}

.network-item:hover .select-icon {
  color: $primary-color;
}

// 信号强度样式
.signal-excellent {
  color: $success-color;
  font-weight: bold;
}

.signal-good {
  color: #85ce61;
}

.signal-fair {
  color: $warning-color;
}

.signal-weak {
  color: $danger-color;
}
</style>
