<template>
  <el-card class="pin-list-card">
    <template #header>
      <div class="card-header">
        <span>管脚列表</span>
        <div class="header-actions">
          <el-input
            v-model="searchKeyword"
            placeholder="搜索GPIO/功能/模块..."
            size="small"
            clearable
            style="width: 200px"
          >
            <template #prefix>
              <el-icon><Search /></el-icon>
            </template>
          </el-input>
        </div>
      </div>
    </template>

    <!-- 筛选栏 -->
    <div class="filter-bar">
      <el-radio-group v-model="filterStatus" size="small">
        <el-radio-button label="">全部 ({{ totalCount }})</el-radio-button>
        <el-radio-button label="used">已使用 ({{ usedCount }})</el-radio-button>
        <el-radio-button label="free">空闲 ({{ freeCount }})</el-radio-button>
        <el-radio-button label="reserved">预留 ({{ reservedCount }})</el-radio-button>
        <el-radio-button label="conflict">冲突 ({{ conflictCount }})</el-radio-button>
      </el-radio-group>

      <div class="filter-modules">
        <el-select
          v-model="filterModule"
          placeholder="按模块筛选"
          size="small"
          clearable
          style="width: 140px"
        >
          <el-option
            v-for="module in moduleList"
            :key="module"
            :label="module"
            :value="module"
          />
        </el-select>
      </div>
    </div>

    <!-- 统计信息 -->
    <div class="stats-bar">
      <el-tag type="info" size="small">
        共 {{ filteredPins.length }} 个引脚
      </el-tag>
      <el-tag v-if="searchKeyword || filterStatus || filterModule" type="warning" size="small" closable @close="clearFilters">
        筛选条件已应用
      </el-tag>
    </div>

    <!-- 引脚表格 -->
    <el-table
      :data="filteredPins"
      stripe
      :default-sort="{ prop: 'gpio', order: 'ascending' }"
      style="width: 100%"
      max-height="500"
      :row-class-name="getRowClassName"
    >
      <el-table-column prop="gpio" label="GPIO" width="90" sortable fixed>
        <template #default="{ row }">
          <span class="gpio-num" :class="'gpio-' + row.status">{{ row.gpio }}</span>
        </template>
      </el-table-column>

      <el-table-column prop="type" label="类型" width="100">
        <template #default="{ row }">
          <el-tag :type="getTypeTagType(row.type)" size="small" effect="plain">
            {{ row.type || 'GPIO' }}
          </el-tag>
        </template>
      </el-table-column>

      <el-table-column prop="module" label="模块" width="110">
        <template #default="{ row }">
          <el-tag
            v-if="row.module"
            :type="getModuleTagType(row.module)"
            size="small"
            effect="plain"
          >
            {{ row.module }}
          </el-tag>
          <span v-else class="text-muted">—</span>
        </template>
      </el-table-column>

      <el-table-column prop="function" label="功能/用途" min-width="160">
        <template #default="{ row }">
          <span :class="{ 'text-warning': row.is_core }">{{ row.function || '—' }}</span>
          <el-tag v-if="row.is_core" type="danger" size="small" effect="dark" class="ml-2">
            核心
          </el-tag>
        </template>
      </el-table-column>

      <el-table-column prop="status" label="状态" width="100">
        <template #default="{ row }">
          <el-tag :type="getStatusTagType(row.status)" size="small">
            {{ getStatusLabel(row.status) }}
          </el-tag>
        </template>
      </el-table-column>

      <el-table-column prop="warning" label="注意事项" width="200">
        <template #default="{ row }">
          <span v-if="row.warning" class="warning-text">{{ row.warning }}</span>
          <span v-else class="text-muted">—</span>
        </template>
      </el-table-column>

      <el-table-column prop="description" label="说明" min-width="200">
        <template #default="{ row }">
          <span :class="{ 'text-muted': !row.description }">{{ row.description || '—' }}</span>
        </template>
      </el-table-column>
    </el-table>
  </el-card>
</template>

<script setup>
import { ref, computed } from 'vue'
import { Search } from '@element-plus/icons-vue'

const props = defineProps({
  pins: {
    type: Array,
    default: () => []
  }
})

