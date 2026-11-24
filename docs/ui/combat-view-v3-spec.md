# 作战视图 v3 · 实现规格

> 设计稿：`docs/ui/combat-view-v3.png`（1920×1080 基准），HTML 源：`docs/ui/combat-view-v3.html`
> 工作区：`D:\cpptaskpath\HJ_client`
> 目标：主视图（图传 / 地图）按 16:9 独占画面，HUD 以固定尺寸浮在四角；窗口尺寸变化只缩放画面与整组 HUD，不改变 HUD 的相对布局。

---

## 1. 布局模型：三层

| 层 | 内容 | 是否参与缩放 |
|---|---|---|
| 画面层 | 图传 / 战场地图（二选一） | 是。按 16:9 等比缩放、居中 |
| HUD 层 | 基地血、计时、前哨站、链路、本机、小地图、事件条 | 整组等比缩放，**不重排** |
| 背景层 | letterbox 留边、准星 | 否。留边填纯色 `#05080d` |

关键约束：

- 画面层是唯一的比例基准。窗口比例 ≠ 16:9 时，画面等比缩放并居中，两侧/上下留边，**不拉伸、不裁切**。
- HUD 锚定的是**画面区域的四角**，不是窗口四角；letterbox 多出来的空间不分配给任何 HUD。
- 事件条宽度固定，不撑满画面；右侧留白是设计意图。

## 2. 基准尺寸表（1920×1080 画面基准）

| 组件 | 尺寸 | 锚点 / 位置 | 备注 |
|---|---|---|---|
| 己方基地卡 | 320×96 | 左上 (36, 32) | 红方色 |
| 敌方基地卡 | 320×96 | 右上 (36, 32) | 蓝方色 |
| 计时卡 | 260×100 | 顶部居中，top 28 | 大字 + 状态 + 比分小字 |
| 前哨站卡 | 320×64 | 顶部居中，top 142 | 两个 cell，竖分隔线 |
| 链路卡 | 320×36 | 右上，top 140 | 在敌方基地卡下方 |
| 本机卡 | 430×100 | 左下 (36, 36) | 血量 + 弹量 + 热量 + 复活 |
| 小地图 | 300×256 | 右下 (36, 36) | 含标题、图例 |
| 事件条 | 1020×46 | 左下起 x=490，bottom 36 | 内容左对齐，不撑满 |
| 准星 | 120×120 | 画面中心 | 半透明，可选 |
| 圆角 | 12 | HUD 卡 | 按 scale 缩放 |
| 边距 | 36 | HUD 与画面边缘 | 按 scale 缩放 |

## 3. 缩放规则

设画面区域宽高为 `W × H`（已按 16:9 letterbox 后的内容区）：

```
scale = W / 1920          // 等价于 H / 1080
```

- 所有 HUD 的尺寸、位置、圆角、字号、内边距都乘以 `scale`。
- 字号用 `QFont::setPixelSize(round(base * scale))`，不要用点数（避免 DPI 二次缩放）。
- 最小可读字号 10px（逻辑像素）；`scale < 0.6` 时允许 HUD 相对尺寸略放大（保持可读），但要保证四角不重叠。
- 只在 `resizeEvent` 里重算，不要在每帧/每次数据更新时重排。

## 4. 视觉规格

- 卡片背景：`rgba(8,13,20,0.74)`，1px 边框 `rgba(148,176,205,0.2)`，圆角 12×scale，`backdrop-filter` 不可用（Qt 无原生毛玻璃，可用半透明纯色）。
- 主色：金 `#e8a33d` / 金亮 `#ffc971`；红方 `#ff5a67`；蓝方 `#3ea8ff`。
- 正文 `#eaf1f7`，次要文字 `#8da0b3`，弱化 `#6d7885`。
- 字体：`Microsoft YaHei` / `Noto Sans CJK SC`，字重 700/800 用于数字。
- 数字统一 `tabular-nums`（Qt 用等宽数字字体或 `QFont::setStyleHint` 即可，不强制）。

## 5. 组件内容与数据源（沿用现有解析，不新增协议字段）

