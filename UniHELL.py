#Copyright (c) 2024 XIAN Kangran
import wave,struct,argparse
from math import sin,cos,asin,acos
from numpy import sign # type: ignore

SAMPLE_RATE = 96000 #采样率
BDF = 'font.bdf' #字体文件名
fltn=0.5 #滤波器系数

PI = 3.14159265358979323846
delta_lenth = 0
older = 0
olderd = 1

def cosflt(cycle=1,t=0):
    global fltn
    if (t>=0 and t<(cycle*fltn)):
        return 0.5*(1-cos(PI*t/cycle/fltn))
    elif (t<0 and t>(-cycle*fltn)):
        return 0.5*(1-cos(PI+PI*t/cycle/fltn))
    else:
        return 1
def write_sin(freq = 0.0,ms = 0.0,phi = 0.0,amp = 1.0,flt = 0,cycle = 1):
    global delta_lenth,older,olderd,fltn
    RATE = SAMPLE_RATE
    P = PI
    num_samples = int(RATE * ms / 1000)
    delta_lenth += RATE * ms / 1000 - num_samples
    if delta_lenth >= 1:
        num_samples += int(delta_lenth)
        delta_lenth -= int(delta_lenth)
    phi_samples = RATE * phi
    older = amp * sin((2 * P * freq * num_samples + phi_samples) / RATE)
    olderd = amp * cos((2 * P * freq * num_samples + phi_samples) / RATE)
    if flt==1:
        for i in range(num_samples):
            outsin = int(32767 * cosflt(SAMPLE_RATE*cycle,i) * amp * sin((2 * P * freq * i + phi_samples) / RATE))
            yield outsin
    elif flt==-1:
        for i in range(num_samples):
            if i<=num_samples-int(num_samples*fltn):
                outsin = int(32767 * amp * sin((2 * P * freq * i + phi_samples) / RATE))
            else:
                outsin = int(32767 * cosflt(SAMPLE_RATE*cycle,-i+int(num_samples*(1-fltn))) * amp * sin((2 * P * freq * i + phi_samples) / RATE))
            yield outsin
    elif flt==2:
        if fltn <= 0.5:
            for i in range(num_samples):
                if i<=num_samples-int(num_samples*fltn):
                    outsin = int(32767 * cosflt(SAMPLE_RATE*cycle,i) * amp * sin((2 * P * freq * i + phi_samples) / RATE))
                else:
                    outsin = int(32767 * cosflt(SAMPLE_RATE*cycle,-i+int(num_samples*(1-fltn))) * amp * sin((2 * P * freq * i + phi_samples) / RATE))
                yield outsin
        else:
            for i in range(num_samples):
                outsin = int(32767 * cosflta(SAMPLE_RATE*cycle,i) * amp * sin((2 * P * freq * i + phi_samples) / RATE))
                yield outsin
    else:
        for i in range(num_samples):
            outsin = int(32767 * amp * sin((2 * P * freq * i + phi_samples) / RATE))
            yield outsin

def write_cos(freq = 0.0,ms = 0.0,phi = 0.0,amp = 1.0,flt = 0,cycle = 1):
    global delta_lenth,older,olderd,fltn
    RATE = SAMPLE_RATE
    P = PI
    num_samples = int(RATE * ms / 1000)
    delta_lenth += RATE * ms / 1000 - num_samples
    if delta_lenth >= 1:
        num_samples += int(delta_lenth)
        delta_lenth -= int(delta_lenth)
    phi_samples = RATE * phi
    older = amp * cos((2 * P * freq * num_samples + phi_samples) / RATE)
    olderd = - amp * sin((2 * P * freq * num_samples + phi_samples) / RATE)
    if flt==1:
        for i in range(num_samples):
            outcos = int(32767 * cosflt(SAMPLE_RATE*cycle,i) * amp * cos((2 * P * freq * i + phi_samples) / RATE))
            yield outcos
    elif flt==-1:
        for i in range(num_samples):
            if i<=num_samples-int(num_samples*fltn):
                outcos = int(32767 * amp * cos((2 * P * freq * i + phi_samples) / RATE))
            else:
                outcos = int(32767 * cosflt(SAMPLE_RATE*cycle,-i+int(num_samples*(1-fltn))) * amp * cos((2 * P * freq * i + phi_samples) / RATE))
            yield outcos
    elif flt==2:
        if fltn <= 0.5:
            for i in range(num_samples):
                if i<=num_samples-int(num_samples*fltn):
                    outcos = int(32767 * cosflt(SAMPLE_RATE*cycle,i) * amp * cos((2 * P * freq * i + phi_samples) / RATE))
                else:
                    outcos = int(32767 * cosflt(SAMPLE_RATE*cycle,-i+int(num_samples*(1-fltn))) * amp * cos((2 * P * freq * i + phi_samples) / RATE))
                yield outcos
        else:
          for i in range(num_samples):
                    outcos = int(32767 * cosflta(SAMPLE_RATE*cycle,i) * amp * cos((2 * P * freq * i + phi_samples) / RATE))
                    yield outcos  
    else:
        for i in range(num_samples):
            outcos = int(32767 * amp * cos((2 * P * freq * i + phi_samples) / RATE))
            yield outcos

def write_tone(freq = 0,ms = 0,phi = 0,amp = 1,sc = 0,phi_con = 1,cycle = 1,flt = 0):
    global delta_lenth,older,olderd
    data = b''
    if sc == 0:
         if phi_con:
            lst = list(write_sin(freq,ms,sign(olderd) * asin(older) + abs(sign(olderd) - 1) / 2 * PI,amp,cycle=cycle,flt=flt))
         else:
            lst = list(write_sin(freq,ms,phi,amp,cycle=cycle,flt=flt))
    else:
         if phi_con:
            lst = list(write_cos(freq,ms,-acos(older),amp,cycle=cycle,flt=flt))
         else:
            lst = list(write_cos(freq,ms,phi,amp,cycle=cycle,flt=flt))
    for digit in lst:
        data += struct.pack('<h',digit)
    return data