// 筛选状态
const filterStatus = ref('')
const filterModule = ref('')
const searchKeyword = ref('')

// 状态常量
const STATUS = {
  USED: 'used',
  FREE: 'free',
  RESERVED: 'reserved',
  CONFLICT: 'conflict'
}

// 统计计算
const totalCount = computed(() => props.pins.length)
const usedCount = computed(() => props.pins.filter(p => p.status === STATUS.USED).length)
const freeCount = computed(() => props.pins.filter(p => p.status === STATUS.FREE).length)
const reservedCount = computed(() => props.pins.filter(p => p.status === STATUS.RESERVED).length)
const conflictCount = computed(() => props.pins.filter(p => p.status === STATUS.CONFLICT).length)

// 模块列表
const moduleList = computed(() => {
  const modules = [...new Set(props.pins.map(p => p.module).filter(Boolean))]
  return modules.sort()
})

// 筛选后的引脚列表
const filteredPins = computed(() => {
  return props.pins.filter(pin => {
    // 状态筛选
    if (filterStatus.value && pin.status !== filterStatus.value) {
      return false
    }
    // 模块筛选
    if (filterModule.value && pin.module !== filterModule.value) {
      return false
    }
    // 关键字搜索
    if (searchKeyword.value) {
      const keyword = searchKeyword.value.toLowerCase()
      const match = (val) => val && val.toLowerCase().includes(keyword)
      if (!match(pin.gpio?.toString()) &&
          !match(pin.function) &&
          !match(pin.module) &&
          !match(pin.description)) {
        return false
      }
    }
    return true
  })
})

// 清除筛选
const clearFilters = () => {
  filterStatus.value = ''
  filterModule.value = ''
  searchKeyword.value = ''
}

// 获取状态标签文案
const getStatusLabel = (status) => ({
  used: '已使用',
  free: '空闲',
  reserved: '预留',
  conflict: '冲突'
}[status] || '—')

// 获取状态标签类型
const getStatusTagType = (status) => ({
  used: 'success',
  free: 'info',
  reserved: 'warning',
  conflict: 'danger'
}[status] || 'info')

// 获取类型标签类型
const getTypeTagType = (type) => ({
  'RTC': 'warning',
  'Analog': 'success',
  'Core': 'danger'
}[type] || 'info')

// 获取模块标签类型
const getModuleTagType = (module) => ({
  '执行器': 'primary',
  '传感器': 'success',
  '摄像头': 'warning',
  '音频': 'danger',
  'PSRAM': 'info',
  'SPI Flash': 'info',
  'SPI': 'info',
  'TF卡': 'primary',
  'I2C': '',
  '系统': 'warning',
  'Core': 'danger'
}[module] || 'info')

// 获取行样式类名
const getRowClassName = ({ row }) => {
  if (row.is_core) return 'core-row'
  if (row.status === STATUS.CONFLICT) return 'conflict-row'
  return ''
}
</script>

<style scoped>
.pin-list-card {
  margin-bottom: 20px;
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.header-actions {
  display: flex;
  gap: 12px;
}

.filter-bar {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 12px;
  flex-wrap: wrap;
  gap: 12px;
}

.filter-modules {
  display: flex;
  gap: 8px;
}

.stats-bar {
  display: flex;
  gap: 8px;
  margin-bottom: 12px;
}

.gpio-num {
  font-weight: bold;
  padding: 2px 8px;
  border-radius: 4px;
}

.gpio-used { color: #67c23a; background: #f0f9eb; }
.gpio-free { color: #909399; background: #f4f4f5; }
.gpio-reserved { color: #e6a23c; background: #fdf6ec; }
.gpio-conflict { color: #f56c6c; background: #fef0f0; }

.text-muted { color: #c0c4cc; }
.text-warning { color: #e6a23c; font-weight: 500; }

.warning-text {
  color: #f56c6c;
  font-size: 12px;
}

.ml-2 { margin-left: 8px; }

:deep(.core-row) {
  background-color: #fef0f0 !important;
}

:deep(.conflict-row) {
  background-color: #fef0f0 !important;
}
</style>
