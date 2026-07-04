#!/usr/bin/env python3
"""StrikeSense 网站、社区下载站和 nightly 发布管理工具。"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import sys
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RESCENTER_JS = ROOT / "rescenter.js"
NIGHTLY_DIR = ROOT / "app" / "nightly"
NIGHTLY_MANIFEST = ROOT / "app" / "nightly-manifest.json"
NIGHTLY_INFO = ROOT / "app" / "nightly-info.json"


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
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8", newline="\n")


def write_json(path: Path, payload: object) -> None:
    write_text(path, json.dumps(payload, ensure_ascii=False, indent=2) + "\n")


def js_string(value: str) -> str:
    return json.dumps(value, ensure_ascii=False)


def normalize_tags(tags: list[str]) -> list[str]:
    seen: set[str] = set()
    result: list[str] = []
    for tag in tags:
        cleaned = tag.strip()
        if cleaned and cleaned not in seen:
            seen.add(cleaned)
            result.append(cleaned)
    return result


def prompt_text(message: str, default: str | None = None) -> str:
    suffix = f" [{default}]" if default else ""
    value = input(f"{message}{suffix}: ").strip()
    return value if value else (default or "")


def prompt_int(message: str, default: int | None = None, required: bool = False) -> int | None:
    while True:
        raw = prompt_text(message, str(default) if default is not None else None)
        if not raw:
            if required:
                print("这个值不能为空。")
                continue
            return default
        try:
            return int(raw)
        except ValueError:
            print("请输入整数。")


def prompt_bool(message: str, default: bool = False) -> bool:
    suffix = "Y/n" if default else "y/N"
    raw = input(f"{message} [{suffix}]: ").strip().lower()
    if not raw:
        return default
    return raw in {"y", "yes", "1", "true", "t"}


def prompt_multiline(message: str) -> list[str]:
    print(f"{message}，空行结束：")
    lines: list[str] = []
    while True:
        line = input().rstrip("\n")
        if not line.strip():
            break
        lines.append(line)
    return lines


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


def save_resources(resources: list[ResourceItem]) -> None:
    content = read_text(RESCENTER_JS)
    _, body_start, body_end = get_database_body(content)
    prefix = content[:body_start]
    suffix = content[body_end:]
    body = "\n" + "\n".join(format_resource(item) for item in resources) + "\n"
    write_text(RESCENTER_JS, prefix + body + suffix)


def get_resource_by_id(resources: list[ResourceItem], item_id: int) -> ResourceItem:
    for item in resources:
        if item.id == item_id:
            return item
    raise ValueError(f"没有找到 ID={item_id} 的资源条目。")


def list_resources(_: argparse.Namespace | None = None) -> None:
    resources = parse_resource_items(read_text(RESCENTER_JS))
    for item in resources:
        print(f"{item.id:>4} | {item.filename} | {', '.join(item.tags)} | {item.download_url}")
    debug(f"共读取 {len(resources)} 个资源条目。")


def add_resource(args: argparse.Namespace) -> None:
    resources = parse_resource_items(read_text(RESCENTER_JS))
    next_id = max((item.id for item in resources), default=0) + 1
    item_id = args.id or next_id
    if any(item.id == item_id for item in resources):
        raise ValueError(f"资源 ID={item_id} 已存在。")

    resources.append(
        ResourceItem(
            id=item_id,
            filename=args.name,
            img=args.image,
            desc=args.desc,
            tags=normalize_tags(args.tag),
            provider=args.provider,
            download_url=args.download,
        )
    )
    resources.sort(key=lambda item: item.id)
    save_resources(resources)
    debug(f"新增资源完成：{item_id} {args.name}")


def update_resource(args: argparse.Namespace) -> None:
    resources = parse_resource_items(read_text(RESCENTER_JS))
    target = get_resource_by_id(resources, args.id)
    updated = ResourceItem(
        id=target.id,
        filename=args.name or target.filename,
        img=args.image or target.img,
        desc=args.desc or target.desc,
        tags=normalize_tags(args.tag) if args.tag else target.tags,
        provider=args.provider or target.provider,
        download_url=args.download or target.download_url,
    )
    resources = [updated if item.id == args.id else item for item in resources]
    save_resources(resources)
    debug(f"更新资源完成：{args.id} {updated.filename}")


def remove_resource(args: argparse.Namespace) -> None:
    resources = parse_resource_items(read_text(RESCENTER_JS))
    target = get_resource_by_id(resources, args.id)
    resources = [item for item in resources if item.id != args.id]
    save_resources(resources)
    debug(f"删除资源完成：{target.id} {target.filename}")


def check_files(_: argparse.Namespace | None = None) -> None:
    missing: list[str] = []
    resources = parse_resource_items(read_text(RESCENTER_JS))
    for item in resources:
        for label, rel_path in (("预览图", item.img), ("下载文件", item.download_url)):
            target = ROOT / rel_path
            if target.exists():
                debug(f"{item.id} {item.filename} {label}存在：{rel_path}")
            else:
                missing.append(f"{item.id} {item.filename} 缺少{label}: {rel_path}")

    if missing:
        print("\n".join(missing))
        raise SystemExit(1)
    debug("所有资源条目的预览图和下载文件都存在。")


def iter_nightly_files() -> list[Path]:
    if not NIGHTLY_DIR.exists():
        return []
    return sorted(path for path in NIGHTLY_DIR.rglob("*") if path.is_file())


def build_nightly_manifest() -> dict[str, list[dict[str, object]]]:
    files: list[dict[str, object]] = []
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
    write_json(NIGHTLY_MANIFEST, manifest)
    debug(f"夜间版清单已生成：{NIGHTLY_MANIFEST.relative_to(ROOT)}")
    return manifest


def list_nightly(_: argparse.Namespace | None = None) -> None:
    manifest = build_nightly_manifest()
    files = manifest["files"]
    if not files:
        print("当前没有夜间版文件。")
        return
    for item in files:
        print(f"{item['name']} | {item['size']} bytes | {item['path']}")
    debug(f"共读取 {len(files)} 个夜间版文件。")


def clear_nightly(reset_info: bool = False) -> None:
    if NIGHTLY_DIR.exists():
        shutil.rmtree(NIGHTLY_DIR)
        debug(f"夜间版目录已删除：{NIGHTLY_DIR.relative_to(ROOT)}")
    if reset_info:
        write_json(
            NIGHTLY_INFO,
            {
                "version": "",
                "last_updated": "",
                "changes": [],
                "files": [],
            },
        )
        debug(f"已重置：{NIGHTLY_INFO.relative_to(ROOT)}")
    build_nightly_manifest()


def resolve_source_path(raw: str) -> Path:
    source = Path(raw)
    if source.is_absolute():
        return source
    return (ROOT / source).resolve()


def copy_into_nightly(source: Path) -> Path:
    if not source.exists() or not source.is_file():
        raise ValueError(f"夜间版源文件不存在：{source}")
    NIGHTLY_DIR.mkdir(parents=True, exist_ok=True)
    destination = NIGHTLY_DIR / source.name
    shutil.copy2(source, destination)
    debug(f"复制夜间版文件：{source} -> {destination.relative_to(ROOT)}")
    return destination


def publish_nightly(args: argparse.Namespace) -> None:
    if args.clean:
        debug("发布前先清空旧的 nightly 目录。")
        clear_nightly(reset_info=False)

    copied: list[Path] = []
    for raw_source in args.file:
        copied.append(copy_into_nightly(resolve_source_path(raw_source)))

    manifest = build_nightly_manifest()
    info_payload = {
        "version": args.version,
        "last_updated": args.updated or datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "changes": args.note or [],
        "files": [path.name for path in copied],
    }
    write_json(NIGHTLY_INFO, info_payload)
    debug(f"夜间版信息已写入：{NIGHTLY_INFO.relative_to(ROOT)}")
    debug(f"本次发布共处理 {len(manifest['files'])} 个夜间版文件。")


def find_nightly_sources() -> list[Path]:
    candidates: list[Path] = []
    for path in ROOT.glob("*.exe"):
        if path.name != "StrikeSense.exe":
            candidates.append(path)
    for path in (ROOT / "app").glob("*.exe"):
        if path.name != "StrikeSense.exe":
            candidates.append(path)
    return sorted(candidates, key=lambda p: p.stat().st_mtime, reverse=True)


def prompt_resource_tags(default: list[str] | None = None) -> list[str]:
    default_text = ",".join(default or [])
    raw = prompt_text("标签（逗号分隔）", default_text)
    return normalize_tags([part for part in raw.split(",") if part.strip()])


def interactive_add_resource() -> None:
    resources = parse_resource_items(read_text(RESCENTER_JS))
    next_id = max((item.id for item in resources), default=0) + 1
    item_id = prompt_int("资源 ID", next_id, required=True) or next_id
    name = prompt_text("资源名称")
    image = prompt_text("预览图路径")
    desc = prompt_text("资源简介")
    tags = prompt_resource_tags()
    provider = prompt_text("提供者")
    download = prompt_text("下载路径")
    args = argparse.Namespace(
        id=item_id,
        name=name,
        image=image,
        desc=desc,
        tag=tags,
        provider=provider,
        download=download,
    )
    add_resource(args)


def interactive_update_resource() -> None:
    resources = parse_resource_items(read_text(RESCENTER_JS))
    item_id = prompt_int("要更新的资源 ID", required=True)
    target = get_resource_by_id(resources, int(item_id))
    print(f"当前条目：{target.id} | {target.filename} | {', '.join(target.tags)}")
    name = prompt_text("资源名称", target.filename)
    image = prompt_text("预览图路径", target.img)
    desc = prompt_text("资源简介", target.desc)
    tags = prompt_resource_tags(target.tags)
    provider = prompt_text("提供者", target.provider)
    download = prompt_text("下载路径", target.download_url)
    args = argparse.Namespace(
        id=int(item_id),
        name=name,
        image=image,
        desc=desc,
        tag=tags,
        provider=provider,
        download=download,
    )
    update_resource(args)


def interactive_remove_resource() -> None:
    item_id = prompt_int("要删除的资源 ID", required=True)
    resources = parse_resource_items(read_text(RESCENTER_JS))
    target = get_resource_by_id(resources, int(item_id))
    print(f"待删除：{target.id} | {target.filename} | {', '.join(target.tags)}")
    if prompt_bool("确认删除", False):
        remove_resource(argparse.Namespace(id=int(item_id)))
    else:
        print("已取消。")


def interactive_publish_nightly() -> None:
    sources = find_nightly_sources()
    if sources:
        print("检测到可发布的源文件：")
        for idx, path in enumerate(sources, start=1):
            print(f"  {idx}. {path.relative_to(ROOT)}")
        raw = prompt_text("源文件编号或路径（多个用逗号分隔）", "1")
        chosen: list[str] = []
        for part in [piece.strip() for piece in raw.split(",") if piece.strip()]:
            if part.isdigit():
                idx = int(part)
                if 1 <= idx <= len(sources):
                    chosen.append(str(sources[idx - 1]))
            else:
                chosen.append(part)
    else:
        print("没有检测到可自动发布的源文件，请手动输入路径。")
        chosen = []
        while not chosen:
            raw = prompt_text("源文件路径（多个用逗号分隔）")
            chosen = [piece.strip() for piece in raw.split(",") if piece.strip()]

    version = prompt_text("nightly 版本号", datetime.now().strftime("%Y-%m-%d"))
    notes = prompt_multiline("更新说明")
    clean = prompt_bool("发布前清空旧 nightly 目录", True)
    updated = prompt_text("更新时间", datetime.now().strftime("%Y-%m-%d %H:%M:%S"))
    args = argparse.Namespace(
        file=chosen,
        version=version,
        note=notes,
        updated=updated,
        clean=clean,
    )
    publish_nightly(args)
    print("发布完成。")
    print(f"清单：{NIGHTLY_MANIFEST.relative_to(ROOT)}")
    print(f"信息：{NIGHTLY_INFO.relative_to(ROOT)}")
    print("下一步通常是提交并推送到发布分支，然后让站点重新部署。")


def interactive_menu() -> None:
    while True:
        print()
        print("StrikeSense 管理菜单")
        print("1. 列出资源")
        print("2. 新增资源")
        print("3. 更新资源")
        print("4. 删除资源")
        print("5. 检查资源文件")
        print("6. 列出夜间版文件")
        print("7. 发布夜间版")
        print("8. 重建夜间版清单")
        print("9. 清空夜间版目录")
        print("0. 退出")
        choice = prompt_text("请选择", "0")

        try:
            if choice == "1":
                list_resources()
            elif choice == "2":
                interactive_add_resource()
            elif choice == "3":
                interactive_update_resource()
            elif choice == "4":
                interactive_remove_resource()
            elif choice == "5":
                check_files()
            elif choice == "6":
                list_nightly()
            elif choice == "7":
                interactive_publish_nightly()
            elif choice == "8":
                build_nightly_manifest()
            elif choice == "9":
                if prompt_bool("确认清空夜间版目录", False):
                    clear_nightly(reset_info=True)
                else:
                    print("已取消。")
            elif choice == "0":
                return
            else:
                print("无效选择。")
        except Exception as exc:
            print(f"发生错误：{exc}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="管理 StrikeSense 网站、社区下载站和 nightly 发布。")
    parser.add_argument("--menu", action="store_true", help="直接进入交互式菜单")
    subparsers = parser.add_subparsers(dest="command")

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

    update_parser = subparsers.add_parser("update-resource", help="更新资源中心条目")
    update_parser.add_argument("--id", required=True, type=int, help="目标资源 ID")
    update_parser.add_argument("--name", help="新的资源名称")
    update_parser.add_argument("--image", help="新的预览图路径")
    update_parser.add_argument("--desc", help="新的资源简介")
    update_parser.add_argument("--tag", action="append", help="新的资源标签，可重复传入")
    update_parser.add_argument("--provider", help="新的提供者")
    update_parser.add_argument("--download", help="新的下载路径")
    update_parser.set_defaults(func=update_resource)

    remove_parser = subparsers.add_parser("remove-resource", help="删除资源中心条目")
    remove_parser.add_argument("--id", required=True, type=int, help="目标资源 ID")
    remove_parser.set_defaults(func=remove_resource)

    check_parser = subparsers.add_parser("check-files", help="检查资源中心引用文件是否存在")
    check_parser.set_defaults(func=check_files)

    nightly_list_parser = subparsers.add_parser("list-nightly", help="列出夜间版文件")
    nightly_list_parser.set_defaults(func=list_nightly)

    manifest_parser = subparsers.add_parser("build-nightly-manifest", help="扫描 nightly 目录并生成清单")
    manifest_parser.set_defaults(func=build_nightly_manifest)

    clear_parser = subparsers.add_parser("clear-nightly", help="清空夜间版目录")
    clear_parser.add_argument("--reset-info", action="store_true", help="同时重置 nightly-info.json")
    clear_parser.set_defaults(func=lambda args: clear_nightly(args.reset_info))

    publish_parser = subparsers.add_parser("publish-nightly", help="复制夜间版文件并生成清单/信息")
    publish_parser.add_argument("--file", action="append", required=True, help="要发布的夜间版文件，可重复传入")
    publish_parser.add_argument("--version", required=True, help="nightly 版本号")
    publish_parser.add_argument("--note", action="append", help="更新说明，可重复传入")
    publish_parser.add_argument("--updated", help="手动指定更新时间文本")
    publish_parser.add_argument("--clean", action="store_true", help="发布前先清空旧的 nightly 目录")
    publish_parser.set_defaults(func=publish_nightly)

    return parser


def main() -> None:
    parser = build_parser()
    if len(sys.argv) == 1:
        interactive_menu()
        return

    args = parser.parse_args()
    if args.menu:
        interactive_menu()
        return

    if not hasattr(args, "func"):
        parser.print_help()
        return

    args.func(args)


if __name__ == "__main__":
    main()
