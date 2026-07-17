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
const args = process.argv.slice(2);
const markdownFileName = args[0] || "VSCRIPT_MANUAL.md";
const outputFileName = args[1] || "scriptdocs.html";
const pageTitle = args[2] || "StrikeSense - 脚本文档";
const sidebarTitle = args[3] || "快速索引";
const searchPlaceholder = args[4] || "搜索变量、函数、关键字...";
const noteText = args[5] || "子分类默认折叠，搜索时会自动展开相关分类。";
const markdownPath = path.join(workspaceRoot, markdownFileName);
const indexPath = path.join(externalRoot, "index.html");
const outputPath = path.join(externalRoot, outputFileName);

function extractShell(indexHtml) {
  const header = indexHtml.match(/<header class="navbar">[\s\S]*?<\/header>/)?.[0];
  const footer = indexHtml.match(/<footer[\s\S]*?<\/footer>/)?.[0];
  if (!header || !footer) {
    throw new Error("未能从 index.html 提取公共 header/footer");
  }

  const sanitizedFooter = footer
    .replace(/\s*<h3>[\s\S]*?<\/h3>/i, "")
    .replace(/\s*<p>[\s\S]*?Windows 10[\s\S]*?<\/p>/i, "")
    .replace(/\s*<a[^>]*href="app\/StrikeSense\.exe"[\s\S]*?<\/a>/i, "")
    .replace(/\n\s*\n/g, "\n");

  return { header, footer: sanitizedFooter };
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
    if (token.type !== "heading" || ![2, 3, 4].includes(token.depth)) continue;
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

function buildTocTree(toc) {
  const roots = [];
  let currentH2 = null;
  let currentH3 = null;

  for (const item of toc) {
    const node = { ...item, children: [] };
    if (item.depth === 2) {
      roots.push(node);
      currentH2 = node;
      currentH3 = null;
      continue;
    }
    if (item.depth === 3 && currentH2) {
      currentH2.children.push(node);
      currentH3 = node;
      continue;
    }
    if (item.depth === 4 && currentH3) {
      currentH3.children.push(node);
    }
  }

  return roots;
}

function createRenderer(toc) {
  const headingIds = new Map(toc.map((item) => [`${item.depth}:${item.text}`, item.id]));
  const renderer = new marked.Renderer();

  renderer.heading = ({ depth, text }) => {
    const key = `${depth}:${text}`;
    const id = headingIds.get(key) ?? slugify(text, depth);
    return `<h${depth} id="${id}">${text}</h${depth}>`;
  };

  return renderer;
}

function escapeHtml(text) {
  return String(text)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

function buildSidebar(toc) {
  const tree = buildTocTree(toc);
  let toggleIndex = 0;

  return tree
    .map((group) => {
      const sectionHtml = group.children
        .map((section) => {
          const toggleId = `tocChildren${++toggleIndex}`;
          const hasChildren = section.children.length > 0;
          const childrenHtml = hasChildren
            ? `<div class="toc-children docs-hidden" id="${toggleId}">
${section.children.map((item) => `<a class="toc-link toc-sub2 toc-leaf" href="#${item.id}" data-search="${escapeHtml(item.text.toLowerCase())}">${escapeHtml(item.text)}</a>`).join("")}
</div>`
            : "";

          const toggleHtml = hasChildren
            ? `<button class="toc-toggle" type="button" data-target="${toggleId}" aria-expanded="false">展开</button>`
            : "";

          return `<div class="toc-section">
  <div class="toc-row">
    <a class="toc-link toc-sub toc-branch" href="#${section.id}" data-search="${escapeHtml(section.text.toLowerCase())}">${escapeHtml(section.text)}</a>
    ${toggleHtml}
  </div>
  ${childrenHtml}
</div>`;
        })
        .join("");

      return `<div class="toc-group">
  <a class="toc-link toc-top" href="#${group.id}" data-search="${escapeHtml(group.text.toLowerCase())}">${escapeHtml(group.text)}</a>
  <div class="toc-group-children">
    ${sectionHtml}
  </div>
</div>`;
    })
    .join("");
}

function buildDocument({ header, footer, bodyHtml, sidebarHtml }) {
  return `<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>${escapeHtml(pageTitle)}</title>
    <link rel="stylesheet" href="style.css">
    <style>
        .docs-shell {
            max-width: 1440px;
            margin: 0 auto;
            padding: 125px 20px 70px;
            display: grid;
            grid-template-columns: minmax(0, 1fr) 340px;
            gap: 24px;
            align-items: start;
        }
        .docs-sidebar {
            position: sticky;
            top: 88px;
            background: var(--bg-card);
            border: 1px solid rgba(9, 114, 122, 0.08);
            border-radius: 12px;
            box-shadow: 0 10px 28px rgba(9, 114, 122, 0.05);
            padding: 14px;
            max-height: calc(100vh - 108px);
            overflow: hidden;
        }
        .docs-sidebar h2 {
            font-size: 17px;
            margin-bottom: 10px;
            color: var(--text-main);
        }
        .docs-search {
            width: 100%;
            min-height: 38px;
            border: 1px solid rgba(9, 114, 122, 0.15);
            border-radius: 10px;
            padding: 8px 11px;
            background: var(--bg-main);
            color: var(--text-main);
            margin-bottom: 10px;
            outline: none;
        }
        .docs-search:focus {
            border-color: var(--col-tb);
            box-shadow: 0 0 0 3px rgba(0, 153, 255, 0.1);
        }
        .docs-toc {
            display: grid;
            gap: 8px;
            max-height: calc(100vh - 240px);
            overflow: auto;
            padding-right: 4px;
        }
        .toc-group {
            display: grid;
            gap: 8px;
        }
        .toc-group-children {
            display: grid;
            gap: 8px;
        }
        .toc-section {
            display: grid;
            gap: 6px;
        }
        .toc-row {
            display: grid;
            grid-template-columns: minmax(0, 1fr) auto;
            gap: 8px;
            align-items: center;
        }
        .toc-children {
            display: grid;
            gap: 6px;
        }
        .toc-link {
            display: block;
            text-decoration: none;
            color: var(--text-main);
            background: var(--bg-main);
            border: 1px solid transparent;
            border-radius: 10px;
            padding: 7px 10px;
            transition: 0.2s ease;
            line-height: 1.35;
        }
        .toc-link:hover {
            border-color: var(--col-bp);
            color: var(--col-td);
        }
        .toc-sub {
            margin-left: 12px;
            font-size: 13px;
        }
        .toc-sub2 {
            margin-left: 24px;
            font-size: 12px;
            opacity: 0.94;
        }
        .toc-toggle {
            border: 1px solid rgba(9, 114, 122, 0.15);
            background: var(--bg-main);
            color: var(--text-main);
            border-radius: 9px;
            padding: 5px 8px;
            cursor: pointer;
            font-size: 12px;
            white-space: nowrap;
        }
        .toc-toggle:hover {
            border-color: var(--col-bp);
            color: var(--col-td);
        }
        .docs-note {
            margin-top: 8px;
            color: var(--text-dim);
            font-size: 12px;
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
        .markdown-body table {
            width: 100%;
            border-collapse: collapse;
            margin: 18px 0;
        }
        .markdown-body th,
        .markdown-body td {
            border: 1px solid rgba(9, 114, 122, 0.15);
            padding: 10px 12px;
            text-align: left;
            vertical-align: top;
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
            <h2>${escapeHtml(sidebarTitle)}</h2>
            <input id="docsSearch" class="docs-search" type="search" placeholder="${escapeHtml(searchPlaceholder)}" />
            <nav class="docs-toc" id="docsToc">
                ${sidebarHtml}
            </nav>
            <p class="docs-note">${escapeHtml(noteText)}</p>
        </aside>
    </main>
    ${footer}
    <script src="script.js"></script>
    <script>
        (() => {
            const search = document.getElementById("docsSearch");
            const links = Array.from(document.querySelectorAll("#docsToc .toc-link"));
            const sections = Array.from(document.querySelectorAll("#docsToc .toc-section"));
            const groups = Array.from(document.querySelectorAll("#docsToc .toc-group"));
            const toggles = Array.from(document.querySelectorAll("#docsToc .toc-toggle"));
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

            function setExpanded(button, expanded) {
                if (!button) return;
                const targetId = button.getAttribute("data-target");
                const target = targetId ? document.getElementById(targetId) : null;
                if (!target) return;
                button.setAttribute("aria-expanded", expanded ? "true" : "false");
                button.textContent = expanded ? "收起" : "展开";
                target.classList.toggle("docs-hidden", !expanded);
            }

            toggles.forEach((button) => {
                button.addEventListener("click", () => {
                    const expanded = button.getAttribute("aria-expanded") === "true";
                    setExpanded(button, !expanded);
                });
            });

            function applyFilter() {
                const keyword = (search.value || "").trim().toLowerCase();
                const isFiltering = keyword.length > 0;
                const matchedMap = new Map();

                links.forEach((link) => {
                    const matched = !isFiltering
                        || (sectionTexts.get(link) || "").includes(keyword)
                        || (link.textContent || "").toLowerCase().includes(keyword);
                    matchedMap.set(link, matched);
                });

                links.forEach((link) => {
                    const matched = matchedMap.get(link) || false;
                    if (link.classList.contains("toc-top")) {
                        link.classList.remove("docs-hidden");
                    } else if (link.classList.contains("toc-branch")) {
                        link.classList.remove("docs-hidden");
                    } else {
                        link.classList.toggle("docs-hidden", isFiltering && !matched);
                    }
                    toggleSection(headingMap.get(link), matched || !isFiltering);
                });

                sections.forEach((section) => {
                    const branchLink = section.querySelector(".toc-branch");
                    const childLinks = Array.from(section.querySelectorAll(".toc-leaf"));
                    const selfMatched = branchLink ? (matchedMap.get(branchLink) || false) : false;
                    const childMatched = childLinks.some((link) => matchedMap.get(link));
                    const visible = !isFiltering || selfMatched || childMatched;
                    section.classList.toggle("docs-hidden", !visible);

                    const toggle = section.querySelector(".toc-toggle");
                    if (toggle) {
                        setExpanded(toggle, isFiltering ? childMatched : false);
                    }
                });

                groups.forEach((group) => {
                    const topLink = group.querySelector(".toc-top");
                    const groupMatched = topLink ? (matchedMap.get(topLink) || false) : false;
                    const visibleSection = Array.from(group.querySelectorAll(".toc-section"))
                        .some((section) => !section.classList.contains("docs-hidden"));
                    const visible = !isFiltering || groupMatched || visibleSection;
                    group.classList.toggle("docs-hidden", !visible);
                });
            }

            applyFilter();
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
