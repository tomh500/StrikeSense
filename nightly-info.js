document.addEventListener('DOMContentLoaded', () => {
    const nightlyBtn = document.getElementById('showNightlyFiles');
    const nightlyTooltip = document.getElementById('nightlyTooltip');
    const tooltipLoading = document.getElementById('tooltipLoading');
    const tooltipContent = document.getElementById('tooltipContent');

    let isFetched = false;

    // 鼠标移入按钮
    nightlyBtn.addEventListener('mouseenter', async () => {
        nightlyTooltip.style.display = 'block'; // 显示浮层

        if (!isFetched) {
            try {
                // ！！！核心：请将此处替换为真实的 JSON 在线绝对路径或 GitHub Raw 链接！！！
                const response = await fetch('https://raw.githubusercontent.com/tomh500/StrikeSense/refs/heads/sspage/app/nightly-info.json');
                
                if (!response.ok) throw new Error('Status code not 200');
                const data = await response.json();

                // 隐藏加载中文本
                tooltipLoading.style.display = 'none';

                // 组装开发者改动日志 HTML
                let html = `<b style="color: #f59e0b; font-size: 14px;">最新改动 (${data.version || 'Nightly'})</b>`;
                html += `<div style="margin: 6px 0; font-size: 11px; color: #71717a;">更新于: ${data.last_updated || '近期'}</div>`;
                html += `<ul style="margin: 0; padding-left: 16px; color: #e4e4e7; line-height: 1.5;">`;
                
                if (data.changes && data.changes.length > 0) {
                    data.changes.forEach(item => {
                        html += `<li style="margin-bottom: 4px;">${item}</li>`;
                    });
                } else {
                    html += `<li>包含一些常规内部优化</li>`;
                }
                html += `</ul>`;

                tooltipContent.innerHTML = html;
                isFetched = true; // 标记成功，下次移入不用再重复请求
            } catch (error) {
                console.error('夜间版清单获取失败:', error);
                tooltipLoading.innerHTML = `<span style="color: #ef4444;">⚠️ 未能实时加载改动清单<br><span style="color: #a1a1aa; font-size:11px;">GitHub 偶尔网络波动，可正常点击按钮下载。</span></span>`;
            }
        }
    });

    // 鼠标移出按钮
    nightlyBtn.addEventListener('mouseleave', () => {
        nightlyTooltip.style.display = 'none';
    });
});