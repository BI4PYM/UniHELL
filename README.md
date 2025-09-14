# UniHELL
用于UTF-8（特别是汉字）的Hellschreiber调制器

生成ASK的FELD HELL格式，14px字体，兼容一般的HELL解调软件，实现了ASK的简单包络成形（滤波）

嫌模糊可以把代码里的fltn=0.5改成0.4或者0.2，越小边界越清晰，带宽也越大。

本项目不分发任何字体文件。使用之前请下载bdf格式字体，推荐使用“文泉驿点阵宋体1.0.0-RC1”的wenquanyi_13px.bdf字体文件。下载链接http://wenq.org/wqy2/index.cgi?BitmapSong

必选参数：欲发送的内容（字符串）

可选参数：--output或-o 输出非压缩PCM WAV文件名，默认为output.wav，非.wav结尾会自动添加.wav

可选参数：--freq或-f 输出WAV音频的载波频率，单位为Hz，默认为1500Hz

例

`python .\UniHELL.py "你好，世界！" --output "output" -f 1500`
