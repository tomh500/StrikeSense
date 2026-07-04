(() => {
    const state = {
        nextId: 1,
        drag: null,
        query: '',
    };

    const defs = {
        value_number: {
            label: '数值积木',
            group: 'values',
            tone: 'value',
            kind: 'expr',
            hint: '把数字拖进参数槽。',
            chips: ['数字', '表达式'],
            fields: [{ type: 'number', name: 'value', label: '值', value: '1' }],
            compile(block) {
                return readField(block, 'value') || '0';
            },
        },
        value_string: {
            label: '字符串积木',
            group: 'values',
            tone: 'value',
            kind: 'expr',
            hint: '生成带引号的文本。',
            chips: ['文本', '字符串'],
            fields: [{ type: 'text', name: 'value', label: '文本', value: 'hello' }],
            compile(block) {
                return quote(readField(block, 'value'));
            },
        },
        value_bool: {
            label: '布尔积木',
            group: 'values',
            tone: 'value',
            kind: 'expr',
            hint: 'true 或 false。',
            chips: ['真', '假'],
            fields: [{ type: 'select', name: 'value', label: '状态', value: 'true', options: [['true', 'true'], ['false', 'false']] }],
            compile(block) {
                return readField(block, 'value') || 'true';
            },
        },
        value_var: {
            label: '变量积木',
            group: 'values',
            tone: 'value',
            kind: 'expr',
            hint: '引用变量或字段。',
            chips: ['变量', '引用'],
            fields: [{ type: 'text', name: 'value', label: '名称', value: 'health' }],
            compile(block) {
                return sanitizeIdentifier(readField(block, 'value') || 'value');
            },
        },
        value_compare: {
            label: '比较积木',
            group: 'values',
            tone: 'value',
            kind: 'expr',
            hint: '拼出 if/while 的条件。',
            chips: ['==', '!=', '>', '<', '>=', '<='],
            fields: [{ type: 'select', name: 'op', label: '运算符', value: '==', options: [['==', '=='], ['!=', '!='], ['>', '>'], ['<', '<'], ['>=', '>='], ['<=', '<=']] }],
            slots: [
                { name: 'left', kind: 'expr', label: '左侧', placeholder: '拖一个表达式' },
                { name: 'right', kind: 'expr', label: '右侧', placeholder: '拖一个表达式' },
            ],
            compile(block, ctx) {
                return `(${ctx.expr('left', '0')} ${readField(block, 'op')} ${ctx.expr('right', '0')})`;
            },
        },
        value_logic: {
            label: '逻辑积木',
            group: 'values',
            tone: 'value',
            kind: 'expr',
            hint: '把两个条件合成 AND / OR。',
            chips: ['&&', '||'],
            fields: [{ type: 'select', name: 'op', label: '逻辑', value: '&&', options: [['&&', '&&'], ['||', '||']] }],
            slots: [
                { name: 'left', kind: 'expr', label: '左条件', placeholder: '拖一个条件表达式' },
                { name: 'right', kind: 'expr', label: '右条件', placeholder: '拖一个条件表达式' },
            ],
            compile(block, ctx) {
                return `(${ctx.expr('left', 'false')} ${readField(block, 'op')} ${ctx.expr('right', 'false')})`;
            },
        },
        value_not: {
            label: '取反积木',
            group: 'values',
            tone: 'value',
            kind: 'expr',
            hint: '给条件加 !。',
            chips: ['!'],
            slots: [{ name: 'value', kind: 'expr', label: '条件', placeholder: '拖一个表达式' }],
            compile(block, ctx) {
                return `!${ctx.expr('value', 'false')}`;
            },
        },
        value_call: {
            label: '函数调用',
            group: 'values',
            tone: 'function',
            kind: 'expr',
            hint: '把函数当成表达式。',
            chips: ['调用', '参数'],
            fields: [{ type: 'text', name: 'name', label: '函数名', value: 'GetSteamAccounts' }],
            slots: [
                { name: 'arg1', kind: 'expr', label: '参数 1', placeholder: '拖一个参数' },
                { name: 'arg2', kind: 'expr', label: '参数 2', placeholder: '拖一个参数' },
                { name: 'arg3', kind: 'expr', label: '参数 3', placeholder: '拖一个参数' },
            ],
            compile(block, ctx) {
                const args = ['arg1', 'arg2', 'arg3'].map((slot) => ctx.expr(slot, '')).filter(Boolean).join(', ');
                return `${sanitizeIdentifier(readField(block, 'name') || 'Func')}(${args})`;
            },
        },
        stmt_assign_decl: {
            label: '变量声明',
            group: 'control',
            tone: 'control',
            kind: 'stmt',
            hint: '先选类型，再拖一个初始值。',
            chips: ['声明', '赋值'],
            fields: [
                { type: 'select', name: 'type', label: '类型', value: 'int', options: [['auto', 'auto'], ['int', 'int'], ['float', 'float'], ['double', 'double'], ['string', 'string'], ['bool', 'bool']] },
                { type: 'text', name: 'name', label: '变量名', value: 'count' },
            ],
            slots: [{ name: 'value', kind: 'expr', label: '初始值', placeholder: '拖一个表达式' }],
            compile(block, ctx) {
                return `${readField(block, 'type')} ${sanitizeIdentifier(readField(block, 'name') || 'value')} = ${ctx.expr('value', '0')};`;
            },
        },
        stmt_assign_set: {
            label: '赋值积木',
            group: 'control',
            tone: 'control',
            kind: 'stmt',
            hint: '重设已有变量。',
            chips: ['=', '重置'],
            fields: [{ type: 'text', name: 'name', label: '变量名', value: 'count' }],
            slots: [{ name: 'value', kind: 'expr', label: '新值', placeholder: '拖一个表达式' }],
            compile(block, ctx) {
                return `${sanitizeIdentifier(readField(block, 'name') || 'value')} = ${ctx.expr('value', '0')};`;
            },
        },
        stmt_assign_change: {
            label: '增减积木',
            group: 'control',
            tone: 'control',
            kind: 'stmt',
            hint: '适合计数器。',
            chips: ['+=', '-='],
            fields: [
                { type: 'text', name: 'name', label: '变量名', value: 'count' },
                { type: 'select', name: 'op', label: '方式', value: '+=', options: [['+=', '加上'], ['-=', '减去']] },
            ],
            slots: [{ name: 'value', kind: 'expr', label: '数值', placeholder: '拖一个表达式' }],
            compile(block, ctx) {
                return `${sanitizeIdentifier(readField(block, 'name') || 'value')} ${readField(block, 'op')} ${ctx.expr('value', '1')};`;
            },
        },
        control_if: {
            label: '如果积木',
            group: 'control',
            tone: 'control',
            kind: 'stmt',
            hint: '条件成立时执行。',
            chips: ['if', '首次成立'],
            fields: [{ type: 'checkbox', name: 'on', label: '首次成立', value: true }],
            slots: [
                { name: 'condition', kind: 'expr', label: '条件', placeholder: '拖一个条件表达式' },
                { name: 'body', kind: 'stmt', label: '执行内容', placeholder: '把语句拖到这里' },
            ],
            compile(block, ctx) {
                const prefix = readField(block, 'on') === 'true' ? 'on:' : '';
                return `if(${prefix}${ctx.expr('condition', 'false')}){\n${ctx.stmts('body', 1)}\n}`;
            },
        },
        control_if_else: {
            label: '如果 / 否则',
            group: 'control',
            tone: 'control',
            kind: 'stmt',
            hint: '给条件准备两个分支。',
            chips: ['if', 'else'],
            slots: [
                { name: 'condition', kind: 'expr', label: '条件', placeholder: '拖一个条件表达式' },
                { name: 'then', kind: 'stmt', label: '满足时', placeholder: '把语句拖到这里' },
                { name: 'else', kind: 'stmt', label: '否则时', placeholder: '把语句拖到这里' },
            ],
            compile(block, ctx) {
                return `if(${ctx.expr('condition', 'false')}){\n${ctx.stmts('then', 1)}\n}else{\n${ctx.stmts('else', 1)}\n}`;
            },
        },
        control_while: {
            label: 'while 循环',
            group: 'control',
            tone: 'control',
            kind: 'stmt',
            hint: '条件一直成立就一直执行。',
            chips: ['while'],
            slots: [
                { name: 'condition', kind: 'expr', label: '循环条件', placeholder: '拖一个条件表达式' },
                { name: 'body', kind: 'stmt', label: '循环体', placeholder: '把语句拖到这里' },
            ],
            compile(block, ctx) {
                return `while(${ctx.expr('condition', 'false')}){\n${ctx.stmts('body', 1)}\n}`;
            },
        },
        control_repeat: {
            label: '重复 N 次',
            group: 'control',
            tone: 'control',
            kind: 'stmt',
            hint: '次数也可以来自数值块。',
            chips: ['for', '计数'],
            fields: [{ type: 'text', name: 'index', label: '计数器', value: 'i' }],
            slots: [
                { name: 'count', kind: 'expr', label: '次数', placeholder: '拖一个数值表达式' },
                { name: 'body', kind: 'stmt', label: '循环体', placeholder: '把语句拖到这里' },
            ],
            compile(block, ctx) {
                const idx = sanitizeIdentifier(readField(block, 'index') || 'i');
                return `for(int ${idx}=0; ${idx}<${ctx.expr('count', '0')}; ${idx}=${idx}+1){\n${ctx.stmts('body', 1)}\n}`;
            },
        },
        control_function_def: {
            label: '函数定义',
            group: 'functions',
            tone: 'function',
            kind: 'stmt',
            hint: '定义自己的函数。',
            chips: ['void', 'int', 'string'],
            fields: [
                { type: 'select', name: 'ret', label: '返回值', value: 'void', options: [['void', 'void'], ['int', 'int'], ['float', 'float'], ['double', 'double'], ['string', 'string'], ['bool', 'bool'], ['auto', 'auto']] },
                { type: 'text', name: 'name', label: '函数名', value: 'MyFunction' },
                { type: 'text', name: 'params', label: '参数', value: 'int a, int b' },
            ],
            slots: [{ name: 'body', kind: 'stmt', label: '函数体', placeholder: '把语句拖到这里' }],
            compile(block, ctx) {
                return `${readField(block, 'ret') || 'void'} ${sanitizeIdentifier(readField(block, 'name') || 'MyFunction')}(${readField(block, 'params') || ''}){\n${ctx.stmts('body', 1)}\n}`;
            },
        },
        control_return: {
            label: 'return 积木',
            group: 'functions',
            tone: 'function',
            kind: 'stmt',
            hint: '函数里返回结果。',
            chips: ['return'],
            slots: [{ name: 'value', kind: 'expr', label: '返回值', placeholder: '可留空' }],
            compile(block, ctx) {
                const value = ctx.expr('value', '');
                return value ? `return ${value};` : 'return;';
            },
        },
        control_label: {
            label: '标签积木',
            group: 'functions',
            tone: 'function',
            kind: 'stmt',
            hint: '配合 goto 使用。',
            chips: ['label'],
            fields: [{ type: 'text', name: 'name', label: '标签名', value: 'done' }],
            compile(block) {
                return `${sanitizeIdentifier(readField(block, 'name') || 'done')}:`;
            },
        },
        control_goto: {
            label: 'goto 积木',
            group: 'functions',
            tone: 'function',
            kind: 'stmt',
            hint: '跳转到标签。',
            chips: ['goto'],
            fields: [{ type: 'text', name: 'name', label: '标签名', value: 'done' }],
            compile(block) {
                return `goto ${sanitizeIdentifier(readField(block, 'name') || 'done')};`;
            },
        },
        action_log: {
            label: '日志积木',
            group: 'actions',
            tone: 'action',
            kind: 'stmt',
            hint: '输出调试信息。',
            chips: ['Log'],
            slots: [{ name: 'value', kind: 'expr', label: '内容', placeholder: '拖一个表达式' }],
            compile(block, ctx) {
                return `Log(${ctx.expr('value', quote('调试'))});`;
            },
        },
        action_sleep: {
            label: '等待积木',
            group: 'actions',
            tone: 'action',
            kind: 'stmt',
            hint: '暂停执行。',
            chips: ['Sleep'],
            slots: [{ name: 'value', kind: 'expr', label: '毫秒', placeholder: '拖一个数值表达式' }],
            compile(block, ctx) {
                return `Sleep(${ctx.expr('value', '1000')});`;
            },
        },
        action_open: {
            label: 'Open 积木',
            group: 'actions',
            tone: 'action',
            kind: 'stmt',
            hint: '打开文件或链接。',
            chips: ['Open'],
            slots: [{ name: 'target', kind: 'expr', label: '目标', placeholder: '拖一个字符串积木' }],
            compile(block, ctx) {
                return `Open(${ctx.expr('target', quote(''))});`;
            },
        },
        action_browser: {
            label: 'Browser 积木',
            group: 'actions',
            tone: 'action',
            kind: 'stmt',
            hint: '打开网址。',
            chips: ['Browser'],
            fields: [
                { type: 'checkbox', name: 'top', label: '打开后置顶', value: false },
                { type: 'checkbox', name: 'prefer', label: '优先复用浏览器', value: true },
            ],
            slots: [{ name: 'url', kind: 'expr', label: '网址', placeholder: '拖一个字符串积木' }],
            compile(block, ctx) {
                return `Browser(${ctx.expr('url', quote('https://example.com'))}, ${readField(block, 'top')}, ${readField(block, 'prefer')});`;
            },
        },
        action_top: {
            label: 'Top 积木',
            group: 'actions',
            tone: 'action',
            kind: 'stmt',
            hint: '抬前某个进程窗口。',
            chips: ['Top'],
            fields: [
                { type: 'text', name: 'process', label: '进程', value: 'cs2.exe' },
                { type: 'checkbox', name: 'activate', label: '激活', value: true },
            ],
            compile(block) {
                return `Top(${quote(readField(block, 'process') || '')}, ${readField(block, 'activate')});`;
            },
        },
        action_ensure_window: {
            label: '窗口接管',
            group: 'actions',
            tone: 'action',
            kind: 'stmt',
            hint: '不存在就启动，存在就切过去。',
            chips: ['Ensure'],
            fields: [
                { type: 'text', name: 'process', label: '进程', value: 'PotPlayerMini64.exe' },
                { type: 'text', name: 'launch', label: '启动目标', value: 'C:/Program Files/PotPlayer/PotPlayerMini64.exe' },
                { type: 'checkbox', name: 'activate', label: '激活', value: true },
            ],
            compile(block) {
                return `EnsureProcessWindow(${quote(readField(block, 'process') || '')}, ${quote(readField(block, 'launch') || '')}, ${readField(block, 'activate')});`;
            },
        },
        action_write_gsi: {
            label: '重写 GSI',
            group: 'actions',
            tone: 'action',
            kind: 'stmt',
            hint: '常用于修复本地配置。',
            chips: ['WriteSteamGSIConfig'],
            compile() {
                return 'WriteSteamGSIConfig();';
            },
        },
        action_set_death_volume: {
            label: '死亡音量',
            group: 'actions',
            tone: 'action',
            kind: 'stmt',
            hint: '修改死后音量比例。',
            chips: ['SetDeathVolume'],
            slots: [{ name: 'value', kind: 'expr', label: '值', placeholder: '拖一个数值表达式' }],
            compile(block, ctx) {
                return `SetDeathVolume(${ctx.expr('value', '0.5')});`;
            },
        },
        action_set_process_volume: {
            label: '进程音量',
            group: 'actions',
            tone: 'action',
            kind: 'stmt',
            hint: '设置某个进程音量百分比。',
            chips: ['SetProcessVolume'],
            fields: [{ type: 'text', name: 'process', label: '进程', value: 'chrome.exe' }],
            slots: [{ name: 'value', kind: 'expr', label: '百分比', placeholder: '拖一个数值表达式' }],
            compile(block, ctx) {
                return `SetProcessVolume(${quote(readField(block, 'process') || '')}, ${ctx.expr('value', '100')});`;
            },
        },
        action_set_process_mute: {
            label: '进程静音',
            group: 'actions',
            tone: 'action',
            kind: 'stmt',
            hint: '打开或关闭静音。',
            chips: ['SetProcessMute'],
            fields: [
                { type: 'text', name: 'process', label: '进程', value: 'msedge.exe' },
                { type: 'checkbox', name: 'muted', label: '静音', value: true },
            ],
            compile(block) {
                return `SetProcessMute(${quote(readField(block, 'process') || '')}, ${readField(block, 'muted')});`;
            },
        },
        action_shell_execute: {
            label: 'ShellExecute',
            group: 'actions',
            tone: 'danger',
            kind: 'stmt',
            hint: '高权限函数。',
            chips: ['危险', '高权限'],
            slots: [{ name: 'command', kind: 'expr', label: '命令', placeholder: '拖一个字符串表达式' }],
            compile(block, ctx) {
                return `ShellExecute(${ctx.expr('command', quote(''))});`;
            },
        },
    };

    const groups = [
        { id: 'values', title: '数值与表达式' },
        { id: 'control', title: '流程控制' },
        { id: 'actions', title: '动作与 API' },
        { id: 'functions', title: '函数与自定义' },
    ];

    const els = {
        palette: {
            values: document.getElementById('palette-values'),
            control: document.getElementById('palette-control'),
            actions: document.getElementById('palette-actions'),
            functions: document.getElementById('palette-functions'),
        },
        search: document.getElementById('toolboxSearch'),
        root: document.getElementById('programRoot'),
        board: document.getElementById('board'),
        empty: document.getElementById('emptyState'),
        preview: document.getElementById('codePreview'),
        blockCount: document.getElementById('blockCount'),
        fileLabel: document.getElementById('fileLabel'),
        scriptName: document.getElementById('scriptName'),
        metaName: document.getElementById('metaName'),
        metaAuthor: document.getElementById('metaAuthor'),
        metaVersion: document.getElementById('metaVersion'),
        metaNotice: document.getElementById('metaNotice'),
        metaModifier: document.getElementById('metaModifier'),
    };

    function init() {
        renderPalette();
        bindEvents();
        updatePreview();
    }

    function renderPalette() {
        groups.forEach((group) => {
            const container = els.palette[group.id];
            container.innerHTML = '';
            Object.entries(defs)
                .filter(([, def]) => def.group === group.id)
                .forEach(([type, def]) => {
                    container.appendChild(renderPaletteItem(type, def));
                });
        });
        applyFilter();
    }

    function renderPaletteItem(type, def) {
        const button = document.createElement('button');
        button.type = 'button';
        button.className = `palette-item palette-item--${def.tone}`;
        button.draggable = true;
        button.dataset.template = type;
        button.dataset.search = `${def.label} ${def.hint} ${(def.chips || []).join(' ')}`.toLowerCase();
        button.innerHTML = `
            <div class="palette-item__name">
                <span>${def.label}</span>
                <span>${toneIcon(def.tone)}</span>
            </div>
            <div class="palette-item__hint">${def.hint}</div>
            <div class="palette-item__chips">
                ${(def.chips || []).map((chip) => `<span class="palette-chip">${chip}</span>`).join('')}
            </div>
        `;
        button.addEventListener('dragstart', onPaletteDragStart);
        button.addEventListener('click', () => {
            insertBlock(els.root, createBlock(type));
            updatePreview();
        });
        return button;
    }

    function bindEvents() {
        els.search.addEventListener('input', (event) => {
            state.query = event.target.value.trim().toLowerCase();
            applyFilter();
        });

        els.scriptName.addEventListener('input', updatePreview);
        [els.metaName, els.metaAuthor, els.metaVersion, els.metaNotice, els.metaModifier].forEach((input) => {
            input.addEventListener('input', updatePreview);
        });

        document.getElementById('loadSampleBtn').addEventListener('click', loadSampleProgram);
        document.getElementById('sampleFlowBtn').addEventListener('click', loadSampleProgram);
        document.getElementById('clearBtn').addEventListener('click', clearProgram);
        document.getElementById('refreshBtn').addEventListener('click', updatePreview);
        document.getElementById('downloadBtn').addEventListener('click', downloadScript);
        document.getElementById('downloadBtnBottom').addEventListener('click', downloadScript);
        document.getElementById('copyCodeBtn').addEventListener('click', copyCode);

        els.board.addEventListener('dragover', (event) => {
            event.preventDefault();
            els.board.classList.add('dragover');
        });
        els.board.addEventListener('dragleave', () => els.board.classList.remove('dragover'));
        els.board.addEventListener('drop', onBoardDrop);

        els.board.addEventListener('input', updatePreview);
        els.board.addEventListener('change', updatePreview);

        document.addEventListener('dragend', () => {
            state.drag = null;
            document.querySelectorAll('.block--dragging').forEach((block) => block.classList.remove('block--dragging'));
            els.board.classList.remove('dragover');
        });
    }

    function onPaletteDragStart(event) {
        state.drag = { kind: 'template', type: event.currentTarget.dataset.template };
        event.dataTransfer.effectAllowed = 'copy';
        event.dataTransfer.setData('text/plain', JSON.stringify(state.drag));
    }

    function onBlockDragStart(event) {
        const block = event.currentTarget.closest('.vs-block');
        state.drag = { kind: 'move', id: block.dataset.nodeId };
        block.classList.add('block--dragging');
        event.dataTransfer.effectAllowed = 'move';
        event.dataTransfer.setData('text/plain', JSON.stringify(state.drag));
    }

    function onBoardDrop(event) {
        event.preventDefault();
        els.board.classList.remove('dragover');
        const payload = readPayload(event);
        if (!payload) {
            return;
        }

        const container = findContainer(event.target) || els.root;
        placePayload(payload, container, event.clientY);
        updatePreview();
    }

    function readPayload(event) {
        try {
            const raw = event.dataTransfer.getData('text/plain');
            if (raw) {
                return JSON.parse(raw);
            }
        } catch (_error) {
            // fallback to last drag state
        }
        return state.drag;
    }

    function findContainer(target) {
        const slot = target.closest?.('.vs-slot');
        if (slot) {
            return slot.querySelector('.vs-slot__body');
        }
        const root = target.closest?.('.board__stack');
        return root || null;
    }

    function placePayload(payload, container, clientY) {
        if (payload.kind === 'template') {
            const templateDef = defs[payload.type];
            if (!templateDef) {
                return;
            }
            if (!canDropTemplateIntoContainer(templateDef, container)) {
                return;
            }
            insertBlock(container, createBlock(payload.type), clientY);
            return;
        }

        if (payload.kind === 'move') {
            const block = document.querySelector(`[data-node-id="${payload.id}"]`);
            if (!block) {
                return;
            }
            if (!canDropBlockIntoContainer(block, container)) {
                return;
            }
            insertBlock(container, block, clientY);
        }
    }

    function canDropTemplateIntoContainer(def, container) {
        const slot = container.closest?.('.vs-slot');
        if (slot) {
            return slot.dataset.slotKind === def.kind;
        }
        return def.kind === 'stmt';
    }

    function canDropBlockIntoContainer(block, container) {
        const def = defs[block.dataset.template];
        return def ? canDropTemplateIntoContainer(def, container) : false;
    }

    function insertBlock(container, block, clientY = Number.POSITIVE_INFINITY) {
        if (!container) {
            return;
        }

        const slot = container.closest?.('.vs-slot');
        if (slot?.dataset.slotKind === 'expr') {
            container.innerHTML = '';
            container.appendChild(block);
            return;
        }

        const children = [...container.querySelectorAll(':scope > .vs-block')];
        const before = children.find((child) => clientY < child.getBoundingClientRect().top + child.getBoundingClientRect().height / 2);
        if (before) {
            container.insertBefore(block, before);
        } else {
            container.appendChild(block);
        }
    }

    function createBlock(type) {
        const def = defs[type];
        if (!def) {
            throw new Error(`未知积木类型: ${type}`);
        }

        const block = document.createElement('section');
        block.className = `vs-block vs-block--${def.tone}`;
        block.dataset.template = type;
        block.dataset.nodeId = `node-${state.nextId++}`;

        const top = document.createElement('div');
        top.className = 'vs-block__top';

        const handle = document.createElement('button');
        handle.type = 'button';
        handle.className = 'vs-block__handle';
        handle.draggable = true;
        handle.textContent = '⠿';
        handle.addEventListener('dragstart', onBlockDragStart);

        const title = document.createElement('div');
        title.className = 'vs-block__title';
        title.textContent = def.label;

        const remove = document.createElement('button');
        remove.type = 'button';
        remove.className = 'vs-block__delete';
        remove.textContent = '×';
        remove.addEventListener('click', () => {
            block.remove();
            updatePreview();
        });

        top.append(handle, title, remove);
        block.appendChild(top);

        const body = document.createElement('div');
        body.className = 'vs-block__body';

        if (def.fields?.length) {
            const row = document.createElement('div');
            row.className = 'vs-fields';
            def.fields.forEach((field) => row.appendChild(buildField(field)));
            body.appendChild(row);
        }

        if (def.slots?.length) {
            def.slots.forEach((slotDef) => body.appendChild(buildSlot(slotDef)));
        }

        block.appendChild(body);
        return block;
    }

    function buildField(field) {
        const wrap = document.createElement('label');
        wrap.className = 'vs-field';

        const caption = document.createElement('span');
        caption.className = 'vs-field__label';
        caption.textContent = field.label;
        wrap.appendChild(caption);

        let input;
        if (field.type === 'select') {
            input = document.createElement('select');
            field.options.forEach(([value, text]) => {
                const option = document.createElement('option');
                option.value = value;
                option.textContent = text;
                input.appendChild(option);
            });
            input.value = field.value ?? field.options?.[0]?.[0] ?? '';
        } else if (field.type === 'checkbox') {
            input = document.createElement('input');
            input.type = 'checkbox';
            input.checked = Boolean(field.value);
        } else if (field.type === 'number') {
            input = document.createElement('input');
            input.type = 'number';
            input.step = 'any';
            input.value = field.value ?? '0';
        } else {
            input = document.createElement('input');
            input.type = 'text';
            input.value = field.value ?? '';
        }

        input.dataset.field = field.name;
        input.addEventListener('click', (event) => event.stopPropagation());
        wrap.appendChild(input);
        return wrap;
    }

    function buildSlot(slotDef) {
        const slot = document.createElement('div');
        slot.className = `vs-slot vs-slot--${slotDef.kind}`;
        slot.dataset.slotName = slotDef.name;
        slot.dataset.slotKind = slotDef.kind;

        const head = document.createElement('div');
        head.className = 'vs-slot__head';

        const label = document.createElement('span');
        label.className = 'vs-slot__label';
        label.textContent = slotDef.label;
        head.appendChild(label);

        if (slotDef.kind === 'expr') {
            const clear = document.createElement('button');
            clear.type = 'button';
            clear.className = 'slot-add-btn';
            clear.textContent = '清空';
            clear.addEventListener('click', () => {
                body.innerHTML = '';
                updatePreview();
            });
            head.appendChild(clear);
        }

        const body = document.createElement('div');
        body.className = 'vs-slot__body';
        body.dataset.placeholder = slotDef.placeholder || '把积木拖到这里';

        body.addEventListener('dragover', (event) => {
            event.preventDefault();
            slot.classList.add('dragover');
        });
        body.addEventListener('dragleave', () => slot.classList.remove('dragover'));
        body.addEventListener('drop', (event) => {
            event.preventDefault();
            slot.classList.remove('dragover');
            const payload = readPayload(event);
            if (!payload) {
                return;
            }
            const def = payload.kind === 'template' ? defs[payload.type] : defs[document.querySelector(`[data-node-id="${payload.id}"]`)?.dataset.template];
            if (!def || def.kind !== slot.dataset.slotKind) {
                return;
            }
            if (payload.kind === 'template') {
                insertIntoSlot(body, createBlock(payload.type));
            } else {
                const block = document.querySelector(`[data-node-id="${payload.id}"]`);
                if (block) {
                    insertIntoSlot(body, block);
                }
            }
            updatePreview();
        });

        slot.append(head, body);
        return slot;
    }

    function insertIntoSlot(body, block) {
        if (body.closest('.vs-slot')?.dataset.slotKind === 'expr') {
            body.innerHTML = '';
        }
        body.appendChild(block);
    }

    function compileTree() {
        const blocks = [...els.root.querySelectorAll(':scope > .vs-block')];
        if (blocks.length === 0) {
            return `${headerText()}\n\n// 把积木拖到这里，代码会自动出现。`;
        }
        return [headerText(), '', ...blocks.map((block) => compileBlock(block, 0)).filter(Boolean)].join('\n\n');
    }

    function compileBlock(block, indentLevel) {
        const def = defs[block.dataset.template];
        if (!def) {
            return '';
        }

        const ctx = {
            expr: (slotName, fallback = '') => compileExprSlot(block, slotName, fallback),
            stmts: (slotName, nextIndent = 0) => compileStmtSlot(block, slotName, indentLevel + nextIndent),
        };

        const code = def.compile(block, ctx);
        const indent = '    '.repeat(indentLevel);
        return code
            .split('\n')
            .map((line) => `${indent}${line}`)
            .join('\n');
    }

    function compileExprSlot(block, slotName, fallback) {
        const body = block.querySelector(`.vs-slot[data-slot-name="${slotName}"] .vs-slot__body`);
        const child = body?.querySelector(':scope > .vs-block');
        return child ? stripTrailingSemicolon(compileBlock(child, 0)) : fallback;
    }

    function compileStmtSlot(block, slotName, indentLevel) {
        const body = block.querySelector(`.vs-slot[data-slot-name="${slotName}"] .vs-slot__body`);
        const children = body ? [...body.querySelectorAll(':scope > .vs-block')] : [];
        if (children.length === 0) {
            return `${'    '.repeat(indentLevel)}// 这里可以继续拖入语句块`;
        }
        return children.map((child) => compileBlock(child, indentLevel)).join('\n');
    }

    function headerText() {
        const meta = readMeta();
        return [
            '// ==================================================',
            `// @name: ${meta.name}`,
            `// @author: ${meta.author}`,
            '// @provider: StrikeSense',
            `// @version: ${meta.version}`,
            `// @notice: ${meta.notice}`,
            `// @modifier: ${meta.modifier}`,
            '// ==================================================',
        ].join('\n');
    }

    function readMeta() {
        return {
            name: els.metaName.value.trim() || '未命名脚本',
            author: els.metaAuthor.value.trim() || 'anonymous',
            version: els.metaVersion.value.trim() || '1.0.0',
            notice: els.metaNotice.value.trim() || '由积木编程工作台生成。',
            modifier: els.metaModifier.value.trim() || 'self_user=anonymous',
        };
    }

    function updatePreview() {
        els.preview.textContent = compileTree();
        els.blockCount.textContent = String(els.root.querySelectorAll(':scope > .vs-block').length);
        els.fileLabel.textContent = normalizeFilename(els.scriptName.value);
        els.empty.style.display = els.root.querySelector(':scope > .vs-block') ? 'none' : 'grid';
    }

    function clearProgram() {
        els.root.innerHTML = '';
        updatePreview();
    }

    function loadSampleProgram() {
        clearProgram();

        const decl = createBlock('stmt_assign_decl');
        setFieldValue(decl, 'type', 'int');
        setFieldValue(decl, 'name', 'count');
        fillExprSlot(decl, 'value', createBlock('value_number'), '0');

        const ifElse = createBlock('control_if_else');
        fillExprSlot(ifElse, 'condition', createBlock('value_compare'));
        const compare = ifElse.querySelector('.vs-slot[data-slot-name="condition"] .vs-block');
        fillExprSlot(compare, 'left', createBlock('value_var'), 'health');
        setFieldValue(compare, 'op', '<=');
        fillExprSlot(compare, 'right', createBlock('value_number'), '20');

        const thenLog = createBlock('action_log');
        fillExprSlot(thenLog, 'value', createBlock('value_string'), '当前血量较低');
        fillStmtSlot(ifElse, 'then', thenLog);

        const elseLog = createBlock('action_log');
        fillExprSlot(elseLog, 'value', createBlock('value_string'), '血量还算稳定');
        fillStmtSlot(ifElse, 'else', elseLog);

        const repeat = createBlock('control_repeat');
        setFieldValue(repeat, 'index', 'i');
        fillExprSlot(repeat, 'count', createBlock('value_number'), '3');
        const sleep = createBlock('action_sleep');
        fillExprSlot(sleep, 'value', createBlock('value_number'), '300');
        fillStmtSlot(repeat, 'body', sleep);

        const func = createBlock('control_function_def');
        setFieldValue(func, 'ret', 'void');
        setFieldValue(func, 'name', 'Tip');
        setFieldValue(func, 'params', 'string text');
        const funcLog = createBlock('action_log');
        fillExprSlot(funcLog, 'value', createBlock('value_var'), 'text');
        fillStmtSlot(func, 'body', funcLog);

        insertBlock(els.root, decl);
        insertBlock(els.root, ifElse);
        insertBlock(els.root, repeat);
        insertBlock(els.root, func);
        updatePreview();
    }

    function fillExprSlot(owner, slotName, child, value = '') {
        const body = owner.querySelector(`.vs-slot[data-slot-name="${slotName}"] .vs-slot__body`);
        if (!body) {
            return;
        }
        body.innerHTML = '';
        body.appendChild(child);
        if (value !== '') {
            setFieldValue(child, 'value', value);
        }
    }

    function fillStmtSlot(owner, slotName, child) {
        const body = owner.querySelector(`.vs-slot[data-slot-name="${slotName}"] .vs-slot__body`);
        if (body) {
            body.appendChild(child);
        }
    }

    function setFieldValue(block, name, value) {
        const field = block.querySelector(`[data-field="${name}"]`);
        if (!field) {
            return;
        }
        if (field.type === 'checkbox') {
            field.checked = value === true || value === 'true';
        } else {
            field.value = value;
        }
    }

    function readField(block, name) {
        const field = block.querySelector(`[data-field="${name}"]`);
        if (!field) {
            return '';
        }
        if (field.type === 'checkbox') {
            return String(field.checked);
        }
        return field.value.trim();
    }

    function normalizeFilename(value) {
        const trimmed = value.trim();
        if (!trimmed) {
            return 'my_custom_script.vscript';
        }
        return trimmed.endsWith('.vscript') ? trimmed : `${trimmed}.vscript`;
    }

    function quote(value) {
        return `"${String(value ?? '').replace(/\\/g, '\\\\').replace(/"/g, '\\"').replace(/\n/g, '\\n')}"`;
    }

    function sanitizeIdentifier(value) {
        return String(value ?? '')
            .trim()
            .replace(/\s+/g, '_')
            .replace(/[^a-zA-Z0-9_]/g, '_') || 'value';
    }

    function stripTrailingSemicolon(code) {
        return code.replace(/;\s*$/, '');
    }

    function toneIcon(tone) {
        if (tone === 'control') return '▣';
        if (tone === 'action') return '▶';
        if (tone === 'function') return 'ƒ';
        if (tone === 'danger') return '!';
        return '●';
    }

    function applyFilter() {
        document.querySelectorAll('.palette-item').forEach((item) => {
            const text = item.dataset.search || '';
            item.style.display = !state.query || text.includes(state.query) ? 'grid' : 'none';
        });
    }

    async function copyCode() {
        try {
            await navigator.clipboard.writeText(els.preview.textContent);
        } catch (_error) {
            // 复制失败就保持页面可用
        }
    }

    function downloadScript() {
        const blob = new Blob([els.preview.textContent || compileTree()], { type: 'text/plain;charset=utf-8' });
        const link = document.createElement('a');
        link.href = URL.createObjectURL(blob);
        link.download = normalizeFilename(els.scriptName.value);
        link.click();
        URL.revokeObjectURL(link.href);
    }

    init();
})();
