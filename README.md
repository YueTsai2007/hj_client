# 悍匠战场终端

在 Ubuntu 中，从项目根目录执行：

```bash
cmake -S . -B build/hj-client
cmake --build build/hj-client --target HJ_Client -j 4
./build/hj-client/HJ_Client --host 127.0.0.1 --port 3333 --id 3 --video-port 3334
```

可通过 `--help` 查看选项。连接真实裁判系统时将地址、端口和机器人 ID 按实际配置修改。
本地可运行后端示例服务器 `RM2027-CustomTerminal-CommunicationBackend/build/rm_api_demo server 3333` 测试连接。

当前地图是示意图，尚未接入坐标。机器人血量按后端文档映射为己方和敌方各 1、3、4、7 号；当收到与机器人编号及阵营匹配且未过期的最大血量时显示比例血条；否则仅显示血量数值。机器人位置与雷达位置以原始坐标显示，未映射到示意地图。图传、界面视觉对照和真实设备尚需完整联调。

客户端订阅全部 23 类下行消息。点击“消息详情”可查看每类消息的接收次数、最后接收时间和最近一次消息提供的字段；二进制字段以 Base64 表示。高频消息只保留最近一条，界面每 500 毫秒刷新；事件消息仅在触发时更新。详情从原始 Protobuf 载荷解码，未列出的可选字段表示未提供。

主界面的 `--` 表示字段未提供或数据不可用；明确发送的零值显示为 `0`。断线时主界面清除实时值，详情保留并标记历史数据。重新连接后，在收到该类型的新消息前仍会标为历史数据。

地图下方可以选择雷达线格式：默认嵌套（nested），也可选择平铺（flat）。接收端格式必须与发送端一致；切换后须等待下一条雷达消息，旧数据显示会被清除。实际赛场使用的格式仍需联调确认。

启动后端后台
```python
python3 RM2027-CustomTerminal-CommunicationBackend/python/run_server.py --host 127.0.0.1 --port 3333
```
启动客户端
```bash
./build/hj-client/HJ_Client --host 127.0.0.1 --port 3333 --id 3 --video-port 3334
```
后台消息demo
```python
python3 python/run_demo_server.py --host 127.0.0.1 --port 3333
```

---

## 作战视图 / 数据视图（新增）

界面分为两页，`F3` 在「作战视图」和「数据视图」之间切换。旧版紧凑布局已废弃。

| 快捷键 | 作用 |
|---|---|
| `F2` | 作战视图内：图传 / 地图 切换 |
| `F3` | 作战视图 ↔ 数据视图 |
| `F4` | 准星开关（状态记忆在 QSettings） |
| `O` | 兑换 / 买活面板（按 `--id` 决定 17mm 或 42mm） |
| `F11` / `Esc` | 全屏 / 退出全屏 |

- **作战视图**：16:9 letterbox 舞台，窗口比例不符时上下或左右留黑边；HUD 按 `scale = 画面宽/1920` 整组缩放，不重排。
- **数据视图**：左列上地图、左下战场事件与即时战术数据；右列上双方机器人、右下图传；顶部为前哨/经济/堡垒/买活状态条。
- **机制提示条**：`GlobalSpecialMechanism` 有生效机制时出现，按官方枚举翻译：1 己方堡垒被激活、2 对方堡垒被激活、3 R 前哨阵地、4 B 前哨阵地、5 R 基地阵地、6 B 基地阵地；3–6 的时间带符号（<0 对方推进、>0 己方推进、0 无变化）。无机制时整条隐藏。
- **模块自检**：`RobotModuleStatus` 的 8 个模块（电源/RFID/灯带/小发射/大发射/UWB/装甲/图传），`1` 绿正常、`0` 红异常、无数据灰。
- **刷新策略**：**收到即更新**。每条下行消息到达后由 `recordMessage` 触发一次刷新（同一轮事件循环内多次消息合并），不依赖固定频率的定时器；定时器只负责过期判断。

## 地图与坐标归一化

