from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import inch
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.cidfonts import UnicodeCIDFont
from reportlab.platypus import (
    Image as RLImage,
    PageBreak,
    Paragraph,
    Preformatted,
    SimpleDocTemplate,
    Spacer,
    Table,
    TableStyle,
)


ROOT = Path(__file__).resolve().parents[1]
APP_DIR = ROOT / "app"
ASSET_DIR = APP_DIR / "how2use_assets"
OUTPUT_PDF = APP_DIR / "how2use.pdf"

PAGE_WIDTH, PAGE_HEIGHT = A4
CONTENT_WIDTH = PAGE_WIDTH - 96

GSI_CONTENT = """"Console Sample v.1"
{
  "uri" "http://127.0.0.1:1009"
  "timeout" "5.0"
  "buffer" "0.1"
  "throttle" "0.5"
  "heartbeat" "30.0"
  "output"
  {
    "precision_time" "3"
    "precision_position" "1"
    "precision_vector" "3"
  }
  "data"
  {
    "provider"               "1"
    "map"                    "1"
    "team"                   "1"
    "round"                  "1"
    "player_state"           "1"
    "player_id"              "1"
    "player_match_stats"     "1"
    "allplayers_id"          "1"
    "allplayers_state"       "1"
    "allplayers_match_stats" "1"
    "bomb"                   "1"
    "player_name"            "1"
    "player_weapons"         "1"
  }
}
"""

PLACEHOLDERS = [
    ("01-install-flow.png", "安装确认界面 / GSI 与启动项检查", (1600, 900)),
    ("02-evolution-branch.png", "进化分支 / TextGUI / 通知 / 准星", (1600, 900)),
    ("03-file-location.png", "文件位置 / 音效资源 / GSI 状态", (1600, 900)),
    ("04-legit-config.png", "合法配置 / autoexec / 常用注入块", (1600, 900)),
    ("05-item-helper.png", "道具助手 / Overlay / 热键说明", (1600, 900)),
    ("06-vscript-panel.png", "自定脚本 / 挂载与执行", (1600, 900)),
    ("07-cscript-panel.png", "CScript / Ticker / CustomTicker", (1600, 900)),
    ("08-settings-theme.png", "程序设置 / 遗产核心 / 主题", (1600, 900)),
]


def ensure_dirs() -> None:
    APP_DIR.mkdir(parents=True, exist_ok=True)
    ASSET_DIR.mkdir(parents=True, exist_ok=True)


def pick_font(size: int) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    candidates = [
        Path("C:/Windows/Fonts/segoeuib.ttf"),
        Path("C:/Windows/Fonts/segoeui.ttf"),
        Path("C:/Windows/Fonts/arial.ttf"),
    ]
    for candidate in candidates:
        if candidate.exists():
            return ImageFont.truetype(str(candidate), size)
    return ImageFont.load_default()


def create_placeholder_image(path: Path, title: str, size: tuple[int, int]) -> None:
    if path.exists():
        return

    width, height = size
    image = Image.new("RGB", size, "#1d142b")
    draw = ImageDraw.Draw(image)

    for offset in range(height):
        ratio = offset / max(1, height - 1)
        r = int(29 + (78 - 29) * ratio)
        g = int(20 + (47 - 20) * ratio)
        b = int(43 + (104 - 43) * ratio)
        draw.line([(0, offset), (width, offset)], fill=(r, g, b))

    draw.rounded_rectangle((70, 70, width - 70, height - 70), radius=42, outline="#d7c2ff", width=4)
    draw.rounded_rectangle((140, 150, width - 140, height - 170), radius=32, outline="#9f7dff", width=3)

    title_font = pick_font(54)
    file_font = pick_font(34)
    note_font = pick_font(28)

    main_title = "StrikeSense HOW2USE"
    file_name = path.name
    size_text = f"{width} x {height}"

    draw.text((width / 2, 220), main_title, fill="#ffffff", anchor="mm", font=title_font)
    draw.text((width / 2, 310), file_name, fill="#f0d8ff", anchor="mm", font=file_font)
    draw.text((width / 2, 388), size_text, fill="#d7c2ff", anchor="mm", font=file_font)
    draw.text((width / 2, 510), title, fill="#ffffff", anchor="mm", font=note_font)
    draw.text((width / 2, 585), "Replace this image with the real software screenshot", fill="#d8d0e7", anchor="mm", font=note_font)
    draw.text((width / 2, 640), "Keep the same file name to regenerate the PDF", fill="#b6a8cf", anchor="mm", font=note_font)

    image.save(path)


