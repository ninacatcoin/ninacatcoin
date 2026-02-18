#!/usr/bin/env python3
"""
Fetch block hashes via RPC and rewrite the auto-generated mainnet checkpoint block
between the markers in src/checkpoints/checkpoints.cpp.
"""

import argparse
import json
import pathlib
import sys
import urllib.request

START_MARK = "// >>> AUTO-GENERATED MAINNET CHECKPOINTS BEGIN"
END_MARK = "// <<< MAINNET CHECKPOINTS END >>>"


def rpc_get_block_hash(rpc_url: str, height: int) -> str:
    """Use get_block to remain compatible with daemons lacking get_block_hash."""
    payload = {
        "jsonrpc": "2.0",
        "id": "0",
        "method": "get_block",
        "params": {"height": height},
    }
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(
        rpc_url, data=data, headers={"Content-Type": "application/json"}
    )
    with urllib.request.urlopen(req) as resp:
        body = resp.read().decode("utf-8")
    parsed = json.loads(body)
    if "error" in parsed:
        raise RuntimeError(f"RPC error at height {height}: {parsed['error']}")
    result = parsed.get("result", {})
    header = result.get("block_header", {})
    block_hash = header.get("hash")
    if not block_hash:
        raise RuntimeError(f"Unexpected RPC response at height {height}: {parsed}")
    return block_hash


def build_checkpoint_lines(hashes: dict[int, str]) -> list[str]:
    lines = []
    for height in sorted(hashes.keys()):
        h = hashes[height]
        lines.append(f'    ADD_CHECKPOINT2({height}, "{h}", "");')
    return lines


def rewrite_block(file_path: pathlib.Path, new_lines: list[str]) -> None:
    content = file_path.read_text(encoding="utf-8").splitlines()
    try:
        start_idx = next(i for i, line in enumerate(content) if START_MARK in line)
        end_idx = next(i for i, line in enumerate(content) if END_MARK in line)
    except StopIteration as exc:
        raise SystemExit("No checkpoint markers found in checkpoints.cpp") from exc
    if end_idx <= start_idx:
        raise SystemExit("Checkpoint markers are in the wrong order")

    updated = (
        content[: start_idx + 1]
        + new_lines
        + content[end_idx:]
    )
    file_path.write_text("\n".join(updated) + "\n", encoding="utf-8")


def parse_heights(arg: str) -> list[int]:
    try:
        heights = {int(x) for x in arg.split(",") if x.strip() != ""}
    except ValueError as exc:
        raise argparse.ArgumentTypeError(f"Invalid height in '{arg}'") from exc
    if not heights:
        raise argparse.ArgumentTypeError("At least one height is required")
    return sorted(heights)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Update mainnet checkpoints in checkpoints.cpp using RPC block hashes"
    )
    parser.add_argument(
        "--rpc-url",
        default="http://127.0.0.1:19081/json_rpc",
        help="RPC endpoint for the daemon (default: %(default)s)",
    )
    parser.add_argument(
        "--heights",
        default="0,1,10,100,1000,10000",
        type=parse_heights,
        help="Comma-separated block heights to checkpoint (default: %(default)s)",
    )
    parser.add_argument(
        "--file",
        default="src/checkpoints/checkpoints.cpp",
        type=pathlib.Path,
        help="Path to checkpoints.cpp (default: %(default)s)",
    )
    args = parser.parse_args()

    hashes: dict[int, str] = {}
    for height in args.heights:
        try:
            block_hash = rpc_get_block_hash(args.rpc_url, height)
        except RuntimeError as exc:
            msg = str(exc)
            if "greater than current top block height" in msg:
                print(f"Skipping height {height}: {msg}")
                break
            raise
        hashes[height] = block_hash
        print(f"height {height}: {block_hash}")

    lines = build_checkpoint_lines(hashes)
    rewrite_block(args.file, lines)
    print(f"Updated {args.file} between checkpoint markers.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
