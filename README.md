# UniHELL
用于UTF-8（特别是汉字）的Hellschreiber调制器

生成ASK的FELD HELL格式，14px字体，兼容一般的HELL解调软件

必选参数：欲发送的内容（字符串）

可选参数：--output或-o 输出非压缩PCM WAV文件名，默认为a.wav，非.wav结尾会自动添加.wav

可选参数：--freq或-f 输出WAV音频的载波频率，单位为Hz，默认为1000Hz

例

`python .\UniHELL.py "你好，世界！" --output "output"`

下一步计划：实现ASK的包络成形
