#!/usr/bin/env python3
from __future__ import annotations

import argparse
import html
import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
WEBSITE = ROOT / "website-src"
DATA_DIR = WEBSITE / "data"
DOCS_DIR = WEBSITE / "docs"
RESOURCE_JSON = DATA_DIR / "resources.json"
RESOURCE_JS = DATA_DIR / "resources.generated.js"
VSCRIPT_MD = ROOT / "VSCRIPT_MANUAL.md"
VSCRIPT_HTML = DOCS_DIR / "vscript.html"


def read_resources() -> list[dict]:
    if not RESOURCE_JSON.exists():
        return []
    return json.loads(RESOURCE_JSON.read_text(encoding="utf-8"))


def write_resources(resources: list[dict]) -> None:
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    RESOURCE_JSON.write_text(
        json.dumps(resources, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )


def build_resource_js(resources: list[dict]) -> None:
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    RESOURCE_JS.write_text(
        "window.STRIKESENSE_RESOURCES = "
        + json.dumps(resources, ensure_ascii=False, indent=2)
        + ";\n",
        encoding="utf-8",
    )


def slugify(text: str) -> str:
    slug = re.sub(r"[^0-9a-zA-Z\u4e00-\u9fff]+", "-", text.strip()).strip("-").lower()
    return slug or "resource"


def apply_inline_markup(text: str) -> str:
    escaped = html.escape(text, quote=False)
    escaped = re.sub(r"`([^`]+)`", lambda m: f"<code>{html.escape(m.group(1))}</code>", escaped)
    escaped = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", escaped)
    escaped = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", r'<a href="\2">\1</a>', escaped)
    return escaped


def markdown_to_html(markdown: str) -> str:
    lines = markdown.splitlines()
    output: list[str] = []
    in_code = False
    code_lines: list[str] = []
    paragraph_lines: list[str] = []
    in_ul = False
    in_ol = False

    def flush_paragraph() -> None:
        nonlocal paragraph_lines
        if paragraph_lines:
            text = " ".join(part.strip() for part in paragraph_lines).strip()
            if text:
                output.append(f"<p>{apply_inline_markup(text)}</p>")
        paragraph_lines = []

    def close_lists() -> None:
        nonlocal in_ul, in_ol
        if in_ul:
            output.append("</ul>")
            in_ul = False
        if in_ol:
            output.append("</ol>")
            in_ol = False

    for raw in lines:
        line = raw.rstrip("\n")
        stripped = line.strip()

        if stripped.startswith("```"):
            flush_paragraph()
            close_lists()
            if in_code:
                output.append("<pre><code>" + html.escape("\n".join(code_lines)) + "</code></pre>")
                code_lines = []
                in_code = False
            else:
                in_code = True
            continue

        if in_code:
            code_lines.append(line)
            continue

        if not stripped:
            flush_paragraph()
            close_lists()
            continue

        heading = re.match(r"^(#{1,6})\s+(.+)$", stripped)
        if heading:
            flush_paragraph()
            close_lists()
            level = len(heading.group(1))
            title = heading.group(2).strip()
            output.append(f'<h{level} id="{slugify(title)}">{apply_inline_markup(title)}</h{level}>')
            continue

        bullet = re.match(r"^[-*]\s+(.+)$", stripped)
        if bullet:
            flush_paragraph()
            if in_ol:
                output.append("</ol>")
                in_ol = False
            if not in_ul:
                output.append("<ul>")
                in_ul = True
            output.append(f"<li>{apply_inline_markup(bullet.group(1))}</li>")
            continue

        ordered = re.match(r"^\d+\.\s+(.+)$", stripped)
        if ordered:
            flush_paragraph()
            if in_ul:
                output.append("</ul>")
                in_ul = False
            if not in_ol:
                output.append("<ol>")
                in_ol = True
            output.append(f"<li>{apply_inline_markup(ordered.group(1))}</li>")
            continue

        paragraph_lines.append(stripped)

    flush_paragraph()
    close_lists()
    if in_code:
        output.append("<pre><code>" + html.escape("\n".join(code_lines)) + "</code></pre>")
    return "\n".join(output)


def wrap_doc_html(title: str, body_html: str) -> str:
    return f"""<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>StrikeSense | {html.escape(title)}</title>
    <link rel="stylesheet" href="../style.css">
    <style>
        .doc-shell {{
            width: min(980px, calc(100% - 32px));
            margin: 0 auto;
            padding: 28px 0 72px;
        }}
        .doc-card {{
            background: var(--panel);
            border: 1px solid var(--line);
            border-radius: 8px;
            box-shadow: var(--shadow);
            padding: 28px;
        }}
        .doc-card p, .doc-card li {{
            color: var(--text-soft);
            line-height: 1.75;
        }}
        .doc-card h1, .doc-card h2, .doc-card h3 {{
            margin-top: 28px;
        }}
        .doc-card h1:first-child {{
            margin-top: 0;
        }}
        .doc-card a {{
            color: var(--accent-deep);
        }}
        .doc-card code {{
            word-break: break-word;
        }}
    </style>
</head>
<body>
    <header class="site-header">
        <div class="shell nav-shell">
            <a class="brand" href="../index.html">
                <span class="brand-mark">S</span>
                <span class="brand-text">StrikeSense</span>
            </a>
            <nav class="site-nav">
                <a href="../index.html">首页</a>
                <a href="../rescenter.html">下载站</a>
                <a href="../docs.html" class="is-active">文档</a>
                <a href="../oem.html">OEM 密钥生成</a>
            </nav>
        </div>
    </header>
    <section class="page-hero">
        <div class="shell">
            <div class="eyebrow">Generated Document</div>
            <h1>{html.escape(title)}</h1>
            <p>此页面由 <code>tools/website_admin.py</code> 根据仓库源文档自动生成。</p>
        </div>
    </section>
    <main class="doc-shell">
        <article class="doc-card">
{body_html}
        </article>
    </main>
</body>
</html>
"""


def build_vscript_doc() -> None:
    DOCS_DIR.mkdir(parents=True, exist_ok=True)
    markdown = VSCRIPT_MD.read_text(encoding="utf-8")
    html_body = markdown_to_html(markdown)
    VSCRIPT_HTML.write_text(wrap_doc_html("VScript 手册", html_body), encoding="utf-8")


def sort_resources(resources: list[dict]) -> list[dict]:
    return sorted(resources, key=lambda item: (item.get("priority", 999), item.get("title", "")))


def list_resources() -> None:
    resources = sort_resources(read_resources())
    for item in resources:
        print(f'{item["id"]}: {item["title"]} | tags={",".join(item.get("tags", []))} | url={item["url"]}')


def add_resource(args: argparse.Namespace) -> None:
    resources = read_resources()
    resource_id = args.id or slugify(args.title)
    resources = [item for item in resources if item["id"] != resource_id]
    resources.append({
        "id": resource_id,
        "priority": args.priority,
        "title": args.title,
        "summary": args.summary,
        "provider": args.provider,
        "url": args.url,
        "cta": args.cta,
        "tags": [tag.strip() for tag in args.tags.split(",") if tag.strip()],
    })
    resources = sort_resources(resources)
    write_resources(resources)
    build_resource_js(resources)


def add_script_resource(args: argparse.Namespace) -> None:
    tags = ["脚本"]
    if args.tags:
        tags.extend(tag.strip() for tag in args.tags.split(",") if tag.strip())
    add_resource(argparse.Namespace(
        id=args.id,
        priority=args.priority,
        title=args.title,
        summary=args.summary,
        provider=args.provider,
        url=args.url,
        cta=args.cta,
        tags=",".join(tags),
    ))


def remove_resource(args: argparse.Namespace) -> None:
    resources = [item for item in read_resources() if item["id"] != args.id]
    write_resources(resources)
    build_resource_js(resources)


def build_all() -> None:
    resources = sort_resources(read_resources())
    write_resources(resources)
    build_resource_js(resources)
    build_vscript_doc()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="StrikeSense website admin tool")
    sub = parser.add_subparsers(dest="command", required=True)

    sub.add_parser("build-all")
    sub.add_parser("build-vscript-doc")
    sub.add_parser("list-resources")

    add = sub.add_parser("add-resource")
    add.add_argument("--id")
    add.add_argument("--priority", type=int, default=999)
    add.add_argument("--title", required=True)
    add.add_argument("--summary", required=True)
    add.add_argument("--provider", default="StrikeSense")
    add.add_argument("--url", required=True)
    add.add_argument("--cta", default="打开")
    add.add_argument("--tags", default="")

    add_script = sub.add_parser("add-script-resource")
    add_script.add_argument("--id")
    add_script.add_argument("--priority", type=int, default=999)
    add_script.add_argument("--title", required=True)
    add_script.add_argument("--summary", required=True)
    add_script.add_argument("--provider", default="StrikeSense")
    add_script.add_argument("--url", required=True)
    add_script.add_argument("--cta", default="查看")
    add_script.add_argument("--tags", default="")

    remove = sub.add_parser("remove-resource")
    remove.add_argument("--id", required=True)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    if args.command == "build-all":
        build_all()
    elif args.command == "build-vscript-doc":
        build_vscript_doc()
    elif args.command == "list-resources":
        list_resources()
    elif args.command == "add-resource":
        add_resource(args)
    elif args.command == "add-script-resource":
        add_script_resource(args)
    elif args.command == "remove-resource":
        remove_resource(args)


if __name__ == "__main__":
    main()
