
document.addEventListener('DOMContentLoaded', () => {
    const mainCanvas = document.getElementById('mainCanvas');
    const ctx = mainCanvas.getContext('2d');

    // 独立离屏标注画线层 (1920x1080 绝对分辨率)
    const drawingCanvas = document.createElement('canvas');
    drawingCanvas.width = 1920;
    drawingCanvas.height = 1080;
    const drawCtx = drawingCanvas.getContext('2d');

    // 限制单张图片最大体积为 10MB
    const MAX_FILE_SIZE = 10 * 1024 * 1024; 

    // 图像核心缓存结构
    let imgA = null;
    let imgB = null;

    // 图片 A 与 B 相互独立的实时无级拉伸、自由位移坐标配置项
    const transformA = { scale: 1.0, x: 0, y: 0 };
    const transformB = { scale: 1.0, x: 0, y: 0 };

    // 历史堆栈（用于标注层的 Ctrl+Z / Ctrl+Y 撤销重做）
    const undoStack = [];
    const redoStack = [];
    const MAX_STACK_SIZE = 30;

    // 鼠标/触屏 交互模式状态机：'draw'(画笔) | 'dragA'(拖拽A) | 'dragB'(拖拽B) | 'transformA'(变形A) | 'transformB'(变形B)
    let currentMode = 'draw'; 
    let isInteracting = false;
    let startPos = { x: 0, y: 0 };
    let startTransform = { scale: 1.0, x: 0, y: 0 };

    // 战术标注状态
    let currentTool = 'brush'; 
    let currentColor = '#ff4d4d'; 
    let currentSize = 10;

    // DOM 交互元素绑定
    const mapSelect = document.getElementById('mapSelect');
    const mapBadge = document.getElementById('mapBadge');
    const fileNameInput = document.getElementById('fileName');
    const textRemarkInput = document.getElementById('textRemark');
    
    const uploadA = document.getElementById('uploadA');
    const uploadB = document.getElementById('uploadB');
    const statusA = document.getElementById('statusA');
    const statusB = document.getElementById('statusB');
    
    const controlsA = document.getElementById('controlsA');
    const controlsB = document.getElementById('controlsB');

    // 图片 A 调节杆
    const sliderScaleA = document.getElementById('scaleA');
    const sliderXA = document.getElementById('offsetX_A');
    const sliderYA = document.getElementById('offsetY_A');
    const valScaleA = document.getElementById('valScaleA');
    const valX_A = document.getElementById('valX_A');
    const valY_A = document.getElementById('valY_A');

    // 图片 B 调节杆
    const sliderScaleB = document.getElementById('scaleB');
    const sliderXB = document.getElementById('offsetX_B');
    const sliderYB = document.getElementById('offsetY_B');
    const valScaleB = document.getElementById('valScaleB');
    const valX_B = document.getElementById('valX_B');
    const valY_B = document.getElementById('valY_B');

    // 标注栏
    const brushSize = document.getElementById('brushSize');
    const sizeVal = document.getElementById('sizeVal');
    const btnClear = document.getElementById('btnClear');
    const btnDownload = document.getElementById('btnDownload');
    const toolButtons = document.querySelectorAll('.tool-btn');

    // 保存初始空白状态到撤销栈
    saveState();

    // 全局启动
    init();

    function init() {
        render();
        bindEvents();
        bindHotkeys();
    }

    /**
     * 核心渲染主循环
     */
    function render() {
        // 1. 初始化纯色基底
        ctx.fillStyle = '#1e222b';
        ctx.fillRect(0, 0, 1920, 1080);

        // 2. 绘制左半边图片 A
        if (imgA) {
            drawInteractiveImage(ctx, imgA, 0, 0, 960, 1080, transformA);
            if (currentMode === 'dragA' || currentMode === 'transformA') {
                drawActiveOverlay(ctx, 0, 0, 960, 1080);
            }
        } else {
            drawPlaceholder(ctx, '尚未载入图片 A (站位/投掷区)', 0, 0, 960, 1080);
        }

        // 3. 绘制右半边图片 B
        if (imgB) {
            drawInteractiveImage(ctx, imgB, 960, 0, 960, 1080, transformB);
            if (currentMode === 'dragB' || currentMode === 'transformB') {
                drawActiveOverlay(ctx, 960, 0, 960, 1080);
            }
        } else {
            drawPlaceholder(ctx, '尚未载入图片 B (瞄准点/落点区)', 960, 0, 960, 1080);
        }

        // 4. 高透光立体居中截断边界线
        ctx.strokeStyle = 'rgba(255, 255, 255, 0.28)';
        ctx.lineWidth = 4;
        ctx.beginPath();
        ctx.moveTo(960, 0);
        ctx.lineTo(960, 1080);
        ctx.stroke();

        // 5. 渲染四个角落的操作手柄视觉提示 (仅在图片存在时)
        if (imgA) drawResizeHandles(ctx, 0, 0, 960, 1080);
        if (imgB) drawResizeHandles(ctx, 960, 0, 960, 1080);

        // 6. 渲染防光晕大备注文本
        const remarkText = textRemarkInput.value.trim();
        if (remarkText) {
            ctx.save();
            ctx.font = 'bold 85px "Microsoft YaHei", -apple-system, sans-serif';
            ctx.fillStyle = '#ffffff';
            ctx.textAlign = 'left';
            ctx.textBaseline = 'top';
            ctx.strokeStyle = 'rgba(0, 0, 0, 0.85)';
            ctx.lineWidth = 12;
            ctx.strokeText(remarkText, 45, 45);
            ctx.fillText(remarkText, 45, 45);
            ctx.restore();
        }

        // 7. 将标注涂鸦层压印至最上方
        ctx.drawImage(drawingCanvas, 0, 0);
    }

    /**
     * 实现底层图像的自由拉伸、自由缩放及平移裁切
     */
    function drawInteractiveImage(context, img, targetX, targetY, targetW, targetH, trans) {
        context.save();
        context.beginPath();
        context.rect(targetX, targetY, targetW, targetH);
        context.clip();

        const imgRatio = img.width / img.height;
        const targetRatio = targetW / targetH;
        let baseW, baseH;

        if (imgRatio > targetRatio) {
            baseH = targetH;
            baseW = baseH * imgRatio;
        } else {
            baseW = targetW;
            baseH = baseW / imgRatio;
        }

        const finalW = baseW * trans.scale;
        const finalH = baseH * trans.scale;

        const x = targetX + (targetW - finalW) / 2 + trans.x;
        const y = targetY + (targetH - finalH) / 2 + trans.y;

        context.drawImage(img, x, y, finalW, finalH);
        context.restore();
    }

    function drawPlaceholder(context, text, x, y, w, h) {
        context.save();
        context.strokeStyle = 'rgba(255, 255, 255, 0.08)';
        context.lineWidth = 3;
        context.setLineDash([10, 8]);
        context.strokeRect(x + 30, y + 30, w - 60, h - 60);
        context.fillStyle = '#4b5563';
        context.font = '24px "Microsoft YaHei", sans-serif';
        context.textAlign = 'center';
        context.textBaseline = 'middle';
        context.fillText(text, x + w / 2, y + h / 2);
        context.restore();
    }

    // 绘制正在操作时的微亮遮罩
    function drawActiveOverlay(context, x, y, w, h) {
        context.save();
        context.fillStyle = 'rgba(0, 153, 255, 0.08)';
        context.fillRect(x, y, w, h);
        context.strokeStyle = '#0099ff';
        context.lineWidth = 4;
        context.strokeRect(x, y, w, h);
        context.restore();
    }

    // 绘制角落的缩放手柄（模仿画图软件效果）
    function drawResizeHandles(context, x, y, w, h) {
        context.save();
        context.fillStyle = '#ffffff';
        context.strokeStyle = '#0077cc';
        context.lineWidth = 2;
        const size = 16;
        
        // 四个角
        const points = [
            {x: x + 20, y: y + 20},
            {x: x + w - 20, y: y + 20},
            {x: x + 20, y: y + h - 20},
            {x: x + w - 20, y: y + h - 20}
        ];
        points.forEach(p => {
            context.fillRect(p.x - size/2, p.y - size/2, size, size);
            context.strokeRect(p.x - size/2, p.y - size/2, size, size);
        });
        context.restore();
    }

    /**
     * 10MB 限制智能过滤
     */
    function checkAndLoadImage(file, callback) {
        if (!file) return;
        if (file.size > MAX_FILE_SIZE) {
            const sizeInMB = (file.size / (1024 * 1024)).toFixed(2);
            const userConfirmed = confirm(`⚠️ 安全检测提示：\\n\\n当前选中的截图文件【${file.name}】体积为 ${sizeInMB}MB，已超过 10MB 限定阈值。\\n您确定坚持加载这张图片吗？`);
            if (!userConfirmed) {
                callback(null, false);
                return;
            }
        }
        const reader = new FileReader();
        reader.onload = (e) => {
            const img = new Image();
            img.onload = () => { callback(img, file.name); };
            img.src = e.target.result;
        };
        reader.readAsDataURL(file);
    }

    /**
     * 获取高精度画布映射坐标
     */
    function getCanvasCoordinates(e) {
        const rect = mainCanvas.getBoundingClientRect();
        const clientX = e.touches ? e.touches[0].clientX : e.clientX;
        const clientY = e.touches ? e.touches[0].clientY : e.clientY;
        return {
            x: (clientX - rect.left) * (mainCanvas.width / rect.width),
            y: (clientY - rect.top) * (mainCanvas.height / rect.height)
        };
    }

    /**
     * 更新底部的滑动输入条数值同步
     */
    function updateSliders() {
        sliderScaleA.value = transformA.scale;
        valScaleA.textContent = transformA.scale.toFixed(2);
        sliderXA.value = transformA.x;
        valX_A.textContent = transformA.x;
        sliderYA.value = transformA.y;
        valY_A.textContent = transformA.y;

        sliderScaleB.value = transformB.scale;
        valScaleB.textContent = transformB.scale.toFixed(2);
        sliderXB.value = transformB.x;
        valX_B.textContent = transformB.x;
        sliderYB.value = transformB.y;
        valY_B.textContent = transformB.y;
    }

    /**
     * 历史记录状态管理 (撤销/重做堆栈)
     */
    function saveState() {
        if (undoStack.length >= MAX_STACK_SIZE) undoStack.shift();
        undoStack.push(drawingCanvas.toDataURL());
        redoStack.length = 0; // 清空重做栈
    }

    function executeUndo() {
        if (undoStack.length > 1) {
            redoStack.push(undoStack.pop());
            const previousStateSrc = undoStack[undoStack.length - 1];
            loadStateToDrawingCanvas(previousStateSrc);
        }
    }

    function executeRedo() {
        if (redoStack.length > 0) {
            const nextStateSrc = redoStack.pop();
            undoStack.push(nextStateSrc);
            loadStateToDrawingCanvas(nextStateSrc);
        }
    }

    function loadStateToDrawingCanvas(src) {
        const img = new Image();
        img.onload = () => {
            drawCtx.clearRect(0, 0, drawingCanvas.width, drawingCanvas.height);
            drawCtx.drawImage(img, 0, 0);
            render();
        };
        img.src = src;
    }

    /**
     * 触发图片导出与隐式下载
     */
    function triggerDownload() {
        if (!imgA && !imgB) {
            alert('提示：请先上传并配置您的道具位置或瞄准点截图再执行导出！');
            return;
        }
        const chosenMap = mapSelect.value;
        let finalName = fileNameInput.value.trim();
        if (!finalName) finalName = 'prop_artifact';
        const outputFullName = `${finalName}.jpg`;

        mainCanvas.toBlob((blob) => {
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = outputFullName;
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            URL.revokeObjectURL(url);
        }, 'image/jpeg', 0.92);
    }

    /**
     * 快捷键全局拦截绑定 (Ctrl+Z, Ctrl+Y, Ctrl+S)
     */
    function bindHotkeys() {
        window.addEventListener('keydown', (e) => {
            const isCtrl = e.ctrlKey || e.metaKey; // 兼容Mac Command键
            const key = e.key.toLowerCase();

            if (isCtrl) {
                if (key === 'z') {
                    e.preventDefault();
                    executeUndo();
                } else if (key === 'y') {
                    e.preventDefault();
                    executeRedo();
                } else if (key === 's') {
                    e.preventDefault();
                    triggerDownload();
                }
            }
        });
    }

    /**
     * 事件监听注册管线
     */
    function bindEvents() {
        mapSelect.addEventListener('change', (e) => {
            mapBadge.textContent = `当前地图: ${e.target.value}`;
        });

        textRemarkInput.addEventListener('input', render);

        // 导入图片 A
        uploadA.addEventListener('change', (e) => {
            checkAndLoadImage(e.target.files[0], (img, fileName) => {
                if (img) {
                    imgA = img;
                    statusA.textContent = fileName;
                    controlsA.style.opacity = "1";
                    controlsA.style.pointerEvents = "auto";
                    render();
                } else { e.target.value = ''; }
            });
        });

        // 导入图片 B
        uploadB.addEventListener('change', (e) => {
            checkAndLoadImage(e.target.files[0], (img, fileName) => {
                if (img) {
                    imgB = img;
                    statusB.textContent = fileName;
                    controlsB.style.opacity = "1";
                    controlsB.style.pointerEvents = "auto";
                    render();
                } else { e.target.value = ''; }
            });
        });

        // 滑块手动调节连动
        const onSliderInput = (trans, prop, slider, valEl, isInt = false) => {
            trans[prop] = isInt ? parseInt(slider.value) : parseFloat(slider.value);
            valEl.textContent = isInt ? slider.value : trans[prop].toFixed(2);
            render();
        };
        sliderScaleA.addEventListener('input', () => onSliderInput(transformA, 'scale', sliderScaleA, valScaleA));
        sliderXA.addEventListener('input', () => onSliderInput(transformA, 'x', sliderXA, valX_A, true));
        sliderYA.addEventListener('input', () => onSliderInput(transformA, 'y', sliderYA, valY_A, true));
        sliderScaleB.addEventListener('input', () => onSliderInput(transformB, 'scale', sliderScaleB, valScaleB));
        sliderXB.addEventListener('input', () => onSliderInput(transformB, 'x', sliderXB, valX_B, true));
        sliderYB.addEventListener('input', () => onSliderInput(transformB, 'y', sliderYB, valY_B, true));

        // 工具切换
        toolButtons.forEach(btn => {
            btn.addEventListener('click', () => {
                toolButtons.forEach(b => b.classList.remove('active'));
                btn.classList.add('active');
                currentTool = btn.dataset.tool;
                if (currentTool === 'brush') currentColor = btn.dataset.color;
            });
        });

        brushSize.addEventListener('input', (e) => {
            currentSize = parseInt(e.target.value);
            sizeVal.textContent = currentSize;
        });

        btnClear.addEventListener('click', () => {
            drawCtx.clearRect(0, 0, drawingCanvas.width, drawingCanvas.height);
            saveState();
            render();
        });

        btnDownload.addEventListener('click', triggerDownload);

        /**
         * ─────────────────────────────────────────────────────────
         * 🖱️ 画布内高级动态交互系统：识别点击位置是涂鸦、移动还是缩放
         * ─────────────────────────────────────────────────────────
         */
        mainCanvas.addEventListener('mousedown', (e) => {
            e.preventDefault();
            isInteracting = true;
            const pos = getCanvasCoordinates(e);
            startPos = pos;

            // 1. 判断点击区域落在左半边(A)还是右半边(B)
            const isLeftHalf = pos.x < 960;

            // 2. 检查是否点在角落的手柄范围内 (距离各有效角手柄小于 35px 则判定为画图软件缩放模式)
            let clickedOnHandle = false;
            const handleRadius = 35; 

            if (isLeftHalf && imgA) {
                const checkPoints = [
                    {x: 20, y: 20}, {x: 940, y: 20}, {x: 20, y: 1060}, {x: 940, y: 1060}
                ];
                clickedOnHandle = checkPoints.some(p => Math.hypot(pos.x - p.x, pos.y - p.y) < handleRadius);
                if (clickedOnHandle) {
                    currentMode = 'transformA';
                    startTransform = { ...transformA };
                }
            } else if (!isLeftHalf && imgB) {
                const checkPoints = [
                    {x: 980, y: 20}, {x: 1900, y: 20}, {x: 980, y: 1060}, {x: 1900, y: 1060}
                ];
                clickedOnHandle = checkPoints.some(p => Math.hypot(pos.x - p.x, pos.y - p.y) < handleRadius);
                if (clickedOnHandle) {
                    currentMode = 'transformB';
                    startTransform = { ...transformB };
                }
            }

            // 3. 如果没点到手柄，且键盘按住了 Space 键，或者特定工具非画笔时，触发拖拽位移；否则为默认画笔标注
            if (!clickedOnHandle) {
                if (e.shiftKey || e.button === 1 || currentTool === 'eraser' === false && (isLeftHalf && imgA || !isLeftHalf && imgB) && e.altKey) {
                    // 辅助触发或默认快捷条件
                    currentMode = isLeftHalf ? 'dragA' : 'dragB';
                } else if ((isLeftHalf && imgA) || (!isLeftHalf && imgB)) {
                    // 如果用户双击或者想要快速拖拽，可以通过长按右键或结合情况。这里我们做更画图软件化的设计：
                    // 点击在图片上有图时，默认是画笔；如果想拖拽/缩放，点四个角是缩放，点中间通过按住 Alt 或 Shift 键进行拖拽。
                    // 为了最大化降低门槛：直接规定，没有按住特殊键时，点手柄是缩放拉伸，点击其他地方如果点到了图片，我们优先进行战术标注绘画！
                    currentMode = 'draw';
                } else {
                    currentMode = 'draw';
                }
            }

            // 额外支持：如果没有按下任何控制键，且没点中手柄，就走画笔线
            if (currentMode === 'draw') {
                drawCtx.beginPath();
                drawCtx.moveTo(pos.x, pos.y);
                if (currentTool === 'eraser') {
                    drawCtx.globalCompositeOperation = 'destination-out';
                    drawCtx.lineWidth = currentSize * 2.5;
                } else {
                    drawCtx.globalCompositeOperation = 'source-over';
                    drawCtx.lineWidth = currentSize;
                    drawCtx.strokeStyle = currentColor;
                    drawCtx.lineCap = 'round';
                    drawCtx.lineJoin = 'round';
                }
                drawCtx.lineTo(pos.x, pos.y);
                drawCtx.stroke();
            }
            
            render();
        });

        mainCanvas.addEventListener('mousemove', (e) => {
            if (!isInteracting) {
                // 动态变换鼠标指针样式，提示用户手柄处可拉伸，图片内可绘画
                const pos = getCanvasCoordinates(e);
                const isLeftHalf = pos.x < 960;
                let onHandle = false;
                if (isLeftHalf && imgA) {
                    const checkPoints = [{x: 20, y: 20}, {x: 940, y: 20}, {x: 20, y: 1060}, {x: 940, y: 1060}];
                    onHandle = checkPoints.some(p => Math.hypot(pos.x - p.x, pos.y - p.y) < 35);
                } else if (!isLeftHalf && imgB) {
                    const checkPoints = [{x: 980, y: 20}, {x: 1900, y: 20}, {x: 980, y: 1060}, {x: 1900, y: 1060}];
                    onHandle = checkPoints.some(p => Math.hypot(pos.x - p.x, pos.y - p.y) < 35);
                }
                mainCanvas.style.cursor = onHandle ? 'nwse-resize' : 'crosshair';
                return;
            }

            const pos = getCanvasCoordinates(e);
            const dx = pos.x - startPos.x;
            const dy = pos.y - startPos.y;

            if (currentMode === 'draw') {
                drawCtx.lineTo(pos.x, pos.y);
                drawCtx.stroke();
            } 
            // 鼠标上下滑动进行“画图软件式”的无级缩放拉伸
            else if (currentMode === 'transformA') {
                const scaleFactor = 1 - (dy / 300); // 往上拉放大，往下拉缩小
                transformA.scale = Math.max(0.2, Math.min(3.0, startTransform.scale * scaleFactor));
                updateSliders();
            } else if (currentMode === 'transformB') {
                const scaleFactor = 1 - (dy / 300);
                transformB.scale = Math.max(0.2, Math.min(3.0, startTransform.scale * scaleFactor));
                updateSliders();
            }
            // 辅助拖拽位移
            else if (currentMode === 'dragA') {
                transformA.x = startTransform.x + dx;
                transformA.y = startTransform.y + dy;
                updateSliders();
            } else if (currentMode === 'dragB') {
                transformB.x = startTransform.x + dx;
                transformB.y = startTransform.y + dy;
                updateSliders();
            }

            render();
        });

        const handleMouseUp = () => {
            if (isInteracting) {
                if (currentMode === 'draw') {
                    saveState(); // 绘画结束，保存这一笔到撤销栈
                }
                isInteracting = false;
                currentMode = 'draw';
                render();
            }
        };

        window.addEventListener('mouseup', handleMouseUp);
    }
});