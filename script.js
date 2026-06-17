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
        
        question.addEventListener('click', () => {
            const isActive = item.classList.contains('active');
            
            // 排他闭包：关闭其他打开的选项
            faqItems.forEach(i => i.classList.remove('active'));
            
            // 如果此前未开启，则激活当前项
            if (!isActive) {
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
});