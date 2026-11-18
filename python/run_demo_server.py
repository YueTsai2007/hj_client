#!/usr/bin/env python3
"""发布全部 RM2027 下行示例，用于本地客户端联调。"""
import argparse
import json
import math
import os
from pathlib import Path
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
BACKEND = ROOT / "RM2027-CustomTerminal-CommunicationBackend"
POSITIONS = {
    1: (.22, .67), 3: (.39, .55), 4: (.32, .28), 7: (.16, .39),
    101: (.78, .33), 103: (.61, .45), 104: (.68, .72), 107: (.84, .61),
}
MAX_HEALTH = {
    1: 200, 3: 150, 4: 150, 7: 400,
    101: 200, 103: 150, 104: 150, 107: 400,
}
OVERRIDES = {
    "GameStatus": dict(current_round=1, total_rounds=3, red_score=2, blue_score=1,
                       current_stage=4, stage_countdown_sec=398, stage_elapsed_sec=22,
                       is_paused=False, game_result=0, end_reason=0),
    "GlobalUnitStatus": dict(base_health=5000, enemy_base_health=4710,
                             robot_health=[150, 80, 120, 350, 180, 115, 75, 330],
                             robot_bullets=[12, 32, 24, 48, 10, 25, 18, 42],
                             total_damage_ally=510, total_damage_enemy=640),
    "GlobalLogisticsStatus": dict(remaining_economy=120, total_economy_obtained=480,
                                  tech_level=2, encryption_level=1),
    "RobotStaticStatus": dict(max_heat=120),
    "RobotDynamicStatus": dict(current_health=80, current_heat=36.5, remaining_ammo=32,
                               is_out_of_combat=False),
    "RobotRespawnStatus": dict(is_pending_respawn=False, current_respawn_progress=0,
                               total_respawn_progress=10, can_free_respawn=False,
                               can_pay_for_respawn=False, gold_cost_for_respawn=60),
    "Event": dict(event_id=1, param="模拟赛况事件"),
    "PenaltyInfo": dict(penalty_type=1, penalty_effect_sec=5, total_penalty_num=1),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=3333)
    parser.add_argument("--lib", type=Path, help="已编译的 librm2027c.so")
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error("端口必须在 1 到 65535 之间")

    library = args.lib or ROOT / "build" / "librm2027c.so"
    if args.lib or library.is_file():
        os.environ["RM2027C_LIB"] = str(library.resolve())
    sys.path.insert(0, str(BACKEND / "python"))
    from rm2027c import Server, messages

    fixture = BACKEND / "tests" / "scenarios" / "all_down.json"
    samples = {step["topic"]: step["data"]
               for step in json.loads(fixture.read_text(encoding="utf-8"))["steps"]}
    downlink = {message.name: message for message in messages() if message.direction == "down"}
    missing = sorted(downlink.keys() - samples.keys())
    if missing:
        parser.error(f"示例缺少消息：{', '.join(missing)}")
    print(f"模拟服务器 {args.host}:{args.port}，已装载 {len(downlink)} 种下行消息", flush=True)

    with Server(args.host, args.port) as server:
        tick = 0
        while True:
            if not server.subscribed_client_count:
                tick = 0
            else:
                for topic, meta in downlink.items():
                    if meta.triggered and tick % 10:
                        continue
                    if topic == "RobotPosition":
                        for robot_id, (x, y) in POSITIONS.items():
                            server.send(topic, dict(robot_id=robot_id, x=x + .025 * math.sin(tick / 4),
                                                    y=y, z=0, yaw=0))
                        continue
                    sample = {**samples[topic], **OVERRIDES.get(topic, {})}
                    if topic == "RobotStaticStatus":
                        for robot_id, maximum in MAX_HEALTH.items():
                            server.send(topic, {**sample, "robot_id": robot_id, "max_health": maximum})
                        continue
                    if topic == "GameStatus":
                        sample["stage_countdown_sec"] = 398 - tick % 399
                        sample["stage_elapsed_sec"] = 22 + tick % 399
                    server.send(topic, sample)
                tick += 1
            time.sleep(1)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
    except RuntimeError as error:
        sys.exit(str(error))
