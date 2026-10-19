/* UniHELL C core: BDF text, Hell AM/FSK, WAV and PGM. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <mmsystem.h>
#include <dwrite.h>
#include <d2d1.h>
#endif

#define FS 44100
#define ROWS 14
#define PI 3.14159265358979323846
#define UNIHELL_VERSION "2.0.0"
#define UNIHELL_VERSION_W L"2.0.0"
#define MAX_TEXT_BYTES (64u*1024u*1024u)
#define MAX_CANVAS_PIXELS (64u*1024u*1024u)
#define MAX_WAV_FRAMES (32u*1024u*1024u)

enum {
    CLI_S_USAGE,
    CLI_S_TX_NEEDS,
    CLI_S_RX_NEEDS,
    CLI_S_UNKNOWN_MODE,
    CLI_S_RENDER_FONT,
    CLI_S_WRITE_FAIL,
    CLI_S_READ_FAIL,
    CLI_S_TX_DONE,
    CLI_S_RX_DONE,
    CLI_S_LIST_CMT,
    CLI_S_LIST_MODE,
    CLI_S_COUNT
};

static int cli_lang;
static int cli_lang_ready;

static const char *const cli_text[2][CLI_S_COUNT] = {
    {
        "usage:\n"
        "  unihell-cli [--lang en|zh] tx \"TEXT\" [-m mode] [-o out.wav] [-F font]\n"
        "  unihell-cli [--lang en|zh] rx in.wav [-m mode] [-o out.pgm]\n"
        "  unihell-cli [--lang en|zh] list\n"
        "  unihell-cli [--lang en|zh] --version\n",
        "tx needs text or --text-file\n",
        "rx needs input.wav\n",
        "unknown mode: %s\n",
        "cannot render font: %s\n",
        "cannot write %s\n",
        "cannot read %s\n",
        "mode %s font %s columns %d seconds %.3f wav %s\n",
        "mode %s columns %d pgm %s\n",
        "%s %.4g col/s cmt 14 tones bw %.1f Hz\n",
        "%s %.4g col/s %s baud %.0f bw %.1f Hz\n"
    },
    {
        "用法：\n"
        "  unihell-cli [--lang en|zh] tx \"文本\" [-m 模式] [-o 输出.wav] [-F 字体]\n"
        "  unihell-cli [--lang en|zh] rx 输入.wav [-m 模式] [-o 输出.pgm]\n"
        "  unihell-cli [--lang en|zh] list\n"
        "  unihell-cli [--lang en|zh] --version\n",
        "tx 需要文本或 --text-file\n",
        "rx 需要输入 WAV 文件\n",
        "未知模式：%s\n",
        "无法渲染字体：%s\n",
        "无法写入 %s\n",
        "无法读取 %s\n",
        "模式 %s 字体 %s 列数 %d 时长 %.3f 秒 WAV %s\n",
        "模式 %s 列数 %d PGM %s\n",
        "%s %.4g 列/秒 CMT 14 音 带宽 %.1f Hz\n",
        "%s %.4g 列/秒 %s 波特 %.0f 带宽 %.1f Hz\n"
    }
};

#define CT(id) cli_text[cli_lang][(id)]

#ifdef _WIN32
const GUID IID_IDWriteFactory = {0xb859ee5a,0xd838,0x4b5b,{0xa2,0xe8,0x1a,0xdc,0x7d,0x93,0xdb,0x48}};
const GUID IID_ID2D1Factory = {0x06152247,0x6f50,0x465a,{0x92,0x45,0x11,0x8b,0xfd,0x3b,0x60,0x07}};
#endif

typedef struct {
    int w,h;
    unsigned char *p;
} Img;

static FT_Library emoji_ft;
static FT_Face emoji_face;
static hb_font_t *emoji_font;
static int emoji_ready;

typedef struct {
    double re,im;
} Cpx;

typedef struct {
    const char *name;
    double col;
    int fsk;
    double shift;
    int psk,cmt;
} Mode;

static Mode modes[] = {
    {"feld",17.5,0,0,0,0},
    {"slow",2.1875,0,0,0,0},
    {"x5",87.5,0,0,0,0},
    {"x9",157.5,0,0,0,0},
    {"fsk245",17.5,1,122.5,0,0},
    {"fsk105",17.5,1,55,0,0},
    {"hell80",35,1,300,0,0},
    {"psk245",17.5,0,0,1,0},
    {"psk105",7.5,0,0,1,0},
    {"cmt14",17.5,0,0,0,1}
};

static int hell_rx_phase=50;
static int hell_rx_rate=1000;

static Mode *mode_of(const char *s) {
    int i;
    for (i=0;i<(int)(sizeof(modes)/sizeof(modes[0]));i++)
        if (!strcmp(s,modes[i].name)) return &modes[i];
    return 0;
}

static double mode_bw(Mode *m,double alpha) {
    if (m->cmt) return 400;
    if (m->psk) return m->col*ROWS*(1+alpha);
    if (m->fsk) {
        if (!strcmp(m->name,"fsk245")) return 490;
        if (!strcmp(m->name,"fsk105")) return 220;
        if (!strcmp(m->name,"hell80")) return 800;
        return m->shift+m->col*ROWS;
    }
    double base=m->col*ROWS*(1+alpha);
    return base;
}

static void *xmalloc(size_t n) {
    void *p = malloc(n ? n : 1);
    if (!p) { fprintf(stderr,"out of memory\n"); exit(1); }
    return p;
}

static void *xcalloc(size_t n,size_t s) {
    void *p = calloc(n ? n : 1,s ? s : 1);
    if (!p) { fprintf(stderr,"out of memory\n"); exit(1); }
    return p;
}

static void *xrealloc(void *p,size_t n) {
    void *q = realloc(p,n ? n : 1);
    if (!q) { fprintf(stderr,"out of memory\n"); exit(1); }
    return q;
}

static void asset_path(const char *rel,char *out,int n) {
#ifdef _WIN32
    char exe[MAX_PATH];
    DWORD k=GetModuleFileNameA(0,exe,MAX_PATH);
    if (k && k<MAX_PATH) {
        while (k>0 && exe[k-1]!='\\' && exe[k-1]!='/') k--;
        exe[k]=0;
        snprintf(out,n,"%s%s",exe,rel);
        return;
    }
#endif
    snprintf(out,n,"%s",rel);
}

static uint32_t nextcp(const char **sp) {
    const unsigned char *s = (const unsigned char *)*sp;
    uint32_t c;
    if (!*s) return 0;
    c = *s++;
    if (c>=0x80) {
        if ((c&0xe0)==0xc0 && (s[0]&0xc0)==0x80) {
            c = ((c&0x1f)<<6)|(s[0]&0x3f);
            s += 1;
        } else if ((c&0xf0)==0xe0 && (s[0]&0xc0)==0x80 && (s[1]&0xc0)==0x80) {
            c = ((c&0x0f)<<12)|((s[0]&0x3f)<<6)|(s[1]&0x3f);
            s += 2;
        } else if ((c&0xf8)==0xf0 && (s[0]&0xc0)==0x80 && (s[1]&0xc0)==0x80 && (s[2]&0xc0)==0x80) {
            c = ((c&7)<<18)|((s[0]&0x3f)<<12)|((s[1]&0x3f)<<6)|(s[2]&0x3f);
            s += 3;
        } else {
            c = '?';
        }
    }
    *sp = (const char *)s;
    return c;
}

static void emoji_setup(void) {
    char path[MAX_PATH];
    if (emoji_ready) return;
    asset_path("fonts/Twemoji.Mozilla.ttf",path,MAX_PATH);
    if (FT_Init_FreeType(&emoji_ft)) return;
    if (FT_New_Face(emoji_ft,path,0,&emoji_face)) return;
    FT_Set_Pixel_Sizes(emoji_face,0,64);
    hb_blob_t *blob=hb_blob_create_from_file(path);
    hb_face_t *face=hb_face_create(blob,0);
    emoji_font=hb_font_create(face);
    emoji_ready=1;
}

static int emoji_seq_len(const char *p) {
    const char *q=p;
    uint32_t c=nextcp(&q);
    int flag = c>=0x1f1e6 && c<=0x1f1ff;
    if (!((c>=0x1f000 && c<=0x1faff) || (c>=0x2600 && c<=0x27bf) || flag || c==0xfe0f))
        return 0;
    int n=(int)(q-p);
    if (flag) {
        const char *r=q;
        uint32_t d=nextcp(&r);
        if (d>=0x1f1e6 && d<=0x1f1ff) return (int)(r-p);
        return n;
    }
    while (*q) {
        const char *save=q;
        uint32_t d=nextcp(&q);
        if (d==0xfe0f || d==0x20e3 || (d>=0x1f3fb && d<=0x1f3ff)) {
            n=(int)(q-p);
            continue;
        }
        if (d==0x200d) {
            const char *r=q;
            uint32_t e=nextcp(&r);
            if (e) { q=r; n=(int)(q-p); continue; }
        }
        q=save;
        break;
    }
    return n;
}

static void resize_area(const unsigned char *src,int sw,int x0,int y0,int cw,int ch,unsigned char *dst,int dw,int dh) {
    for (int y=0;y<dh;y++) {
        int sy0=y0+y*ch/dh,sy1=y0+(y+1)*ch/dh;
        if (sy1<=sy0) sy1=sy0+1;
        for (int x=0;x<dw;x++) {
            int sx0=x0+x*cw/dw,sx1=x0+(x+1)*cw/dw;
            if (sx1<=sx0) sx1=sx0+1;
            double sum=0;
            int cnt=0;
            for (int yy=sy0;yy<sy1;yy++) for (int xx=sx0;xx<sx1;xx++) {
                sum+=src[yy*sw+xx];
                cnt++;
            }
            dst[y*dw+x]=(unsigned char)(sum/(cnt?cnt:1)+0.5);
        }
    }
}

static Img emoji_canvas(const char *s,int len,int fsk) {
    Img im={1,ROWS,0};
    int cn_flag=len==8 && !memcmp(s,"\xf0\x9f\x87\xa8\xf0\x9f\x87\xb3",8);
    int us_flag=len==8 && !memcmp(s,"\xf0\x9f\x87\xba\xf0\x9f\x87\xb8",8);
    if (!emoji_ready) emoji_setup();
    if (!emoji_ready) return im;
    hb_buffer_t *buf=hb_buffer_create();
    hb_buffer_add_utf8(buf,s,len,0,-1);
    hb_buffer_guess_segment_properties(buf);
    hb_shape(emoji_font,buf,0,0);
    unsigned n=hb_buffer_get_length(buf);
    hb_glyph_info_t *info=hb_buffer_get_glyph_infos(buf,0);
    int W=1024,H=128,base=80,pen=4;
    unsigned char *ink=xcalloc((size_t)W*H,1);
    unsigned char *alpha=xcalloc((size_t)W*H,1);
    unsigned i;
    for (i=0;i<n;i++) {
        if (FT_Load_Glyph(emoji_face,info[i].codepoint,FT_LOAD_RENDER|FT_LOAD_COLOR)) continue;
        FT_GlyphSlot g=emoji_face->glyph;
        int x0=pen+g->bitmap_left,y0=base-g->bitmap_top;
        unsigned x,y;
        for (y=0;y<g->bitmap.rows;y++) for (x=0;x<g->bitmap.width;x++) {
            int xx=x0+(int)x,yy=y0+(int)y;
            if (xx<0||xx>=W||yy<0||yy>=H) continue;
            unsigned char v,a=255;
            if (g->bitmap.pixel_mode==FT_PIXEL_MODE_BGRA) {
                unsigned char *p=g->bitmap.buffer+y*g->bitmap.pitch+x*4;
                double lum=p[0]*0.114+p[1]*0.587+p[2]*0.299;
                a=p[3];
                v=(unsigned char)((255-lum)*a/255);
            } else {
                v=g->bitmap.buffer[y*g->bitmap.pitch+x];
                a=v;
            }
            if (v>ink[yy*W+xx]) ink[yy*W+xx]=v;
            if (a>alpha[yy*W+xx]) alpha[yy*W+xx]=a;
        }
        pen+=g->advance.x>>6;
    }
    hb_buffer_destroy(buf);
    for (int y=0;y<H;y++) for (int x=0;x<W;x++) {
        double v=ink[y*W+x];
        v=(v-128.0)*1.5+128.0;
        if (v<0) v=0;
        if (v>255) v=255;
        ink[y*W+x]=(unsigned char)(v+0.5);
        if (alpha[y*W+x]>100) {
            int edge=0;
            for (int dy=-2;dy<=2 && !edge;dy++) for (int dx=-2;dx<=2;dx++) {
                int xx=x+dx,yy=y+dy;
                if (xx<0||xx>=W||yy<0||yy>=H||alpha[yy*W+xx]<=100) { edge=1; break; }
            }
            if (edge) ink[y*W+x]=255;
        }
    }
    int x0=W,x1=-1,y0=H,y1=-1;
    for (int y=0;y<H;y++) for (int x=0;x<W;x++) if (ink[y*W+x]>8) {
        if (x<x0) x0=x;
        if (x>x1) x1=x;
        if (y<y0) y0=y;
        if (y>y1) y1=y;
    }
    if (x1>=x0) {
        int cw=x1-x0+1,ch=y1-y0+1,nh=ch>13?13:ch,nw=(int)ceil((double)cw*nh/ch);
        if (nw>18) { nw=18; nh=(int)ceil((double)ch*18/cw); if (nh>13) nh=13; }
        if (nw<1) nw=1;
        im.w=18; im.p=xcalloc((size_t)18*ROWS,1);
        unsigned char *tmp=xcalloc((size_t)nw*nh,1);
        resize_area(ink,W,x0,y0,cw,ch,tmp,nw,nh);
        if (fsk) for (int y=0;y<nh;y++) for (int x=0;x<nw;x++)
            tmp[y*nw+x]=(unsigned char)(tmp[y*nw+x]>100?255:0);
        int ox=(18-nw)/2;
        for (int y=0;y<nh;y++) for (int x=0;x<nw;x++)
            im.p[(1+y)*18+ox+x]=tmp[y*nw+x];
        if (fsk && cn_flag) {
            static const int sx[]={3,3,2,4,3,6,7,6,8};
            static const int sy[]={3,2,3,3,4,2,4,6,5};
            for (int y=1;y<=12;y++) for (int x=0;x<18;x++)
                im.p[(1+y)*18+x]=255;
            for (int i=0;i<9;i++) im.p[(1+sy[i])*18+sx[i]]=0;
        } else if (fsk && us_flag) {
            for (int y=0;y<13;y++) for (int x=0;x<18;x++)
                im.p[(1+y)*18+x]=(unsigned char)((y/2)%2==0?255:0);
            for (int y=0;y<7;y++) for (int x=0;x<9;x++)
                im.p[(1+y)*18+x]=255;
            for (int y=1;y<7;y+=2) for (int x=1;x<9;x+=2)
                im.p[(1+y)*18+x]=0;
        }
        free(tmp);
    }
    free(ink);
    free(alpha);
    return im;
}

static double *mk_taps(double alpha,int span,double sps,int *len) {
    if (!(sps>1)) sps=1440;
    if (!(alpha>=0 && alpha<=1)) alpha=0.35;
    if (span<1) span=1;
    if (span>32) span=32;
    int n = (int)round(span*sps);
    if (n<1) n=1;
    if (n>1000000) n=1000000;
    int L = 2*n+1,i;
    double *h = xmalloc(sizeof(double)*L);
    double sum = 0;
    for (i=0;i<L;i++) {
        double t = (i-n)/sps;
        if (fabs(t)<1e-9) {
            h[i] = 1-alpha+4*alpha/PI;
        } else if (alpha>0 && fabs(fabs(t)-1/(4*alpha))<1e-9) {
            h[i] = alpha/sqrt(2.0)*((1+2/PI)*sin(PI/(4*alpha))+(1-2/PI)*cos(PI/(4*alpha)));
        } else {
            double den = 1-pow(4*alpha*t,2);
            h[i] = (sin(PI*t*(1-alpha))+4*alpha*t*cos(PI*t*(1+alpha)))/(PI*t*den);
        }
        sum += h[i];
    }
    if (fabs(sum)<1e-12) sum = 1;
    for (i=0;i<L;i++) h[i] /= sum;
    *len = L;
    return h;
}

static void fft(Cpx *a,int n,int inv) {
    int i,j,len;
    for (i=1,j=0;i<n;i++) {
        int bit=n>>1;
        for (;j&bit;bit>>=1) j^=bit;
        j^=bit;
        if (i<j) { Cpx t=a[i]; a[i]=a[j]; a[j]=t; }
    }
    for (len=2;len<=n;len<<=1) {
        double ang=2*PI/len*(inv?1:-1);
        Cpx wl={cos(ang),sin(ang)};
        for (i=0;i<n;i+=len) {
            Cpx w={1,0};
            for (j=0;j<len/2;j++) {
                Cpx u=a[i+j],v;
                v.re=a[i+j+len/2].re*w.re-a[i+j+len/2].im*w.im;
                v.im=a[i+j+len/2].re*w.im+a[i+j+len/2].im*w.re;
                a[i+j].re=u.re+v.re; a[i+j].im=u.im+v.im;
                a[i+j+len/2].re=u.re-v.re; a[i+j+len/2].im=u.im-v.im;
                Cpx nw={w.re*wl.re-w.im*wl.im,w.re*wl.im+w.im*wl.re};
                w=nw;
            }
        }
    }
    if (inv) for (i=0;i<n;i++) { a[i].re/=n; a[i].im/=n; }
}

static double *fir_same(const double *x,int n,const double *h,int L) {
    int i,j,start=(L-1)/2;
    double *z = xcalloc(n,sizeof(double));
    if (L>256 && n>4096) {
        int N=1;
        Cpx *a,*b;
        while (N<n+L-1) N<<=1;
        a=xcalloc(N,sizeof(Cpx));
        b=xcalloc(N,sizeof(Cpx));
        for (i=0;i<n;i++) a[i].re=x[i];
        for (i=0;i<L;i++) b[i].re=h[i];
        fft(a,N,0);
        fft(b,N,0);
        for (i=0;i<N;i++) {
            double re=a[i].re*b[i].re-a[i].im*b[i].im;
            double im=a[i].re*b[i].im+a[i].im*b[i].re;
            a[i].re=re; a[i].im=im;
        }
        fft(a,N,1);
        for (i=0;i<n;i++) z[i]=a[i+start].re;
        free(a);
        free(b);
        return z;
    }
    double *y = xcalloc((size_t)n+L, sizeof(double));
    for (i=0;i<n;i++) {
        if (x[i]==0) continue;
        for (j=0;j<L;j++) y[i+j] += x[i]*h[j];
    }
    for (i=0;i<n;i++) z[i] = y[i+start];
    free(y);
    return z;
}

static double *pulse_shape(const double *v,int nv,double sps,double alpha,int span,int *nout) {
    int n = (int)round(nv*sps),i;
    double *imp,*h,*out;
    int L;
    if (n<1) n=1;
    imp = xcalloc(n,sizeof(double));
    h = mk_taps(alpha,span,sps,&L);
    for (i=0;i<nv;i++) {
        int k = (int)((i+0.5)*sps);
        if (k>=n) k=n-1;
        imp[k] = v[i];
    }
    out = fir_same(imp,n,h,L);
    for (i=0;i<n;i++) out[i] *= sps;
    free(imp);
    free(h);
    *nout = n;
    return out;
}

static double *modulate_psk(Img *im,Mode *m,double freq,int fs,double alpha,int span,int *nout) {
    int nv=im->w*ROWS,i,n,state=1;
    double sps=fs/(m->col*ROWS);
    double *sym=xmalloc(sizeof(double)*nv),*p,*w;
    for (int col=0;col<im->w;col++) for (int row=ROWS-1;row>=0;row--) {
        int k=col*ROWS+(ROWS-1-row);
        if (im->p[row*im->w+col]>100) state=-state;
        sym[k]=0.35*state;
    }
    p=pulse_shape(sym,nv,sps,alpha,span,&n);
    w=xmalloc(sizeof(double)*n);
    for (i=0;i<n;i++) w[i]=p[i]*cos(2*PI*freq*i/fs);
    free(sym);
    free(p);
    int fade=(int)(fs*0.005);
    if (fade*2<n) for (i=0;i<fade;i++) {
        double a=(double)i/fade;
        w[i]*=a;
        w[n-1-i]*=a;
    }
    for (i=0;i<n;i++) {
        if (w[i]>1) w[i]=1;
        if (w[i]<-1) w[i]=-1;
    }
    *nout=n;
    return w;
}

static double *modulate_cmt(Img *im,Mode *m,double freq,int fs,int *nout) {
    double sps=fs/m->col;
    int n=(int)lround(im->w*sps),i,row;
    double *w=xcalloc(n,sizeof(double));
    for (i=0;i<n;i++) {
        double cp=i/sps;
        int col=(int)cp;
        if (col>=im->w) col=im->w-1;
        for (row=0;row<ROWS;row++) {
            double a=im->p[row*im->w+col]/255.0;
            double f=freq+(row-(ROWS-1)/2.0)*17.5;
            w[i]+=0.07*a*cos(2*PI*f*i/fs);
        }
    }
    int fade=(int)(fs*0.005);
    if (fade*2<n) for (i=0;i<fade;i++) {
        double a=(double)i/fade;
        w[i]*=a;
        w[n-1-i]*=a;
    }
    for (i=0;i<n;i++) {
        if (w[i]>1) w[i]=1;
        if (w[i]<-1) w[i]=-1;
    }
    *nout=n;
    return w;
}

static double *modulate(Img *im,Mode *m,double freq,int fs,double alpha,int span,int *nout) {
    int nv = im->w*ROWS,i,col,row,n;
    double *v = xmalloc(sizeof(double)*nv);
    double *p,*w;
    double sps = fs/(m->col*ROWS);
    if (m->psk) {
        free(v);
        return modulate_psk(im,m,freq,fs,alpha,span,nout);
    }
    if (m->cmt) {
        free(v);
        return modulate_cmt(im,m,freq,fs,nout);
    }
    for (col=0;col<im->w;col++)
        for (row=ROWS-1;row>=0;row--)
            v[col*ROWS+(ROWS-1-row)] = im->p[row*im->w+col]/255.0;
    if (m->fsk) {
        double fo = freq-m->shift/2.0, ff = freq+m->shift/2.0;
        for (i=0;i<nv;i++)
            v[i]=v[i]>0.35?fo:ff;
        p = pulse_shape(v,nv,sps,alpha,span,&n);
        w = xmalloc(sizeof(double)*n);
        double ph=0;
        for (i=0;i<n;i++) {
            ph += 2*PI*p[i]/fs;
            w[i] = cos(ph);
        }
    } else {
        p = pulse_shape(v,nv,sps,alpha,span,&n);
        w = xmalloc(sizeof(double)*n);
        for (i=0;i<n;i++) w[i] = p[i]*cos(2*PI*freq*i/fs);
    }
    free(v);
    free(p);
    int fade = (int)(fs*0.005);
    if (fade*2<n) {
        for (i=0;i<fade;i++) {
            double a = (double)i/fade;
            w[i] *= a;
            w[n-1-i] *= a;
        }
    }
    for (i=0;i<n;i++) {
        if (w[i]>1) w[i]=1;
        if (w[i]<-1) w[i]=-1;
    }
    *nout = n;
    return w;
}

static void w16(FILE *f,int v) {
    unsigned char b[2] = {(unsigned char)(v&255),(unsigned char)((v>>8)&255)};
    fwrite(b,1,2,f);
}

static void w32(FILE *f,uint32_t v) {
    unsigned char b[4] = {(unsigned char)(v&255),(unsigned char)((v>>8)&255),(unsigned char)((v>>16)&255),(unsigned char)((v>>24)&255)};
    fwrite(b,1,4,f);
}

static int write_wav(const char *path,const double *x,int n,int fs) {
    FILE *f = fopen(path,"wb");
    int i;
    if (!f) return 0;
    fwrite("RIFF",1,4,f); w32(f,36+n*2); fwrite("WAVEfmt ",1,8,f);
    w32(f,16); w16(f,1); w16(f,1); w32(f,fs); w32(f,fs*2); w16(f,2); w16(f,16);
    fwrite("data",1,4,f); w32(f,n*2);
    for (i=0;i<n;i++) {
        double v = x[i]*32767;
        if (v>32767) v=32767;
        if (v<-32768) v=-32768;
        w16(f,(int)lrint(v));
    }
    fclose(f);
    return 1;
}

static uint32_t r32(const unsigned char *b) {
    return b[0]|(b[1]<<8)|(b[2]<<16)|((uint32_t)b[3]<<24);
}

static int read_wav(const char *path,double **out,int *nout,int *fsout) {
    FILE *f = fopen(path,"rb");
    unsigned char hdr[12],fmt[16];
    uint32_t datalen=0,datapos=0;
    int ch=0,bits=0,fs=0,i;
    int have_fmt=0,have_data=0;
    long file_size;
    if (!f) return 0;
    if (fseek(f,0,SEEK_END)) { fclose(f); return 0; }
    file_size=ftell(f);
    if (file_size<12 || fseek(f,0,SEEK_SET)) { fclose(f); return 0; }
    if (fread(hdr,1,12,f)!=12 || memcmp(hdr,"RIFF",4) || memcmp(hdr+8,"WAVE",4)) {
        fclose(f);
        return 0;
    }
    while (fread(hdr,1,8,f)==8) {
        uint32_t sz = r32(hdr+4);
        long pos=ftell(f);
        if (pos<0 || pos>file_size || (uint64_t)sz>(uint64_t)(file_size-pos)) break;
        if (!memcmp(hdr,"fmt ",4)) {
            if (sz<16 || fread(fmt,1,16,f)!=16) break;
            if ((fmt[0]|(fmt[1]<<8))!=1) break;
            ch = fmt[2]|(fmt[3]<<8);
            fs = fmt[4]|(fmt[5]<<8)|(fmt[6]<<16)|((uint32_t)fmt[7]<<24);
            bits = fmt[14]|(fmt[15]<<8);
            if (ch<1 || ch>8 || fs<1000 || fs>384000 || bits!=16) break;
            have_fmt=1;
            if (sz>16 && fseek(f,(long)sz-16+(sz&1),SEEK_CUR)) break;
        } else if (!memcmp(hdr,"data",4)) {
            datalen = sz;
            datapos = (uint32_t)pos;
            have_data=1;
            if (fseek(f,(long)sz+(sz&1),SEEK_CUR)) break;
        } else {
            if (fseek(f,(long)sz+(sz&1),SEEK_CUR)) break;
        }
    }
    if (!have_fmt || !have_data || !fs || bits!=16 || ch<1 || !datalen) {
        fclose(f);
        return 0;
    }
    if (fseek(f,datapos,SEEK_SET)) { fclose(f); return 0; }
    uint64_t frames=(uint64_t)datalen/(2u*(unsigned)ch);
    if (!frames || frames>MAX_WAV_FRAMES || frames>(uint64_t)INT_MAX) {
        fclose(f);
        return 0;
    }
    int n=(int)frames;
    double *x = xmalloc(sizeof(double)*n);
    for (i=0;i<n;i++) {
        int c,sum=0;
        for (c=0;c<ch;c++) {
            int lo=fgetc(f),hi=fgetc(f);
            if (lo==EOF || hi==EOF) { free(x); fclose(f); return 0; }
            int v = lo|(hi<<8);
            if (v & 0x8000) v -= 65536;
            sum += v;
        }
        x[i] = (double)sum/(ch*32768.0);
    }
    fclose(f);
    *out = x;
    *nout = n;
    *fsout = fs;
    return 1;
}

static double *shape_filter(const double *x,int n,int fs,double pr,double alpha,int span) {
    double *y;
    int L;
    double *h = mk_taps(alpha,span,fs/pr,&L);
    y = fir_same(x,n,h,L);
    free(h);
    return y;
}

typedef struct {
    double b0,b1,b2,a1,a2,z1,z2;
} Biquad;

static void biquad_lp(Biquad *q,double cut,double fs,double qv) {
    double w=2*PI*cut/fs,cs=cos(w),sn=sin(w),al=sn/(2*qv);
    double b0=(1-cs)/2,b1=1-cs,b2=b0,a0=1+al;
    q->b0=b0/a0; q->b1=b1/a0; q->b2=b2/a0;
    q->a1=-2*cs/a0; q->a2=(1-al)/a0;
    q->z1=q->z2=0;
}

static double biquad_run(Biquad *q,double x) {
    double y=q->b0*x+q->z1;
    q->z1=q->b1*x-q->a1*y+q->z2;
    q->z2=q->b2*x-q->a2*y;
    return y;
}

static Img demod_am(const double *x,int n,int fs,Mode *m,double freq,double alpha,int span,double agc_tau,double squelch,double bw) {
    double pr = m->col*ROWS,spp=fs/pr,cut=bw>0?bw/2:0.65*pr;
    double *env = xmalloc(sizeof(double)*n);
    double *filt;
    int i,start,count,cols,col,row;
    Img im;
    Biquad i1,i2,q1,q2;
    biquad_lp(&i1,cut,fs,0.5411961);
    biquad_lp(&i2,cut,fs,1.3065630);
    biquad_lp(&q1,cut,fs,0.5411961);
    biquad_lp(&q2,cut,fs,1.3065630);
    for (i=0;i<n;i++) {
        double t = 2*PI*freq*i/fs;
        double vi=biquad_run(&i1,biquad_run(&i2,x[i]*cos(t)));
        double vq=biquad_run(&q1,biquad_run(&q2,x[i]*sin(t)));
        env[i]=hypot(vi,vq);
    }
    double emax=0,xmax=0;
    for (i=0;i<n;i++) {
        if (env[i]>emax) emax=env[i];
        if (fabs(x[i])>xmax) xmax=fabs(x[i]);
    }
    if (xmax>1e-6 && emax<0.2*xmax) memset(env,0,sizeof(double)*n);
    double per=spp*(hell_rx_rate/1000.0);
    start=(int)(per*(hell_rx_phase/100.0));
    if (start<0) start=0;
    if (start>=(int)per) start=0;
    filt = shape_filter(env,n,fs,pr,alpha,span);
    count = (int)((n-start)/per);
    cols = count/ROWS;
    if (cols<1) cols=1;
    im.w=cols; im.h=ROWS; im.p=xcalloc((size_t)cols*ROWS,1);
    for (col=0;col<cols;col++) {
        for (row=0;row<ROWS;row++) {
            int k = col*ROWS+(ROWS-1-row);
            int idx = start+(int)lrint(k*per);
            double v = idx<n ? filt[idx] : 0;
            if (v<0) v=0;
            if (v>1) v=1;
            im.p[row*cols+col] = (unsigned char)lrint(v*255);
        }
    }
    double peak=0,decay=exp(-1.0/(pr*(agc_tau>0?agc_tau:1.0)));
    double *rawmax=xmalloc(sizeof(double)*cols);
    for (col=0;col<cols;col++) {
        double mx=0;
        for (row=0;row<ROWS;row++) if (im.p[row*cols+col]>mx) mx=im.p[row*cols+col];
        rawmax[col]=mx;
    }
    for (col=0;col<cols;col++) {
        double mx=0,scale;
        for (row=0;row<ROWS;row++) {
            double v = im.p[row*cols+col];
            if (v>mx) mx=v;
        }
        if (mx>peak) peak=mx;
        else peak*=decay;
        if (peak<1e-9) continue;
        scale=0.95*255.0/peak;
        for (row=0;row<ROWS;row++) {
            double v = im.p[row*cols+col]*scale;
            im.p[row*cols+col] = v>255 ? 255 : (unsigned char)lrint(v);
        }
    }
    if (squelch>0) {
        for (col=0;col<cols;col++) if (rawmax[col] < squelch*255.0)
            for (row=0;row<ROWS;row++) im.p[row*cols+col]=0;
    }
    free(rawmax);
    free(filt);
    free(env);
    return im;
}

static Img demod_fsk(const double *x,int n,int fs,Mode *m,double freq,double alpha,int span) {
    double pr=m->col*ROWS,spp=fs/pr;
    int start,count,cols,col,row,D,win,L;
    double *r=xcalloc(n,sizeof(double)),*q=xcalloc(n,sizeof(double));
    double *fr,*fi;
    double *df=xmalloc(sizeof(double)*n),*pre=xmalloc(sizeof(double)*(n+1));
    Img im;
    double *h=mk_taps(alpha,span,spp,&L);
    for (int i=0;i<n;i++) {
        double ph=2*PI*freq*i/fs;
        r[i]=x[i]*cos(ph);
        q[i]=-x[i]*sin(ph);
    }
    fr=fir_same(r,n,h,L);
    fi=fir_same(q,n,h,L);
    free(r); free(q);
    D=(int)lrint(spp/4); if (D<1) D=1;
    win=(int)lrint(spp); if (win<1) win=1;
    pre[0]=0;
    for (int i=0;i<n;i++) {
        double d=0;
        if (i>=D) {
            double cr=fr[i]*fr[i-D]+fi[i]*fi[i-D];
            double ci=fi[i]*fr[i-D]-fr[i]*fi[i-D];
            d=atan2(ci,cr)*fs/(2*PI*D);
        }
        df[i]=d;
        pre[i+1]=pre[i]+d;
    }
    double per=spp*(hell_rx_rate/1000.0);
    start=(int)(per*(hell_rx_phase/100.0));
    if (start<0) start=0;
    if (start>=(int)per) start=0;
    count=(int)((n-start)/per);
    cols=count/ROWS;
    if (cols<1) cols=1;
    double emax=0,xmax=0;
    for (int i=0;i<n;i++) {
        double e=hypot(fr[i],fi[i]);
        if (e>emax) emax=e;
        if (fabs(x[i])>xmax) xmax=fabs(x[i]);
    }
    int blank=xmax>1e-6 && emax<0.2*xmax;
    im.w=cols; im.h=ROWS; im.p=xcalloc((size_t)cols*ROWS,1);
    for (col=0;col<cols;col++) {
        for (int qq=0;qq<ROWS;qq++) {
            int q=col*ROWS+qq;
            row=ROWS-1-qq;
            int idx=start+(int)lrint(q*per);
            int a=idx-win/2,b=idx+win/2;
            double avg=0,soft,amp;
            if (idx<0) idx=0;
            if (idx>=n) idx=n-1;
            if (a<0) a=0;
            if (b>n) b=n;
            if (b>a) avg=(pre[b]-pre[a])/(b-a);
            soft=-avg/(m->shift/2.0);
            amp=fr[idx]*fr[idx]+fi[idx]*fi[idx];
            int enc=!blank && amp>1e-8 && soft>0.15;
            im.p[row*cols+col]=(unsigned char)(enc?255:0);
        }
    }
    free(fr); free(fi); free(df); free(pre); free(h);
    return im;
}

static Img demod_psk(const double *x,int n,int fs,Mode *m,double freq,double alpha,int span) {
    double sps=fs/(m->col*ROWS);
    int L,start=(int)(sps/2),cols;
    double *h=mk_taps(alpha,span,sps,&L);
    double *r=xmalloc(sizeof(double)*n),*q=xmalloc(sizeof(double)*n),*fr,*fi;
    Img im;
    for (int i=0;i<n;i++) {
        double ph=2*PI*freq*i/fs;
        r[i]=x[i]*cos(ph);
        q[i]=-x[i]*sin(ph);
    }
    fr=fir_same(r,n,h,L);
    fi=fir_same(q,n,h,L);
    free(r); free(q);
    cols=(int)((n-start)/sps)/ROWS;
    if (cols<1) cols=1;
    im.w=cols; im.h=ROWS; im.p=xcalloc((size_t)cols*ROWS,1);
    int ds=(int)lrint(sps);
    for (int col=0;col<cols;col++) for (int row=0;row<ROWS;row++) {
        int k=col*ROWS+(ROWS-1-row);
        int i1=start+(int)lrint(k*sps),i0=i1-ds;
        int bit=0;
        if (i0>=0 && i1<n) {
            double cr=fr[i1]*fr[i0]+fi[i1]*fi[i0];
            double ci=fi[i1]*fr[i0]-fr[i1]*fi[i0];
            bit=fabs(atan2(ci,cr))>PI/2;
        }
        im.p[row*cols+col]=(unsigned char)(bit?255:0);
    }
    free(fr); free(fi); free(h);
    return im;
}

static Img demod_cmt(const double *x,int n,int fs,Mode *m,double freq) {
    double sps=fs/m->col;
    int cols=(int)(n/sps),win=(int)lrint(sps);
    Img im;
    double *colmax;
    double gmax=0;
    if (cols<1) cols=1;
    if (win<8) win=8;
    im.w=cols; im.h=ROWS; im.p=xcalloc((size_t)cols*ROWS,1);
    colmax=xmalloc(sizeof(double)*cols);
    for (int col=0;col<cols;col++) {
        double mag[ROWS],mx=0;
        int center=(int)lrint((col+0.5)*sps);
        for (int row=0;row<ROWS;row++) {
            double f=freq+(row-(ROWS-1)/2.0)*17.5;
            double si=0,sq=0;
            int a=center-win/2,b=a+win;
            if (a<0) a=0;
            if (b>n) b=n;
            for (int j=a;j<b;j++) {
                si+=x[j]*cos(2*PI*f*j/fs);
                sq+=x[j]*sin(2*PI*f*j/fs);
            }
            mag[row]=hypot(si,sq);
            if (mag[row]>mx) mx=mag[row];
        }
        colmax[col]=mx;
        if (mx>gmax) gmax=mx;
        for (int row=0;row<ROWS;row++) {
            int v=mx>1e-9 && mag[row]>=0.5*mx ? 255 : 0;
            im.p[row*cols+col]=(unsigned char)v;
        }
    }
    if (gmax>1e-9) {
        for (int col=0;col<cols;col++) if (colmax[col]<0.18*gmax)
            for (int row=0;row<ROWS;row++) im.p[row*cols+col]=0;
    }
    free(colmax);
    return im;
}

static Img demod_mode(const double *x,int n,int fs,Mode *m,double freq,double alpha,int span,double agc,double sql,double bw) {
    if (m->fsk) return demod_fsk(x,n,fs,m,freq,alpha,span);
    if (m->psk) return demod_psk(x,n,fs,m,freq,alpha,span);
    if (m->cmt) return demod_cmt(x,n,fs,m,freq);
    return demod_am(x,n,fs,m,freq,alpha,span,agc,sql,bw);
}

static int save_pgm(const char *path,Img *im,int scale,int dual,int blackboard) {
    FILE *f = fopen(path,"wb");
    int x,y,r,hh=im->h*(dual?2:1);
    if (!f) return 0;
    if (scale<1) scale=1;
    fprintf(f,"P5\n%d %d\n255\n",im->w*scale,hh*scale);
    for (y=0;y<hh;y++) {
        int sy = y%im->h;
        for (r=0;r<scale;r++) {
            for (x=0;x<im->w;x++) {
                int k;
                unsigned char v=im->p[sy*im->w+x];
                if (!blackboard) v = 255-v;
                for (k=0;k<scale;k++) fputc(v,f);
            }
        }
    }
    fclose(f);
    return 1;
}

static char *read_file(const char *path) {
    FILE *f=fopen(path,"rb");
    long n;
    char *s;
    if (!f) return 0;
    if (fseek(f,0,SEEK_END)) { fclose(f); return 0; }
    n=ftell(f);
    if (n<0 || (unsigned long)n>MAX_TEXT_BYTES || fseek(f,0,SEEK_SET)) {
        fclose(f);
        return 0;
    }
    s=xmalloc((size_t)n+1);
    if (fread(s,1,(size_t)n,f)!=(size_t)n) { fclose(f); free(s); return 0; }
    s[n]=0; fclose(f);
    if (n>=3 && (unsigned char)s[0]==0xef &&
        (unsigned char)s[1]==0xbb && (unsigned char)s[2]==0xbf)
        memmove(s,s+3,strlen(s+3)+1);
    return s;
}

static const char *default_font(void) {
    static char a[MAX_PATH];
    asset_path("fonts/wenquanyi_13px.pcf",a,MAX_PATH);
    return a;
}

static hb_bool_t hb_nom_glyph(hb_font_t *font,void *font_data,hb_codepoint_t u,hb_codepoint_t *g,void *user_data) {
    FT_Face face=(FT_Face)font_data;
    (void)font; (void)user_data;
    *g=FT_Get_Char_Index(face,u);
    return *g!=0;
}

static hb_position_t hb_h_advance(hb_font_t *font,void *font_data,hb_codepoint_t gid,void *user_data) {
    FT_Face face=(FT_Face)font_data;
    (void)font; (void)user_data;
    if (FT_Load_Glyph(face,gid,FT_LOAD_DEFAULT)) return 0;
    return face->glyph->advance.x;
}

static int is_arabic_cp(uint32_t c) {
    return (c>=0x0600 && c<=0x06ff) || (c>=0x0750 && c<=0x077f) ||
           (c>=0x08a0 && c<=0x08ff) || (c>=0xfb50 && c<=0xfdff) ||
           (c>=0xfe70 && c<=0xfeff) || (c>=0x1ee00 && c<=0x1eeff);
}

static int is_neutral_cp(uint32_t c) {
    if (c==0x20 || c==0x09 || c==0x0a || c==0x0d || c==0xa0) return 1;
    if (c>=0x30 && c<=0x39) return 1;
    if (c>=0x21 && c<=0x2f) return 1;
    if (c>=0x3a && c<=0x40) return 1;
    if (c>=0x5b && c<=0x60) return 1;
    if (c>=0x7b && c<=0x7e) return 1;
    if (c==0x060c || c==0x061b || c==0x061f || c==0x06d4) return 1;
    if (c>=0x066a && c<=0x066d) return 1;
    if (c>=0x2000 && c<=0x206f) return 1;
    return 0;
}

static const char *arabic_primary_path(void) {
    static char p[MAX_PATH];
    asset_path("fonts/Unifont-Arabic.bdf",p,MAX_PATH);
    return p;
}

static Img hb_text_canvas(const char *fontpath,const char *text) {
    Img im={1,ROWS,0};
    FT_Library ft;
    FT_Face face;
    hb_blob_t *blob;
    hb_face_t *hface;
    hb_font_t *font;
    hb_buffer_t *buf;
    int bdf = strstr(fontpath,".bdf") || strstr(fontpath,".pcf");
    int px = bdf ? 14 : (strstr(fontpath,"FusionPixel") ? 12 : 64);
    if (FT_Init_FreeType(&ft)) return im;
    if (FT_New_Face(ft,fontpath,0,&face)) { FT_Done_FreeType(ft); return im; }
    FT_Set_Pixel_Sizes(face,0,px);
    blob=hb_blob_create_from_file(fontpath);
    hface=hb_face_create(blob,0);
    font=hb_font_create(hface);
    hb_font_funcs_t *funcs=0;
    if (bdf) {
        funcs=hb_font_funcs_create();
        hb_font_funcs_set_nominal_glyph_func(funcs,hb_nom_glyph,face,0);
        hb_font_funcs_set_glyph_h_advance_func(funcs,hb_h_advance,face,0);
        hb_font_set_funcs(font,funcs,face,0);
    }
    buf=hb_buffer_create();
    char *txt=xmalloc(strlen(text)+1);
    strcpy(txt,text);
    for (char *p=txt;*p;p++) if (*p=='\n' || *p=='\r') *p=' ';
    hb_buffer_add_utf8(buf,txt,-1,0,-1);
    hb_buffer_guess_segment_properties(buf);
    hb_shape(font,buf,0,0);
    unsigned n=hb_buffer_get_length(buf);
    hb_glyph_info_t *info=hb_buffer_get_glyph_infos(buf,0);
    hb_glyph_position_t *pos=hb_buffer_get_glyph_positions(buf,0);
    int cap=4096,H=256,asc=face->size->metrics.ascender>>6,base=asc+16;
    unsigned char *c=xcalloc((size_t)cap*H,1);
    int minx=cap,miny=H,maxx=-1,maxy=-1;
    double scale=bdf ? 1.0 : (double)px/face->units_per_EM;
    int pen=8;
    for (unsigned i=0;i<n;i++) {
        if (FT_Load_Glyph(face,info[i].codepoint,FT_LOAD_RENDER|FT_LOAD_COLOR)) continue;
        FT_GlyphSlot g=face->glyph;
        int gx=pen+(int)lround(pos[i].x_offset*scale)+g->bitmap_left;
        int gy=base-(int)lround(pos[i].y_offset*scale)-g->bitmap_top;
        int right=gx+(int)g->bitmap.width;
        if (right>cap) {
            int newcap=cap;
            while (newcap<right) {
                if (newcap>INT_MAX/2) { newcap=right; break; }
                newcap*=2;
            }
            if ((size_t)newcap*H>MAX_CANVAS_PIXELS) break;
            c=xrealloc(c,(size_t)newcap*H);
            memset(c+(size_t)cap*H,0,(size_t)(newcap-cap)*H);
            cap=newcap;
        }
        for (unsigned y=0;y<g->bitmap.rows;y++) for (unsigned x=0;x<g->bitmap.width;x++) {
            int xx=gx+(int)x,yy=gy+(int)y;
            if (xx<0||xx>=cap||yy<0||yy>=H) continue;
            unsigned char v;
            if (g->bitmap.pixel_mode==FT_PIXEL_MODE_BGRA) {
                unsigned char *p=g->bitmap.buffer+y*g->bitmap.pitch+x*4;
                double lum=p[0]*0.114+p[1]*0.587+p[2]*0.299;
                v=(unsigned char)((255-lum)*p[3]/255);
            } else if (g->bitmap.pixel_mode==FT_PIXEL_MODE_MONO) {
                unsigned char byte=g->bitmap.buffer[y*g->bitmap.pitch+x/8];
                v=(byte & (0x80>>(x&7))) ? 255 : 0;
            } else {
                v=g->bitmap.buffer[y*g->bitmap.pitch+x];
            }
            if (v>8) {
                if (xx<minx) minx=xx;
                if (xx>maxx) maxx=xx;
                if (yy<miny) miny=yy;
                if (yy>maxy) maxy=yy;
            }
            if (v>c[yy*cap+xx]) c[yy*cap+xx]=v;
        }
        pen+=bdf ? (pos[i].x_advance>>6) : (int)lround(pos[i].x_advance*scale);
    }
    hb_buffer_destroy(buf);
    if (funcs) hb_font_funcs_destroy(funcs);
    hb_font_destroy(font);
    hb_face_destroy(hface);
    hb_blob_destroy(blob);
    FT_Done_Face(face);
    FT_Done_FreeType(ft);
    free(txt);
    if (maxx>=minx) {
        int cw=maxx-minx+1,ch=maxy-miny+1,nh=bdf?ch:(ch>13?13:ch);
        int nw=(int)ceil((double)cw*nh/ch);
        if (nh>ROWS) nh=ROWS;
        if (nw<1) nw=1;
        int top=1+(miny-(base-13));
        if (top<1) top=1;
        if (top+nh>ROWS) top=ROWS-nh;
        im.w=nw; im.p=xcalloc((size_t)nw*ROWS,1);
        unsigned char *tmp=xcalloc((size_t)nw*nh,1);
        resize_area(c,cap,minx,miny,cw,ch,tmp,nw,nh);
        for (int y=0;y<nh;y++) for (int x=0;x<nw;x++)
            im.p[(top+y)*nw+x]=tmp[y*nw+x];
        free(tmp);
    }
    free(c);
    return im;
}

static int utf8_floor(const char *s,int off) {
    while (off>0 && ((unsigned char)s[off]&0xc0)==0x80) off--;
    return off;
}

typedef struct {
    int start,end,emoji,rtl;
} Seg;

static Img render_hb_map(const char *fontpath,const char *text,int fsk,int **mapout) {
    Seg *segs=0;
    int nseg=0,cap=0,pos=0;
    while (text[pos]) {
        int el=emoji_seq_len(text+pos);
        if (el) {
            if (nseg==cap) {
                cap=cap?cap*2:8;
                segs=xrealloc(segs,sizeof(Seg)*cap);
            }
            segs[nseg++]=(Seg){pos,pos+el,1,0};
            pos+=el;
            continue;
        }
        int start=pos,rtl=-1;
        while (text[pos]) {
            if (emoji_seq_len(text+pos)) break;
            const char *q=text+pos;
            uint32_t c=nextcp(&q);
            if (q==text+pos) break;
            int ar=is_arabic_cp(c);
            int strong=ar || !is_neutral_cp(c);
            if (strong) {
                int d=ar?1:0;
                if (rtl<0) rtl=d;
                else if (rtl!=d) break;
            }
            pos=(int)(q-text);
        }
        if (pos==start) { pos++; continue; }
        if (nseg==cap) {
            cap=cap?cap*2:8;
            segs=xrealloc(segs,sizeof(Seg)*cap);
        }
        segs[nseg++]=(Seg){start,pos,0,rtl>0};
    }
    int base_rtl=0;
    for (int k=0;k<nseg;k++) if (!segs[k].emoji) { base_rtl=segs[k].rtl; break; }
    int manual_ar=strstr(fontpath,"Arabic")||strstr(fontpath,"Unifont");
    unsigned char *out=0;
    int W=0,*map=0;
    for (int k=0;k<nseg;k++) {
        Seg *sg=&segs[base_rtl?nseg-1-k:k];
        Img seg;
        if (sg->emoji) {
            seg=emoji_canvas(text+sg->start,sg->end-sg->start,fsk);
        } else {
            int len=sg->end-sg->start;
            char *run=xmalloc(len+1);
            memcpy(run,text+sg->start,len);
            run[len]=0;
            const char *fp=fontpath;
            if (sg->rtl) {
                if (!manual_ar) {
                    fp=arabic_primary_path();
                }
            } else if (manual_ar) {
                fp=default_font();
            }
            seg=hb_text_canvas(fp,run);
            free(run);
        }
        if (seg.p && seg.w>0) {
            int so=sg->start,eo=sg->end;
            int gap=W?1:0;
            int nw=W+gap+seg.w;
            unsigned char *q=xcalloc((size_t)nw*ROWS,1);
            if (out) for (int y=0;y<ROWS;y++)
                memcpy(q+y*nw,out+y*W,W);
            for (int y=0;y<ROWS;y++)
                memcpy(q+y*nw+W+gap,seg.p+y*seg.w,seg.w);
            if (mapout) {
                int *nm=xmalloc(sizeof(int)*nw);
                if (W) memcpy(nm,map,sizeof(int)*W);
                if (gap) nm[W]=so;
                for (int x=0;x<seg.w;x++) {
                    int off=so+(int)((long long)(x+1)*(eo-so)/seg.w);
                    nm[W+gap+x]=utf8_floor(text,off);
                }
                free(map);
                map=nm;
            }
            free(out);
            out=q;
            W=nw;
        }
        free(seg.p);
    }
    free(segs);
    int pad=W?3:0,wout=W?W+pad:1;
    Img im={wout,ROWS,xcalloc((size_t)wout*ROWS,1)};
    if (out) for (int y=0;y<ROWS;y++)
        memcpy(im.p+y*wout,out+y*W,W);
    free(out);
    if (mapout) {
        int *fm=xmalloc(sizeof(int)*wout);
        if (W) memcpy(fm,map,sizeof(int)*W);
        for (int x=W;x<wout;x++) fm[x]=(int)strlen(text);
        free(map);
        *mapout=fm;
    }
    return im;
}

static Img render_hb(const char *fontpath,const char *text,int fsk) {
    return render_hb_map(fontpath,text,fsk,0);
}

#ifdef _WIN32
static void play_wave(const double *x,int n,int fs,int dev) {
    WAVEFORMATEX wf={WAVE_FORMAT_PCM,1,fs,fs*2,2,16,0};
    WAVEHDR hdr;
    HWAVEOUT h;
    UINT id = dev<0 ? WAVE_MAPPER : (UINT)dev;
    short *buf = xmalloc(sizeof(short)*n);
    int i;
    for (i=0;i<n;i++) {
        double v=x[i]*32767;
        if (v>32767) v=32767;
        if (v<-32768) v=-32768;
        buf[i]=(short)lrint(v);
    }
    memset(&hdr,0,sizeof(hdr));
    hdr.lpData=(LPSTR)buf;
    hdr.dwBufferLength=n*2;
    if (waveOutOpen(&h,id,&wf,0,0,CALLBACK_NULL)) {
        fprintf(stderr,"waveOutOpen failed\n");
        free(buf);
        return;
    }
    waveOutPrepareHeader(h,&hdr,sizeof(hdr));
    waveOutWrite(h,&hdr,sizeof(hdr));
    while (!(hdr.dwFlags & WHDR_DONE)) Sleep(10);
    waveOutUnprepareHeader(h,&hdr,sizeof(hdr));
    waveOutClose(h);
    free(buf);
}

static void print_ascii(Img *im,int width) {
    const char *ch=" .:-=+*#%@";
    int step=im->w/width;
    int x,y;
    if (step<1) step=1;
    for (y=0;y<im->h;y++) {
        for (x=0;x<im->w;x+=step) {
            int v=im->p[y*im->w+x]*(int)(strlen(ch)-1)/255;
            putchar(ch[v]);
        }
        putchar('\n');
    }
}

static void live_rx(Mode *m,double freq,double alpha,int span,int seconds,int dev,const char *save,int scale,int blackboard) {
    WAVEFORMATEX wf={WAVE_FORMAT_PCM,1,FS,FS*2,2,16,0};
    HWAVEIN h;
    UINT id = dev<0 ? WAVE_MAPPER : (UINT)dev;
    short *buf=xmalloc(sizeof(short)*8192);
    double *dbuf=0;
    int dlen=0,i;
    DWORD t0;
    if (waveInOpen(&h,id,&wf,0,0,CALLBACK_NULL)) {
        fprintf(stderr,"waveInOpen failed\n");
        free(buf);
        return;
    }
    waveInStart(h);
    t0=GetTickCount();
    while (seconds<=0 || (int)((GetTickCount()-t0)/1000)<seconds) {
        WAVEHDR hdr;
        memset(&hdr,0,sizeof(hdr));
        hdr.lpData=(LPSTR)buf;
        hdr.dwBufferLength=8192*2;
        if (waveInPrepareHeader(h,&hdr,sizeof(hdr))) break;
        if (waveInAddBuffer(h,&hdr,sizeof(hdr))) break;
        while (!(hdr.dwFlags & WHDR_DONE)) Sleep(10);
        waveInUnprepareHeader(h,&hdr,sizeof(hdr));
        dbuf=xrealloc(dbuf,sizeof(double)*(dlen+8192));
        for (i=0;i<8192;i++) dbuf[dlen+i]=buf[i]/32768.0;
        dlen+=8192;
        if (dlen>FS*20) {
            memmove(dbuf,dbuf+dlen-FS*10,sizeof(double)*FS*10);
            dlen=FS*10;
        }
        Img im=demod_mode(dbuf,dlen,FS,m,freq,alpha,span,1.0,0,0);
        if (im.w>0) {
            printf("\r\n%s cols %d\n",m->name,im.w);
            print_ascii(&im,100);
            save_pgm(save,&im,scale,1,blackboard);
        }
    }
    waveInStop(h);
    waveInClose(h);
    free(buf);
    free(dbuf);
}

static const wchar_t *ttf_family(const char *path) {
    static wchar_t buf[128];
    if (strstr(path,"FusionPixel")) return L"Fusion Pixel 12px Mono zh-Hant";
    if (strstr(path,"NotoSansJP")) return L"Noto Sans JP";
    if (strstr(path,"Twemoji")) return L"Twemoji Mozilla";
    const char *p=strrchr(path,'/');
    if (!p) p=strrchr(path,'\\');
    p = p ? p+1 : path;
    buf[0]=0;
    if (MultiByteToWideChar(CP_UTF8,0,p,-1,buf,128)<=0) return L"";
    buf[127]=0;
    return buf;
}

static Img render_ttf(const char *fontpath,const char *text) {
    Img im={1,ROWS,0};
    wchar_t *wp=xmalloc(sizeof(wchar_t)*(strlen(fontpath)+1));
    wchar_t *wt=xmalloc(sizeof(wchar_t)*(strlen(text)*2+2));
    wp[0]=0;
    wt[0]=0;
    IDWriteFactory *dw=0;
    IDWriteTextFormat *fmt=0;
    IDWriteTextLayout *lay=0;
    ID2D1Factory *d2f=0;
    ID2D1DCRenderTarget *rt=0;
    ID2D1RenderTarget *r2=0;
    ID2D1SolidColorBrush *br=0;
    DWRITE_TEXT_METRICS tm;
    HDC dc=CreateCompatibleDC(0);
    HBITMAP bmp=0;
    HGDIOBJ oldbmp=0;
    BITMAPINFO bi;
    void *bits=0;
    int W,H,x,y;
    if (!dc) goto fail;
    MultiByteToWideChar(CP_UTF8,0,fontpath,-1,wp,(int)strlen(fontpath)+1);
    MultiByteToWideChar(CP_UTF8,0,text,-1,wt,(int)(strlen(text)*2+2));
    AddFontResourceExW(wp,FR_PRIVATE,0);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,&IID_IDWriteFactory,(IUnknown**)&dw);
    if (!dw) goto fail;
    int px = strstr(fontpath,"FusionPixel") ? 12 : 14;
    if (dw->lpVtbl->CreateTextFormat(dw,ttf_family(fontpath),0,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,px,L"",&fmt)) goto fail;
    if (dw->lpVtbl->CreateTextLayout(dw,wt,(UINT32)wcslen(wt),fmt,4096,64,&lay)) goto fail;
    lay->lpVtbl->GetMetrics(lay,&tm);
    W=(int)ceil(tm.widthIncludingTrailingWhitespace)+4;
    H=64;
    if (W<4) W=4;
    if (W>1000000) goto fail;
    memset(&bi,0,sizeof(bi));
    bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth=W;
    bi.bmiHeader.biHeight=-H;
    bi.bmiHeader.biPlanes=1;
    bi.bmiHeader.biBitCount=32;
    bi.bmiHeader.biCompression=BI_RGB;
    bmp=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,0,0);
    if (!bmp || !bits) goto fail;
    oldbmp=SelectObject(dc,bmp);
    if (D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&IID_ID2D1Factory,0,(void**)&d2f)) goto fail;
    if (!d2f) goto fail;
    D2D1_RENDER_TARGET_PROPERTIES rp;
    memset(&rp,0,sizeof(rp));
    rp.type=D2D1_RENDER_TARGET_TYPE_DEFAULT;
    rp.pixelFormat.format=DXGI_FORMAT_B8G8R8A8_UNORM;
    rp.pixelFormat.alphaMode=D2D1_ALPHA_MODE_IGNORE;
    rp.usage=D2D1_RENDER_TARGET_USAGE_NONE;
    rp.minLevel=D2D1_FEATURE_LEVEL_DEFAULT;
    if (d2f->lpVtbl->CreateDCRenderTarget(d2f,&rp,&rt)) goto fail;
    if (!rt) goto fail;
    RECT rc={0,0,W,H};
    rt->lpVtbl->BindDC(rt,dc,&rc);
    r2=(ID2D1RenderTarget*)rt;
    r2->lpVtbl->BeginDraw(r2);
    D2D1_COLOR_F white={1,1,1,1},black={0,0,0,1};
    r2->lpVtbl->Clear(r2,&white);
    if (r2->lpVtbl->CreateSolidColorBrush(r2,&black,0,&br)) goto fail;
    if (!br) goto fail;
    D2D1_POINT_2F pt={0,0};
    r2->lpVtbl->DrawTextLayout(r2,pt,lay,(ID2D1Brush*)br,D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
    if (r2->lpVtbl->EndDraw(r2,0,0)) goto fail;
    unsigned char *g=xcalloc((size_t)W*H,1);
    unsigned char *b=(unsigned char*)bits;
    int x0=W,x1=0,y0=H,y1=0;
    for (y=0;y<H;y++) for (x=0;x<W;x++) {
        unsigned char *q=b+(y*W+x)*4;
        int v=255-(int)(q[0]*0.114+q[1]*0.587+q[2]*0.299);
        g[y*W+x]=v;
        if (v>8) { if(x<x0)x0=x; if(x>x1)x1=x; if(y<y0)y0=y; if(y>y1)y1=y; }
    }
    if (x1>=x0 && y1>=y0) {
        int cw=x1-x0+1,ch=y1-y0+1,nh=ch>ROWS-1?ROWS-1:ch,nw=(int)ceil((double)cw*nh/ch);
        im.w=nw<1?1:nw; im.h=ROWS; im.p=xcalloc((size_t)im.w*ROWS,1);
        for (y=0;y<nh;y++) for (x=0;x<im.w;x++) {
            int sy=y0+y*ch/nh, sx=x0+x*cw/im.w;
            im.p[(ROWS-nh+y)*im.w+x]=g[sy*W+sx];
        }
    }
    free(g);
fail:
    if (br) ((IUnknown*)br)->lpVtbl->Release((IUnknown*)br);
    if (rt) ((IUnknown*)rt)->lpVtbl->Release((IUnknown*)rt);
    if (d2f) ((IUnknown*)d2f)->lpVtbl->Release((IUnknown*)d2f);
    if (lay) ((IUnknown*)lay)->lpVtbl->Release((IUnknown*)lay);
    if (fmt) ((IUnknown*)fmt)->lpVtbl->Release((IUnknown*)fmt);
    if (dw) ((IUnknown*)dw)->lpVtbl->Release((IUnknown*)dw);
    if (dc && oldbmp) SelectObject(dc,oldbmp);
    if (bmp) DeleteObject(bmp);
    if (dc) DeleteDC(dc);
    RemoveFontResourceExW(wp,FR_PRIVATE,0);
    free(wp); free(wt);
    return im;
}
#endif

static int cli_select_language(const char *code) {
    if (!code || !*code) return 0;
    if (!strncmp(code,"zh",2) || !strcmp(code,"chinese")) {
        cli_lang=1;
        return 1;
    }
    if (!strncmp(code,"en",2) || !strcmp(code,"english")) {
        cli_lang=0;
        return 1;
    }
    return 0;
}

static int parse_int_arg(const char *s,int lo,int hi,int *out) {
    char *end;
    long value;
    if (!s || !*s) return 0;
    errno=0;
    value=strtol(s,&end,10);
    if (errno==ERANGE || end==s || *end || value<lo || value>hi) return 0;
    *out=(int)value;
    return 1;
}

static int parse_double_arg(const char *s,double lo,double hi,double *out) {
    char *end;
    double value;
    if (!s || !*s) return 0;
    errno=0;
    value=strtod(s,&end);
    if (errno==ERANGE || end==s || *end || !isfinite(value) ||
        value<lo || value>hi) return 0;
    *out=value;
    return 1;
}

static void cli_default_language(void) {
    const char *env=getenv("UNIHELL_LANG");
    cli_lang=0;
    if (env && cli_select_language(env)) {
        cli_lang_ready=1;
        return;
    }
#ifdef _WIN32
    if (PRIMARYLANGID(GetUserDefaultUILanguage())==LANG_CHINESE) cli_lang=1;
#else
    env=getenv("LANG");
    if (env && !strncmp(env,"zh",2)) cli_lang=1;
#endif
    cli_lang_ready=1;
}

static int cli_prepare_args(int *argc,char ***argv) {
    char **out=xcalloc((size_t)*argc+1,sizeof(char *));
    int n=0;
    for (int i=0;i<*argc;i++) {
        if (!strcmp((*argv)[i],"--lang")) {
            if (i+1>=*argc || !cli_select_language((*argv)[i+1])) {
                fprintf(stderr,"invalid --lang (use en or zh)\n");
                return 0;
            }
            i++;
        } else if (!strncmp((*argv)[i],"--lang=",7)) {
            if (!cli_select_language((*argv)[i]+7)) {
                fprintf(stderr,"invalid --lang (use en or zh)\n");
                return 0;
            }
        } else {
            out[n++]=(*argv)[i];
        }
    }
    out[n]=0;
    *argc=n;
    *argv=out;
    return 1;
}

static int run(int argc,char **argv) {
    if (!cli_lang_ready) cli_default_language();
    if (!cli_prepare_args(&argc,&argv)) return 1;
    const char *cmd = argc>1 ? argv[1] : "";
    if (!strcmp(cmd,"--version") || !strcmp(cmd,"-v") || !strcmp(cmd,"version")) {
        printf("UniHELL CLI %s\n",UNIHELL_VERSION);
        return 0;
    }
    if (!strcmp(cmd,"--help") || !strcmp(cmd,"-h") || !strcmp(cmd,"help")) {
        fputs(CT(CLI_S_USAGE),stdout);
        return 0;
    }
    if (!strcmp(cmd,"list")) {
        int i;
        for (i=0;i<(int)(sizeof(modes)/sizeof(modes[0]));i++) {
            if (modes[i].cmt) {
                printf(CT(CLI_S_LIST_CMT),modes[i].name,modes[i].col,mode_bw(&modes[i],0.35));
                continue;
            }
            const char *t=modes[i].fsk?"fsk":modes[i].psk?"psk":"am";
            double baud=modes[i].col*ROWS;
            printf(CT(CLI_S_LIST_MODE),modes[i].name,modes[i].col,t,baud,mode_bw(&modes[i],0.35));
        }
        return 0;
    }
    if (!strcmp(cmd,"tx")) {
        const char *text=0,*textfile=0,*out="output.wav",*fontpath=default_font(),*src=0;
        const char *mode="feld";
        double freq=1500,alpha=0.35;
        int span=4,i,live=0,dev=-1;
        Img im;
        Mode *m;
        double *w;
        int n;
        for (i=2;i<argc;i++) {
            if (!strcmp(argv[i],"-m")) {
                if (i+1>=argc) { fprintf(stderr,"-m needs a mode\n"); return 1; }
                mode=argv[++i];
            } else if (!strcmp(argv[i],"-o")) {
                if (i+1>=argc) { fprintf(stderr,"-o needs a path\n"); return 1; }
                out=argv[++i];
            } else if (!strcmp(argv[i],"-f")) {
                if (i+1>=argc || !parse_double_arg(argv[++i],0,3000,&freq)) {
                    fprintf(stderr,"invalid -f\n"); return 1;
                }
            } else if (!strcmp(argv[i],"-F")) {
                if (i+1>=argc) { fprintf(stderr,"-F needs a font path\n"); return 1; }
                fontpath=argv[++i];
            } else if (!strcmp(argv[i],"--font")) {
                if (i+1>=argc) { fprintf(stderr,"--font needs a font path\n"); return 1; }
                fontpath=argv[++i];
            } else if (!strcmp(argv[i],"--alpha")) {
                if (i+1>=argc || !parse_double_arg(argv[++i],0,1,&alpha)) {
                    fprintf(stderr,"invalid --alpha\n"); return 1;
                }
            } else if (!strcmp(argv[i],"--span")) {
                if (i+1>=argc || !parse_int_arg(argv[++i],1,16,&span)) {
                    fprintf(stderr,"invalid --span\n"); return 1;
                }
            } else if (!strcmp(argv[i],"--text-file")) {
                if (i+1>=argc) { fprintf(stderr,"--text-file needs a path\n"); return 1; }
                textfile=argv[++i];
            } else if (!strcmp(argv[i],"--source")) {
                if (i+1>=argc) { fprintf(stderr,"--source needs a path\n"); return 1; }
                src=argv[++i];
            }
            else if (!strcmp(argv[i],"--live")) live=1;
            else if (!strcmp(argv[i],"-d") || !strcmp(argv[i],"--device")) {
                if (i+1>=argc || !parse_int_arg(argv[++i],-1,100000,&dev)) {
                    fprintf(stderr,"invalid device\n"); return 1;
                }
            }
            else if (argv[i][0]!='-') {
                if (!text) text=argv[i];
                else {
                    size_t l=strlen(text)+strlen(argv[i])+2;
                    char *q=xmalloc(l);
                    snprintf(q,l,"%s %s",text,argv[i]);
                    text=q;
                }
            } else {
                fprintf(stderr,"unknown option: %s\n",argv[i]);
                return 1;
            }
        }
        if (textfile) text=read_file(textfile);
        if (!text) { fputs(CT(CLI_S_TX_NEEDS),stderr); return 1; }
        m = mode_of(mode);
        if (!m) { fprintf(stderr,CT(CLI_S_UNKNOWN_MODE),mode); return 1; }
        im = render_hb(fontpath,text,m->fsk||m->psk);
#ifdef _WIN32
        if (!im.p) im = render_ttf(fontpath,text);
#endif
        if (!im.p) { fprintf(stderr,CT(CLI_S_RENDER_FONT),fontpath); return 1; }
        if (src) save_pgm(src,&im,4,0,0);
        w = modulate(&im,m,freq,FS,alpha,span,&n);
        if (!write_wav(out,w,n,FS)) { fprintf(stderr,CT(CLI_S_WRITE_FAIL),out); return 1; }
        printf(CT(CLI_S_TX_DONE),m->name,fontpath,im.w,(double)n/FS,out);
#ifdef _WIN32
        if (live) play_wave(w,n,FS,dev);
#endif
        return 0;
    }
    if (!strcmp(cmd,"rx")) {
        const char *in=0,*out="rx.pgm",*mode="feld";
        double freq=1500,alpha=0.35;
        int span=4,scale=1,dual=1,blackboard=0,i,live=0,seconds=10,dev=-1;
        Mode *m;
        double *x;
        int n,fs;
        Img im;
        for (i=2;i<argc;i++) {
            if (!strcmp(argv[i],"-m")) {
                if (i+1>=argc) { fprintf(stderr,"-m needs a mode\n"); return 1; }
                mode=argv[++i];
            } else if (!strcmp(argv[i],"-o")) {
                if (i+1>=argc) { fprintf(stderr,"-o needs a path\n"); return 1; }
                out=argv[++i];
            } else if (!strcmp(argv[i],"-f")) {
                if (i+1>=argc || !parse_double_arg(argv[++i],0,3000,&freq)) {
                    fprintf(stderr,"invalid -f\n"); return 1;
                }
            } else if (!strcmp(argv[i],"--alpha")) {
                if (i+1>=argc || !parse_double_arg(argv[++i],0,1,&alpha)) {
                    fprintf(stderr,"invalid --alpha\n"); return 1;
                }
            } else if (!strcmp(argv[i],"--span")) {
                if (i+1>=argc || !parse_int_arg(argv[++i],1,16,&span)) {
                    fprintf(stderr,"invalid --span\n"); return 1;
                }
            } else if (!strcmp(argv[i],"--scale")) {
                if (i+1>=argc || !parse_int_arg(argv[++i],1,8,&scale)) {
                    fprintf(stderr,"invalid --scale\n"); return 1;
                }
            } else if (!strcmp(argv[i],"--phase")) {
                if (i+1>=argc || !parse_int_arg(argv[++i],0,100,&hell_rx_phase)) {
                    fprintf(stderr,"invalid --phase\n"); return 1;
                }
            } else if (!strcmp(argv[i],"--rate")) {
                if (i+1>=argc || !parse_int_arg(argv[++i],900,1100,&hell_rx_rate)) {
                    fprintf(stderr,"invalid --rate\n"); return 1;
                }
            }
            else if (!strcmp(argv[i],"--single")) dual=0;
            else if (!strcmp(argv[i],"--blackboard")) blackboard=1;
            else if (!strcmp(argv[i],"--live")) live=1;
            else if (!strcmp(argv[i],"--seconds")) {
                if (i+1>=argc || !parse_int_arg(argv[++i],1,86400,&seconds)) {
                    fprintf(stderr,"invalid --seconds\n"); return 1;
                }
            } else if (!strcmp(argv[i],"-d") || !strcmp(argv[i],"--device")) {
                if (i+1>=argc || !parse_int_arg(argv[++i],-1,100000,&dev)) {
                    fprintf(stderr,"invalid device\n"); return 1;
                }
            } else if (argv[i][0]!='-') {
                in=argv[i];
            } else {
                fprintf(stderr,"unknown option: %s\n",argv[i]);
                return 1;
            }
        }
        m = mode_of(mode);
        if (!m) { fprintf(stderr,CT(CLI_S_UNKNOWN_MODE),mode); return 1; }
#ifdef _WIN32
        if (live) {
            live_rx(m,freq,alpha,span,seconds,dev,out,scale,blackboard);
            return 0;
        }
#endif
        if (!in) { fputs(CT(CLI_S_RX_NEEDS),stderr); return 1; }
        if (!read_wav(in,&x,&n,&fs)) { fprintf(stderr,CT(CLI_S_READ_FAIL),in); return 1; }
        im=demod_mode(x,n,fs,m,freq,alpha,span,1.0,0,0);
        if (!save_pgm(out,&im,scale,dual,blackboard)) { fprintf(stderr,CT(CLI_S_WRITE_FAIL),out); return 1; }
        printf(CT(CLI_S_RX_DONE),m->name,im.w,out);
        return 0;
    }
    fputs(CT(CLI_S_USAGE),stderr);
    return 1;
}

#ifdef _WIN32
static char **utf8_args(int *argc) {
    LPWSTR *w = CommandLineToArgvW(GetCommandLineW(),argc);
    char **a = xcalloc(*argc+1,sizeof(char *));
    int i;
    for (i=0;i<*argc;i++) {
        int n = WideCharToMultiByte(CP_UTF8,0,w[i],-1,0,0,0,0);
        a[i] = xmalloc(n);
        WideCharToMultiByte(CP_UTF8,0,w[i],-1,a[i],n,0,0);
    }
    LocalFree(w);
    return a;
}
#endif

#ifndef HELL_NO_MAIN
int main(int argc,char **argv) {
#ifdef _WIN32
    argv = utf8_args(&argc);
    SetConsoleOutputCP(CP_UTF8);
#endif
    cli_default_language();
    return run(argc,argv);
}
#endif
