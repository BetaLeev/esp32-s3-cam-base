<template>
  <div class="board-view">
    <!-- 统计卡片 -->
    <el-row :gutter="16" class="stat-row">
      <el-col :span="6">
        <el-card shadow="hover" class="stat-card stat-total">
          <div class="stat-value">{{ stats.total }}</div>
          <div class="stat-label">总引脚数</div>
        </el-card>
      </el-col>
      <el-col :span="6">
        <el-card shadow="hover" class="stat-card stat-used">
          <div class="stat-value">{{ stats.used }}</div>
          <div class="stat-label">已使用</div>
        </el-card>
      </el-col>
      <el-col :span="6">
        <el-card shadow="hover" class="stat-card stat-free">
          <div class="stat-value">{{ stats.free }}</div>
          <div class="stat-label">空闲</div>
        </el-card>
      </el-col>
      <el-col :span="6">
        <el-card shadow="hover" class="stat-card stat-reserved">
          <div class="stat-value">{{ stats.reserved }}</div>
          <div class="stat-label">预留</div>
        </el-card>
      </el-col>
    </el-row>

    <!-- 引脚列表 -->
    <el-card class="pin-card">
      <template #header>
        <div class="card-header">
          <span>板子引脚总览</span>
          <el-radio-group v-model="filterStatus" size="small">
            <el-radio-button label="">全部</el-radio-button>
            <el-radio-button label="used">已使用</el-radio-button>
            <el-radio-button label="free">空闲</el-radio-button>
            <el-radio-button label="reserved">预留</el-radio-button>
          </el-radio-group>
        </div>
      </template>

      <el-table
        :data="filteredPins"
        stripe
        :default-sort="{ prop: 'gpio', order: 'ascending' }"
        style="width: 100%"
      >
        <el-table-column prop="gpio" label="GPIO" width="90" sortable>
          <template #default="{ row }">
            <span class="gpio-num">{{ row.gpio }}</span>
          </template>
        </el-table-column>
        <el-table-column prop="module" label="模块" width="110">
          <template #default="{ row }">
            <el-tag
              v-if="row.module"
              :type="moduleTagType(row.module)"
              size="small"
              effect="plain"
            >
              {{ row.module }}
            </el-tag>
            <span v-else class="text-muted">—</span>
          </template>
        </el-table-column>
        <el-table-column prop="function" label="功能 / 使用者" min-width="160">
          <template #default="{ row }">
            {{ row.function || '—' }}
          </template>
        </el-table-column>
        <el-table-column prop="status" label="状态" width="110" :filters="statusFilters" :filter-method="filterStatusInTable">
          <template #default="{ row }">
            <el-tag :type="statusTagType(row.status)" size="small">
              {{ statusLabel(row.status) }}
            </el-tag>
          </template>
        </el-table-column>
        <el-table-column prop="description" label="说明" min-width="220">
          <template #default="{ row }">
            <span :class="{ 'text-muted': !row.description }">{{ row.description || '—' }}</span>
          </template>
        </el-table-column>
      </el-table>
    </el-card>
  </div>
</template>

<script setup>
import { ref, computed } from 'vue'

// 引脚状态定义
const STATUS = {
  USED: 'used',       // 已使用
  FREE: 'free',       // 空闲
  RESERVED: 'reserved', // 预留
  CONFLICT: 'conflict'  // 冲突
}

