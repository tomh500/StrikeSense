document.addEventListener('DOMContentLoaded', () => {
    const menuToggle = document.getElementById('menuToggle');
    const navLinks = document.getElementById('navLinks');
    const links = navLinks ? navLinks.querySelectorAll('a') : [];

    if (menuToggle && navLinks) {
        menuToggle.addEventListener('click', () => {
            menuToggle.classList.toggle('open');
            navLinks.classList.toggle('open');
        });
    }

    links.forEach((link) => {
        link.addEventListener('click', () => {
            if (!menuToggle || !navLinks) return;
            menuToggle.classList.remove('open');
            navLinks.classList.remove('open');
        });
    });

    const faqItems = Array.from(document.querySelectorAll('.faq-item'));
    faqItems.forEach((item, index) => {
        const question = item.querySelector(':scope > .faq-question');
        const answer = item.querySelector(':scope > .faq-answer');
        if (!question || !answer) return;

        item.classList.toggle('active', index === 0);

        question.addEventListener('click', (event) => {
            event.stopPropagation();
            const willOpen = !item.classList.contains('active');
            faqItems.forEach((faqItem) => faqItem.classList.remove('active'));
            if (willOpen) item.classList.add('active');
        });
    });

    const navbar = document.querySelector('.navbar');
    if (navbar) {
        const syncNavbar = () => {
            navbar.classList.toggle('is-scrolled', window.scrollY > 40);
        };
        syncNavbar();
        window.addEventListener('scroll', syncNavbar);
    }

    const downloadModal = document.getElementById('downloadModal');
    const nightlyList = document.getElementById('nightlyList');
    const showNightlyFiles = document.getElementById('showNightlyFiles');
    const downloadButtons = document.querySelectorAll('.js-download-choice');

    if (!downloadModal || !nightlyList || !showNightlyFiles || downloadButtons.length === 0) {
        return;
    }

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