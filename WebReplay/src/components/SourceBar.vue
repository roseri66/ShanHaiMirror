<script setup lang="ts">
// 来源选择：内置样例下拉 + 拖拽/选择本地文件。
//
// 拖拽区**不是唯一入口**——手机和触屏没法拖，所以同一块区域也是一个 <label>，
// 点它就打开文件选择框。只做拖拽等于把移动端用户挡在门外。
import { ref } from 'vue'
import { BUILTIN_SAMPLES } from '../samples'

defineProps<{ sampleId: string; loadedName: string | null }>()
const emit = defineEmits<{
  'update:sampleId': [id: string]
  file: [file: File]
}>()

const dragging = ref(0) // 计数而非布尔：子元素的 dragleave 会误清标志

function onDrop(e: DragEvent) {
  dragging.value = 0
  const file = e.dataTransfer?.files?.[0]
  if (file) emit('file', file)
}

function onPick(e: Event) {
  const input = e.target as HTMLInputElement
  const file = input.files?.[0]
  if (file) emit('file', file)
  // 清空，否则连着选同一个文件不会再触发 change
  input.value = ''
}
</script>

<template>
  <div class="sourceBar">
    <div class="pickSample">
      <label for="sample">内置样例</label>
      <select
        id="sample"
        :value="sampleId"
        @change="emit('update:sampleId', ($event.target as HTMLSelectElement).value)"
      >
        <option v-for="s in BUILTIN_SAMPLES" :key="s.id" :value="s.id">{{ s.label }}</option>
      </select>
    </div>

    <label
      class="drop"
      :class="{ over: dragging > 0 }"
      @dragenter.prevent="dragging++"
      @dragover.prevent
      @dragleave.prevent="dragging--"
      @drop.prevent="onDrop"
    >
      <input type="file" accept=".json,application/json" @change="onPick" />
      <span v-if="loadedName" class="loaded">
        已载入 <b>{{ loadedName }}</b> · 点此或拖入另一份
      </span>
      <span v-else>把自己的 <code>DecisionLog_*.json</code> 拖到这里，或点击选择</span>
    </label>
  </div>
</template>

<style scoped>
.sourceBar {
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: 0.6rem 0.9rem;
  margin-bottom: 0.9rem;
  font-size: 0.85rem;
}

.pickSample {
  display: flex;
  align-items: center;
  gap: 0.4rem;
}
.pickSample label {
  color: var(--text-dim);
  font-size: 0.8rem;
}
select {
  font: inherit;
  font-size: 0.85rem;
  padding: 0.25rem 0.5rem;
  border-radius: 0.3rem;
  border: 1px solid var(--border-strong);
  background: var(--bg-panel);
  color: var(--text-h);
  max-width: 100%;
}

.drop {
  flex: 1 1 16rem;
  min-width: 0;
  display: flex;
  align-items: center;
  gap: 0.4rem;
  padding: 0.45rem 0.7rem;
  border: 1px dashed var(--border-strong);
  border-radius: 0.4rem;
  color: var(--text-dim);
  font-size: 0.78rem;
  cursor: pointer;
  transition: border-color 0.15s, background 0.15s;
}
.drop:hover,
.drop:focus-within {
  border-color: var(--text-dim);
}
.drop.over {
  border-color: var(--slot-1);
  background: color-mix(in srgb, var(--slot-1) 10%, transparent);
  color: var(--text-h);
}
.drop input {
  position: absolute;
  width: 1px;
  height: 1px;
  opacity: 0;
  pointer-events: none;
}
.loaded b {
  color: var(--text-h);
}
code {
  background: var(--bg-sunken);
  padding: 0 0.25em;
  border-radius: 0.2rem;
}
</style>
