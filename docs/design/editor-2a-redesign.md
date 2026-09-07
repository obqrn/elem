# 2a 纯文本块编辑器 — 重新设计（v2）

状态：已定案并实现（9781e213），探针 8/8 + selftest 64 组全绿。
上一版实现：`rich-editor` 分支（517388e8），因交互问题整体返工。

## 1. 目标与范围

在阶段 1（rich_text_layout 排版内核）之上实现**可用的纯文本块编辑器**：

- 扁平块模型：paragraph / heading1-6 / list_item / quote / code_block
- 块类型仅映射字体（颜色/样式区分是 2b 的事）
- UTF-8 光标、选区、拖选
- 剪贴板（复制/剪切/粘贴）、IME 直接输入
- 块级快照 undo/redo
- 键盘：方向键、Home/End、退格（块首退格=与上一块合并）、Enter 拆块

**明确不做**（2b/3）：样式 span、颜色、markdown 导入导出、滚动（内容超出窗口时的滚动方案留到 2b 与样式排版一起设计）。

## 2. 架构

```
text_document          —— 数据：扁平 vector<text_block>，insert/erase 返回受影响块区间
text_editor_element    —— 交互：焦点/caret/选区/键盘/IME/剪贴板/增量重排
rich_text_layout       —— 排版（阶段 1，不改）
rich_editor 示例       —— 窗口结构照抄 rich_text（见约束 C1）
```

元素必须是值类型（is_base_of_v<element, T>）；排版走 canvas 抽象（measure_text/fill_text），不泄漏 cairo 类型。

## 3. 设计约束（本次返工的全部教训，违反即返工）

### C1 窗口结构：无 scroller
示例用与 rich_text 完全相同的结构：`margin → htile(vtile(editor, vstretch(empty)), hstretch(empty))`。
scroller 的 hover/click 重绘路径与编辑器交互是上一版所有症状（移入变暗、点击闪烁、幽灵光标）的来源。滚动需求在 2b 自实现。

### C2 wrap 宽度固定
编辑器 wrap 宽度 = 构造参数，**永不跟随容器分配宽**（layout(ctx) 不得改写 _width）。
上一版 layout 采用容器宽 → htile 拉伸编辑器至全窗口宽 → 文档按 884 逻辑宽重排 → 内容与 caret 全部跑出窗口（"光标没效果"根因）。

### C3 刷新区域化（点击/焦点链上禁止全窗刷新）
- click down：只刷 union(旧 caret, 新 caret, 旧选区 extent)——旧选区不清刷会残留（"多个光标"根因）
- click up：不刷
- drag：只刷 union(旧选区 extent, 新选区 extent)——点击的微小抖动走 drag 路径，不得全刷
- 焦点变化（begin_focus/end_focus/click 焦点转移）：只刷焦点元素区域
- click 放弃焦点（点空白）：只刷旧焦点元素，**不得** refresh 整个 view

### C4 块渲染缓存按 device density 建物理像素
缓存 pixmap：物理像素 = 逻辑尺寸 × device_scale（pixmap(point{sz*ds}, 1.0f)），canvas scale(ds) 绘制；
blit 用 draw(pm, src, dest) 显式映射回逻辑 rect。
上一版用 pixmap(逻辑尺寸, ds) → 字形按 1/ds 密度栅格化再放大 → 字体模糊。

### C5 焦点链协议（不破坏库语义）
- 编辑器 override wants_control() = true（否则 composite click 走 relinquish 分支，text() 事件永远到不了）
- wants_focus() = true
- 点击窗口外 → 失焦 → caret 消失，是标准行为（与记事本一致），不做"防消失"

### C6 字体与 DPI
- 示例主题字体直配 Microsoft YaHei（库 match 无字形级 fallback，逗号多族第一命中即返回，CJK 会 tofu）
- host 的坐标/scale 处理不动（app 已声明 Per-Monitor DPI aware）
- 编辑器内坐标统一 bounds.top 基准（绘制与命中一致）

### C7 探针方法论（自动化验证的唯一可信通道）
- PostMessage 发 **逻辑客户区坐标**给 GetWindow(GW_CHILD) 子窗口（PS 探针进程 DPI unaware，发物理坐标会被系统再放大 1.5）
- 渲染判定用 PrintWindow + 像素统计；"闪烁/振荡"类结论必须用屏幕 BitBlt（PrintWindow 自身有 DWM 伪影）
- 窗口启动后等 12 秒（slide 动画 <7s 未稳定）
- 真实点击用 AttachThreadInput 绕前台锁定；连续探针前 taskkill 旧 exe
- 调试日志一律 fopen 文件、edit 工具添加（python/heredoc 转义链路会写坏 `\n`）
- 用 python 改文件后必须 open(p,'wb') 转回 LF（Windows python 默认写 CRLF）

## 4. 与上一版的差异总结

| 项 | 上一版（rich-editor） | 本版 |
|---|---|---|
| 示例结构 | scroller 包装 | 与 rich_text 同构（C1） |
| wrap | 跟随容器宽 | 固定构造值（C2） |
| 块缓存 | pixmap(逻辑, ds) 低密度 | 物理像素=逻辑×ds（C4） |
| click 刷新 | 曾漏旧选区 extent | union 含旧选区（C3） |
| 空白点击 | 全窗刷新 | 只刷旧焦点元素（C3） |

## 5. 验证策略

1. `--selftest`：文档模型（插入/删除/undo/redo/UTF-8 边界）、布局查询（byte_at/x_at/caret_pos 互逆、尾随空格）、编辑操作（拆块/合并块/caret 移动/选区）
2. 探针：PostMessage 点击（逻辑坐标）→ caret 出现在正确位置；拖选→点击别处→高亮清零；打字→文本增加；连点 4 次→单 caret 零残影
3. 人工：真实鼠标点击/拖选/打字，窗口缩放

## 6. 评审问题定案

1. **块级快照 undo**（定案）：每操作快照受影响块，深度 1000，成本与文档大小无关。操作日志对扁平块模型过度设计，且重放有复合操作一致性风险。
2. **中文 IME**（定案：2b 补全）：2a 不碰 host 的 WM_IME 链（host 目前只发 WM_CHAR，改动是全局库影响面）。中文经 IME 系统候选窗 commit 后插入，可用；内联 composition（caret 下 preedit）与 2b 样式绘制一起设计。
3. **code_block 内 Enter**（定案：块内保留换行符）：非 code 块 Enter 拆块（新块继承类型）；code_block 内 Enter 插入字面 `\n` 单块多行。不变量放宽为"仅 code_block 可含 `\n`"；跨块 erase 合并时若 last 块是 code_block 结果强制 code_block；set_type 到非 code 类型时块内 `\n` 展平为空格。
4. **可视区裁剪 + 块缓存失效**（定案）：draw 按可视区裁剪（`_block_y` 二分定位）；字符编辑仅重排受影响块（段落级增量），结构性编辑（拆/合块）全量重排；块缓存失效标记跟随 relayout。

## 7. 返工中修复的上一版缺陷（实测确认）

- `insert_text` caret 落点错误（总落在受影响末块末尾；Enter 拆块后光标跑到行尾）——本版按"插入文本末尾"计算。
- 键盘链每键全窗 refresh——本版 caret 移动/编辑全部区域化（C3 扩展到键盘链）。
- **选区高亮画在块缓存 blit 之前，被不透明 pixmap 完全覆盖**（探针发现高亮从未上屏）——本版高亮在 blit 后绘制。
- caret 高度用 `_size*1.4` 近似——本版用布局行高（空块才近似）。
