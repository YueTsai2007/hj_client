# 作战视图 v3 · 验收记录

> 规格：`docs/ui/combat-view-v3-spec.md`　基准稿：`docs/ui/combat-view-v3.png`
> 实现：`client/combat_view.{h,cpp}` + `client/window.cpp::buildCombatView/layoutCombatHud/updateCombatHud`

## 实现方式

- `CombatView`：16:9 letterbox 舞台，`resizeEvent` 里算内容区矩形，画面层（QStackedWidget：VideoView / Map）与 HUD 层同尺寸、居中，留边填 `#05080d`。
- HUD：`HudCard`（自绘半透明圆角卡）+ 绝对定位；`scale = 内容区宽 / 1920`，卡片 geometry、圆角、字号、内边距整组缩放；只在 resize 时重算。
- 数据：`updateCombatHud()` 从既有 `records_` 取 GameStatus / GlobalUnitStatus / RobotDynamicStatus / RobotRespawnStatus / RadarInfoToClient，不改协议层。
- F2：作战视图内切图传/地图；数据视图内仍是原来的“切换地图/图传”。
- F3：作战视图 ↔ 数据视图；数据视图即原有 `workspace_` 分屏，未改动行为。
- HUD 层 `WA_TransparentForMouseEvents`，不挡画面。
- 附加：`HJ_WIN_W/H` 指定初始窗口尺寸、`HJ_SNAPSHOT` 3 秒后截图退出、`HJ_START_DATA_VIEW=1` 直接进数据视图（均为验收辅助，不影响正常使用）。

## 验收对照

| # | 项 | 结果 | 截图 |
|---|---|---|---|
| 1 | 1920×1080 与基准稿对齐 | 各卡片按规格坐标 ×scale 定位（四舍五入误差 ≤1px）；布局一致 | `1920x1080.png` |
| 2 | 1366×768 不重叠、可读 | 通过；计时卡内文字略紧（已知项） | `1366x768.png` |
| 3 | 2560×1080 左右留边、HUD 尺寸不变 | 通过：内容 1920 居中，两侧黑边，HUD 按 1920 基准不放大 | `2560x1080.png` |
| 4 | 事件条 1020×46、不撑满 | 通过 | 各图 |
| 5 | F2 切图传/地图 HUD 不丢 | 通过（同一 HUD 层，切页不动布局） | — |
| 6 | F3 数据视图不回归 | 通过：分屏、列表、消息详情原样 | `dataview-1920x1080.png` |
| 7 | 断线/无数据 | HUD 显示 `--` / 灰化，不崩溃（演示中断服务端验证） | — |

## 已知偏差 / 待办

1. 准星未实现（规格标注为可选）。
2. 小地图复用现有 `Map`，保留“模拟坐标”标注与当前画风，未按设计稿重绘。
3. 1366×768 下计时卡（260×100）内部时间与“比赛中·第几局”偏紧，后续可把计时卡基准字号 56→48 或单独允许该卡小比例放大。
4. 卡片内部控件的固定最小高度（进度条 5px）在小比例下已做缩放，但个别间距仍是近似值。
5. HUD 未做拖拽/自定义（规格明确不做）。
