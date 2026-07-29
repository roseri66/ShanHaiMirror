// ============================================================================
// 载入来源的测试（M4：拖拽 / 选择文件）
//
// 这一层要挡住的是**用户随手拖进来的任何东西**：截图、压缩包、别的项目的 JSON、
// 半截下载完的文件。任何一种都不能让页面白屏或抛异常到控制台 ——
// 页面挂了，面试官不会去看控制台，只会关掉标签页。
// ============================================================================

import { describe, expect, it } from 'vitest'
import { loadFromText, MAX_FILE_BYTES, isProbablyJsonFile } from './loadSource'
import greenSample from '../../../Docs/samples/DecisionLog_Sample.json'

describe('loadFromText · 文本 → 解析结果', () => {
  it('合法日志 → 解析成功', () => {
    const r = loadFromText(JSON.stringify(greenSample))
    expect(r.ok).toBe(true)
    if (!r.ok) return
    expect(r.run.floors).toHaveLength(3)
  })

  it('不是 JSON → 可读报错，不抛异常', () => {
    // JSON.parse 会抛 SyntaxError。让它冒出去就是白屏。
    const r = loadFromText('这不是 JSON')
    expect(r.ok).toBe(false)
    if (r.ok) return
    expect(r.errors[0]).toContain('不是合法的 JSON')
  })

  it('截断的 JSON → 同样安全失败', () => {
    const r = loadFromText('{"schemaVersion": 1, "floors": [')
    expect(r.ok).toBe(false)
  })

  it('空文件 / 全空白 → 明确说是空的，不说"格式不对"', () => {
    // 这两种情况的成因完全不同：空文件多半是拖错了或没下载完，
    // 说成"格式不对"会把人引到错误的方向去查
    for (const text of ['', '   \n\t ']) {
      const r = loadFromText(text)
      expect(r.ok).toBe(false)
      if (r.ok) return
      expect(r.errors[0]).toContain('空')
    }
  })

  it('合法 JSON 但不是决策日志 → 落到版本闸门的报错', () => {
    const r = loadFromText('{"hello":"world"}')
    expect(r.ok).toBe(false)
    if (r.ok) return
    expect(r.errors.join()).toContain('schemaVersion')
  })

  it('顶层是数组的合法 JSON 也不崩', () => {
    expect(loadFromText('[1,2,3]').ok).toBe(false)
  })

  it('带 BOM 的文件能正常解析', () => {
    // UE 导出的是 UTF-8 无 BOM，但用户可能用记事本另存过，那会加上 BOM。
    // JSON.parse 遇到 BOM 会直接抛错，而这不是用户的错。
    const r = loadFromText('﻿' + JSON.stringify(greenSample))
    expect(r.ok).toBe(true)
  })
})

describe('isProbablyJsonFile · 拖进来之前先看一眼', () => {
  it('认 .json 后缀与 application/json', () => {
    expect(isProbablyJsonFile({ name: 'Run_20260729.json', type: '' })).toBe(true)
    expect(isProbablyJsonFile({ name: 'log', type: 'application/json' })).toBe(true)
    expect(isProbablyJsonFile({ name: 'A.JSON', type: '' })).toBe(true)
  })

  it('图片、压缩包一律拒绝', () => {
    expect(isProbablyJsonFile({ name: 'screenshot.png', type: 'image/png' })).toBe(false)
    expect(isProbablyJsonFile({ name: 'logs.zip', type: 'application/zip' })).toBe(false)
  })

  it('无后缀无 MIME 的文件放行，交给解析器判断', () => {
    // 宁可让它走到解析器那里拿到一句具体的报错，
    // 也好过在这里就以"看起来不像"为由拒绝——那句提示帮不上任何忙
    expect(isProbablyJsonFile({ name: 'decisionlog', type: '' })).toBe(true)
  })
})

describe('体积上限', () => {
  it('上限是个具体数字，且足够装下真实日志', () => {
    // 真实一局的日志约 8KB；上限留足余量但不至于让浏览器卡死
    expect(MAX_FILE_BYTES).toBeGreaterThan(1_000_000)
    expect(MAX_FILE_BYTES).toBeLessThanOrEqual(20_000_000)
  })
})