def ensure_placeholder_assets() -> None:
    for file_name, title, size in PLACEHOLDERS:
        create_placeholder_image(ASSET_DIR / file_name, title, size)


def register_fonts() -> None:
    pdfmetrics.registerFont(UnicodeCIDFont("STSong-Light"))


def styles():
    sample = getSampleStyleSheet()
    return {
        "title": ParagraphStyle(
            "TitleCN",
            parent=sample["Title"],
            fontName="STSong-Light",
            fontSize=27,
            leading=34,
            textColor=colors.HexColor("#231733"),
            alignment=TA_CENTER,
            spaceAfter=14,
        ),
        "sub": ParagraphStyle(
            "SubCN",
            parent=sample["BodyText"],
            fontName="STSong-Light",
            fontSize=11.5,
            leading=18,
            textColor=colors.HexColor("#5d5274"),
            alignment=TA_CENTER,
            spaceAfter=18,
        ),
        "h1": ParagraphStyle(
            "HeadingOneCN",
            parent=sample["Heading1"],
            fontName="STSong-Light",
            fontSize=18,
            leading=24,
            textColor=colors.HexColor("#2f2146"),
            spaceBefore=8,
            spaceAfter=10,
        ),
        "h2": ParagraphStyle(
            "HeadingTwoCN",
            parent=sample["Heading2"],
            fontName="STSong-Light",
            fontSize=14.5,
            leading=20,
            textColor=colors.HexColor("#4d3195"),
            spaceBefore=10,
            spaceAfter=8,
        ),
        "body": ParagraphStyle(
            "BodyCN",
            parent=sample["BodyText"],
            fontName="STSong-Light",
            fontSize=10.8,
            leading=17,
            textColor=colors.HexColor("#2c2438"),
            spaceAfter=7,
        ),
        "bullet": ParagraphStyle(
            "BulletCN",
            parent=sample["BodyText"],
            fontName="STSong-Light",
            fontSize=10.6,
            leading=16,
            leftIndent=14,
            firstLineIndent=-10,
            textColor=colors.HexColor("#2c2438"),
            spaceAfter=6,
        ),
        "small": ParagraphStyle(
            "SmallCN",
            parent=sample["BodyText"],
            fontName="STSong-Light",
            fontSize=9.5,
            leading=14,
            textColor=colors.HexColor("#625476"),
            spaceAfter=5,
        ),
        "code": ParagraphStyle(
            "CodeBox",
            parent=sample["Code"],
            fontName="Courier",
            fontSize=8.4,
            leading=11,
            backColor=colors.HexColor("#f6f1ff"),
            borderPadding=9,
            borderRadius=None,
            borderColor=colors.HexColor("#d9ccff"),
            borderWidth=0.8,
            leftIndent=0,
            rightIndent=0,
            spaceAfter=10,
        ),
    }


def cover_block(text_styles: dict[str, ParagraphStyle]):
    return [
        Spacer(1, 1.0 * inch),
        Paragraph("StrikeSense 使用教程", text_styles["title"]),
        Paragraph(
            "适用于网站下载页内附带的快速上手文档。本文基于程序当前安装逻辑、GSI 写入逻辑、启动项补全逻辑与各模块界面整理，默认输出为 PDF。",
            text_styles["sub"],
        ),
        Paragraph("开始前必须先确认安装项", text_styles["h1"]),
        Paragraph(
            "1. 请先保证 <b>GSI 文件</b> 已经安装完成。<br/>"
            "2. 请先保证 <b>CS2 启动项</b> 已经补齐，至少应包含 <b>-vulkan -condebug</b>。<br/>"
            "3. 如果功能异常，先不要急着改模块参数，优先核查这两项是否真的已经写入成功。",
            text_styles["body"],
        ),
        Paragraph(
            "正常情况下，StrikeSense 会自动尝试写入这两项；实际代码里即便启动项补全失败，GSI 文件通常仍然可以成功写入。所以排查时请把“GSI 是否存在”和“启动项是否补齐”分开看。",
            text_styles["body"],
        ),
        Paragraph(
            "如果你玩的不是 Steam 原生启动，而是某些第三方平台，请额外去 <b>第三方平台自己的启动项设置</b> 里检查参数是否被覆盖。若原生设置无效、平台侧也确认过仍然有问题，可以再尝试打开 <b>宽容窗口检测</b>。但这会带来一些额外 BUG，虽然通常只是轻微影响体验，我们也 <b>不对第三方平台或第三方服务器做任何可用保证和安全保证</b>。",
            text_styles["body"],
        ),
        Paragraph(
            "你可以把第三方服务器理解成私营规则环境，很多规矩本身就是 OWNER 说了算。极端情况下，对方平台或服务器就算提出很离谱的限制，也不是 StrikeSense 能控制的范围。我们的目标是理论上兼容全部服务器，但第三方规则本身不在我们保证范围内。",
            text_styles["small"],
        ),
        Paragraph(
            "如果你需要解锁全部功能，请优先以 <b>管理员身份运行</b>。部分写入动作、部分依赖更高权限的功能，以及某些完整能力，在普通权限下可能只能部分工作。",
            text_styles["body"],
        ),
        Paragraph(
            "如果你使用的是 <b>OEM 解锁</b>，可以去官网生成密钥，然后创建对应文件，放入 <b>%UserProfile%/StrikeSense</b> 程序数据目录即可。",
            text_styles["body"],
        ),
        Spacer(1, 0.2 * inch),
    ]


