document.addEventListener('DOMContentLoaded', () => {
    const body = document.body;
    const menuToggle = document.getElementById('menuToggle');
    const navLinks = document.getElementById('navLinks');
    const links = navLinks ? navLinks.querySelectorAll('a') : [];

    const createSiteLoader = () => {
        if (!body) return null;
        const loader = document.createElement('div');
        loader.className = 'site-loader';
        loader.innerHTML = `
            <div class="site-loader__panel">
                <span class="site-loader__badge">StrikeSense</span>
                <strong>正在载入工作台站点</strong>
                <div class="site-loader__bar"><span></span></div>
            </div>
        `;
        body.appendChild(loader);
        return loader;
    };

    const revealTargets = () => {
        const selectors = [
            '.hero-publish__copy > *',
            '.hero-publish__visual > *',
            'body > section',
            'main > *',
            '.section-header',
            '.publish-summary-card',
            '.about-text--publish',
            '.preview-shell',
            '.preview-flow-card',
            '.capability-card',
            '.faq-item',
            '.docs-entry-card',
            '.docs-hub-guide',
            '.markdown-body',
            '.tool-panel',
            '.docs-shell',
            '.redirect-card',
            '.res-hero',
            '.res-main-section',
            '.itemmaker-main',
            '.studio-shell'
        ];
        const seen = new Set();
        const elements = [];

        selectors.forEach((selector) => {
            document.querySelectorAll(selector).forEach((element) => {
                if (seen.has(element)) return;
                seen.add(element);
                element.classList.add('reveal-up');
                elements.push(element);
            });
        });

        if (elements.length === 0) return;

        const observer = new IntersectionObserver((entries, currentObserver) => {
            entries.forEach((entry) => {
                if (!entry.isIntersecting) return;
                entry.target.classList.add('is-visible');
                currentObserver.unobserve(entry.target);
            });
        }, {
            threshold: 0.12,
            rootMargin: '0px 0px -8% 0px'
        });

        elements.forEach((element, index) => {
            element.style.setProperty('--reveal-delay', `${Math.min(index % 6, 5) * 55}ms`);
            if (element.getBoundingClientRect().top < window.innerHeight * 0.82) {
                window.setTimeout(() => {
                    element.classList.add('is-visible');
                }, Math.min(index, 6) * 60 + 80);
                return;
            }
            observer.observe(element);
        });
    };

    const initPageMotion = () => {
        if (!body) return;

        body.classList.add('page-motion');
        const loader = createSiteLoader();

        const finishEnter = () => {
            body.classList.add('page-motion--ready');
            if (loader) {
                loader.classList.add('site-loader--done');
                window.setTimeout(() => loader.remove(), 620);
            }
        };

        requestAnimationFrame(() => {
            body.classList.add('page-motion--enter');
        });

        if (document.readyState === 'complete') {
            window.setTimeout(finishEnter, 120);
        } else {
            window.addEventListener('load', () => {
                window.setTimeout(finishEnter, 150);
            }, { once: true });
            window.setTimeout(finishEnter, 1400);
        }

        document.querySelectorAll('a[href]').forEach((anchor) => {
            anchor.addEventListener('click', (event) => {
                if (event.defaultPrevented) return;
                if (event.button !== 0) return;
                if (event.metaKey || event.ctrlKey || event.shiftKey || event.altKey) return;
                if (anchor.target && anchor.target !== '_self') return;
                if (anchor.hasAttribute('download')) return;

                const rawHref = anchor.getAttribute('href');
                if (!rawHref || rawHref.startsWith('#') || rawHref.startsWith('javascript:')) return;

                const targetUrl = new URL(anchor.href, window.location.href);
                if (targetUrl.origin !== window.location.origin) return;
                if (!/\.html?$/i.test(targetUrl.pathname)) return;
                if (targetUrl.pathname === window.location.pathname && targetUrl.hash) return;

                event.preventDefault();
                body.classList.add('page-leaving');
                window.setTimeout(() => {
                    window.location.href = targetUrl.href;
                }, 220);
            });
        });
    };

    initPageMotion();
    revealTargets();

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

    const extractNightlyTimestamp = (name, fallbackModified = 0) => {
        const match = typeof name === 'string' ? name.match(/(\d{12})(?=\.exe$)/i) : null;
        if (match) {
            return Number.parseInt(match[1], 10);
        }
        return Number.isFinite(fallbackModified) ? fallbackModified : 0;
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

    if (downloadModal && nightlyList && showNightlyFiles && downloadButtons.length > 0) {
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
                const files = Array.isArray(manifest.files) ? [...manifest.files] : [];

                if (files.length === 0) {
                    nightlyList.innerHTML = '<p class="nightly-list__status">当前没有检测到夜间版文件。</p>';
                    return;
                }

                files.sort((left, right) => {
                    const leftValue = extractNightlyTimestamp(left.name, left.modified);
                    const rightValue = extractNightlyTimestamp(right.name, right.modified);
                    return rightValue - leftValue;
                });

                const fileRows = files.map((file, index) => `
                    <a class="nightly-file${index === 0 ? ' nightly-file--latest' : ''}" href="${file.path}" download>
                        <span>
                            <strong>${file.name}${index === 0 ? '<span class="nightly-file__badge">最新</span>' : ''}</strong>
                            <small>${formatFileSize(file.size)}</small>
                        </span>
                        <span class="nightly-file__download">下载</span>
                    </a>
                `).join('');
                nightlyList.innerHTML = `<div class="nightly-list__header">检测到 ${files.length} 个夜间版文件，已按日期从新到旧排序</div>${fileRows}`;
            } catch (error) {
                nightlyList.innerHTML = '<p class="nightly-list__status">夜间版清单读取失败，请稍后再试。</p>';
                console.log('夜间版清单读取失败：', error);
            }
        });
    }
});
