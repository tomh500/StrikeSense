import markdown
import re


def build_docs():
    with open("VSCRIPT_MANUAL.md", "r", encoding="utf-8") as f:
        md_text = f.read()

    with open("index.html", "r", encoding="utf-8") as f:
        index_html = f.read()

    header = re.search(r"<header class=\"navbar\">.*?</header>", index_html, re.S).group(0)
    footer = re.search(r"<footer.*?</footer>", index_html, re.S).group(0)

    html_body = markdown.markdown(md_text, extensions=["fenced_code", "tables", "attr_list"])

    full_html = f"""<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>StrikeSense - VScript 文档</title>
    <link rel="stylesheet" href="style.css">
    <style>
        .docs-container {{ max-width: 900px; margin: 80px auto; padding: 20px; }}
        pre {{ background: #1e1e1e; color: #d4d4d4; padding: 15px; border-radius: 8px; overflow-x: auto; }}
        code {{ background: #f0f0f0; padding: 2px 4px; border-radius: 4px; color: #c7254e; }}
        table {{ border-collapse: collapse; width: 100%; margin: 20px 0; }}
        th, td {{ border: 1px solid #ddd; padding: 10px; }}
    </style>
</head>
<body>
    {header}
    <main class="docs-container markdown-body">
        {html_body}
    </main>
    {footer}
    <script src="script.js"></script>
</body>
</html>"""

    with open("vscriptdocs.html", "w", encoding="utf-8") as f:
        f.write(full_html)

    print("已成功生成 vscriptdocs.html")


if __name__ == "__main__":
    build_docs()