def image_block(file_name: str, height: float = 3.45 * inch):
    image = RLImage(str(ASSET_DIR / file_name))
    image._restrictSize(CONTENT_WIDTH, height)
    image.hAlign = "CENTER"
    return image


def troubleshooting_table(text_styles: dict[str, ParagraphStyle]) -> Table:
    table_head = ParagraphStyle(
        "TableHead",
        parent=text_styles["small"],
        fontName="STSong-Light",
        fontSize=10.2,
        leading=13,
        textColor=colors.HexColor("#2f2146"),
        spaceAfter=0,
    )
    table_body = ParagraphStyle(
        "TableBody",
        parent=text_styles["small"],
        fontName="STSong-Light",
        fontSize=8.9,
        leading=12,
        textColor=colors.HexColor("#2c2438"),
        spaceAfter=0,
    )
    rows = [
        [Paragraph("现象", table_head), Paragraph("先看哪里", table_head), Paragraph("怎么处理", table_head)],
        [
            Paragraph("网站下载后完全没有反应", table_body),
            Paragraph("是否真的安装了 GSI 文件", table_body),
            Paragraph("重新在软件里执行一次安装；仍不行就手动创建 cfg 文件。", table_body),
        ],
        [
            Paragraph("部分模块可开，但游戏事件不触发", table_body),
            Paragraph("gamestate_integration_square.cfg 内容是否完整", table_body),
            Paragraph("对照本文手动创建，重启 CS2。", table_body),
        ],
        [
            Paragraph("软件提示自动添加启动项失败", table_body),
            Paragraph("Steam 是否完全关闭、是否存在 localconfig.vdf、账号是否启动过 CS2", table_body),
            Paragraph("手动补上 -vulkan -condebug，重启 Steam。", table_body),
        ],
        [
            Paragraph("第三方平台启动后仍无效", table_body),
            Paragraph("第三方平台自己的启动项、覆盖层与窗口检测设置", table_body),
            Paragraph("先去第三方平台补同样的启动项；仍不行再尝试宽容窗口检测，但会有轻微额外 BUG。", table_body),
        ],
        [
            Paragraph("CScript 没触发", table_body),
            Paragraph("autoexec.cfg / StrikeTicker.cfg / CustomTicker", table_body),
            Paragraph("游戏内执行 exec autoexec 或重启游戏。", table_body),
        ],
    ]
    table = Table(rows, colWidths=[1.38 * inch, 1.95 * inch, 2.27 * inch], repeatRows=1)
    table.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#ece3fb")),
        ("TEXTCOLOR", (0, 0), (-1, 0), colors.HexColor("#2f2146")),
        ("FONTNAME", (0, 0), (-1, -1), "STSong-Light"),
        ("FONTSIZE", (0, 0), (-1, -1), 8.9),
        ("LEADING", (0, 0), (-1, -1), 12),
        ("GRID", (0, 0), (-1, -1), 0.6, colors.HexColor("#d9ccff")),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, colors.HexColor("#faf7ff")]),
        ("LEFTPADDING", (0, 0), (-1, -1), 7),
        ("RIGHTPADDING", (0, 0), (-1, -1), 7),
        ("TOPPADDING", (0, 0), (-1, -1), 6),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 6),
    ]))
    return table