- **底图**：取自《RoboMaster 2027 超级对抗赛比赛规则手册 V1.0.0》图 4-1 战场俯视渲染图，裁去页边后降饱和、压暗，存为 `client/reference/field.png`，经 `resources.qrc` 以 `:/images/field.png` 打包；`client/map.cpp` 以 `KeepAspectRatioByExpanding` 铺满 + 暗色蒙版 + 金色边框绘制，**场地结构本身是官方的，不再手绘**。
- **标定画布**：`map.cpp` 内部使用 900×482 的逻辑画布（对应场地 28m×15m 的比例），所有标记按 `fit = min(控件宽/900, 控件高/482)` 缩放。
- **归一化坐标**：绘图接口 `Map::setRobotPosition(id, pos, yaw)` 接收 **0~1 归一化坐标**（x 向右、y 向上）；画布内场地矩形 `(20,11) 860×460` 精确对应 28m×15m，映射为
  `px = 20 + x/28*860`、`py = 11 + (1-y/15)*460`（再叠加校准偏置，`flipped_` 时 `x → 1-x`）。
- **当前数据来源**：`RobotPosition` 已按协议换算后接入——协议 0x0203 明确坐标为 **米**，原点在**红方补给站附近的场地围挡交点**，X 轴沿场地长边指向蓝方（场地 28m × 15m）。代码按量级判断：数值 > 1.5 视为米，除以 28 / 15 归一化；0~1 视为 demo 服务器直接发的归一化值。
- **接入真机时的标定步骤**：
  1. 确认 `RobotPosition` 是否如 0x0203 一样以米为单位、原点在红方补给区角（若裁判系统换坐标系，改 `window_network.cpp` 里 `/28`、`/15` 两处）；
  2. 确认己方视角：蓝方是否需要 `x = 1 - x` 做镜像（目前未翻转）；
  3. `RadarInfoToClient.target_pos_x/y` 同样按米换算后调用 `setRobotPosition`；
  4. 若底图与坐标仍有偏差，用下面的校准量微调，不再改动映射公式。
- **上赛场的微调（可选）**：映射本身按协议算，但真实场地允许尺寸公差（规则手册：≥100mm 结构误差 ±5%），且底图裁切也有几像素误差。因此保留 4 个校准量，用环境变量传入，单位是 900×482 画布像素：
  | 变量 | 作用 | 默认 |
  |---|---|---|
  | `HJ_MAP_CAL_X` / `HJ_MAP_CAL_Y` | 整体平移 | 0 |
  | `HJ_MAP_CAL_W` / `HJ_MAP_CAL_H` | 场地宽/高修正（应对场地实际尺寸不标准） | 0 |
  例：`HJ_MAP_CAL_X=6 HJ_MAP_CAL_W=-10 ./HJ_Client ...`
- **yaw 朝向箭头**：`RobotPosition.yaw` 接入为箭头旋转角，协议 0x0203 定义朝向单位为**度、正北为 0**；若实车方向反了，在 `map.cpp` 旋转处加 180°。

## 更新频率与过期阈值

集中定义在 `client/topics.h`（唯一的频率/超时来源，避免各处散写）。频率取自通信协议表 1-5：

| 消息 | 频率 | 过期阈值 |
|---|---|---|
| GameStatus | 1Hz | 3s |
| GlobalUnitStatus | 3Hz | 1.5s |
| GlobalLogisticsStatus | 10Hz | 0.8s |
| RobotDynamicStatus | 10Hz | 0.8s |
| RobotStaticStatus | 3Hz | 1.5s |
| RobotRespawnStatus | 3Hz | 1.5s |
| RobotModuleStatus | 3Hz | 1.5s |
| RobotPosition | 1Hz | 3s |
| RadarInfoToClient | ≤5Hz | 1.2s |
| Buff | 3Hz | 1.5s |
| GlobalSpecialMechanism | 1Hz | 3s |
| Event / PenaltyInfo | 触发式 | 3s |

HUD 只在上述消息的接收计数变化时重建，因此界面刷新率跟随实际下行频率，而不是固定定时器硬刷。