| 组件 | 内容 | 现有数据源 |
|---|---|---|
| 基地卡 | 数值 + 比例条 + `HP` | `GlobalUnitStatus.baseHealth / enemyBaseHealth`（按 ownSide 翻转） |
| 计时卡 | 剩余时间 + 比赛阶段 + 比分小字 | `GameStatus` |
| 前哨站 | 己方/敌方前哨血量 | `GlobalUnitStatus.outpost_R_health / outpost_B_health`（按 ownSide） |
| 链路卡 | 已连接 · 图传 fps · MQTT 频率 | 现有 `connection_ / dataStatus_ / videoLink_` 聚合 |
| 本机卡 | 血量/上限 + 条 + 弹量 + 热量 + 复活 | `RobotDynamicStatus` + `RobotRespawnStatus` |
| 小地图 | 雷达/机器人点 + 图例 + 雷达延迟 | `RadarInfoToClient` + `RobotPosition` |
| 事件条 | 告警 + 判罚 + 最近事件 | `Event` / `PenaltyInfo` / `updateAlert` 既有逻辑 |
| 准星 | 中心十字 | 静态 |

字段缺失/未连接时保持现有行为：显示 `--` 或灰化，不伪造数据。

## 6. 代码改造点

现有结构（`client/window.cpp::setupUi()`）：

- `header` 是 `QGridLayout`（基地血条 + 比分 + 计时 + 链路）
- `workspace_` 是水平 `QSplitter`：主视图 | `sidebar_`（垂直 splitter：图传/我方/对方/战场事件/消息详情）
- 底部 `localPanel` 是本机状态条

改造建议：

1. **新增作战页容器**：一个 `QWidget`（如 `combatPage_`），内部放：
   - `AspectBox`：自定义 `QWidget`，在 `resizeEvent` 里按 16:9 计算内容矩形并居中，背景填充 `#05080d`；
   - `QStackedWidget`（画面层）：`VideoView` 与 `Map` 两页，F2 切换；
   - HUD 覆盖层：透明 `QWidget`，子控件绝对定位；`resizeEvent` 里按 `scale` 重算 geometry 与字体。
2. **F3 切换**：`QStackedWidget` 第二页放现有 `workspace_`（数据视图），保留现有分屏、拖拽换位、消息详情。
3. **不要在作战视图里用 `QSplitter`**；`arrangeModules()` / `sidebar_` 的尺寸持久化只服务数据视图。
4. **HUD 层设置 `Qt::WA_TransparentForMouseEvents`**，避免挡住画面层交互（若视频/地图无交互可忽略，但建议设置）。
5. **小地图**复用现有 `Map` 控件，尺寸改为 300×256×scale；检查 `client/map.cpp` 内部是否有固定像素假设。
6. 移除/隐藏原 header、alert、localPanel 在作战视图中的显示；数据视图保留原布局。
7. 不改协议层、不改 `window_network.cpp` 的订阅与解析。

## 7. 验收标准（可操作）

1. 1920×1080 全屏：与 `combat-view-v3.png` 对齐，主要 HUD 位置误差 ≤ 6px；四角不遮画面中心。
2. 1366×768 窗口：HUD 整组缩放后不重叠、字号可读；画面完整 16:9。
3. 2560×1080（21:9）：画面高度不变、左右留边；HUD 相对画面位置与尺寸不变。
4. 事件条不撑满画面，右侧留白稳定。
5. F2 切换图传/地图后 HUD 不丢、不重排。
6. F3 切到数据视图：现有分屏功能（拖拽换位、消息详情、列表滚动）不回归。
7. 断线/无数据：HUD 显示占位与灰化，不崩溃、不闪空。

## 8. 不做的事 / 风险

- 不做：HUD 自定义拖拽、毛玻璃、透明窗口、数据视图重设计。
- 风险：`Map` 按控件尺寸绘制，改小地图尺寸前先确认内部比例逻辑；HUD 字体缩放注意高 DPI（用逻辑像素）；视频帧在 HUD 下层，注意 z-order。
- 回滚：新代码走独立文件/分支，`window.cpp` 改动可 revert；设计稿文件不删。