def module_section(title: str, intro: str, bullets: list[str], image_name: str, text_styles: dict[str, ParagraphStyle]):
    content = [Paragraph(title, text_styles["h1"]), Paragraph(intro, text_styles["body"]), image_block(image_name)]
    content.append(Spacer(1, 0.12 * inch))
    for bullet in bullets:
        content.append(Paragraph(f"• {bullet}", text_styles["bullet"]))
    content.append(Spacer(1, 0.12 * inch))
    return content


def page_number(canvas, doc):
    canvas.saveState()
    canvas.setFont("Helvetica", 9)
    canvas.setFillColor(colors.HexColor("#8578a1"))
    canvas.drawRightString(PAGE_WIDTH - 48, 26, f"Page {doc.page}")
    canvas.restoreState()


def build_story(text_styles: dict[str, ParagraphStyle]):
    story = []
    story.extend(cover_block(text_styles))

    story.extend(module_section(
        "一、安装与首次启动",
        "程序启动后会先检查是否已经存在 <b>gamestate_integration_square.cfg</b>。如果没找到，程序会自动弹安装流程；它会优先读取已保存的 cfg 目录，其次通过 Steam 注册表和 appmanifest_730.acf 自动定位 CS2 安装目录，最后才会要求你手动选择 cfg 目录。",
        [
            "正确的 cfg 目录通常在：Steam/steamapps/common/Counter-Strike Global Offensive/game/csgo/cfg",
            "安装成功后，程序会把 cfg 路径记录到 %UserProfile%/StrikeSense/setting/gsi.json，后续优先沿用这条路径",
            "GSI 写入成功后，程序会继续尝试为本机 Steam 账号补上 -vulkan 和 -condebug",
            "如果启动项是这次才加上的，请完全重启 Steam，再启动 CS2",
        ],
        "01-install-flow.png",
        text_styles,
    ))

    story.append(Paragraph("安装失败时该怎么判断", text_styles["h2"]))
    story.append(Paragraph("程序当前的失败分支主要有这些：找不到 Steam 注册表路径、找不到本地 Steam 账号 userdata、找不到 localconfig.vdf、Steam 占用配置文件、当前账号从未在这台机器启动过 CS2、或者 localconfig.vdf 格式异常。正常情况下这两个函数不会轻易失效；更常见的是启动项自动追加失败，但 GSI 文件已经写进去了。", text_styles["body"]))
    story.append(Spacer(1, 0.1 * inch))

    story.extend(module_section(
        "二、进化分支",
        "这是当前日常使用最频繁的一页，负责一部分实战可见的增强内容，包括 TextGUI、通知、外置准星、即时音量调整，以及若干界面化开关与视觉参数。",
        [
            "默认配置会优先把 TextGUI 与通知提示开出来，其余危险或高干扰功能保持关闭",
            "如果你只想先看软件有没有正常工作，先看 TextGUI 和通知是否已经能在实战里出现",
            "即时音量调整需要对应权限时，程序会直接给出提示；调不好时先恢复默认，再一项一项重新开",
            "这页更适合做“先把观感和提示跑起来”的第一轮上手",
        ],
        "02-evolution-branch.png",
        text_styles,
    ))

    story.extend(module_section(
        "三、文件位置",
        "界面标题虽然叫“文件位置”，但它承担的不只是打开目录，还负责把音效资源、音乐包、GSI 工作状态、低内存模式和音量状态整理到一起。",
        [
            "首次使用建议先打开相关目录，确认资源文件到底放到了哪里",
            "自定义击杀音效、MVP、胜负、安拆包等音频时，优先从这一页进入目录",
            "如果某个声音不响，先排查文件是否真的放对，再排查格式是否符合当前支持范围",
            "看到 GSI 状态没跑起来时，不要先怪音效模块，先回头查 GSI 文件本体",
        ],
        "03-file-location.png",
        text_styles,
    ))

    story.extend(module_section(
        "四、合法配置",
        "这一页用于帮你更快整理 autoexec 相关内容。实际代码会针对常见配置块做一键写入或追加，并支持直接打开 autoexec 继续手调。",
        [
            "适合处理 SOCD、滚轮跳、跳投、跨平台常见绑定块等高频合法配置",
            "建议先备份你自己的 autoexec，再在软件里一点点追加，避免把旧习惯全冲掉",
            "写入后如果游戏已经开着，记得在控制台执行 exec autoexec，或者直接重启游戏",
            "如果你习惯纯手写，也可以把它当作可视化模板生成器",
        ],
        "04-legit-config.png",
        text_styles,
    ))

    story.extend(module_section(
        "五、道具助手",
        "道具助手主要服务于 Overlay 和道具图使用流。它可以在游戏前台直接显示教学覆盖，并且有独立热键和目录入口。",
        [
            "先把资源目录准备好，再回到页面里绑定开关、导航和确认热键",
            "Overlay 开启后请进图实测一次，确认热键、透明层和点位图都能正常出现",
            "如果界面里看起来开了，但游戏里没出现，优先检查覆盖显示相关权限和资源文件路径",
            "不确定哪里改乱了时，先恢复默认设置，再重新绑定热键",
        ],
        "05-item-helper.png",
        text_styles,
    ))

    story.extend(module_section(
        "六、自定脚本（VScript）",
        "VScript 适合做“基于官方状态的逻辑编排”。当前版本已经支持挂载、执行、轮询、卸载、分页浏览，以及对控制台日志、Steam 账号对象、模块状态、TextGUI、音频和图片行为的调用。",
        [
            "第一次上手建议先去脚本目录看示例脚本，再回软件里挂载",
            "如果脚本要持续轮询，就观察挂载后的状态是否一直保持轮询中",
            "写脚本前可先去网站文档页看 API，再决定做提示、切换、记录还是联动",
            "脚本能挂上但没有实际表现时，优先排查依赖的模块本身是否已经开启",
        ],
        "06-vscript-panel.png",
        text_styles,
    ))

    story.extend(module_section(
        "七、CScript（BETA）",
        "CScript 走的是 ticker / 绑定链路，程序会以 64Hz 频率驱动所选 Ticker 按键，并尝试向 autoexec.cfg 写入 StrikeTicker.cfg 的绑定。它更接近“按键驱动脚本层”。",
        [
            "启用后会生成 StrikeTicker.cfg，并按需生成 CustomTicker/*.cfg",
            "autoexec.cfg 中会写入由软件管理的绑定块，不建议你手工乱删中间那一段",
            "如果游戏已经启动，记得手动执行 exec autoexec 或直接重启游戏",
            "CScript 不工作时，先看 autoexec.cfg、StrikeTicker.cfg、CustomTicker 是否真的生成了",
        ],
        "07-cscript-panel.png",
        text_styles,
    ))

    story.extend(module_section(
        "八、程序设置、遗产核心与主题",
        "除了功能模块本身，程序还有主题、语言、旧核心页和程序级设置页。它们主要负责长期使用体验，而不是单次开关。",
        [
            "主题建议先选一个你能长期看着不累的，再去细调 TextGUI 和通知",
            "中英文切换属于常驻设置，切换后可顺手检查页面布局是否符合自己的阅读习惯",
            "遗产核心更适合熟悉旧工作流的用户，新用户先把前面几页跑顺再回来碰它",
            "如果你要长期常驻软件，程序设置页里的目录、窗口行为和基础偏好值得先整理一遍",
        ],
        "08-settings-theme.png",
        text_styles,
    ))

    story.append(Paragraph("九、字面意思容易模糊的开关说明", text_styles["h1"]))
    for line in [
        "<b>低内存模式</b>：重点是降低常驻资源占用，不是说所有资源都会立刻被清空，更适合长时间挂后台。",
        "<b>宽容窗口检测</b>：它是兼容性兜底选项，不是默认推荐项。主要给第三方平台或特殊窗口环境排错使用，代价是可能带来轻微额外 BUG。",
        "<b>即时音量调整</b>：指按状态实时调节 CS2 的音量比例，不是直接改整个系统总音量。",
        "<b>TextGUI</b>：指游戏内常驻的信息文字层，不是普通的一次性弹窗提示。",
        "<b>通知</b>：指事件触发时的提示样式、位置与持续时间；如果你嫌吵或嫌挡视线，可以减弱通知但保留 TextGUI。",
        "<b>遗产核心</b>：更偏旧工作流页面，不代表它更高级，只是保留了旧逻辑。",
        "<b>自定脚本 / VScript</b>：更偏官方状态联动与逻辑编排。",
        "<b>CScript</b>：更偏按键驱动和 ticker 绑定链路，而且当前仍属于 BETA。",
        "<b>OEM 解锁</b>：不是单点按钮功能，而是生成密钥后把对应文件放入程序数据目录来解锁。",
    ]:
        story.append(Paragraph(f"• {line}", text_styles["bullet"]))

    story.append(PageBreak())
    story.append(Paragraph("十、必要时手动创建 GSI 文件", text_styles["h1"]))
    story.append(Paragraph("如果你确认软件没把 GSI 文件写进去，或者你想自己手动补齐，那么请在 CS2 的 cfg 目录里创建文件 <b>gamestate_integration_square.cfg</b>，内容如下。文件名要完全一致，内容也不要漏字段。", text_styles["body"]))
    story.append(Preformatted(GSI_CONTENT, text_styles["code"]))
    story.append(Paragraph("创建完以后，重启 CS2；如果你刚补了启动项，最好连 Steam 一起完整重启。", text_styles["body"]))
    story.append(Paragraph("启动项部分请确认至少包含：<b>-vulkan -condebug</b>。如果自动添加失败，就手动补到 CS2 启动项里。", text_styles["body"]))

    story.append(Paragraph("十一、排查顺序", text_styles["h1"]))
    story.append(Paragraph("建议你按下面的顺序排查，不要一上来就乱改模块参数。", text_styles["body"]))
    story.append(troubleshooting_table(text_styles))
    story.append(Spacer(1, 0.12 * inch))
    story.append(Paragraph("尤其要记住：自动补启动项失败，不等于 GSI 文件也失败。只要 cfg 已经写进去，很多依赖 GSI 的模块依然是有机会正常工作的。", text_styles["body"]))
    story.append(Paragraph("如果你使用第三方对战平台，请把“Steam 启动项”和“第三方平台启动项”视为两套都要核查的东西。某些平台会覆盖或接管启动参数，这时要以平台侧实际生效的设置为准。", text_styles["body"]))
    story.append(Paragraph("宽容窗口检测可以作为最后一步兼容性尝试，但它不是无代价的保险开关，可能引入一些轻微体验问题。只有在第三方平台环境下确实排完常规项还不行时，再考虑打开。", text_styles["small"]))

    story.append(Paragraph("十二、遇到问题时如何正确求助", text_styles["h1"]))
    story.append(Paragraph("遇到问题后，请去 <b>不和谐频道</b> 提交帖子，并尽量一次性给全信息。这样别人才能真正帮你复现和定位。", text_styles["body"]))
    for line in [
        "• 先描述你做了什么操作，再描述问题是在哪一步出现的。",
        "• 把 Debugger 调试器的日志截图发出来，不要只说“没反应”。",
        "• 说明复现步骤，例如：先开软件、再开 CS2、进入对局后切换某个模块。",
        "• 把你自己的参数、开关状态、启动项、是否改过 cfg 目录一起写上。",
        "• 不要只发“用不了”“有BUG”“为什么不行”这种一句话反馈，这样既烦人，也几乎无法得到解决。",
    ]:
        story.append(Paragraph(line, text_styles["bullet"]))

    story.append(Spacer(1, 0.2 * inch))
    story.append(Paragraph("附：占位图替换说明", text_styles["h2"]))
    story.append(Paragraph("本教程当前使用的截图位都来自 app/how2use_assets 目录。你只要把同名图片替换进去，再重新运行本脚本，就能生成带正式截图的新 PDF。当前默认占位图尺寸统一为 1600 × 900。", text_styles["small"]))
    for file_name, title, size in PLACEHOLDERS:
        story.append(Paragraph(f"• {file_name} — {title} — 建议尺寸 {size[0]} × {size[1]}", text_styles["small"]))

    return story


def build_pdf() -> None:
    ensure_dirs()
    ensure_placeholder_assets()
    register_fonts()
    text_styles = styles()

    document = SimpleDocTemplate(
        str(OUTPUT_PDF),
        pagesize=A4,
        leftMargin=48,
        rightMargin=48,
        topMargin=48,
        bottomMargin=42,
        title="StrikeSense 使用教程",
        author="Codex",
    )
    story = build_story(text_styles)
    document.build(story, onFirstPage=page_number, onLaterPages=page_number)


if __name__ == "__main__":
    build_pdf()
