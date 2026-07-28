# WebReplay · 决策回放器

把一份 `DecisionLog_*.json` 变成一屏能讲清
**「LLM 想改什么 → 四道护栏拦没拦 → 实际改了什么」** 的静态页面。

纯前端、可离线、可拖拽载入。范围决策见 [`Docs/DECISIONS.md`](../Docs/DECISIONS.md) **D-21**。

```powershell
npm install
npm run dev     # 开发
npm test        # Vitest（46 个）
npm run build   # vue-tsc 类型检查 + 生产构建
```

## 进度

| 里程碑 | 内容 | 状态 |
|---|---|---|
| M0 | 类型契约 + `parseDecisionLog()` + 内置样例 | ✅ |
| M1 | 概览统计 + 层时间轴 + 点击切层 | ✅ |
| **M2** | **护栏灯带 + `rawIntent`/`decision` 左右并排** | 待做（**卖点，不可砍**） |
| M3 | 五维雷达图（手写 SVG）+ 约束列 + 白泽列 | 待做 |
| M4 | 拖拽载入 + 空态/错误态 + 响应式 | 待做 |
| M5 | GitHub Pages 部署 | 待做 |

## 几条不打算改的决定

**不引 UI 框架、状态管理库、图表库。** 一个页面不需要它们。雷达图手写 SVG 约 40 行，
为一个五边形引 ~300KB 的 ECharts 不划算。判据是「专用件的复杂度 vs 需求的复杂度」。

**`src/types/decisionLog.ts` 是 C++ 头文件的镜像，不是独立契约。**
字段名与取值域的唯一真源是
[`SHMDecisionLogFormat.h`](../UnrealProject/Source/ShanHaiMirror/Director/SHMDecisionLogFormat.h)，
改那边必须同步改这边。这是 D-21 承认的代价，注释能缓解但消不掉。

**内置样例直接 import `../Docs/samples/`，不放副本。** 样例是 UE 侧导出的产物，
复制进来等于给同一份数据造第二个真源——正是把前端放进同一个仓库想避免的事。
代价是本项目不能脱离仓库单独构建（靠 `vite.config.ts` 的 `server.fs.allow` 放行）。

**前端对 JSON 同样不该信任。** 这不是防御性编程的口号，是把 UE 侧"不信任 LLM 输出"
的态度延伸到这一层。日志可能来自旧版本、被手改过、或者压根是别的文件。两条规矩：
一次收集全部问题而不是抛第一个就停；能降级就不报错，只有"根本不是这个格式"才致命。

**数据的出处必须在页面上可见。** JSON 里的 `_note` 渲染不出来，而这个页面把每份数据
都摆成「本局概览 / runId / 层数」的样子，等于在暗示它是一局游戏。
夹具被当成对局记录是这里最容易犯的诚信错误，所以有出处横幅，`fixture` 用警示色。
见 [`Docs/踩坑记录.md`](../Docs/踩坑记录.md) #23。
