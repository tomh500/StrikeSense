/**
 * 💎 StrikeSense 资源中心联动过滤及动态数据分页引擎
 */

// 1. 模拟核心数据库（包含：文件名、预览图路径、简介、分类多标签组、提供者、模拟压缩包下载链接）
const RESOURCE_DATABASE = [
    {
        id: 1,
        filename: "音乐操作台",
        img: "img/rescenter/musictool.png", // 示范音乐盒图
        desc: "一个可视化的音乐操作台，可以快速剪切音频和导出音频，需要安装FFmpeg、Python3.13更新版本和PyQt6 PyQt6-multimedia依赖库。可用于制作自定义的游戏提示音效包。本脚本由AI辅助编写。",
        tags: ["官方", "其他"],
        provider: "StrikeSense 运营团队",
        downloadUrl: "app/rescenter/musictool.py"
    },
    {
        id: 2,
        filename: "CSGO:LEGACY MUSIC KIT",
        img: "img/rescenter/CSGO.png", // 示范音乐盒图
        desc: "【官方经典】《反恐精英：全球攻势》经典音乐包，包含丰富的背景音乐和特效音效，完美还原游戏原声。",
        tags: ["官方", "音乐盒"],
        provider: "StrikeSense 运营团队",
        downloadUrl: "app/rescenter/CSGO_Legacy.7z"
    },
    {
        id: 3,
        filename: "CF穿越火线音乐盒",
        img: "img/rescenter/CF_MUSIC.png", // 示范音乐盒图
        desc: "《穿越火线》经典音乐包，包含丰富的背景音乐和特效音效，完美还原游戏原声。",
        tags: ["社区", "音乐盒"],
        provider: "无损平方集团",
        downloadUrl: "app/rescenter/CF_MUSIC.7z"
    },
    {
        id: 4,
        filename: "迷你瓦垃圾击杀音效",
        img: "img/rescenter/minivarage.png",
        desc: "迷你世界击杀语音+瓦罗兰特经典击杀音效。",
        tags: ["社区", "击杀音效"],
        provider: "无损平方集团",
        downloadUrl: "app/rescenter/minivarage.7z"
    },
       {
        id: 5,
        filename: "人生何处不青山",
        img: "img/rescenter/人生何处不青山.jpg",
        desc: "CS2的官方音乐盒。",
        tags: ["官方", "音乐盒"],
        provider: "StrikeSense 运营团队",
        downloadUrl: "app/rescenter/人生何处不青山.7z"
    },
           {
        id: 6,
        filename: "终极",
        img: "img/rescenter/终极.jpg",
        desc: "CS2的官方音乐盒。",
        tags: ["官方", "音乐盒"],
        provider: "StrikeSense 运营团队",
        downloadUrl: "app/rescenter/终极.7z"
    },
    {
        id: 7,
        filename: "理由",
        img: "img/rescenter/理由.png",
        desc: "CS2的官方音乐盒。",
        tags: ["官方", "音乐盒"],
        provider: "StrikeSense 运营团队",
        downloadUrl: "app/rescenter/理由.7z"
    },
    {   id: 8,
        filename: "燥起来",
        img: "img/rescenter/燥起来.png",
        desc: "CS2的官方音乐盒。",
        tags: ["官方", "音乐盒"],
        provider: "StrikeSense 运营团队",
        downloadUrl: "app/rescenter/燥起来.7z"
    },
    
];

// 2. 分页及过滤器核心状态配置
const CONFIG = {
    itemsPerPage: 3,     // 示范每页展示 3 条数据（超过 3 条自动生成分页导航）
    currentPage: 1,      // 当前初始页码
    selectedTags: [],    // 选中的标签（空数组代表不限标签，多选时为“且/或”相交过滤）
    searchKeyword: ""    // 实时搜索关键词
};

// 3. 缓存 DOM 节点
const doms = {
    resGrid: document.getElementById('resGrid'),
    paginationWrap: document.getElementById('paginationWrap'),
    searchInput: document.getElementById('searchInput'),
    tagsGroup: document.getElementById('tagsGroup')
};

// 4. 数据过滤器逻辑
function getFilteredData() {
    // 1. 先进行过滤
    let data = RESOURCE_DATABASE.filter(item => {
        const kw = CONFIG.searchKeyword.toLowerCase().trim();
        const matchesKeyword = !kw || 
            item.filename.toLowerCase().includes(kw) ||
            item.desc.toLowerCase().includes(kw) ||
            item.provider.toLowerCase().includes(kw);
        
        const matchesTags = CONFIG.selectedTags.length === 0 || 
            CONFIG.selectedTags.every(t => item.tags.includes(t));
            
        return matchesKeyword && matchesTags;
    });

    // 2. 进行优先级排序
    data.sort((a, b) => {
        const getPriority = (item) => {
            if (item.tags.includes('官方')) return 1; // 官方最高优先级
            if (item.tags.includes('社区')) return 2; // 社区次之
            return 3;                                // 其他排最后
        };
        
        return getPriority(a) - getPriority(b);
    });

    return data;
}

