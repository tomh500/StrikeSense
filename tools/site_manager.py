#!/usr/bin/env python3
"""StrikeSense 网站与社区下载站管理工具。"""

from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


ROOT = Path(__file__).resolve().parents[1]
RESCENTER_JS = ROOT / "rescenter.js"
NIGHTLY_DIR = ROOT / "app" / "nightly"
NIGHTLY_MANIFEST = ROOT / "app" / "nightly-manifest.json"


@dataclass(frozen=True)
class ResourceItem:
    id: int
    filename: str
    img: str
    desc: str
    tags: list[str]
    provider: str
    download_url: str


def debug(message: str) -> None:
    print(f"[站点工具] {message}")


def read_text(path: Path) -> str:
    debug(f"读取文本文件：{path.relative_to(ROOT)}")
    return path.read_text(encoding="utf-8")


def write_text(path: Path, content: str) -> None:
    debug(f"写入文本文件：{path.relative_to(ROOT)}")
    path.write_text(content, encoding="utf-8", newline="\n")


def js_string(value: str) -> str:
    return json.dumps(value, ensure_ascii=False)


def split_array_items(array_body: str) -> list[str]:
    items: list[str] = []
    start: int | None = None
    depth = 0
    in_string: str | None = None
    escaped = False

    for index, char in enumerate(array_body):
        if in_string:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == in_string:
                in_string = None
            continue

        if char in {'"', "'"}:
            in_string = char
        elif char == "{":
            if depth == 0:
                start = index
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0 and start is not None:
                items.append(array_body[start : index + 1])
                start = None

    return items


def get_database_body(content: str) -> tuple[str, int, int]:
    match = re.search(r"const\s+RESOURCE_DATABASE\s*=\s*\[", content)
    if not match:
        raise ValueError("没有找到 RESOURCE_DATABASE 数组。")

    body_start = match.end()
    depth = 1
    in_string: str | None = None
    escaped = False
    for index in range(body_start, len(content)):
        char = content[index]
        if in_string:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == in_string:
                in_string = None
            continue

        if char in {'"', "'"}:
            in_string = char
        elif char == "[":
            depth += 1
        elif char == "]":
            depth -= 1
            if depth == 0:
                return content[body_start:index], body_start, index

    raise ValueError("RESOURCE_DATABASE 数组没有正确结束。")


def parse_resource_items(content: str) -> list[ResourceItem]:
    body, _, _ = get_database_body(content)
    resources: list[ResourceItem] = []
    for raw in split_array_items(body):
        item_id = int(re.search(r"\bid\s*:\s*(\d+)", raw).group(1))
        filename = re.search(r"\bfilename\s*:\s*(['\"])(.*?)\1", raw, re.S).group(2)
        img = re.search(r"\bimg\s*:\s*(['\"])(.*?)\1", raw, re.S).group(2)
        desc = re.search(r"\bdesc\s*:\s*(['\"])(.*?)\1", raw, re.S).group(2)
        provider = re.search(r"\bprovider\s*:\s*(['\"])(.*?)\1", raw, re.S).group(2)
        download_url = re.search(r"\bdownloadUrl\s*:\s*(['\"])(.*?)\1", raw, re.S).group(2)
        tags_raw = re.search(r"\btags\s*:\s*\[(.*?)\]", raw, re.S).group(1)
        tags = re.findall(r"['\"](.*?)['\"]", tags_raw, re.S)
        resources.append(ResourceItem(item_id, filename, img, desc, tags, provider, download_url))
    return resources


def format_resource(item: ResourceItem) -> str:
    tags = ", ".join(js_string(tag) for tag in item.tags)
    return "\n".join(
        [
            "    {",
            f"        id: {item.id},",
            f"        filename: {js_string(item.filename)},",
            f"        img: {js_string(item.img)},",
            f"        desc: {js_string(item.desc)},",
            f"        tags: [{tags}],",
            f"        provider: {js_string(item.provider)},",
            f"        downloadUrl: {js_string(item.download_url)}",
            "    },",
        ]
    )