def decimal_to_hexadecimal(value):
    if value < 0 or value > 65535:
        raise ValueError("Value must be between 0 and 65535")
    return f"{value:04X}"

def ASK(input = '',freq = 0,ms = 0):
    a = b''
    global older,olderd
    for i in range(len(input)):
        if i-1>=0:
            if input[i] == '1':
                if input[i-1]=='0' and input[i+1]=='0':
                    a+=write_tone(freq,ms,cycle=ms*0.001,flt = 2)
                elif input[i-1]=='0':
                    a+=write_tone(freq,ms,cycle=ms*0.001,flt = 1)
                elif input[i+1]=='0':
                    a+=write_tone(freq,ms,cycle=ms*0.001,flt = -1)
                elif input[i+1]=='1' and input[i+1]=='1':
                    a+=write_tone(freq,ms,cycle=ms*0.001)
            elif input[i] == '0':
                a+=write_tone(0,ms,0,0,cycle=ms*0.001)
                older = 0
                olderd = 1
        else:
            if input[i] == '1':
                a+=write_tone(freq,ms,cycle=ms*0.001)
            elif input[i] == '0':
                a+=write_tone(0,ms,0,0,cycle=ms*0.001)
                older = 0
                olderd = 1
    return a
def FSK(input = '',markfreq = 0,spacefreq = 0,ms = 0):
    a = b''
    global older,olderd
    for i in range(len(input)):
        if input[i] == '1':
            a+=write_tone(markfreq,ms,cycle=ms*0.001)
        elif input[i] == '0':
            a+=write_tone(spacefreq,ms,cycle=ms*0.001)
    return a

def bdf_process(neirong = ''):
    ziku = open(BDF, 'r')
    ziti = str(ziku.read())
    zimap = {}
    zi_size = {} # 横向走纸宽度,有效像素宽,有效像素高,x偏移（可为负数）,y偏移（可为负数）
    for i in range(len(neirong)):
        text='ENCODING ' + str(ord(neirong[i]))
        id = ziti.find(text)
        if id != -1:
            # 读取字体参数
            bmstartid = ziti.find("BITMAP",id) + 7
            bmendid = ziti.find("ENDCHAR",bmstartid) - 1
            dw = [ziti[ziti.find("DWIDTH",id) + 7 : ziti.find("BBX",id) - 3]]
            zi_size[i]=(dw + ziti[ziti.find("BBX",id) + 4 : ziti.find("BITMAP",id) - 1].split())
            bitmap = ziti[bmstartid:bmendid].split("\n")
            # 处理y偏移
            for j in range(abs(int(zi_size[i][4]))):
                if int(zi_size[i][4]) > 0:
                    bitmap.append("00")
                else:
                    bitmap.insert(0,"00")
            if len(bitmap) < 14:
                bitmap.append("00")
            
            for j in range(14-len(bitmap)):
                bitmap.insert(0,"00")
            # 处理x偏移
            if int(zi_size[i][3]) > 0:
                for k in range(14):
                    bitmap[k] = decimal_to_hexadecimal(int(bitmap[k],16) >> abs(int(zi_size[i][3])))
            if int(zi_size[i][3]) < 0:
                for k in range(14):
                    bitmap[k] = decimal_to_hexadecimal(int(bitmap[k],16) << abs(int(zi_size[i][3])))
            # 不等宽补齐
            for j in range(len(bitmap)):
                b = str(bin(int(bitmap[j],16))[2:].zfill(16))
                if int(zi_size[i][1]) > 8:
                    b = b[:-2]
                else:
                    b = b[8:]
                    if int(zi_size[i][0]) > 8:
                        b = b + "000000"
                bitmap[j] = b
            # 从下往上，从左往右扫描
            line = ''
            scan = []
            for k in range(int(zi_size[i][0])):
                for j in range(len(bitmap)):
                    line += bitmap[13-j][k]
                scan.append(line)
                line = ''
            zimap[i] = scan
    return zimap

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('input',type=str,help='欲发送内容')
    parser.add_argument('-o','--output',type=str,default='output.wav',help='wav文件名')
    parser.add_argument('-f','--freq',type=float,default='1500',help='载波频率Hz')
    parser.add_argument('-m','--mode',type=int,default='0',help='0:FELD-HELL')
    args = parser.parse_args()
    zimap = bdf_process(args.input)
    txt = '00000000000000'+'00000000000000'+'11000000000000'+'11000000000000'+'00000000000000'+'00000000000000'+'11000000000000'+'11000000000000'+'00000000000000'+'00000000000000'+'11000000000000'+'11000000000000'+'00000000000000'+'00000000000000'
    for i in range(len(zimap)):
        for j in range(len(zimap[i])):
            txt += zimap[i][j]
    txt += '00000000000000'+'00000000000000'+'11000000000000'+'11000000000000'+'00000000000000'+'00000000000000'+'11000000000000'+'11000000000000'+'00000000000000'+'00000000000000'+'11000000000000'+'11000000000000'
    if args.mode == 0:
        audio = ASK(txt,args.freq,4.08163265)
    if args.output[-4:] != '.wav':
        filename = args.output + '.wav'
    else:
        filename = args.output
    with wave.open(filename,'wb') as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(SAMPLE_RATE)
        f.writeframes(audio)
if __name__ == '__main__':
    main()

