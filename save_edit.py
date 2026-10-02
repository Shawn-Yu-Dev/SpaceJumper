#!/usr/bin/env python3
"""关卡选择器 — 生成二进制 save.dat, 开局即进入指定关卡 (L1~L10)."""

import struct
import sys
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SAVE_PATH = os.path.join(SCRIPT_DIR, "save.dat")

SAVE_MAGIC = 0x5641534D  # "MSAV"
LEVEL_COUNT = 10


def write_save(level_index: int, score: int = 0, coins: int = 0, lives: int = 3):
    unlocked = level_index
    level = level_index

    # 校验和: 与 Save.cpp 完全一致
    chk = unlocked * 31 + level * 17 + score * 7 + coins * 13 + lives * 19

    data = struct.pack(
        "<Iiiiiii",  # LE: magic(u32) + 5*i32 + chk(i32)
        SAVE_MAGIC,
        unlocked,
        level,
        score,
        coins,
        lives,
        chk,
    )

    with open(SAVE_PATH, "wb") as f:
        f.write(data)

    print(f"save.dat 已生成 (L{level_index + 1})")
    print(f"  unlocked={unlocked}  level={level}  score={score}  coins={coins}  lives={lives}")
    print(f"  checksum={chk}  size={len(data)} bytes")


def main():
    if len(sys.argv) < 2:
        print(f"用法: {sys.argv[0]} <关卡号 1~{LEVEL_COUNT}> [分数] [金币] [命数]")
        print(f"示例: {sys.argv[0]} 7          → 从 L7 开始 (0 分 0 币 3 命)")
        print(f"示例: {sys.argv[0]} 7 10000 50 → 从 L7 开始 (10000 分 50 币)")
        return

    try:
        level_num = int(sys.argv[1])
    except ValueError:
        print(f"错误: 关卡号必须是整数 (1~{LEVEL_COUNT})")
        return

    if level_num < 1 or level_num > LEVEL_COUNT:
        print(f"错误: 关卡号必须在 1~{LEVEL_COUNT} 之间")
        return

    score = int(sys.argv[2]) if len(sys.argv) > 2 else 0
    coins = int(sys.argv[3]) if len(sys.argv) > 3 else 0
    lives = int(sys.argv[4]) if len(sys.argv) > 4 else 3

    write_save(level_num - 1, score, coins, lives)


if __name__ == "__main__":
    main()