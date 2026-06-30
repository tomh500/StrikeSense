document.addEventListener('DOMContentLoaded', () => {
    
    // ====== 💎 1. 手机端侧边汉堡菜单抽屉交互 ======
    const menuToggle = document.getElementById('menuToggle');
    const navLinks = document.getElementById('navLinks');
    const links = navLinks.querySelectorAll('a');

    // 点击按钮切换菜单
    menuToggle.addEventListener('click', () => {
        menuToggle.classList.toggle('open');
        navLinks.classList.toggle('open');
    });

    // 点击菜单内任意跳转链接后，自动关闭抽屉
    links.forEach(link => {
        link.addEventListener('click', () => {
            menuToggle.classList.remove('open');
            navLinks.classList.remove('open');
        });
    });

// ====== 💎 2. 常见问题 Q&A 手风琴折叠效果 ======
    const faqItems = document.querySelectorAll('.faq-item');

    faqItems.forEach(item => {
        const question = item.querySelector('.faq-question');
        const answer = item.querySelector('.faq-answer');
        
        question.addEventListener('click', (e) => {
            // 防止点击事件冒泡到内部的其他卡片元素
            e.stopPropagation(); 
            
            // 检查当前点击的这一项是不是已经打开了
            const isActive = item.classList.contains('active');
            
            // 1. 【核心修复】只关闭“其他”选项，互不干扰
            faqItems.forEach(i => {
                if (i !== item) {
                    i.classList.remove('active');
                }
            });
            
            // 2. 切换当前项的状态：如果原来是开的就关掉，原来是关的就打开
            if (isActive) {
                item.classList.remove('active');
            } else {
                item.classList.add('active');
            }
        });
    });

    // 默认保持展开第1个“为什么选择我们（对比同行）的核心优势声明”，一打开网页就形成冲击力
    if (faqItems.length >= 1) {
        faqItems[0].classList.add('active');
    }

    // ====== 💎 3. 滚动动态半透明毛玻璃导航栏效果 ======
    const navbar = document.querySelector('.navbar');
    window.addEventListener('scroll', () => {
        if (window.scrollY > 40) {
            navbar.style.boxShadow = '0 8px 25px rgba(9, 114, 122, 0.08)';
            navbar.style.backgroundColor = 'rgba(243, 250, 252, 0.95)';
        } else {
            navbar.style.boxShadow = 'none';
            navbar.style.backgroundColor = 'rgba(243, 250, 252, 0.85)';
        }
    });

    // 下载版本选择弹窗
    const downloadModal = document.getElementById('downloadModal');
    const nightlyList = document.getElementById('nightlyList');
    const showNightlyFiles = document.getElementById('showNightlyFiles');
    const downloadButtons = document.querySelectorAll('.js-download-choice');

    const formatFileSize = (bytes) => {
        if (!Number.isFinite(bytes) || bytes <= 0) return '大小未知';
        const units = ['B', 'KB', 'MB', 'GB'];
        let size = bytes;
        let unitIndex = 0;
        while (size >= 1024 && unitIndex < units.length - 1) {
            size /= 1024;
            unitIndex += 1;
        }
        return `${size.toFixed(size >= 10 || unitIndex === 0 ? 0 : 1)} ${units[unitIndex]}`;
    };

    const openDownloadModal = () => {
        downloadModal.classList.add('open');
        downloadModal.setAttribute('aria-hidden', 'false');
        nightlyList.hidden = true;
        nightlyList.innerHTML = '';
    };

    const closeDownloadModal = () => {
        downloadModal.classList.remove('open');
        downloadModal.setAttribute('aria-hidden', 'true');
    };

    downloadButtons.forEach((button) => {
        button.addEventListener('click', (event) => {
            event.preventDefault();
            openDownloadModal();
        });
    });

    document.querySelectorAll('[data-download-close]').forEach((button) => {
        button.addEventListener('click', closeDownloadModal);
    });

    showNightlyFiles.addEventListener('click', async () => {
        nightlyList.hidden = false;
        nightlyList.innerHTML = '<p class="nightly-list__status">正在检测夜间版文件...</p>';

        try {
            const response = await fetch('app/nightly-manifest.json', { cache: 'no-store' });
            if (!response.ok) throw new Error(`HTTP ${response.status}`);
            const manifest = await response.json();
            const files = Array.isArray(manifest.files) ? manifest.files : [];

            if (files.length === 0) {
                nightlyList.innerHTML = '<p class="nightly-list__status">当前没有检测到夜间版文件。</p>';
                return;
            }

            const fileRows = files.map((file) => `
                <a class="nightly-file" href="${file.path}" download>
                    <span>
                        <strong>${file.name}</strong>
                        <small>${formatFileSize(file.size)}</small>
                    </span>
                    <span class="nightly-file__download">下载</span>
                </a>
            `).join('');
            nightlyList.innerHTML = `<div class="nightly-list__header">检测到 ${files.length} 个夜间版文件</div>${fileRows}`;
        } catch (error) {
            nightlyList.innerHTML = '<p class="nightly-list__status">夜间版清单读取失败，请稍后再试。</p>';
            console.log('夜间版清单读取失败：', error);
        }
    });
});