// 5. 渲染卡片视图
function renderCards(data) {
    doms.resGrid.innerHTML = "";
    
    if (data.length === 0) {
        doms.resGrid.innerHTML = `<div class="empty-tips">🔍 没有找到符合当前过滤条件的扩展资源，换个关键词试试吧！</div>`;
        return;
    }

    // 计算分页指针切片
    const startIndex = (CONFIG.currentPage - 1) * CONFIG.itemsPerPage;
    const endIndex = startIndex + CONFIG.itemsPerPage;
    const pageData = data.slice(startIndex, endIndex);

    // 拼装 HTML
    pageData.forEach(item => {
        // 生成内部小标签 HTML
        const tagsHtml = item.tags.map(t => {
            const isOfficial = t === '官方' ? 'tag-official' : '';
            return `<span class="inner-tag ${isOfficial}">${t}</span>`;
        }).join('');

        const card = document.createElement('div');
        card.className = 'res-card';
        card.innerHTML = `
            <div class="res-img-box">
                <img src="${item.img}" alt="${item.filename}" onerror="this.style.display='none'">
            </div>
            <div class="res-info-box">
                <div class="res-filename">${item.filename}</div>
                <div class="res-card-tags">${tagsHtml}</div>
                <p class="res-desc">${item.desc}</p>
                <div class="res-footer-meta">
                    <span class="res-provider">贡献者: <strong>${item.provider}</strong></span>
                    <a href="${item.downloadUrl}" class="btn-download" download>下载</a>
                </div>
            </div>
        `;
        doms.resGrid.appendChild(card);
    });
}


// 7. 更新视图核心封装
function updateView() {
    const filteredData = getFilteredData();
    
    // 边界安全校验：防止切换分类或搜索后，总页数暴跌导致旧页码越界
    const totalPages = Math.ceil(filteredData.length / CONFIG.itemsPerPage);
    if (CONFIG.currentPage > totalPages && totalPages > 0) {
        CONFIG.currentPage = totalPages;
    }

    renderCards(filteredData);
    renderPagination(filteredData.length);
}

// 8. 监听器初始化
function initListeners() {
    // A. 搜索框文本键入防抖式监听
   let debounceTimer;
doms.searchInput.addEventListener('input', (e) => {
    clearTimeout(debounceTimer);
    debounceTimer = setTimeout(() => {
        CONFIG.searchKeyword = e.target.value;
        CONFIG.currentPage = 1;
        updateView();
    }, 300); // 延迟 300ms 执行
});

    // B. 分类标签多选点击事件交互
    const tagButtons = doms.tagsGroup.querySelectorAll('.filter-tag');
    tagButtons.forEach(btn => {
        btn.addEventListener('click', () => {
            const tagValue = btn.getAttribute('data-tag');
            
            // 切换多选选中状态
            if (CONFIG.selectedTags.includes(tagValue)) {
                // 如果已存在则移除
                CONFIG.selectedTags = CONFIG.selectedTags.filter(t => t !== tagValue);
                btn.classList.remove('active');
            } else {
                // 如果不存在则追加
                CONFIG.selectedTags.push(tagValue);
                btn.classList.add('active');
            }
            
            CONFIG.currentPage = 1; // 重置页码
            updateView();
        });
    });
}

function getPaginationNumbers(currentPage, totalPages) {
    const delta = 2; // 当前页前后显示的页码数量
    const range = [];
    const rangeWithDots = [];
    let l;

    // 始终显示第一页、最后一页、当前页及前后各 delta 页
    for (let i = 1; i <= totalPages; i++) {
        if (i === 1 || i === totalPages || (i >= currentPage - delta && i <= currentPage + delta)) {
            range.push(i);
        }
    }

    // 在缺口处插入省略号
    for (let i of range) {
        if (l) {
            if (i - l === 2) {
                rangeWithDots.push(l + 1);
            } else if (i - l !== 1) {
                rangeWithDots.push('...');
            }
        }
        rangeWithDots.push(i);
        l = i;
    }
    return rangeWithDots;
}

function renderPagination(totalItems) {
    doms.paginationWrap.innerHTML = "";
    const totalPages = Math.ceil(totalItems / CONFIG.itemsPerPage);
    if (totalPages <= 1) return;

    const pages = getPaginationNumbers(CONFIG.currentPage, totalPages);

    // 上一页按钮
    const prevBtn = createBtn('‹', CONFIG.currentPage > 1, () => {
        CONFIG.currentPage--;
        updateView();
    });
    doms.paginationWrap.appendChild(prevBtn);

    // 渲染页码按钮
    pages.forEach(p => {
        if (p === '...') {
            const span = document.createElement('span');
            span.className = 'page-dots';
            span.innerText = '...';
            doms.paginationWrap.appendChild(span);
        } else {
            const btn = createBtn(p, true, () => {
                CONFIG.currentPage = p;
                updateView();
            }, CONFIG.currentPage === p);
            doms.paginationWrap.appendChild(btn);
        }
    });

    // 下一页按钮
    const nextBtn = createBtn('›', CONFIG.currentPage < totalPages, () => {
        CONFIG.currentPage++;
        updateView();
    });
    doms.paginationWrap.appendChild(nextBtn);
}

// 辅助创建按钮的函数（精简代码）
function createBtn(text, enabled, onClick, isActive = false) {
    const btn = document.createElement('button');
    btn.className = `page-btn ${isActive ? 'active' : ''}`;
    btn.innerText = text;
    btn.disabled = !enabled;
    if (enabled) btn.addEventListener('click', onClick);
    return btn;
}

// 入口初始化启动
document.addEventListener('DOMContentLoaded', () => {
    initListeners();
    updateView();
});