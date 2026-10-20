# UniHELL 2.0.0

<p align="center"><img src="assets/unihell-128.png" width="128" height="128" alt="UniHELL logo"></p>

UniHELL 是一个 Windows 上的 Hellschreiber（地狱传真）收发程序，使用 C99、Win32 API、FreeType 和 HarfBuzz 实现。Hellschreiber 发送的是逐列扫描的位图，不是字符编码；接收端还原成图像，由人眼阅读。

项目提供两个程序：

- `unihell.exe`：带实时声卡、彩色瀑布和文本发射编辑的 GUI。
- `unihell-cli.exe`：命令行收发工具，可生成/解码 WAV 与 PGM，便于离线测试和批量处理。

## 2.0.0 更新

- GUI 增加界面语言选择：汉语、英语、韩语、日语、德语、俄语、乌克兰语、意大利语、法语。
- 语言设置写入 `unihell.ini`；首次启动跟随系统语言，之后保持上次选择。
- 命令行程序更名为 `unihell-cli.exe`，支持中文和英文，可用 `--lang en|zh` 指定。
- 加入项目 Logo，并嵌入 GUI、CLI 可执行文件和窗口图标。

### 核心功能

- 统一 C99 核心，GUI 与 CLI 共用同一套渲染、调制和解调代码。
- 支持 10 种 Hell 模式，收发两端统一 RRC 成形/匹配滤波。
- FSK 改为普通 FSK + 瞬时频率鉴频，与 fldigi 的 FSK Hell 实现对齐。
- 完成实时声卡收发、内部回环、SQL、瀑布对比度、相位/速率微调。
- 发射输入支持逐字红色进度、追加/删除和输入法组合处理。
- 运行时渲染 Unicode 和 Emoji，字体统一放在 `fonts/`。
- 接收缓冲按绝对列计数追加，减少分块接缝和固定位置错位。
- 清理测试产物、Python 代码、旧滤波分支和未使用字体。
- 新增 `VERSION`、`CHANGELOG.md` 和 `build.ps1 -Release` 发布流程。

完整记录见 [CHANGELOG.md](CHANGELOG.md)。

## 功能

- 模式：Feld Hell、Slow Hell、Feld Hell X5、Feld Hell X9、FSK Hell-245、FSK Hell-105、Hell 80、PSK Hell-245、PSK Hell-105、CMT14。
- 44.1 kHz 采样率，14 行 Hell 点阵。
- AM/FSK/PSK 收发均使用 RRC，滚降系数 0.35，span 4；CMT14 为 14 音并行调制。
- GUI：实时声卡收发、内部回环、彩色瀑布、SQL、对比度自动/手动调节、频率拖动、相位/速率微调。
- 文本输入：Unicode、Emoji、已发送字符逐字变红、追加、删除、输入法组合。
- 接收显示：双排图像、自动换行、旧内容上移、右键清空。

## 目录

```
UniHELL/
  src/hell.c          Hell 核心、字体渲染、调制解调、WAV/PGM、CLI
  src/gui.c           Win32 GUI（包含 hell.c）
  src/unihell.rc      图标资源
  assets/             Logo PNG 和 ICO
  fonts/              运行时字体、许可证和 NOTICE
  third_party/        FreeType / HarfBuzz 头文件与静态库
  build.ps1           Windows 构建与发布脚本
  VERSION             版本号
  CHANGELOG.md        版本记录
  unihell.exe         GUI（构建产物）
  unihell-cli.exe     CLI（构建产物）
  unihell.ini         界面语言设置（首次切换后生成）
  README.md
  LICENSE             MIT（代码）
```

## 构建

需要 64 位 MinGW-w64 GCC，以及 `third_party/` 中的 FreeType/HarfBuzz 静态库。在项目根目录执行：

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

