<template>
  <el-card class="params-card">
    <template #header>
      <div class="card-header">
        <span>摄像头参数</span>
        <el-button
          size="small"
          type="primary"
          plain
          @click="emit('applyParams')"
          :disabled="!paramsChanged"
          :loading="actionLoading === 'config'"
        >
          <el-icon><Check /></el-icon>
          应用修改
        </el-button>
      </div>
    </template>

    <el-row :gutter="24">
      <!-- 基础参数 -->
      <el-col :xs="24" :md="12">
        <div class="param-group">
          <div class="param-group-title">图像调整</div>

          <div class="param-item">
            <div class="param-label-row">
              <label>亮度</label>
              <span class="param-value">{{ params.brightness }}</span>
            </div>
            <el-slider
              :model-value="params.brightness"
              :min="-2"
              :max="2"
              :step="1"
              show-stops
              @update:model-value="onUpdate('brightness', $event)"
            />
          </div>

          <div class="param-item">
            <div class="param-label-row">
              <label>对比度</label>
              <span class="param-value">{{ params.contrast }}</span>
            </div>
            <el-slider
              :model-value="params.contrast"
              :min="-2"
              :max="2"
              :step="1"
              show-stops
              @update:model-value="onUpdate('contrast', $event)"
            />
          </div>

          <div class="param-item">
            <div class="param-label-row">
              <label>饱和度</label>
              <span class="param-value">{{ params.saturation }}</span>
            </div>
            <el-slider
              :model-value="params.saturation"
              :min="-2"
              :max="2"
              :step="1"
              show-stops
              @update:model-value="onUpdate('saturation', $event)"
            />
          </div>
        </div>
      </el-col>

      <!-- 翻转与分辨率 -->
      <el-col :xs="24" :md="12">
        <div class="param-group">
          <div class="param-group-title">画面设置</div>

          <div class="param-item row-item">
            <label>水平镜像</label>
            <el-switch
              :model-value="params.hmirror"
              @update:model-value="onUpdate('hmirror', $event)"
            />
          </div>

          <div class="param-item row-item">
            <label>垂直翻转</label>
            <el-switch
              :model-value="params.vflip"
              @update:model-value="onUpdate('vflip', $event)"
            />
          </div>

          <div class="param-item">
            <div class="param-label-row">
              <label>分辨率</label>
              <span class="param-value hint">更改后需重新获取画面</span>
            </div>
            <el-select
              :model-value="params.framesize"
              placeholder="选择分辨率"
              style="width: 100%"
              @update:model-value="onUpdate('framesize', $event)"
            >
              <el-option
                v-for="fs in framesizeList"
                :key="fs.id"
                :label="fs.name"
                :value="fs.id"
              />
            </el-select>
          </div>

          <div class="param-item">
            <div class="param-label-row">
              <label>JPEG 质量</label>
              <span class="param-value hint">越小越清晰 ({{ params.quality }}/63)</span>
            </div>
            <el-slider
              :model-value="params.quality"
              :min="4"
              :max="40"
              :step="1"
              @update:model-value="onUpdate('quality', $event)"
            />
          </div>
        </div>
      </el-col>
    </el-row>
  </el-card>
</template>

<script setup>
import { Check } from '@element-plus/icons-vue'

defineProps({
  // 用户可编辑参数
  params: { type: Object, required: true },
  // 可选分辨率列表
  framesizeList: { type: Array, default: () => [] },
  // 是否存在未应用的参数修改
  paramsChanged: { type: Boolean, default: false },
  // 当前操作加载态: 'start' | 'stop' | 'config' | ''
  actionLoading: { type: String, default: '' }
})

const emit = defineEmits(['updateParam', 'applyParams'])

// 参数变更：通知父组件更新对应字段并标记已修改
const onUpdate = (key, value) => {
  emit('updateParam', { key, value })
}
</script>

<style scoped>
.params-card {
  margin-bottom: 20px;
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 8px;
}

/* ---------- 参数分组 ---------- */
.param-group {
  padding: 8px 4px;
}

.param-group-title {
  font-size: 13px;
  font-weight: 600;
  color: #606266;
  margin-bottom: 12px;
  padding-left: 6px;
  border-left: 3px solid #409eff;
}

.param-item {
  margin-bottom: 20px;
}

.param-item:last-child {
  margin-bottom: 0;
}

.param-label-row {
  display: flex;
  justify-content: space-between;
  align-items: baseline;
  margin-bottom: 4px;
}

.param-label-row label {
  font-size: 14px;
  color: #303133;
}

.param-value {
  font-size: 13px;
  font-weight: 600;
  color: #409eff;
}

.param-value.hint {
  color: #909399;
  font-weight: normal;
  font-size: 12px;
}

.param-item.row-item {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 4px 0;
}

.param-item.row-item label {
  font-size: 14px;
  color: #303133;
}
</style>
