// ============================================================================
// 载入来源 —— 把"用户拖进来的一个文件"变成解析结果
//
// 这一层的职责只有一个：**任何输入都不能让页面挂掉。**
// 用户会拖进截图、压缩包、别的项目的 JSON、没下载完的半截文件。
// 页面一挂，面试官不会去开控制台看原因，只会关掉标签页。
//
// 所以 JSON.parse 必须包住（它抛 SyntaxError），空文件要说"空"而不是"格式不对"
// （成因完全不同，说错会把人引到错误的方向去查）。
// ============================================================================

import { parseDecisionLog, type ParseResult } from './parseDecisionLog'

/**
 * 文件体积上限。真实一局的日志约 8KB，留足余量即可 ——
 * 上限的意义是防止有人拖进一个 2GB 的文件把标签页卡死。
 */
export const MAX_FILE_BYTES = 8 * 1024 * 1024

/** 只需要文件的这两个字段，定成结构类型便于测试（不依赖浏览器的 File） */
export interface FileLike {
  name: string
  type: string
}

/**
 * 拖进来的东西看着像不像 JSON。
 *
 * **无后缀、无 MIME 的一律放行** —— 让它走到解析器那里拿一句具体的报错，
 * 好过在这里以"看起来不像"为由拒绝：那句提示帮不上任何忙。
 * 这里只拦明确不是的（图片、压缩包等）。
 */
export function isProbablyJsonFile(file: FileLike): boolean {
  const name = file.name.toLowerCase()
  const type = file.type.toLowerCase()

  if (name.endsWith('.json')) return true
  if (type.includes('json')) return true

  // 有后缀但不是 json，且 MIME 明确是别的类型 → 拒绝
  const hasExtension = /\.[a-z0-9]+$/.test(name)
  if (hasExtension) return false
  if (type && !type.startsWith('text/')) return false

  return true
}

/**
 * 文本 → 解析结果。
 * 与 `parseDecisionLog` 的区别：这里多担一层"文本可能根本不是 JSON"的责任。
 */
export function loadFromText(text: string): ParseResult {
  // UE 导出的是 UTF-8 无 BOM，但用户可能用记事本另存过。
  // JSON.parse 遇到 BOM 直接抛错，而这不是用户的错。
  const cleaned = text.replace(/^﻿/, '')

  if (cleaned.trim() === '') {
    return {
      ok: false,
      errors: ['文件是空的 —— 是不是拖错了文件，或者下载没完成？'],
      warnings: [],
    }
  }

  let raw: unknown
  try {
    raw = JSON.parse(cleaned)
  } catch (e) {
    const detail = e instanceof Error ? e.message : String(e)
    return {
      ok: false,
      errors: [
        `这个文件不是合法的 JSON，无法解析。浏览器报告：${detail}`,
        '决策日志应当是 SHM.ExportDecisionLog 导出的 .json 文件，或一局结束后自动写到 Saved/DecisionLogs/ 的那一份。',
      ],
      warnings: [],
    }
  }

  return parseDecisionLog(raw)
}

/** 读一个浏览器 File 并解析。体积与类型在这里一并挡掉。 */
export async function loadFromFile(file: File): Promise<ParseResult> {
  if (file.size > MAX_FILE_BYTES) {
    return {
      ok: false,
      errors: [
        `文件有 ${(file.size / 1024 / 1024).toFixed(1)} MB，超过 ${MAX_FILE_BYTES / 1024 / 1024} MB 上限。` +
          '真实一局的决策日志通常只有几十 KB —— 这多半不是决策日志。',
      ],
      warnings: [],
    }
  }

  if (!isProbablyJsonFile(file)) {
    return {
      ok: false,
      errors: [`「${file.name}」看起来不是 JSON 文件。请拖入 SHM.ExportDecisionLog 导出的 .json。`],
      warnings: [],
    }
  }

  try {
    return loadFromText(await file.text())
  } catch (e) {
    // 读取本身失败（权限、文件被删、编码异常）——同样不能让异常冒到界面
    const detail = e instanceof Error ? e.message : String(e)
    return { ok: false, errors: [`读取文件失败：${detail}`], warnings: [] }
  }
}