// ESP32-S3 GPIO 引脚分配表（共49个引脚：GPIO0 ~ GPIO48）
// 数据来源：hw_gpio.h, hw_audio.h, video.c 实际配置
const pins = [
  { gpio: 0,  module: '',       function: '',           status: STATUS.FREE,     description: 'Boot strapping pin, 避免使用' },
  { gpio: 1,  module: '执行器',  function: '电机 PWMA',   status: STATUS.USED,     description: 'LEDC PWM' },
  { gpio: 2,  module: '执行器',  function: '电机 AIN1',   status: STATUS.USED,     description: '方向控制' },
  { gpio: 3,  module: '传感器',  function: 'ADC1_CH2',   status: STATUS.USED,     description: '热敏电阻/光敏电阻共用' },
  { gpio: 4,  module: '摄像头',  function: 'SCCB SDA',   status: STATUS.USED,     description: 'I2C数据' },
  { gpio: 5,  module: '摄像头',  function: 'SCCB SCL',   status: STATUS.USED,     description: 'I2C时钟' },
  { gpio: 6,  module: '摄像头',  function: 'VSYNC',      status: STATUS.USED,     description: '帧同步' },
  { gpio: 7,  module: '摄像头',  function: 'HREF',       status: STATUS.USED,     description: '行同步' },
  { gpio: 8,  module: '摄像头',  function: 'D2',         status: STATUS.USED,     description: 'DVP数据线' },
  { gpio: 9,  module: '摄像头',  function: 'D1',         status: STATUS.USED,     description: 'DVP数据线' },
  { gpio: 10, module: '摄像头',  function: 'D3',         status: STATUS.USED,     description: 'DVP数据线' },
  { gpio: 11, module: '摄像头',  function: 'D0',         status: STATUS.USED,     description: 'DVP数据线' },
  { gpio: 12, module: '摄像头',  function: 'D4',         status: STATUS.USED,     description: 'DVP数据线' },
  { gpio: 13, module: '摄像头',  function: 'PCLK',       status: STATUS.USED,     description: '像素时钟' },
  { gpio: 14, module: '音频',    function: 'I2S BCLK',   status: STATUS.USED,     description: '位时钟' },
  { gpio: 15, module: '摄像头',  function: 'XCLK',       status: STATUS.USED,     description: '外部时钟 15MHz' },
  { gpio: 16, module: '摄像头',  function: 'D7',         status: STATUS.USED,     description: 'DVP数据线' },
  { gpio: 17, module: '摄像头',  function: 'D6',         status: STATUS.USED,     description: 'DVP数据线' },
  { gpio: 18, module: '摄像头',  function: 'D5',         status: STATUS.USED,     description: 'DVP数据线' },
  { gpio: 19, module: '音频',    function: 'I2S DIN',    status: STATUS.USED,     description: '数据输入' },
  { gpio: 20, module: '音频',    function: 'I2S WS',     status: STATUS.USED,     description: '字选择/LRC' },
  { gpio: 21, module: '音频',    function: 'GAIN',       status: STATUS.USED,     description: '增益控制' },
  { gpio: 22, module: 'SPI Flash', function: '内部 Flash', status: STATUS.RESERVED, description: 'SPI0/1 Flash, 不可用' },
  { gpio: 23, module: 'SPI Flash', function: '内部 Flash', status: STATUS.RESERVED, description: 'SPI0/1 Flash, 不可用' },
  { gpio: 24, module: 'SPI Flash', function: '内部 Flash', status: STATUS.RESERVED, description: 'SPI0/1 Flash, 不可用' },
  { gpio: 25, module: 'SPI Flash', function: '内部 Flash', status: STATUS.RESERVED, description: 'SPI0/1 Flash, 不可用' },
  { gpio: 26, module: 'PSRAM',   function: 'Octal PSRAM CS',  status: STATUS.RESERVED, description: '片选' },
  { gpio: 27, module: 'PSRAM',   function: 'Octal PSRAM CLK', status: STATUS.RESERVED, description: '时钟' },
  { gpio: 28, module: 'PSRAM',   function: 'Octal PSRAM DQ0', status: STATUS.RESERVED, description: '' },
  { gpio: 29, module: 'PSRAM',   function: 'Octal PSRAM DQ1', status: STATUS.RESERVED, description: '' },
  { gpio: 30, module: 'PSRAM',   function: 'Octal PSRAM DQ2', status: STATUS.RESERVED, description: '' },
  { gpio: 31, module: 'PSRAM',   function: 'Octal PSRAM DQ3', status: STATUS.RESERVED, description: '' },
  { gpio: 32, module: 'PSRAM',   function: 'Octal PSRAM DQ4', status: STATUS.RESERVED, description: '' },
  { gpio: 33, module: '传感器',  function: 'DHT11',      status: STATUS.USED,     description: '单总线温湿度' },
  { gpio: 34, module: 'SPI',     function: 'CS',         status: STATUS.RESERVED, description: '片选, 未连接' },
  { gpio: 35, module: 'SPI',     function: 'MOSI',       status: STATUS.RESERVED, description: '未连接' },
  { gpio: 36, module: 'SPI',     function: 'CLK',        status: STATUS.RESERVED, description: '未连接' },
  { gpio: 37, module: 'SPI',     function: 'MISO',       status: STATUS.RESERVED, description: '未连接' },
  { gpio: 38, module: 'TF卡',    function: 'SD CMD',     status: STATUS.USED,     description: '命令线' },
  { gpio: 39, module: 'TF卡',    function: 'SD CLK',     status: STATUS.USED,     description: '时钟线' },
  { gpio: 40, module: 'TF卡',    function: 'SD D0',      status: STATUS.USED,     description: '数据线' },
  { gpio: 41, module: 'I2C',     function: 'SCL',        status: STATUS.RESERVED, description: '未连接' },
  { gpio: 42, module: '执行器',  function: '电机 AIN2',   status: STATUS.USED,     description: '方向控制' },
  { gpio: 43, module: '系统',    function: 'UART0 TX',   status: STATUS.USED,     description: '控制台' },
  { gpio: 44, module: '系统',    function: 'UART0 RX',   status: STATUS.USED,     description: '控制台' },
  { gpio: 45, module: '',       function: '',            status: STATUS.FREE,     description: 'Boot strapping pin, 避免使用' },
  { gpio: 46, module: '',       function: '',            status: STATUS.FREE,     description: 'Boot strapping pin, 避免使用' },
  { gpio: 47, module: '音频',    function: 'SD',         status: STATUS.USED,     description: '关断控制' },
  { gpio: 48, module: '执行器',  function: '舵机 SG90',   status: STATUS.USED,     description: 'LEDC PWM' }
]

