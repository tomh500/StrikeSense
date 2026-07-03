import fs from "node:fs/promises";
import path from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const markedModulePath = pathToFileURL(
  "C:/Users/jingy/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/marked/lib/marked.esm.js",
).href;
const { marked } = await import(markedModulePath);

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const workspaceRoot = __dirname;
const externalRoot = "D:/Users/user0/Downloads/Misc/strikesense.web";
const markdownPath = path.join(workspaceRoot, "VSCRIPT_MANUAL.md");
const indexPath = path.join(externalRoot, "index.html");
const outputPath = path.join(externalRoot, "scriptdocs.html");

function extractShell(indexHtml) {
  const header = indexHtml.match(/<header class="navbar">[\s\S]*?<\/header>/)?.[0];
  const footer = indexHtml.match(/<footer[\s\S]*?<\/footer>/)?.[0];
  if (!header || !footer) {
    throw new Error("未能从 index.html 提取公共 header/footer");
  }
  return { header, footer };
}

function slugify(text, fallbackIndex) {
  const slug = text
    .toLowerCase()
    .replace(/<[^>]+>/g, "")
    .replace(/[^\p{Letter}\p{Number}\s-]/gu, "")
    .trim()
    .replace(/\s+/g, "-");
  return slug || `sec-${fallbackIndex}`;
}

function buildToc(tokens) {
  const toc = [];
  let index = 0;
  for (const token of tokens) {
    if (token.type !== "heading" || (token.depth !== 2 && token.depth !== 3)) continue;
    index += 1;
    const rawText = token.text ?? "";
    toc.push({
      depth: token.depth,
      text: rawText,
      id: `sec-${index}-${slugify(rawText, index)}`,
    });
  }
  return toc;
}

function createRenderer(toc) {
  const headingIds = new Map(toc.map((item) => [`${item.depth}:${item.text}`, item.id]));
  const renderer = new marked.Renderer();

  renderer.heading = ({ tokens, depth, text }) => {
    const htmlText = marked.parser(tokens);
    const key = `${depth}:${text}`;
    const id = headingIds.get(key) ?? slugify(text, depth);
    return `<h${depth} id="${id}">${htmlText}</h${depth}>`;
  };

  return renderer;
}

function buildSidebar(toc) {
  return toc
    .map((item) => {
      const cls = item.depth === 3 ? "toc-link toc-sub" : "toc-link";
      const safeText = item.text
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;");
      return `<a class="${cls}" href="#${item.id}" data-search="${safeText.toLowerCase()}">${safeText}</a>`;
    })
    .join("");
}