生成 `unihell.exe` 和 `unihell-cli.exe`。可执行文件静态链接 MinGW 运行时，发布包不依赖额外的 `libwinpthread-1.dll`。字体按“可执行文件所在目录 + `fonts/`”查找，发布时请保持字体目录与可执行文件同级。

### 生成发布包

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Release
```

会在 `dist/` 生成 `UniHELL-2.0.0.zip`，包含 GUI、CLI、字体、许可证、README、CHANGELOG、VERSION 和 `SHA256SUMS.txt`。`dist/` 是构建产物，不纳入源码版本控制。

## 运行

GUI 直接运行 `unihell.exe`。命令行示例：

```text
unihell-cli --version
unihell-cli list
unihell-cli tx "HELLO HELL" -m feld -o out.wav
unihell-cli tx "汉字与 Emoji 😀" -m fsk245 -f 1500 -o out.wav
unihell-cli rx in.wav -m fsk245 -f 1500 -o out.pgm --scale 2
unihell-cli rx in.wav -m feld -o out.pgm --phase 50 --rate 1000
unihell-cli rx --live -m feld --seconds 20 -d 0
```

CLI 默认跟随系统语言选择中文或英文；也可以显式指定：

```text
unihell-cli --lang en list
unihell-cli --lang zh --help
```

`tx` 常用参数：`-m` 模式、`-f` 载波、`-o` 输出 WAV、`-F/--font` 字体、`--alpha` RRC 滚降、`--span` RRC 跨度、`--text-file` 文本文件、`--source` 输出源 PGM、`--live` 直接播放、`-d` 输出设备。

`rx` 常用参数：`-m` 模式、`-f` 载波、`-o` 输出 PGM、`--scale` 放大、`--single` 单排输出、`--blackboard` 反色、`--phase` 像素内相位（0–100）、`--rate` 像素速率微调（950–1050，1000 为标称）、`--live` 实时读取声卡、`-d` 输入设备。

## 模式

| 模式 | 名称 | 列速 | 调制 |
|---|---|---:|---|
| `feld` | Feld Hell | 17.5 列/秒 | AM，245 像素/秒 |
| `slow` | Slow Hell | 2.1875 列/秒 | AM |
| `x5` | Feld Hell X5 | 87.5 列/秒 | AM，1225 像素/秒 |
| `x9` | Feld Hell X9 | 157.5 列/秒 | AM，2205 像素/秒 |
| `fsk245` | FSK Hell-245 | 17.5 列/秒 | 普通 FSK，245 像素/秒，音间隔 122.5 Hz |
| `fsk105` | FSK Hell-105 | 17.5 列/秒 | 普通 FSK，245 像素/秒，音间隔 55 Hz |
| `hell80` | Hell 80 | 35 列/秒 | 普通 FSK，490 像素/秒，音间隔 300 Hz |
| `psk245` | PSK Hell-245 | 17.5 列/秒 | 差分 BPSK |
| `psk105` | PSK Hell-105 | 7.5 列/秒 | 差分 BPSK |
| `cmt14` | CMT14 | 17.5 列/秒 | 14 个音，间隔 17.5 Hz |

`fsk105` 的参数按 fldigi 的 FSKH105 实现对齐；它的名字是 105，但当前 fldigi 源码实际使用 17.5 列/秒（245 像素/秒）和 55 Hz 音间隔。

模式对应的大致接收带宽：FELD 331 Hz、SLOW 41 Hz、X5 1654 Hz、X9 2977 Hz、FSK245 490 Hz、FSK105 220 Hz、HELL80 800 Hz、PSK245 331 Hz、PSK105 142 Hz、CMT14 400 Hz。

## GUI

- 上方是双排接收图；中间是发射文本输入框；下方是 0–3000 Hz 彩色瀑布。
- 底部可选择模式、频率、带宽、输入/输出声卡和界面语言，并提供 SQL、瀑布对比度、自动调整、相位和速率滑块。
- 点击瀑布会把接收中心频率移到点击位置；拖动红框可连续改变接收频率。红框宽度表示接收带宽，FSK 模式下左右边对应传号/空号频率；红框上沿的粗红线表示接收带宽。
- “发射 / 停止 / 接收”是互斥状态。停止只暂停，不清空接收图和瀑布；右键接收显示区才清空。
- 发射时内部回环解调按实际播放进度显示；没有新文本时持续发送空白，不自动切回接收。

### 文本输入

- 已发送字符逐字变红，未发送字符为黑色。
- 输入强制追加到末尾，避免着色时旧文本被选中替换。
- 输入法组合期间不发送拼音，不打断正在播放的旧内容；候选提交后在完整字符边界接续组合后的文本。
- 删除未发送尾部会立即生效；删除已发送部分会从公共前缀处重新发送。
- 每次开始发射先发送 `...`；从发射切到停止或接收前也补发 `...`。

### 接收显示

- 接收图按 2×2 放大显示，双排内容一致。
- 超出可视区域后保留最近内容，旧内容整体上移；自动换行按固定列宽分页。
- 瀑布使用 8192 点 FFT，颜色从黑、紫、蓝、青、绿、黄到红；低于紫色阈值的部分为黑色。
- SQL 低于阈值时暂停显示新解调内容，但保留已有图像。
- 外部发射端与声卡时钟不一致时，先用“速率”把竖笔画调直，再用“相位”把笔画居中；默认 50/1000。

## 信号处理

- Hell 图像按列传输：每列 14 行，先发送底部行，接收端按同一顺序还原。
- AM 模式：复下变频、4 阶 Butterworth 复基带低通、包络检测、RRC 匹配滤波、逐像素采样；带慢速 AGC。
- FSK 模式：普通 FSK，复下变频、RRC 滤波、瞬时频率鉴频、符号内积分和门限判决。
- PSK 模式：差分 BPSK，像素保持相位为白，翻转相位为黑。
- CMT14：14 行像素同时映射到中心频率上下间隔 17.5 Hz 的 14 个音；接收端按列矩形积分并做 50% 门限判决。
- 发射端：RRC 频率/幅度脉冲成形、连续相位载波、5 ms 首尾淡入淡出。
- 接收端每 4096 帧处理一次，输入重采样到 44100 Hz；按完整列对齐裁剪历史缓冲，并用绝对列计数追加，避免分块接缝。

## 字体与许可

默认字体为 `fonts/wenquanyi_13px.pcf`（文泉驿点阵宋体 13px）。GUI 使用默认字体；CLI 可用 `-F/--font` 指定其他字体。`fonts/` 中还包含可选的 CJK、日文和 Emoji 字体，具体授权见 `fonts/NOTICE.md`。

发布包中的 `THIRD_PARTY_LICENSES/` 还包含实际静态链接组件的许可证原文：FreeType 2.13.3（FTL）、FreeType 的 BDF/PCF 驱动、FreeType 内置 zlib 1.3、HarfBuzz 8.5.0（含 ms-use 数据许可证），以及 MinGW-w64 运行时、winpthread、GCC 运行时及其许可证例外。两个可执行文件都不依赖外部的 `libwinpthread-1.dll` 或 `libstdc++-6.dll`。

Portions of this software are copyright © 2024 The FreeType Project (<https://www.freetype.org>). All rights reserved.

代码使用 MIT 许可证，见 `LICENSE`。字体许可证文本与字体放在同一目录。

## 已知限制

- Hellschreiber 本身是图像模式，没有纠错编码，接收质量取决于信噪比、频偏和收发时钟一致性。
- X5/X9 的 RRC 会带来一定码间拖尾；如果与 fldigi 对测发现高速模式模糊，可优先检查 fldigi 的模式、中心频率和发射宽度设置。
- 外部信号出现固定位置的一像素错位时，先保持“速率”为 1000，微调“相位”；整体倾斜才调整“速率”。