// 状态筛选
const filterStatus = ref('')

// 按状态筛选后的引脚列表
const filteredPins = computed(() => {
  if (!filterStatus.value) return pins
  return pins.filter(p => p.status === filterStatus.value)
})

// 统计数据
const stats = computed(() => ({
  total: pins.length,
  used: pins.filter(p => p.status === STATUS.USED).length,
  free: pins.filter(p => p.status === STATUS.FREE).length,
  reserved: pins.filter(p => p.status === STATUS.RESERVED).length
}))

// 状态对应标签文案
const statusLabel = (status) => ({
  used: '已使用',
  free: '空闲',
  reserved: '预留',
  conflict: '冲突'
}[status] || '—')

// 状态对应 el-tag 类型
const statusTagType = (status) => ({
  used: 'success',      // 绿
  free: 'info',         // 灰
  reserved: 'warning',  // 黄
  conflict: 'danger'    // 红
}[status] || 'info')

// 模块对应 el-tag 类型（区分不同模块）
const moduleTagType = (module) => ({
  '执行器': 'primary',
  '传感器': 'success',
  '摄像头': 'warning',
  '音频': 'danger',
  'PSRAM': 'info',
  'SPI': 'info',
  'SPI Flash': 'info',
  'TF卡': 'primary',
  'I2C': 'info',
  '系统': ''
}[module] || 'info')

// 表头过滤选项
const statusFilters = [
  { text: '已使用', value: 'used' },
  { text: '空闲', value: 'free' },
  { text: '预留', value: 'reserved' }
]

const filterStatusInTable = (value, row) => row.status === value
</script>

<style scoped>
.board-view {
  max-width: 1000px;
  margin: 0 auto;
}

.stat-row {
  margin-bottom: 20px;
}

.stat-card {
  text-align: center;
  padding: 10px 0;
}

.stat-card :deep(.el-card__body) {
  padding: 18px 10px;
}

.stat-value {
  font-size: 32px;
  font-weight: bold;
  line-height: 1.2;
}

.stat-label {
  margin-top: 6px;
  font-size: 13px;
  color: #909399;
}

.stat-total .stat-value { color: #303133; }
.stat-used .stat-value { color: #67c23a; }
.stat-free .stat-value { color: #909399; }
.stat-reserved .stat-value { color: #e6a23c; }

.pin-card {
  margin-bottom: 20px;
}

.card-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
}

.gpio-num {
  font-weight: bold;
  color: #303133;
}

.text-muted {
  color: #c0c4cc;
}
</style>