def list_resources(_: argparse.Namespace) -> None:
    resources = parse_resource_items(read_text(RESCENTER_JS))
    for item in resources:
        print(f"{item.id:>3} | {item.filename} | {', '.join(item.tags)} | {item.download_url}")
    debug(f"共读取 {len(resources)} 个资源条目。")


def add_resource(args: argparse.Namespace) -> None:
    content = read_text(RESCENTER_JS)
    resources = parse_resource_items(content)
    next_id = max((item.id for item in resources), default=0) + 1
    item = ResourceItem(
        id=args.id or next_id,
        filename=args.name,
        img=args.image,
        desc=args.desc,
        tags=args.tag,
        provider=args.provider,
        download_url=args.download,
    )
    debug(f"准备新增资源：{item.filename}，ID={item.id}")
    _, body_start, body_end = get_database_body(content)
    prefix = content[:body_start].rstrip()
    body = content[body_start:body_end].rstrip()
    suffix = content[body_end:]
    separator = "\n" if body.endswith(",") else ",\n"
    updated = f"{prefix}\n{body}{separator}{format_resource(item)}\n{suffix}"
    write_text(RESCENTER_JS, updated)
    debug("资源条目已经追加到 rescenter.js。")


def iter_nightly_files() -> Iterable[Path]:
    if not NIGHTLY_DIR.exists():
        return []
    return sorted(path for path in NIGHTLY_DIR.rglob("*") if path.is_file())


def build_manifest(_: argparse.Namespace) -> None:
    debug(f"扫描 nightly 目录：{NIGHTLY_DIR.relative_to(ROOT)}")
    files = []
    for path in iter_nightly_files():
        stat = path.stat()
        files.append(
            {
                "name": path.name,
                "path": path.relative_to(ROOT).as_posix(),
                "size": stat.st_size,
                "modified": int(stat.st_mtime),
            }
        )
        debug(f"发现夜间版文件：{path.relative_to(ROOT)} ({stat.st_size} bytes)")

    manifest = {"files": files}
    write_text(NIGHTLY_MANIFEST, json.dumps(manifest, ensure_ascii=False, indent=2))
    debug(f"夜间版清单已生成：{NIGHTLY_MANIFEST.relative_to(ROOT)}")


def check_files(_: argparse.Namespace) -> None:
    missing: list[str] = []
    resources = parse_resource_items(read_text(RESCENTER_JS))
    for item in resources:
        for label, rel_path in (("预览图", item.img), ("下载文件", item.download_url)):
            target = ROOT / rel_path
            if not target.exists():
                missing.append(f"{item.id} {item.filename} 缺少{label}: {rel_path}")
            else:
                debug(f"{item.id} {item.filename} {label}存在：{rel_path}")

    if missing:
        print("\n".join(missing))
        raise SystemExit(1)
    debug("所有资源条目的预览图与下载文件都存在。")


def main() -> None:
    parser = argparse.ArgumentParser(description="管理 StrikeSense 网站和社区下载站。")
    subparsers = parser.add_subparsers(required=True)

    list_parser = subparsers.add_parser("list-resources", help="列出资源中心条目")
    list_parser.set_defaults(func=list_resources)

    add_parser = subparsers.add_parser("add-resource", help="新增资源中心条目")
    add_parser.add_argument("--id", type=int, help="手动指定资源 ID")
    add_parser.add_argument("--name", required=True, help="资源名称")
    add_parser.add_argument("--image", required=True, help="预览图路径")
    add_parser.add_argument("--desc", required=True, help="资源简介")
    add_parser.add_argument("--tag", action="append", required=True, help="资源标签，可重复传入")
    add_parser.add_argument("--provider", required=True, help="提供者")
    add_parser.add_argument("--download", required=True, help="下载路径")
    add_parser.set_defaults(func=add_resource)

    manifest_parser = subparsers.add_parser("build-nightly-manifest", help="扫描夜间版文件并生成清单")
    manifest_parser.set_defaults(func=build_manifest)

    check_parser = subparsers.add_parser("check-files", help="检查资源中心引用文件是否存在")
    check_parser.set_defaults(func=check_files)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
