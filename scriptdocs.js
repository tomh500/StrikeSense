function escapeHtml(value) {
    return value
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#39;');
}

function renderInline(markdown) {
    return escapeHtml(markdown)
        .replace(/`([^`]+)`/g, '<code>$1</code>')
        .replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>');
}

function closeList(html, listState) {
    if (!listState.type) return;
    html.push(`</${listState.type}>`);
    listState.type = '';
}

function openList(html, listState, type) {
    if (listState.type === type) return;
    closeList(html, listState);
    html.push(`<${type}>`);
    listState.type = type;
}

function renderMarkdown(markdown) {
    const lines = markdown.replace(/\r\n/g, '\n').split('\n');
    const html = [];
    const listState = { type: '' };
    let inCode = false;
    let codeLanguage = '';
    let codeLines = [];

    const flushCode = () => {
        html.push(`<pre><code class="language-${escapeHtml(codeLanguage)}">${escapeHtml(codeLines.join('\n'))}</code></pre>`);
        inCode = false;
        codeLanguage = '';
        codeLines = [];
    };

    for (const line of lines) {
        const codeFence = line.match(/^```(.*)$/);
        if (codeFence) {
            if (inCode) {
                flushCode();
            } else {
                closeList(html, listState);
                inCode = true;
                codeLanguage = codeFence[1].trim();
                codeLines = [];
            }
            continue;
        }

        if (inCode) {
            codeLines.push(line);
            continue;
        }

        if (!line.trim()) {
            closeList(html, listState);
            continue;
        }

        const heading = line.match(/^(#{1,6})\s+(.*)$/);
        if (heading) {
            closeList(html, listState);
            const level = heading[1].length;
            html.push(`<h${level}>${renderInline(heading[2])}</h${level}>`);
            continue;
        }

        const ordered = line.match(/^\s*\d+\.\s+(.*)$/);
        if (ordered) {
            openList(html, listState, 'ol');
            html.push(`<li>${renderInline(ordered[1])}</li>`);
            continue;
        }

        const unordered = line.match(/^\s*[-*]\s+(.*)$/);
        if (unordered) {
            openList(html, listState, 'ul');
            html.push(`<li>${renderInline(unordered[1])}</li>`);
            continue;
        }

        closeList(html, listState);
        html.push(`<p>${renderInline(line)}</p>`);
    }

    if (inCode) flushCode();
    closeList(html, listState);
    return html.join('\n');
}

document.addEventListener('DOMContentLoaded', async () => {
    const target = document.getElementById('markdownBody');
    try {
        const response = await fetch('VSCRIPT_MANUAL.md', { cache: 'no-store' });
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        target.innerHTML = renderMarkdown(await response.text());
    } catch (error) {
        target.innerHTML = '<p>脚本文档加载失败，请稍后再试。</p>';
        console.log('脚本文档加载失败：', error);
    }
});