function buildDocument({ header, footer, bodyHtml, sidebarHtml }) {
  return `<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>StrikeSense - 脚本文档</title>
    <link rel="stylesheet" href="style.css">
    <style>
        .docs-shell {
            max-width: 1380px;
            margin: 0 auto;
            padding: 125px 20px 70px;
            display: grid;
            grid-template-columns: minmax(0, 1fr) 320px;
            gap: 24px;
            align-items: start;
        }
        .docs-sidebar {
            position: sticky;
            top: 95px;
            background: var(--bg-card);
            border: 1px solid rgba(9, 114, 122, 0.08);
            border-radius: 12px;
            box-shadow: 0 10px 28px rgba(9, 114, 122, 0.05);
            padding: 18px;
        }
        .docs-sidebar h2 {
            font-size: 18px;
            margin-bottom: 14px;
            color: var(--text-main);
        }
        .docs-search {
            width: 100%;
            min-height: 42px;
            border: 1px solid rgba(9, 114, 122, 0.15);
            border-radius: 10px;
            padding: 10px 12px;
            background: var(--bg-main);
            color: var(--text-main);
            margin-bottom: 14px;
            outline: none;
        }
        .docs-search:focus {
            border-color: var(--col-tb);
            box-shadow: 0 0 0 3px rgba(0, 153, 255, 0.1);
        }
        .docs-toc {
            display: grid;
            gap: 8px;
            max-height: calc(100vh - 180px);
            overflow: auto;
        }
        .toc-link {
            display: block;
            text-decoration: none;
            color: var(--text-main);
            background: var(--bg-main);
            border: 1px solid transparent;
            border-radius: 10px;
            padding: 9px 11px;
            transition: 0.2s ease;
        }
        .toc-link:hover {
            border-color: var(--col-bp);
            color: var(--col-td);
        }
        .toc-sub {
            margin-left: 12px;
            font-size: 14px;
        }
        .docs-note {
            margin-top: 12px;
            color: var(--text-dim);
            font-size: 13px;
        }
        .docs-hidden {
            display: none !important;
        }
        .markdown-body {
            max-width: none;
            margin: 0;
            scroll-margin-top: 95px;
        }
        .markdown-body h1,
        .markdown-body h2,
        .markdown-body h3,
        .markdown-body h4 {
            scroll-margin-top: 95px;
        }
        .markdown-body pre {
            background: #1e1e1e;
            color: #d4d4d4;
        }
        @media (max-width: 980px) {
            .docs-shell {
                grid-template-columns: 1fr;
            }
            .docs-sidebar {
                position: static;
                order: -1;
            }
            .docs-toc {
                max-height: none;
            }
        }
    </style>
</head>
<body>
    ${header}
    <main class="docs-shell">
        <article class="markdown-body" id="docsBody">
${bodyHtml}
        </article>
        <aside class="docs-sidebar">
            <h2>快速索引</h2>
            <input id="docsSearch" class="docs-search" type="search" placeholder="搜索变量、函数、关键字..." />
            <nav class="docs-toc" id="docsToc">
                ${sidebarHtml}
            </nav>
            <p class="docs-note">搜索会同步过滤右侧目录，并只保留匹配章节。</p>
        </aside>
    </main>
    ${footer}
    <script src="script.js"></script>
    <script>
        (() => {
            const search = document.getElementById("docsSearch");
            const links = Array.from(document.querySelectorAll("#docsToc .toc-link"));
            const headingMap = new Map(
                links.map((link) => [link, document.getElementById(link.getAttribute("href").slice(1))]),
            );

            function nextHeadingElement(current) {
                let node = current?.nextElementSibling ?? null;
                while (node) {
                    if (/^H[1-6]$/.test(node.tagName)) return node;
                    node = node.nextElementSibling;
                }
                return null;
            }

            function collectSectionText(heading) {
                let text = heading?.textContent || "";
                let node = heading?.nextElementSibling ?? null;
                while (node && !/^H[1-6]$/.test(node.tagName)) {
                    text += " " + (node.textContent || "");
                    node = node.nextElementSibling;
                }
                return text.toLowerCase();
            }

            const sectionTexts = new Map(
                links.map((link) => {
                    const heading = headingMap.get(link);
                    return [link, collectSectionText(heading)];
                }),
            );

            function toggleSection(heading, visible) {
                if (!heading) return;
                let node = heading;
                const stop = nextHeadingElement(heading);
                while (node && node !== stop) {
                    node.classList.toggle("docs-hidden", !visible);
                    node = node.nextElementSibling;
                }
            }

            function applyFilter() {
                const keyword = (search.value || "").trim().toLowerCase();
                links.forEach((link) => {
                    const matched = !keyword || (sectionTexts.get(link) || "").includes(keyword);
                    link.classList.toggle("docs-hidden", !matched);
                    toggleSection(headingMap.get(link), matched);
                });
            }

            search.addEventListener("input", applyFilter);
        })();
    </script>
</body>
</html>`;
}

async function main() {
  const [mdText, indexHtml] = await Promise.all([
    fs.readFile(markdownPath, "utf8"),
    fs.readFile(indexPath, "utf8"),
  ]);

  const tokens = marked.lexer(mdText);
  const toc = buildToc(tokens);
  const renderer = createRenderer(toc);
  const bodyHtml = marked.parse(mdText, {
    renderer,
    gfm: true,
    breaks: false,
  });
  const sidebarHtml = buildSidebar(toc);
  const { header, footer } = extractShell(indexHtml);
  const outputHtml = buildDocument({ header, footer, bodyHtml, sidebarHtml });
  await fs.writeFile(outputPath, outputHtml, "utf8");
  console.log(`已生成文档: ${outputPath}`);
}

await main();
