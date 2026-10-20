#define HELL_NO_MAIN
#include "hell.c"
#include "gui_lang.h"
#include "resource.h"
#include <commctrl.h>
#include <richedit.h>
#include <imm.h>

#define UI_TOP 5
#define UI_LEFT 10
#define UI_GAP 7
#define RX_MIN 100
#define TXT_MIN 45
#define WF_MIN 60
#define WF_DEFAULT 90
#define SPLIT_HIT 6
#define WF_W 512
#define WF_HH 96
#define WF_N 8192

#define ID_TX 1001
#define ID_RX 1002
#define ID_SQL 1003
#define ID_CONTRAST 1004
#define ID_AUTO 1005
#define ID_STOP 1007
#define ID_PHASE 1008
#define ID_RATE 1009
#define ID_LANG 1010
#define WM_TXCOLOR (WM_APP+1)
#define WM_STATUS (WM_APP+2)
#define WM_STATUS_RATE (WM_APP+3)
#define WM_WFCONFIG (WM_APP+4)
#define WM_TX_FINISHED (WM_APP+5)

static HWND hwnd,h_text,h_mode,h_freq,h_status,h_tx,h_stop,h_rx,h_in,h_out,h_sql,h_bw;
static HWND h_contrast,h_cval,h_auto;
static HWND h_phase,h_phaseval;
static HWND h_rate,h_rateval;
static HWND h_lang,h_llang;
static HWND h_lmode,h_lfreq,h_lbw,h_lin,h_lout,h_lsql,h_lcontrast,h_lphase,h_lrate,h_sqlval;
static volatile LONG g_lang;
static volatile LONG g_mode_index;
static volatile LONG g_in_device=-1,g_out_device=-1;
static volatile LONG g_sql_value;
static int g_status_id=GUI_S_READY;
static unsigned g_status_rate,g_status_ch;
static Img g_rx;
static unsigned char g_wf[WF_W*WF_HH];
static CRITICAL_SECTION g_cs;
static volatile LONG g_rx_on,g_tx_on,g_mode;
static volatile LONG g_freq=1500;
static volatile LONG g_freq_gen;
static volatile LONG g_tx_stop,g_tx_next_mode;
static volatile LONG g_tx_rev,g_sent_cap=0x7fffffff;
static volatile LONG g_in_dirty,g_out_dirty;
static volatile LONG g_sent_chars;
static volatile LONG g_ime_active;
static volatile LONG g_coloring;
static char *g_doc_snap;
static volatile LONG g_wf_auto=1,g_wf_contrast=50;
static HANDLE g_rx_thread;
static HBRUSH g_editbrush;
static HFONT g_ui,g_edit;
static const char *g_fontpath;
static WNDPROC g_oldfreq;
static WNDPROC g_oldtext;
static int g_dpi=96;
static int g_rx_y=UI_TOP,g_rx_h=320,g_txt_y=332,g_txt_h=60;
static int g_wf_y=399,g_wf_h,g_pane_x=UI_LEFT,g_pane_w=860;
static int g_split1,g_split2,g_pane_avail,g_pane_bottom;
static double g_rx_ratio=320.0/380.0;
static int g_drag;
static int g_drag_freq0,g_drag_x0;

#define T(id) (gui_text[g_lang][(id)])

static int ui_px(int v) {
    return MulDiv(v,g_dpi,96);
}

static void enable_dpi_awareness(void) {
    typedef BOOL (WINAPI *dpi_context_fn)(HANDLE);
    union { FARPROC raw; dpi_context_fn fn; } set_ctx;
    set_ctx.raw=GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetProcessDpiAwarenessContext");
    if (!set_ctx.fn || !set_ctx.fn((HANDLE)(INT_PTR)-4)) SetProcessDPIAware();
    HDC dc=GetDC(0);
    int dpi=dc?GetDeviceCaps(dc,LOGPIXELSX):96;
    if (dc) ReleaseDC(0,dc);
    if (dpi>=96 && dpi<=480) g_dpi=dpi;
}

static int window_dpi(HWND h) {
    typedef UINT (WINAPI *get_dpi_fn)(HWND);
    union { FARPROC raw; get_dpi_fn fn; } get_dpi;
    get_dpi.raw=GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetDpiForWindow");
    if (get_dpi.fn) return (int)get_dpi.fn(h);
    HDC dc=GetDC(h);
    int dpi=dc?GetDeviceCaps(dc,LOGPIXELSX):96;
    if (dc) ReleaseDC(h,dc);
    return dpi;
}

static void apply_ui_fonts(void) {
    HWND ui[]={
        h_freq,h_bw,h_tx,h_stop,h_rx,h_status,h_mode,h_in,h_out,h_lang,
        h_llang,h_sql,h_sqlval,h_contrast,h_cval,h_phase,h_phaseval,
        h_rate,h_rateval,h_auto,h_lmode,h_lfreq,h_lbw,h_lin,h_lout,
        h_lsql,h_lcontrast,h_lphase,h_lrate
    };
    for (int i=0;i<(int)(sizeof(ui)/sizeof(ui[0]));i++)
        if (ui[i]) SendMessageW(ui[i],WM_SETFONT,(WPARAM)g_ui,TRUE);
    if (h_text) SendMessageW(h_text,WM_SETFONT,(WPARAM)g_edit,TRUE);
}

static void refresh_ui_fonts(void) {
    HFONT old_ui=g_ui,old_edit=g_edit;
    g_ui=CreateFontW(ui_px(16),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei");
    g_edit=CreateFontW(ui_px(20),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei");
    apply_ui_fonts();
    if (old_ui) DeleteObject(old_ui);
    if (old_edit) DeleteObject(old_edit);
}

static char *get_utf8(HWND h) {
    int n=GetWindowTextLengthW(h);
    wchar_t *w=xmalloc(sizeof(wchar_t)*(n+1));
    GetWindowTextW(h,w,n+1);
    int m=WideCharToMultiByte(CP_UTF8,0,w,-1,0,0,0,0);
    char *s=xmalloc(m?m:1);
    WideCharToMultiByte(CP_UTF8,0,w,-1,s,m,0,0);
    free(w);
    return s;
}

static char *dupstr(const char *s) {
    size_t n=strlen(s)+1;
    char *p=xmalloc(n);
    memcpy(p,s,n);
    return p;
}

static void color_range(int a,int b,COLORREF c) {
    CHARFORMAT2W cf;
    CHARRANGE cr={a,b};
    if (b<a) return;
    memset(&cf,0,sizeof(cf));
    cf.cbSize=sizeof(cf);
    cf.dwMask=CFM_COLOR;
    cf.crTextColor=c;
    SendMessageW(h_text,EM_EXSETSEL,0,(LPARAM)&cr);
    SendMessageW(h_text,EM_SETCHARFORMAT,SCF_SELECTION,(LPARAM)&cf);
}

static int text_composing(HWND h) {
    if (g_ime_active) return 1;
    if (GetCurrentThreadId()!=GetWindowThreadProcessId(h,0)) return 0;
    HIMC imc=ImmGetContext(h);
    LONG n;
    if (!imc) return 0;
    n=ImmGetCompositionStringW(imc,GCS_COMPSTR,0,0);
    ImmReleaseContext(h,imc);
    return n>0;
}

static void color_tx_text(void) {
    static int busy;
    if (busy || !h_text) return;
    if (text_composing(h_text)) return;
    busy=1;
    g_coloring=1;
    int total=GetWindowTextLengthW(h_text);
    int sent=(int)g_sent_chars;
    int cap=(int)g_sent_cap;
    if (cap<sent) sent=cap;
    if (sent>total) sent=total;
    if (sent<0) sent=0;
    color_range(0,sent,RGB(200,0,0));
    color_range(sent,total,RGB(0,0,0));
    color_range(total,total,RGB(0,0,0));
    SendMessageW(h_text,EM_SETSEL,total,total);
    g_coloring=0;
    busy=0;
}

static int utf8_wlen(const char *s,int n) {
    return MultiByteToWideChar(CP_UTF8,0,s,n,0,0);
}

static int wide_byte_offset(const char *s,int wchars) {
    const char *p=s;
    int n=0;
    while (*p && n<wchars) {
        const char *q=p;
        uint32_t c=nextcp(&q);
        int u=c>0xffff?2:1;
        if (n+u>wchars) break;
        n+=u;
        p=q;
    }
    return (int)(p-s);
}

static int common_prefix_wchars(const char *a,const char *b) {
    int n=0;
    if (!a) a="";
    if (!b) b="";
    while (a[n] && a[n]==b[n]) n++;
    while (n>0 && ((unsigned char)a[n]&0xc0)==0x80) n--;
    return utf8_wlen(a,n);
}

static void doc_changed(void) {
    char *cur=get_utf8(h_text);
    if (g_doc_snap && !strcmp(g_doc_snap,cur)) {
        free(cur);
        return;
    }
    int lcp=common_prefix_wchars(g_doc_snap,cur);
    if (lcp<(int)g_sent_cap) g_sent_cap=lcp;
    if (lcp<(int)g_sent_chars) g_sent_chars=lcp;
    free(g_doc_snap);
    g_doc_snap=cur;
    InterlockedIncrement(&g_tx_rev);
}

static void tx_mark_progress(const char *msg,int base,const int *map,int maplen,int cols) {
    int off,v;
    if (!msg || !map || maplen<=0 || cols<=0) return;
    if (cols>maplen) cols=maplen;
    off=map[cols-1];
    if (off<0) off=0;
    if (off>(int)strlen(msg)) off=(int)strlen(msg);
    v=base+utf8_wlen(msg,off);
    if (v>(int)g_sent_cap) v=(int)g_sent_cap;
    g_sent_chars=v;
    if (IsWindow(hwnd)) PostMessageW(hwnd,WM_TXCOLOR,0,0);
}

static int get_int(HWND h) {
    char *s=get_utf8(h);
    int v=atoi(s);
    free(s);
    return v;
}

static int sql_value(void) {
    return (int)g_sql_value;
}

static int contrast_value(void) {
    return (int)SendMessageW(h_contrast,TBM_GETPOS,0,0);
}

static void set_auto_contrast(int on) {
    g_wf_auto=on;
    if (h_auto) SendMessageW(h_auto,BM_SETCHECK,on?BST_CHECKED:BST_UNCHECKED,0);
}

static void set_freq(int f) {
    wchar_t s[16],old[16];
    if (f<0) f=0;
    if (f>3000) f=3000;
    g_freq=f;
    g_freq_gen++;
    swprintf(s,16,L"%d",f);
    GetWindowTextW(h_freq,old,16);
    if (wcscmp(old,s)) SetWindowTextW(h_freq,s);
    InvalidateRect(hwnd,0,FALSE);
}

static void commit_freq(void) {
    char *s=get_utf8(h_freq),*e;
    int f;
    if (!*s) {
        free(s);
        set_freq(g_freq);
        return;
    }
    f=(int)strtol(s,&e,10);
    if (*e) f=g_freq;
    free(s);
    set_freq(f);
}

static LRESULT CALLBACK freq_proc(HWND h,UINT msg,WPARAM wp,LPARAM lp) {
    if (msg==WM_KEYDOWN && wp==VK_RETURN) {
        commit_freq();
        SetFocus(h_text);
        return 0;
    }
    if (msg==WM_KEYDOWN && wp==VK_ESCAPE) {
        set_freq(g_freq);
        return 0;
    }
    return CallWindowProcW(g_oldfreq,h,msg,wp,lp);
}

static LRESULT CALLBACK text_proc(HWND h,UINT msg,WPARAM wp,LPARAM lp) {
    if (msg==WM_IME_STARTCOMPOSITION) {
        g_ime_active=1;
        InterlockedIncrement(&g_tx_rev);
        return CallWindowProcW(g_oldtext,h,msg,wp,lp);
    }
    if (msg==WM_IME_COMPOSITION) {
        LRESULT r=CallWindowProcW(g_oldtext,h,msg,wp,lp);
        if (lp & GCS_RESULTSTR) {
            HIMC imc=ImmGetContext(h);
            LONG n=imc?ImmGetCompositionStringW(imc,GCS_COMPSTR,0,0):0;
            if (imc) ImmReleaseContext(h,imc);
            if (n<=0) g_ime_active=0;
            InterlockedIncrement(&g_tx_rev);
            PostMessageW(hwnd,WM_TXCOLOR,0,0);
        }
        return r;
    }
    if (msg==WM_IME_ENDCOMPOSITION) {
        g_ime_active=0;
        LRESULT r=CallWindowProcW(g_oldtext,h,msg,wp,lp);
        InterlockedIncrement(&g_tx_rev);
        PostMessageW(hwnd,WM_TXCOLOR,0,0);
        return r;
    }
    if (msg==WM_KILLFOCUS) {
        g_ime_active=0;
        InterlockedIncrement(&g_tx_rev);
        return CallWindowProcW(g_oldtext,h,msg,wp,lp);
    }
    if (msg==WM_SETFOCUS ||
        ((msg==WM_CHAR || msg==WM_PASTE) && !text_composing(h))) {
        int n=GetWindowTextLengthW(h);
        SendMessageW(h,EM_SETSEL,n,n);
    }
    return CallWindowProcW(g_oldtext,h,msg,wp,lp);
}

static void mode_ui(HWND active) {
    SendMessageW(h_tx,BM_SETCHECK,h_tx==active?BST_CHECKED:BST_UNCHECKED,0);
    SendMessageW(h_stop,BM_SETCHECK,h_stop==active?BST_CHECKED:BST_UNCHECKED,0);
    SendMessageW(h_rx,BM_SETCHECK,h_rx==active?BST_CHECKED:BST_UNCHECKED,0);
}

static double sql_level(const double *x,int n) {
    int m=FS/10,sum=0;
    if (m>n) m=n;
    for (int i=n-m;i<n;i++) sum+=x[i]*x[i];
    return m>0?sqrt((double)sum/m):0;
}

static Mode *gui_mode(void) {
    int i=(int)g_mode_index;
    if (i<0 || i>=(int)(sizeof(modes)/sizeof(modes[0]))) i=0;
    return &modes[i];
}

static int mode_bw_i(void) {
    return (int)lround(mode_bw(gui_mode(),0.35));
}

static void sync_bw(void) {
    wchar_t s[16];
    if (!h_bw) return;
    swprintf(s,16,L"%d",mode_bw_i());
    SetWindowTextW(h_bw,s);
}

static int combo_dev(HWND h) {
    int i=(int)SendMessageW(h,CB_GETCURSEL,0,0);
    return i<0?-1:(int)(INT_PTR)SendMessageW(h,CB_GETITEMDATA,i,0);
}

static int device_is_cable(int id,int input,const wchar_t *needle) {
    wchar_t name[256]=L"";
    if (id<0) return 0;
    if (input) {
        WAVEINCAPSW c;
        if (!waveInGetDevCapsW((UINT)id,&c,sizeof(c))) wcscpy(name,c.szPname);
    } else {
        WAVEOUTCAPSW c;
        if (!waveOutGetDevCapsW((UINT)id,&c,sizeof(c))) wcscpy(name,c.szPname);
    }
    return wcsstr(name,needle)!=0;
}

static void layout_ui(void) {
    RECT rc;
    int W,H,pw,ctrl1,ctrl2,ctrl3,avail,wf,rem,rx,tx,devw,xout,langx,gap;
    int rx_min=ui_px(RX_MIN),txt_min=ui_px(TXT_MIN),wf_min=ui_px(WF_MIN);
    if (!hwnd || !h_text) return;
    GetClientRect(hwnd,&rc);
    W=rc.right; H=rc.bottom;
    pw=W-2*ui_px(UI_LEFT);
    if (pw<ui_px(200)) pw=ui_px(200);
    ctrl1=H-ui_px(156);
    ctrl2=H-ui_px(104);
    ctrl3=H-ui_px(52);
    g_pane_bottom=ctrl1-ui_px(30);
    gap=ui_px(UI_GAP);
    avail=g_pane_bottom-ui_px(UI_TOP)-2*gap;
    g_pane_avail=avail;
    if (!g_wf_h) g_wf_h=ui_px(WF_DEFAULT);
    wf=g_wf_h;
    if (wf>avail-rx_min-txt_min) wf=avail-rx_min-txt_min;
    if (wf<wf_min) wf=wf_min;
    rem=avail-wf;
    rx=(int)lround(rem*g_rx_ratio);
    if (rx<rx_min) rx=rx_min;
    if (rx>rem-txt_min) rx=rem-txt_min;
    tx=rem-rx;
    g_pane_x=ui_px(UI_LEFT);
    g_pane_w=pw;
    g_rx_y=ui_px(UI_TOP);
    g_rx_h=rx;
    g_txt_y=g_rx_y+rx+gap;
    g_txt_h=tx;
    g_wf_y=g_txt_y+tx+gap;
    g_wf_h=wf;
    g_split1=g_txt_y-gap/2;
    g_split2=g_wf_y-gap/2;
    MoveWindow(h_text,ui_px(UI_LEFT),g_txt_y,pw,tx,TRUE);
    MoveWindow(h_mode,ui_px(10),ctrl1,ui_px(100),ui_px(200),TRUE);
    MoveWindow(h_freq,ui_px(120),ctrl1,ui_px(70),ui_px(26),TRUE);
    MoveWindow(h_bw,ui_px(200),ctrl1,ui_px(70),ui_px(26),TRUE);
    MoveWindow(h_tx,ui_px(280),ctrl1,ui_px(160),ui_px(28),TRUE);
    MoveWindow(h_stop,ui_px(446),ctrl1,ui_px(85),ui_px(28),TRUE);
    MoveWindow(h_rx,ui_px(537),ctrl1,ui_px(125),ui_px(28),TRUE);
    MoveWindow(h_status,ui_px(672),ctrl1+ui_px(3),W>ui_px(682)?W-ui_px(682):ui_px(120),ui_px(24),TRUE);
    devw=(pw-ui_px(170))/2;
    if (devw<ui_px(100)) devw=ui_px(100);
    xout=ui_px(20)+devw;
    langx=ui_px(30)+2*devw;
    MoveWindow(h_in,ui_px(10),ctrl2,devw,ui_px(200),TRUE);
    MoveWindow(h_out,xout,ctrl2,devw,ui_px(200),TRUE);
    MoveWindow(h_lang,langx,ctrl2,ui_px(150),ui_px(200),TRUE);
    MoveWindow(h_sql,ui_px(10),ctrl3+ui_px(2),ui_px(170),ui_px(28),TRUE);
    MoveWindow(h_sqlval,ui_px(184),ctrl3+ui_px(4),ui_px(36),ui_px(22),TRUE);
    MoveWindow(h_contrast,ui_px(250),ctrl3+ui_px(2),ui_px(170),ui_px(28),TRUE);
    MoveWindow(h_cval,ui_px(424),ctrl3+ui_px(4),ui_px(36),ui_px(22),TRUE);
    MoveWindow(h_auto,ui_px(470),ctrl3,ui_px(110),ui_px(28),TRUE);
    MoveWindow(h_phase,ui_px(590),ctrl3+ui_px(2),ui_px(140),ui_px(28),TRUE);
    MoveWindow(h_phaseval,ui_px(734),ctrl3+ui_px(4),ui_px(36),ui_px(22),TRUE);
    MoveWindow(h_rate,ui_px(820),ctrl3+ui_px(2),ui_px(150),ui_px(28),TRUE);
    MoveWindow(h_rateval,ui_px(974),ctrl3+ui_px(4),ui_px(52),ui_px(22),TRUE);
    MoveWindow(h_lmode,ui_px(10),ctrl1-ui_px(22),ui_px(110),ui_px(20),TRUE);
    MoveWindow(h_lfreq,ui_px(120),ctrl1-ui_px(22),ui_px(100),ui_px(20),TRUE);
    MoveWindow(h_lbw,ui_px(200),ctrl1-ui_px(22),ui_px(150),ui_px(20),TRUE);
    MoveWindow(h_lin,ui_px(10),ctrl2-ui_px(22),devw,ui_px(20),TRUE);
    MoveWindow(h_lout,xout,ctrl2-ui_px(22),devw,ui_px(20),TRUE);
    MoveWindow(h_llang,langx,ctrl2-ui_px(22),ui_px(150),ui_px(20),TRUE);
    MoveWindow(h_lsql,ui_px(10),ctrl3-ui_px(22),ui_px(50),ui_px(20),TRUE);
    MoveWindow(h_lcontrast,ui_px(250),ctrl3-ui_px(22),ui_px(180),ui_px(20),TRUE);
    MoveWindow(h_lphase,ui_px(590),ctrl3-ui_px(22),ui_px(90),ui_px(20),TRUE);
    MoveWindow(h_lrate,ui_px(820),ctrl3-ui_px(22),ui_px(170),ui_px(20),TRUE);
    InvalidateRect(hwnd,0,FALSE);
}

static void fill_devices(HWND h,int input) {
    int old=combo_dev(h),sel=-1;
    SendMessageW(h,CB_RESETCONTENT,0,0);
    int idx=(int)SendMessageW(h,CB_ADDSTRING,0,(LPARAM)T(GUI_S_DEFAULT_DEVICE));
    SendMessageW(h,CB_SETITEMDATA,idx,(LPARAM)(INT_PTR)-1);
    if (old<0) sel=idx;
    int n=input?waveInGetNumDevs():waveOutGetNumDevs();
    for (int i=0;i<n;i++) {
        wchar_t name[256]=L"";
        if (input) {
            WAVEINCAPSW c;
            if (!waveInGetDevCapsW(i,&c,sizeof(c))) wcscpy(name,c.szPname);
        } else {
            WAVEOUTCAPSW c;
            if (!waveOutGetDevCapsW(i,&c,sizeof(c))) wcscpy(name,c.szPname);
        }
        if (!name[0]) swprintf(name,256,T(GUI_S_DEVICE_FMT),i);
        idx=(int)SendMessageW(h,CB_ADDSTRING,0,(LPARAM)name);
        SendMessageW(h,CB_SETITEMDATA,idx,(LPARAM)(INT_PTR)i);
        if (i==old) sel=idx;
    }
    if (sel<0) sel=0;
    SendMessageW(h,CB_SETCURSEL,sel,0);
    int selected=(int)(INT_PTR)SendMessageW(h,CB_GETITEMDATA,sel,0);
    if (input) g_in_device=selected;
    else g_out_device=selected;
}

static void render_status(void) {
    wchar_t s[192];
    if (g_status_id==GUI_S_STATUS_RX_RATE_FMT)
        swprintf(s,192,T(g_status_id),g_status_rate,g_status_ch);
    else
        wcsncpy(s,T(g_status_id),192);
    s[191]=0;
    SetWindowTextW(h_status,s);
}

static void status_id(int id) {
    g_status_id=id;
    render_status();
}

static void status_post(int id) {
    if (IsWindow(hwnd)) PostMessageW(hwnd,WM_STATUS,(WPARAM)id,0);
}

static void status_post_rate(unsigned rate,unsigned channels) {
    if (IsWindow(hwnd)) PostMessageW(hwnd,WM_STATUS_RATE,(WPARAM)rate,(LPARAM)channels);
}

static int lang_from_code(const wchar_t *code) {
    for (int i=0;i<GUI_LANG_COUNT;i++)
        if (!_wcsicmp(code,gui_lang_codes[i])) return i;
    return -1;
}

static int system_ui_language(void) {
    switch (PRIMARYLANGID(GetUserDefaultUILanguage())) {
    case LANG_CHINESE: return GUI_LANG_ZH;
    case LANG_KOREAN: return GUI_LANG_KO;
    case LANG_JAPANESE: return GUI_LANG_JA;
    case LANG_GERMAN: return GUI_LANG_DE;
    case LANG_RUSSIAN: return GUI_LANG_RU;
    case LANG_UKRAINIAN: return GUI_LANG_UK;
    case LANG_ITALIAN: return GUI_LANG_IT;
    case LANG_FRENCH: return GUI_LANG_FR;
    default: return GUI_LANG_EN;
    }
}

static void ui_ini_path(wchar_t *path,int size) {
    DWORD n=GetModuleFileNameW(0,path,size);
    if (!n || n>=(DWORD)size) {
        wcscpy(path,L"unihell.ini");
        return;
    }
    while (n>0 && path[n-1]!='\\' && path[n-1]!='/') n--;
    path[n]=0;
    wcsncat(path,L"unihell.ini",(size_t)size-wcslen(path)-1);
}

static void load_ui_language(void) {
    wchar_t code[16]=L"";
    wchar_t ini[MAX_PATH];
    int lang;
    ui_ini_path(ini,MAX_PATH);
    GetPrivateProfileStringW(L"ui",L"language",L"",code,16,ini);
    lang=lang_from_code(code);
    g_lang=lang>=0?lang:system_ui_language();
}

static void save_ui_language(void) {
    wchar_t ini[MAX_PATH];
    ui_ini_path(ini,MAX_PATH);
    WritePrivateProfileStringW(L"ui",L"language",gui_lang_codes[g_lang],ini);
}

static void apply_ui_language(void) {
    wchar_t title[160];
    if (!h_status) return;
    swprintf(title,160,L"UniHELL " UNIHELL_VERSION_W L" - %s",T(GUI_S_TITLE));
    SetWindowTextW(hwnd,title);
    SetWindowTextW(h_tx,T(GUI_S_TX));
    SetWindowTextW(h_stop,T(GUI_S_STOP));
    SetWindowTextW(h_rx,T(GUI_S_RX));
    SetWindowTextW(h_lmode,T(GUI_S_MODE));
    SetWindowTextW(h_lfreq,T(GUI_S_FREQ));
    SetWindowTextW(h_lbw,T(GUI_S_BW));
    SetWindowTextW(h_lin,T(GUI_S_INPUT));
    SetWindowTextW(h_lout,T(GUI_S_OUTPUT));
    SetWindowTextW(h_lsql,T(GUI_S_SQL));
    SetWindowTextW(h_lcontrast,T(GUI_S_WATERFALL));
    SetWindowTextW(h_lphase,T(GUI_S_PHASE));
    SetWindowTextW(h_lrate,T(GUI_S_RATE));
    SetWindowTextW(h_llang,T(GUI_S_LANGUAGE));
    SetWindowTextW(h_auto,T(GUI_S_AUTO));
    SendMessageW(h_lang,CB_SETCURSEL,g_lang,0);
    fill_devices(h_in,1);
    fill_devices(h_out,0);
    render_status();
    layout_ui();
}

#define RX_MAX_COLS 50000

static void clear_rx(void) {
    EnterCriticalSection(&g_cs);
    free(g_rx.p);
    g_rx.p=0;
    g_rx.w=g_rx.h=0;
    LeaveCriticalSection(&g_cs);
    InvalidateRect(hwnd,0,FALSE);
}

static void append_rx(const Img *im,int start,int n) {
    int drop=0,ow,W;
    unsigned char *p;
    if (!im || !im->p || n<=0 || start<0) return;
    if (start+n>im->w) n=im->w-start;
    if (n<=0) return;
    EnterCriticalSection(&g_cs);
    ow=g_rx.p?g_rx.w:0;
    if (ow+n>RX_MAX_COLS) drop=ow+n-RX_MAX_COLS;
    if (drop>ow) drop=ow;
    W=ow-drop+n;
    p=xcalloc((size_t)W*ROWS,1);
    for (int y=0;y<ROWS;y++) {
        if (ow>drop) memcpy(p+y*W,g_rx.p+y*ow+drop,ow-drop);
        memcpy(p+y*W+(ow-drop),im->p+y*im->w+start,n);
    }
    free(g_rx.p);
    g_rx.w=W; g_rx.h=ROWS; g_rx.p=p;
    LeaveCriticalSection(&g_cs);
    InvalidateRect(hwnd,0,FALSE);
}

static void append_blank(int n) {
    Img z={n,ROWS,xcalloc((size_t)n*ROWS,1)};
    append_rx(&z,0,n);
    free(z.p);
}

static int cmp_double(const void *a,const void *b) {
    double x=*(const double*)a,y=*(const double*)b;
    return x<y?-1:x>y?1:0;
}

static void update_wf(const double *x,int n) {
    static double clo=-70,chi=0;
    static int ready;
    if (n<WF_N) return;
    int N=WF_N;
    Cpx *a=xcalloc(N,sizeof(Cpx));
    for (int i=0;i<N;i++) {
        double w=0.5-0.5*cos(2*PI*i/(N-1));
        a[i].re=x[n-N+i]*w;
    }
    fft(a,N,0);
    double mag[WF_W],db[WF_W],order[WF_W],peak=0;
    for (int i=0;i<WF_W;i++) {
        double f=i*3000.0/WF_W;
        int k=(int)(f*N/FS);
        if (k<1) k=1;
        if (k>=N/2) k=N/2-1;
        mag[i]=hypot(a[k].re,a[k].im);
        if (mag[i]>peak) peak=mag[i];
    }
    for (int i=0;i<WF_W;i++) {
        db[i]=peak>1e-9?20*log10(mag[i]/peak+1e-12):-90;
        order[i]=db[i];
    }
    qsort(order,WF_W,sizeof(double),cmp_double);
    double lo=peak>1e-9?order[WF_W/5]:-90,hi=peak>1e-9?order[WF_W-1]:-78;
    if (lo<-90) lo=-90;
    if (hi>0) hi=0;
    if (hi<lo+12) hi=lo+12;
    if (!g_wf_auto) {
        chi=0;
        clo=-90.0+0.8*g_wf_contrast;
        ready=1;
    } else if (!ready) {
        clo=lo; chi=hi; ready=1;
    } else {
        clo=0.82*clo+0.18*lo;
        chi=0.82*chi+0.18*hi;
    }
    if (g_wf_auto) {
        int cv=(int)lround((90.0-(chi-clo))/0.8);
        int old=(int)g_wf_contrast;
        if (cv<0) cv=0;
        if (cv>100) cv=100;
        if (cv!=old) {
            g_wf_contrast=cv;
            if (IsWindow(hwnd)) PostMessageW(hwnd,WM_WFCONFIG,(WPARAM)cv,0);
        }
    }
    EnterCriticalSection(&g_cs);
    memmove(g_wf+WF_W,g_wf,WF_W*(WF_HH-1));
    for (int i=0;i<WF_W;i++) {
        int v=(int)((db[i]-clo)*255/(chi-clo));
        if (v<0) v=0;
        if (v>255) v=255;
        g_wf[i]=(unsigned char)v;
    }
    LeaveCriticalSection(&g_cs);
    InvalidateRect(hwnd,0,FALSE);
    free(a);
}

static int pick_wavein(UINT id,WAVEFORMATEX *best) {
    static const int rates[]={48000,44100,32000,22050,16000,8000};
    for (int ch=2;ch>=1;ch--) for (int i=0;i<(int)(sizeof(rates)/sizeof(rates[0]));i++) {
        WAVEFORMATEX wf;
        memset(&wf,0,sizeof(wf));
        wf.wFormatTag=WAVE_FORMAT_PCM;
        wf.nChannels=(WORD)ch;
        wf.nSamplesPerSec=rates[i];
        wf.wBitsPerSample=16;
        wf.nBlockAlign=(WORD)(ch*2);
        wf.nAvgBytesPerSec=rates[i]*wf.nBlockAlign;
        if (waveInOpen(0,id,&wf,0,0,WAVE_FORMAT_QUERY)==MMSYSERR_NOERROR) {
            *best=wf;
            return 1;
        }
    }
    return 0;
}

static DWORD WINAPI rx_thread(LPVOID arg) {
    enum { NB=4 };
    WAVEFORMATEX wf;
    HWAVEIN h;
    short *buf[NB];
    WAVEHDR hdr[NB];
    double *mono=xmalloc(sizeof(double)*8192);
    double *out=xmalloc(sizeof(double)*65536);
    double *dbuf=0;
    int dlen=0;
    int frames=4096;
    long fgen=-1;
    int lastopen=1;
    long long dropped_cols=0,last_col=0;
    double rpos=0,prev=0;
    (void)arg;
    for (int b=0;b<NB;b++) buf[b]=xmalloc(sizeof(short)*16384);
    while (g_rx_on) {
        int dev=(int)g_in_device;
        UINT id=dev<0?WAVE_MAPPER:(UINT)dev;
        if (!pick_wavein(id,&wf)) {
            if (g_mode==2) status_post(GUI_S_ERR_INPUT_FORMAT);
            g_rx_on=0;
            break;
        }
        if (waveInOpen(&h,id,&wf,0,0,CALLBACK_NULL)) {
            if (g_mode==2) status_post(GUI_S_ERR_OPEN_INPUT);
            g_rx_on=0;
            break;
        }
        if (g_mode!=2) {
            waveInClose(h);
            g_rx_on=0;
            break;
        }
        g_in_dirty=0;
        rpos=0; prev=0;
        status_post_rate(wf.nSamplesPerSec,wf.nChannels);
        for (int b=0;b<NB;b++) {
            memset(&hdr[b],0,sizeof(WAVEHDR));
            hdr[b].lpData=(LPSTR)buf[b];
            hdr[b].dwBufferLength=frames*wf.nChannels*2;
            if (waveInPrepareHeader(h,&hdr[b],sizeof(WAVEHDR)) ||
                waveInAddBuffer(h,&hdr[b],sizeof(WAVEHDR))) {
                g_rx_on=0;
                break;
            }
        }
        if (!g_rx_on) {
            waveInClose(h);
            break;
        }
        waveInStart(h);
    int qbuf=0;
    while (g_rx_on && !g_in_dirty) {
        int b=qbuf;
        if (!(hdr[b].dwFlags & WHDR_DONE)) {
            Sleep(5);
            continue;
        }
        waveInUnprepareHeader(h,&hdr[b],sizeof(WAVEHDR));
        if (g_freq_gen!=fgen) {
            fgen=g_freq_gen;
            dlen=0;
            dropped_cols=0;
            last_col=0;
            lastopen=-1;
        }
        int ch=wf.nChannels,outn=0;
        for (int i=0;i<frames;i++) {
            int sum=0;
            for (int c=0;c<ch;c++) sum+=buf[b][i*ch+c];
            mono[i]=sum/(32768.0*ch);
        }
        double step=(double)wf.nSamplesPerSec/FS;
        while (rpos<frames-1) {
            int i=(int)floor(rpos);
            double frac=rpos-i;
            double a=i<0?prev:mono[i];
            double b=mono[i+1];
            out[outn++]=a+(b-a)*frac;
            rpos+=step;
        }
        prev=mono[frames-1];
        rpos-=frames;
        dbuf=xrealloc(dbuf,sizeof(double)*(dlen+outn));
        memcpy(dbuf+dlen,out,sizeof(double)*outn);
        dlen+=outn;
        if (dlen>FS*3) {
            double spp=(double)FS/(gui_mode()->col*ROWS);
            double col_samples=spp*ROWS*(hell_rx_rate/1000.0);
            int drop=dlen-FS*2;
            int ncol=(int)lround(drop/col_samples);
            if (ncol<1) ncol=1;
            drop=(int)lround(ncol*col_samples);
            if (drop>=dlen) drop=dlen-1;
            memmove(dbuf,dbuf+drop,sizeof(double)*(dlen-drop));
            dlen-=drop;
            dropped_cols+=ncol;
        }
        Mode *m=gui_mode();
        int freq=(int)g_freq;
        int sql=sql_value();
        double thr=sql>0?pow(10.0,(-60.0+0.54*sql)/20.0):0;
        double bw=mode_bw_i();
        int open=sql==0 || sql_level(dbuf,dlen)>=thr;
        if (open!=lastopen) {
            status_post(open?GUI_S_STATUS_RECEIVING:GUI_S_STATUS_SQL_PAUSE);
            lastopen=open;
        }
        if (open) {
            Img im=demod_mode(dbuf,dlen,FS,m,freq,0.35,4,1.0,0,bw);
            if (im.p) {
                long long abs_cols=dropped_cols+im.w;
                long long keep=last_col-dropped_cols;
                if (keep<0) keep=0;
                if (keep>im.w) keep=im.w;
                int add=(int)(abs_cols-last_col);
                if (add>im.w-keep) add=(int)(im.w-keep);
                if (add>0) append_rx(&im,(int)keep,add);
                last_col=dropped_cols+keep+add;
                free(im.p);
            }
        }
        update_wf(dbuf,dlen);
        if (g_rx_on && !g_in_dirty) {
            memset(&hdr[b],0,sizeof(WAVEHDR));
            hdr[b].lpData=(LPSTR)buf[b];
            hdr[b].dwBufferLength=frames*wf.nChannels*2;
            if (waveInPrepareHeader(h,&hdr[b],sizeof(WAVEHDR)) ||
                waveInAddBuffer(h,&hdr[b],sizeof(WAVEHDR))) g_rx_on=0;
        }
        qbuf=(qbuf+1)%NB;
    }
        waveInStop(h);
        waveInReset(h);
        for (int b=0;b<NB;b++) waveInUnprepareHeader(h,&hdr[b],sizeof(WAVEHDR));
        waveInClose(h);
        dlen=0;
        dropped_cols=0;
        last_col=0;
        lastopen=-1;
        if (g_in_dirty && g_rx_on) status_post(GUI_S_STATUS_SWITCH_INPUT);
    }
    for (int b=0;b<NB;b++) free(buf[b]);
    free(mono);
    free(out);
    free(dbuf);
    g_rx_on=0;
    if (g_mode==2) status_post(GUI_S_STATUS_RX_STOPPED);
    return 0;
}

static int stop_rx(void) {
    g_rx_on=0;
    if (g_rx_thread) {
        if (WaitForSingleObject(g_rx_thread,5000)==WAIT_OBJECT_0) {
            CloseHandle(g_rx_thread);
            g_rx_thread=0;
            return 1;
        }
        return 0;
    }
    return 1;
}

static void start_rx(void) {
    if (!IsWindow(hwnd)) return;
    if (g_rx_thread) {
        if (WaitForSingleObject(g_rx_thread,0)!=WAIT_OBJECT_0) return;
        CloseHandle(g_rx_thread);
        g_rx_thread=0;
    }
    g_rx_on=1;
    g_rx_thread=CreateThread(0,0,rx_thread,0,0,0);
    if (!g_rx_thread) {
        g_rx_on=0;
        status_post(GUI_S_STATUS_RX_STOPPED);
    }
}

static void play_sync(const double *x,int n,int fs,int dev,Img *rx,Mode *m,const char *markmsg,int base,const int *map,int maplen,LONG rev) {
    WAVEFORMATEX wf={WAVE_FORMAT_PCM,1,fs,fs*2,2,16,0};
    short *buf=xmalloc(sizeof(short)*n);
    for (int i=0;i<n;i++) {
        double v=x[i]*32767;
        if (v>32767) v=32767;
        if (v<-32768) v=-32768;
        buf[i]=(short)lrint(v);
    }
    HWAVEOUT h;
    WAVEHDR hdr;
    UINT id=dev<0?WAVE_MAPPER:(UINT)dev;
    memset(&hdr,0,sizeof(hdr));
    hdr.lpData=(LPSTR)buf;
    hdr.dwBufferLength=n*2;
    if (waveOutOpen(&h,id,&wf,0,0,CALLBACK_NULL)) {
        update_wf(x,n);
        append_rx(rx,0,rx->w);
        if (markmsg) {
            int v=base+utf8_wlen(markmsg,(int)strlen(markmsg));
            if (v>(int)g_sent_cap) v=(int)g_sent_cap;
            g_sent_chars=v;
            PostMessageW(hwnd,WM_TXCOLOR,0,0);
        }
        free(buf);
        return;
    }
    waveOutPrepareHeader(h,&hdr,sizeof(hdr));
    waveOutWrite(h,&hdr,sizeof(hdr));
    DWORD t0=GetTickCount();
    DWORD lastwf=0;
    int last=0;
    int cancelled=0;
    int changed=0;
    int cdone=0,dirty_cdone=-1;
    while (!(hdr.dwFlags & WHDR_DONE)) {
        MMTIME mt;
        DWORD played;
        if (!g_tx_on || g_tx_stop) {
            cancelled=1;
            waveOutReset(h);
            break;
        }
        mt.wType=TIME_SAMPLES;
        if (!waveOutGetPosition(h,&mt,sizeof(mt)) && mt.wType==TIME_SAMPLES)
            played=mt.u.sample;
        else
            played=(GetTickCount()-t0)*fs/1000;
        int cols=(int)((double)played*m->col/fs);
        if (cols>rx->w) cols=rx->w;
        if (cols>last) {
            append_rx(rx,last,cols-last);
            last=cols;
            tx_mark_progress(markmsg,base,map,maplen,cols);
            if (markmsg)
                cdone=cols<=maplen?utf8_wlen(markmsg,map[cols-1]):utf8_wlen(markmsg,(int)strlen(markmsg));
        }
        if (g_tx_rev!=rev) {
            if (text_composing(h_text)) {
                dirty_cdone=-1;
            } else if (!markmsg) {
                changed=1;
                waveOutReset(h);
                break;
            } else if (dirty_cdone<0) {
                dirty_cdone=cdone;
            } else if (cdone>dirty_cdone) {
                changed=1;
                waveOutReset(h);
                break;
            }
        } else {
            dirty_cdone=-1;
        }
        if (played-lastwf>=(DWORD)(fs/30)) {
            update_wf(x,(int)(played<(DWORD)n?played:(DWORD)n));
            lastwf=played;
        }
        Sleep(20);
    }
    if (!cancelled && !changed) update_wf(x,n);
    waveOutUnprepareHeader(h,&hdr,sizeof(hdr));
    waveOutClose(h);
    free(buf);
    if (!cancelled && !changed && markmsg) {
        int v=base+utf8_wlen(markmsg,(int)strlen(markmsg));
        if (v>(int)g_sent_cap) v=(int)g_sent_cap;
        g_sent_chars=v;
        PostMessageW(hwnd,WM_TXCOLOR,0,0);
    }
}

static void fill_idle(short *buf,double *d,int n,int tone,double freq,int fs,double *phase) {
    for (int i=0;i<n;i++) {
        double v=tone?0.35*cos(*phase):0;
        d[i]=v;
        buf[i]=(short)lrint(v*32767);
        if (tone) *phase+=2*PI*freq/fs;
    }
    if (tone) *phase=fmod(*phase,2*PI);
}

static void play_stream(Mode *m,double tone_freq,int fs,int dev,double *phase,int limit_ms,int ignore_rev,LONG rev) {
    enum { NB=3 };
    int N=fs/20,tone=tone_freq>0;
    WAVEFORMATEX wf={WAVE_FORMAT_PCM,1,fs,fs*2,2,16,0};
    HWAVEOUT h;
    WAVEHDR hdr[NB];
    short *buf[NB];
    double d[2205],hist[8192]={0},colacc=0;
    UINT id=dev<0?WAVE_MAPPER:(UINT)dev;
    DWORD start=GetTickCount();
    if (N>2205) N=2205;
    if (waveOutOpen(&h,id,&wf,0,0,CALLBACK_NULL)) {
        Sleep(40);
        return;
    }
    for (int b=0;b<NB;b++) {
        buf[b]=xmalloc(sizeof(short)*N);
        memset(&hdr[b],0,sizeof(WAVEHDR));
        hdr[b].lpData=(LPSTR)buf[b];
        hdr[b].dwBufferLength=N*2;
        fill_idle(buf[b],d,N,tone,tone_freq,fs,phase);
        memmove(hist,hist+N,sizeof(double)*(8192-N));
        memcpy(hist+8192-N,d,sizeof(double)*N);
        waveOutPrepareHeader(h,&hdr[b],sizeof(WAVEHDR));
        waveOutWrite(h,&hdr[b],sizeof(WAVEHDR));
    }
    while (g_tx_on && !g_tx_stop && !g_out_dirty &&
           (ignore_rev || g_tx_rev==rev) &&
           (!limit_ms || GetTickCount()-start<(DWORD)limit_ms)) {
        for (int b=0;b<NB;b++) {
            if (!(hdr[b].dwFlags & WHDR_DONE)) continue;
            waveOutUnprepareHeader(h,&hdr[b],sizeof(WAVEHDR));
            memset(&hdr[b],0,sizeof(WAVEHDR));
            hdr[b].lpData=(LPSTR)buf[b];
            hdr[b].dwBufferLength=N*2;
            fill_idle(buf[b],d,N,tone,tone_freq,fs,phase);
            memmove(hist,hist+N,sizeof(double)*(8192-N));
            memcpy(hist+8192-N,d,sizeof(double)*N);
            update_wf(hist,8192);
            colacc+=N*m->col/fs;
            int add=(int)colacc;
            if (add>0) {
                colacc-=add;
                append_blank(add);
            }
            waveOutPrepareHeader(h,&hdr[b],sizeof(WAVEHDR));
            waveOutWrite(h,&hdr[b],sizeof(WAVEHDR));
        }
        Sleep(5);
    }
    waveOutReset(h);
    for (int b=0;b<NB;b++) {
        waveOutUnprepareHeader(h,&hdr[b],sizeof(WAVEHDR));
        free(buf[b]);
    }
    waveOutClose(h);
}

static void send_msg(const char *text,Mode *m,int freq,int dev,double bw,int base,LONG rev) {
    int *map=0;
    Img im=render_hb_map(g_fontpath,text,m->fsk||m->psk,base>=0?&map:0);
    int n;
    double *w;
    Img rx;
    if (!im.p) return;
    if (g_tx_rev!=rev) {
        free(im.p);
        free(map);
        return;
    }
    w=modulate(&im,m,freq,FS,0.35,4,&n);
    rx=demod_mode(w,n,FS,m,freq,0.35,4,1.0,0,bw);
    if (g_tx_rev!=rev && !text_composing(h_text)) {
        free(rx.p);
        free(w);
        free(im.p);
        free(map);
        return;
    }
    if (rx.p) play_sync(w,n,FS,dev,&rx,m,base>=0?text:0,base,map,im.w,rev);
    else play_wave(w,n,FS,dev);
    free(rx.p);
    free(w);
    free(im.p);
    free(map);
}

static DWORD WINAPI tx_thread(LPVOID arg) {
    (void)arg;
    if (!stop_rx()) {
        g_tx_on=0;
        status_post(GUI_S_STATUS_RX_STOPPED);
        if (IsWindow(hwnd)) PostMessageW(hwnd,WM_TX_FINISHED,1,0);
        return 0;
    }
    Mode *m=gui_mode();
    int freq=(int)g_freq;
    double bw=mode_bw_i();
    double phase=0;
    if (device_is_cable((int)g_in_device,1,L"CABLE Output") &&
        !device_is_cable((int)g_out_device,0,L"CABLE Input"))
        status_post(GUI_S_STATUS_CABLE_WARN);
    else
        status_post(GUI_S_STATUS_TX);
    LONG rev=g_tx_rev;
    send_msg("...",m,freq,(int)g_out_device,bw,-1,rev);
    while (g_tx_on) {
        if (g_tx_stop) break;
        int dev=(int)g_out_device;
        if (g_out_dirty) g_out_dirty=0;
        if (text_composing(h_text)) {
            play_stream(m,m->fsk?freq+m->shift/2.0:(m->psk?freq:0),FS,dev,&phase,100,1,rev);
            continue;
        }
        g_sent_cap=0x7fffffff;
        rev=g_tx_rev;
        char *cur=get_utf8(h_text);
        if (g_tx_rev!=rev || text_composing(h_text)) {
            free(cur);
            continue;
        }
        int totalw=utf8_wlen(cur,(int)strlen(cur));
        int sentw=(int)g_sent_chars;
        if (sentw>totalw) {
            sentw=totalw;
            g_sent_chars=totalw;
        }
        if (sentw<0) sentw=0;
        if (sentw<totalw) {
            int off=wide_byte_offset(cur,sentw);
            char *chunk=dupstr(cur+off);
            free(cur);
            send_msg(chunk,m,freq,dev,bw,sentw,rev);
            free(chunk);
        } else {
            free(cur);
            play_stream(m,m->fsk?freq+m->shift/2.0:(m->psk?freq:0),FS,dev,&phase,0,0,rev);
        }
    }
    if (g_tx_stop) {
        g_tx_stop=0;
    send_msg("...",m,freq,(int)g_out_device,bw,-1,g_tx_rev);
    }
    int next=g_tx_next_mode;
    g_tx_next_mode=0;
    g_tx_stop=0;
    g_tx_on=0;
    if (IsWindow(hwnd)) PostMessageW(hwnd,WM_TX_FINISHED,(WPARAM)next,0);
    return 0;
}

static void paint_rx(HDC dc,int x,int y,int w,int h) {
    HBRUSH br=CreateSolidBrush(RGB(255,255,255));
    RECT r={x,y,x+w,y+h};
    FillRect(dc,&r,br);
    DeleteObject(br);
    EnterCriticalSection(&g_cs);
    Img im=g_rx;
    if (im.p && im.w>0) {
        int sx=ui_px(2),sy=ui_px(2),sep=ui_px(5),H=ROWS*2,lh=H*sy+sep;
        int cols=(w-ui_px(4))/sx,lines=(h+sep)/lh;
        int pages,first;
        if (cols<1) cols=1;
        if (lines<1) lines=1;
        pages=(im.w+cols-1)/cols;
        first=pages-lines;
        if (first<0) first=0;
        for (int i=0;first+i<pages && i<lines;i++) {
            int c0=(first+i)*cols,n=im.w-c0;
            int dw;
            unsigned char *rgb;
            BITMAPINFO bi;
            if (n>cols) n=cols;
            dw=n*sx;
            rgb=xmalloc((size_t)dw*H*4);
            for (int yy=0;yy<H;yy++) for (int xx=0;xx<n;xx++) {
                int v=255-im.p[(yy%ROWS)*im.w+c0+xx];
                for (int k=0;k<sx;k++) {
                    unsigned char *p=rgb+((size_t)yy*dw+xx*sx+k)*4;
                    p[0]=p[1]=p[2]=(unsigned char)v;
                    p[3]=255;
                }
            }
            memset(&bi,0,sizeof(bi));
            bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
            bi.bmiHeader.biWidth=dw;
            bi.bmiHeader.biHeight=-H;
            bi.bmiHeader.biPlanes=1;
            bi.bmiHeader.biBitCount=32;
            bi.bmiHeader.biCompression=BI_RGB;
            SetStretchBltMode(dc,COLORONCOLOR);
            StretchDIBits(dc,x,y+i*lh,dw,H*sy,0,0,dw,H,rgb,&bi,DIB_RGB_COLORS,SRCCOPY);
            free(rgb);
        }
    }
    LeaveCriticalSection(&g_cs);
}

static void wf_rgb(int v,unsigned char *p) {
    static const unsigned char c[8][3]={
        {24,0,44},{24,38,155},{0,165,205},{0,210,75},
        {170,230,0},{255,215,0},{255,105,0},{215,0,0}
    };
    if (v<24) {
        p[0]=p[1]=p[2]=0;
        p[3]=255;
        return;
    }
    int t=v*7,i=t/255,f=t%255;
    if (i>6) { i=6; f=255; }
    p[0]=(unsigned char)((c[i][2]*(255-f)+c[i+1][2]*f)/255);
    p[1]=(unsigned char)((c[i][1]*(255-f)+c[i+1][1]*f)/255);
    p[2]=(unsigned char)((c[i][0]*(255-f)+c[i+1][0]*f)/255);
    p[3]=255;
}

static void paint_wf(HDC dc,int x,int y,int w,int h) {
    int W=WF_W,H=WF_HH;
    unsigned char *rgb=xmalloc((size_t)W*H*4);
    EnterCriticalSection(&g_cs);
    for (int yy=0;yy<H;yy++) for (int xx=0;xx<W;xx++) {
        int v=g_wf[yy*W+xx];
        unsigned char *p=rgb+((size_t)yy*W+xx)*4;
        wf_rgb(v,p);
    }
    LeaveCriticalSection(&g_cs);
    BITMAPINFO bi;
    memset(&bi,0,sizeof(bi));
    bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth=W;
    bi.bmiHeader.biHeight=-H;
    bi.bmiHeader.biPlanes=1;
    bi.bmiHeader.biBitCount=32;
    bi.bmiHeader.biCompression=BI_RGB;
    StretchDIBits(dc,x,y,w,h,0,0,W,H,rgb,&bi,DIB_RGB_COLORS,SRCCOPY);
    free(rgb);
    int center=(int)g_freq,bw=mode_bw_i();
    Mode *m=gui_mode();
    double half=m->fsk?m->shift/2.0:bw/2.0;
    int rx0=x+(int)lround((center-half)*w/3000);
    int rx1=x+(int)lround((center+half)*w/3000);
    if (rx0<x) rx0=x;
    if (rx1>x+w) rx1=x+w;
    HBRUSH red=CreateSolidBrush(RGB(255,40,40));
    int edge=ui_px(1);
    RECT rt={rx0,y,rx1,y+edge},rb={rx0,y+h-edge,rx1,y+h};
    RECT rl={rx0,y,rx0+edge,y+h},rr={rx1-edge,y,rx1,y+h};
    FillRect(dc,&rt,red);
    FillRect(dc,&rb,red);
    FillRect(dc,&rl,red);
    FillRect(dc,&rr,red);
    int l0=x+(int)lround((center-bw/2.0)*w/3000);
    int l1=x+(int)lround((center+bw/2.0)*w/3000);
    if (l0<x) l0=x;
    if (l1>x+w) l1=x+w;
    RECT bar={l0,y+ui_px(2),l1,y+ui_px(6)};
    FillRect(dc,&bar,red);
    DeleteObject(red);
}

static LRESULT CALLBACK wndproc(HWND h,UINT msg,WPARAM wp,LPARAM lp) {
    switch (msg) {
    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm=(MINMAXINFO*)lp;
        mm->ptMinTrackSize.x=ui_px(900);
        mm->ptMinTrackSize.y=ui_px(500);
        return 0;
    }
    case WM_DPICHANGED: {
        RECT *r=(RECT*)lp;
        int dpi=HIWORD(wp);
        if (dpi>=96 && dpi!=g_dpi) {
            int old=g_dpi;
            g_dpi=dpi;
            if (g_wf_h) g_wf_h=MulDiv(g_wf_h,dpi,old);
            refresh_ui_fonts();
            SetWindowPos(h,0,r->left,r->top,r->right-r->left,r->bottom-r->top,
                SWP_NOZORDER|SWP_NOACTIVATE);
            layout_ui();
        }
        return 0;
    }
    case WM_SIZE:
        if (wp!=SIZE_MINIMIZED) layout_ui();
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc=BeginPaint(h,&ps);
        RECT rc;
        GetClientRect(h,&rc);
        if (get_int(h_bw)!=mode_bw_i()) sync_bw();
        HBRUSH bg=CreateSolidBrush(RGB(245,245,245));
        FillRect(dc,&rc,bg);
        DeleteObject(bg);
        paint_rx(dc,g_pane_x,g_rx_y,g_pane_w,g_rx_h);
        paint_wf(dc,g_pane_x,g_wf_y,g_pane_w,g_wf_h);
        HBRUSH sp=CreateSolidBrush(g_drag?RGB(70,130,210):RGB(175,175,175));
        RECT r1={g_pane_x,g_split1,g_pane_x+g_pane_w,g_split1+ui_px(2)};
        RECT r2={g_pane_x,g_split2,g_pane_x+g_pane_w,g_split2+ui_px(2)};
        FillRect(dc,&r1,sp);
        FillRect(dc,&r2,sp);
        DeleteObject(sp);
        SetBkMode(dc,TRANSPARENT);
        SelectObject(dc,g_ui);
        SetTextColor(dc,RGB(220,220,220));
        for (int f=0;f<=3000;f+=500) {
            int fx=g_pane_x+f*g_pane_w/3000;
            SIZE sz;
            wchar_t label[16];
            swprintf(label,16,f==3000?L"3000":L"%d",f);
            GetTextExtentPoint32W(dc,label,(int)wcslen(label),&sz);
            fx-=sz.cx/2;
            if (fx<g_pane_x+ui_px(2)) fx=g_pane_x+ui_px(2);
            if (fx+sz.cx>g_pane_x+g_pane_w-ui_px(2))
                fx=g_pane_x+g_pane_w-ui_px(2)-sz.cx;
            TextOutW(dc,fx,g_wf_y+ui_px(4),label,(int)wcslen(label));
        }
        SetTextColor(dc,RGB(20,20,20));
        EndPaint(h,&ps);
        return 0;
    }
    case WM_CTLCOLOREDIT:
        SetBkColor((HDC)wp,RGB(224,242,255));
        return (LRESULT)g_editbrush;
    case WM_HSCROLL:
        if ((HWND)lp==h_sql) {
            wchar_t s[16];
            g_sql_value=(int)SendMessageW(h_sql,TBM_GETPOS,0,0);
            swprintf(s,16,L"%d",(int)g_sql_value);
            SetWindowTextW(h_sqlval,s);
        } else if ((HWND)lp==h_contrast) {
            wchar_t s[16];
            swprintf(s,16,L"%d",contrast_value());
            SetWindowTextW(h_cval,s);
            g_wf_contrast=contrast_value();
            set_auto_contrast(0);
        } else if ((HWND)lp==h_phase) {
            wchar_t s[16];
            hell_rx_phase=(int)SendMessageW(h_phase,TBM_GETPOS,0,0);
            swprintf(s,16,L"%d",hell_rx_phase);
            SetWindowTextW(h_phaseval,s);
            g_freq_gen++;
        } else if ((HWND)lp==h_rate) {
            wchar_t s[16];
            hell_rx_rate=(int)SendMessageW(h_rate,TBM_GETPOS,0,0);
            swprintf(s,16,L"%d",hell_rx_rate);
            SetWindowTextW(h_rateval,s);
            g_freq_gen++;
        }
        return 0;
    case WM_TIMER:
        if (wp==2) {
            KillTimer(h,2);
            commit_freq();
            return 0;
        }
        if (wp==1 && get_int(h_bw)!=mode_bw_i()) {
            sync_bw();
            InvalidateRect(h,0,FALSE);
        }
        return 0;
    case WM_TXCOLOR:
        color_tx_text();
        return 0;
    case WM_STATUS:
        g_status_id=(int)wp;
        render_status();
        return 0;
    case WM_STATUS_RATE:
        g_status_id=GUI_S_STATUS_RX_RATE_FMT;
        g_status_rate=(unsigned)wp;
        g_status_ch=(unsigned)lp;
        render_status();
        return 0;
    case WM_WFCONFIG:
        {
            int cv=(int)wp;
            wchar_t s[16];
            if ((int)SendMessageW(h_contrast,TBM_GETPOS,0,0)!=cv)
                SendMessageW(h_contrast,TBM_SETPOS,TRUE,cv);
            swprintf(s,16,L"%d",cv);
            SetWindowTextW(h_cval,s);
        }
        return 0;
    case WM_TX_FINISHED:
        EnableWindow(h_tx,TRUE);
        if (wp==1) {
            g_mode=0;
            mode_ui(h_stop);
            status_id(GUI_S_STATUS_STOPPED);
        } else if (wp==2) {
            g_mode=2;
            mode_ui(h_rx);
            start_rx();
        }
        return 0;
    case WM_COMMAND:
        if ((HWND)lp==h_text && HIWORD(wp)==EN_CHANGE) {
            if (!g_coloring) doc_changed();
            color_tx_text();
        } else if ((HWND)lp==h_freq && HIWORD(wp)==EN_CHANGE) {
            SetTimer(h,2,350,0);
        } else if ((HWND)lp==h_freq && HIWORD(wp)==EN_KILLFOCUS) {
            commit_freq();
        } else if ((HWND)lp==h_mode && HIWORD(wp)==CBN_SELCHANGE) {
            g_mode_index=(int)SendMessageW(h_mode,CB_GETCURSEL,0,0);
            sync_bw();
            g_freq_gen++;
            InvalidateRect(h,0,FALSE);
        } else if ((HWND)lp==h_lang && HIWORD(wp)==CBN_SELCHANGE) {
            int i=(int)SendMessageW(h_lang,CB_GETCURSEL,0,0);
            if (i>=0 && i<GUI_LANG_COUNT) {
                g_lang=i;
                save_ui_language();
                apply_ui_language();
            }
        } else if ((HWND)lp==h_in && HIWORD(wp)==CBN_SELCHANGE) {
            g_in_device=combo_dev(h_in);
            g_in_dirty=1;
            if (!g_rx_on) status_id(GUI_S_STATUS_INPUT_SELECTED);
        } else if ((HWND)lp==h_out && HIWORD(wp)==CBN_SELCHANGE) {
            g_out_device=combo_dev(h_out);
            g_out_dirty=1;
            if (!g_tx_on) status_id(GUI_S_STATUS_OUTPUT_SELECTED);
        } else if (LOWORD(wp)==ID_TX) {
            sync_bw();
            mode_ui(h_tx);
            g_mode=1;
            if (!g_tx_on) {
                g_tx_stop=0;
                g_tx_next_mode=0;
                g_sent_chars=0;
                g_sent_cap=0x7fffffff;
                free(g_doc_snap);
                g_doc_snap=get_utf8(h_text);
                InterlockedIncrement(&g_tx_rev);
                color_tx_text();
                g_rx_on=0;
                g_tx_on=1;
                status_id(GUI_S_STATUS_TX);
                CreateThread(0,0,tx_thread,0,0,0);
            }
        } else if (LOWORD(wp)==ID_STOP) {
            if (g_tx_on) {
                SetWindowTextW(h_text,L"");
                g_tx_stop=1;
                g_tx_next_mode=1;
                mode_ui(h_tx);
                status_id(GUI_S_STATUS_END);
            } else {
                g_rx_on=0;
                g_mode=0;
                mode_ui(h_stop);
                status_id(GUI_S_STATUS_STOPPED);
            }
        } else if (LOWORD(wp)==ID_RX) {
            sync_bw();
            if (g_tx_on) {
                SetWindowTextW(h_text,L"");
                g_tx_stop=1;
                g_tx_next_mode=2;
                mode_ui(h_tx);
                status_id(GUI_S_STATUS_END);
            } else {
                g_mode=2;
                mode_ui(h_rx);
                if (!g_rx_on) start_rx();
            }
        } else if (LOWORD(wp)==ID_AUTO) {
            set_auto_contrast(SendMessageW(h_auto,BM_GETCHECK,0,0)==BST_CHECKED);
        }
        return 0;
    case WM_LBUTTONDOWN: {
        int x=(short)LOWORD(lp),y=(short)HIWORD(lp);
        if (abs(y-g_split1)<=ui_px(SPLIT_HIT)) {
            g_drag=1;
            SetCapture(h);
        } else if (abs(y-g_split2)<=ui_px(SPLIT_HIT)) {
            g_drag=2;
            SetCapture(h);
        } else if (y>=g_wf_y && y<=g_wf_y+g_wf_h && x>=g_pane_x && x<=g_pane_x+g_pane_w) {
            int center=(int)g_freq,bw=mode_bw_i();
            Mode *m=gui_mode();
            double half=m->fsk?m->shift/2.0:bw/2.0;
            int x0=g_pane_x+(int)lround((center-half)*g_pane_w/3000);
            int x1=g_pane_x+(int)lround((center+half)*g_pane_w/3000);
            int f;
            if (x>=x0-ui_px(6) && x<=x1+ui_px(6)) {
                g_drag=3;
                g_drag_x0=x;
                g_drag_freq0=center;
                SetCapture(h);
                return 0;
            }
            f=(x-g_pane_x)*3000/g_pane_w;
            if (f<half) f=(int)lround(half);
            if (f>3000-half) f=(int)lround(3000-half);
            set_freq(f);
        }
        return 0;
    }
    case WM_MOUSEMOVE: {
        int x=(short)LOWORD(lp),y=(short)HIWORD(lp);
        if (g_drag==3) {
            int f=g_drag_freq0+(x-g_drag_x0)*3000/g_pane_w;
            Mode *m=gui_mode();
            double half=m->fsk?m->shift/2.0:mode_bw_i()/2.0;
            if (f<half) f=(int)lround(half);
            if (f>3000-half) f=(int)lround(3000-half);
            set_freq(f);
        } else if (g_drag==1) {
            int rem=g_rx_h+g_txt_h,rx=y-ui_px(UI_TOP);
            if (rx<ui_px(RX_MIN)) rx=ui_px(RX_MIN);
            if (rx>rem-ui_px(TXT_MIN)) rx=rem-ui_px(TXT_MIN);
            g_rx_ratio=(double)rx/rem;
            layout_ui();
        } else if (g_drag==2) {
            int wf=g_pane_bottom-(y+ui_px(UI_GAP)/2);
            int rem,rx,maxwf=g_pane_avail-ui_px(RX_MIN)-ui_px(TXT_MIN);
            if (wf<ui_px(WF_MIN)) wf=ui_px(WF_MIN);
            if (wf>maxwf) wf=maxwf;
            rem=g_pane_avail-wf;
            rx=g_rx_h;
            if (rx>rem-ui_px(TXT_MIN)) rx=rem-ui_px(TXT_MIN);
            if (rx<ui_px(RX_MIN)) rx=ui_px(RX_MIN);
            g_rx_ratio=(double)rx/rem;
            g_wf_h=wf;
            layout_ui();
        } else if (abs(y-g_split1)<=ui_px(SPLIT_HIT) || abs(y-g_split2)<=ui_px(SPLIT_HIT)) {
            SetCursor(LoadCursor(0,IDC_SIZENS));
        }
        return 0;
    }
    case WM_LBUTTONUP:
        if (g_drag) {
            g_drag=0;
            ReleaseCapture();
            InvalidateRect(h,0,FALSE);
        }
        return 0;
    case WM_SETCURSOR: {
        POINT p;
        if (LOWORD(lp)==HTCLIENT) {
            GetCursorPos(&p);
            ScreenToClient(h,&p);
            if (abs(p.y-g_split1)<=ui_px(SPLIT_HIT) || abs(p.y-g_split2)<=ui_px(SPLIT_HIT)) {
                SetCursor(LoadCursor(0,IDC_SIZENS));
                return TRUE;
            }
            if (p.y>=g_wf_y && p.y<=g_wf_y+g_wf_h) {
                int center=(int)g_freq,bw=mode_bw_i();
                Mode *m=gui_mode();
                double half=m->fsk?m->shift/2.0:bw/2.0;
                int x0=g_pane_x+(int)lround((center-half)*g_pane_w/3000);
                int x1=g_pane_x+(int)lround((center+half)*g_pane_w/3000);
                if (p.x>=x0-ui_px(6) && p.x<=x1+ui_px(6)) {
                    SetCursor(LoadCursor(0,IDC_SIZEWE));
                    return TRUE;
                }
            }
        }
        break;
    }
    case WM_RBUTTONDOWN: {
        int x=(short)LOWORD(lp),y=(short)HIWORD(lp);
        if (x>=g_pane_x && x<=g_pane_x+g_pane_w && y>=g_rx_y && y<=g_rx_y+g_rx_h)
            clear_rx();
        return 0;
    }
    case WM_DESTROY:
        KillTimer(h,1);
        KillTimer(h,2);
        g_rx_on=0;
        g_tx_on=0;
        g_tx_stop=1;
        if (g_rx_thread) {
            WaitForSingleObject(g_rx_thread,3000);
            CloseHandle(g_rx_thread);
            g_rx_thread=0;
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h,msg,wp,lp);
}

static void gui_cwd(void) {
    wchar_t path[MAX_PATH];
    DWORD n=GetModuleFileNameW(0,path,MAX_PATH);
    if (!n || n>=MAX_PATH) return;
    while (n>0 && path[n-1]!='\\' && path[n-1]!='/') n--;
    path[n]=0;
    if (path[0]) SetCurrentDirectoryW(path);
}

static int gui_main(HINSTANCE inst,int show) {
    enable_dpi_awareness();
    gui_cwd();
    load_ui_language();
    WNDCLASSEXW wc;
    LoadLibraryW(L"Msftedit.dll");
    InitCommonControls();
    memset(&wc,0,sizeof(wc));
    wc.cbSize=sizeof(wc);
    wc.lpfnWndProc=wndproc;
    wc.hInstance=inst;
    wc.hCursor=LoadCursor(0,IDC_ARROW);
    wc.hIcon=(HICON)LoadImageW(inst,MAKEINTRESOURCEW(IDI_APPICON),
        IMAGE_ICON,0,0,LR_DEFAULTSIZE|LR_SHARED);
    wc.hIconSm=(HICON)LoadImageW(inst,MAKEINTRESOURCEW(IDI_APPICON),
        IMAGE_ICON,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_SHARED);
    wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
    wc.lpszClassName=L"UniHELLGUI";
    RegisterClassExW(&wc);
    g_fontpath=default_font();
    InitializeCriticalSection(&g_cs);
    g_editbrush=CreateSolidBrush(RGB(224,242,255));
    hwnd=CreateWindowExW(0,L"UniHELLGUI",L"UniHELL " UNIHELL_VERSION_W L" - Hellschreiber",
        WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,ui_px(100),ui_px(40),ui_px(1100),ui_px(800),0,0,inst,0);
    if (!hwnd) return 0;
    int dpi=window_dpi(hwnd);
    if (dpi>=96 && dpi<=480) g_dpi=dpi;
    SetWindowPos(hwnd,0,ui_px(100),ui_px(40),ui_px(1100),ui_px(800),
        SWP_NOZORDER|SWP_NOACTIVATE);
    h_text=CreateWindowExW(WS_EX_CLIENTEDGE,MSFTEDIT_CLASS,L"",WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN,0,0,100,60,hwnd,0,inst,0);
    g_oldtext=(WNDPROC)SetWindowLongPtrW(h_text,GWLP_WNDPROC,(LONG_PTR)text_proc);
    SendMessageW(h_text,EM_SETEVENTMASK,0,ENM_CHANGE);
    SendMessageW(h_text,EM_SETBKGNDCOLOR,0,RGB(224,242,255));
    CHARFORMAT2W cf;
    memset(&cf,0,sizeof(cf));
    cf.cbSize=sizeof(cf);
    cf.dwMask=CFM_COLOR;
    cf.crTextColor=RGB(0,0,0);
    SendMessageW(h_text,EM_SETCHARFORMAT,SCF_ALL,(LPARAM)&cf);
    h_mode=CreateWindowExW(0,L"COMBOBOX",0,WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL,0,0,100,200,hwnd,0,inst,0);
    for (int i=0;i<(int)(sizeof(modes)/sizeof(modes[0]));i++) {
        wchar_t w[32];
        MultiByteToWideChar(CP_UTF8,0,modes[i].name,-1,w,32);
        SendMessageW(h_mode,CB_ADDSTRING,0,(LPARAM)w);
    }
    SendMessageW(h_mode,CB_SETCURSEL,0,0);
    g_mode_index=0;
    h_freq=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"1500",WS_CHILD|WS_VISIBLE|ES_NUMBER,0,0,60,26,hwnd,0,inst,0);
    g_oldfreq=(WNDPROC)SetWindowLongPtrW(h_freq,GWLP_WNDPROC,(LONG_PTR)freq_proc);
    h_bw=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"331",WS_CHILD|WS_VISIBLE|ES_NUMBER|ES_READONLY,0,0,50,26,hwnd,0,inst,0);
    h_tx=CreateWindowW(L"BUTTON",T(GUI_S_TX),WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX|BS_PUSHLIKE,0,0,160,28,hwnd,(HMENU)ID_TX,inst,0);
    h_stop=CreateWindowW(L"BUTTON",T(GUI_S_STOP),WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX|BS_PUSHLIKE,0,0,70,28,hwnd,(HMENU)ID_STOP,inst,0);
    h_rx=CreateWindowW(L"BUTTON",T(GUI_S_RX),WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX|BS_PUSHLIKE,0,0,90,28,hwnd,(HMENU)ID_RX,inst,0);
    mode_ui(h_stop);
    h_status=CreateWindowExW(0,L"STATIC",T(GUI_S_READY),WS_CHILD|WS_VISIBLE,0,0,350,24,hwnd,0,inst,0);
    h_lmode=CreateWindowW(L"STATIC",T(GUI_S_MODE),WS_CHILD|WS_VISIBLE,0,0,50,20,hwnd,0,inst,0);
    h_lfreq=CreateWindowW(L"STATIC",T(GUI_S_FREQ),WS_CHILD|WS_VISIBLE,0,0,50,20,hwnd,0,inst,0);
    h_lbw=CreateWindowW(L"STATIC",T(GUI_S_BW),WS_CHILD|WS_VISIBLE,0,0,50,20,hwnd,0,inst,0);
    h_in=CreateWindowExW(0,L"COMBOBOX",0,WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL,0,0,230,200,hwnd,0,inst,0);
    h_out=CreateWindowExW(0,L"COMBOBOX",0,WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL,0,0,230,200,hwnd,0,inst,0);
    h_llang=CreateWindowW(L"STATIC",T(GUI_S_LANGUAGE),WS_CHILD|WS_VISIBLE,0,0,150,20,hwnd,0,inst,0);
    h_lang=CreateWindowExW(0,L"COMBOBOX",0,WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL,0,0,150,200,hwnd,(HMENU)ID_LANG,inst,0);
    for (int i=0;i<GUI_LANG_COUNT;i++)
        SendMessageW(h_lang,CB_ADDSTRING,0,(LPARAM)gui_lang_names[i]);
    SendMessageW(h_lang,CB_SETCURSEL,g_lang,0);
    h_sql=CreateWindowExW(0,TRACKBAR_CLASSW,L"",WS_CHILD|WS_VISIBLE|TBS_HORZ|TBS_NOTICKS,0,0,150,28,hwnd,(HMENU)ID_SQL,inst,0);
    SendMessageW(h_sql,TBM_SETRANGE,TRUE,MAKELPARAM(0,100));
    SendMessageW(h_sql,TBM_SETPOS,TRUE,0);
    h_sqlval=CreateWindowW(L"STATIC",L"0",WS_CHILD|WS_VISIBLE|SS_CENTER,0,0,36,22,hwnd,0,inst,0);
    h_contrast=CreateWindowExW(0,TRACKBAR_CLASSW,L"",WS_CHILD|WS_VISIBLE|TBS_HORZ|TBS_NOTICKS,0,0,150,28,hwnd,(HMENU)ID_CONTRAST,inst,0);
    SendMessageW(h_contrast,TBM_SETRANGE,TRUE,MAKELPARAM(0,100));
    SendMessageW(h_contrast,TBM_SETPOS,TRUE,50);
    h_cval=CreateWindowW(L"STATIC",L"50",WS_CHILD|WS_VISIBLE|SS_CENTER,0,0,36,22,hwnd,0,inst,0);
    h_auto=CreateWindowW(L"BUTTON",T(GUI_S_AUTO),WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX|BS_PUSHLIKE,0,0,110,28,hwnd,(HMENU)ID_AUTO,inst,0);
    SendMessageW(h_auto,BM_SETCHECK,BST_CHECKED,0);
    h_phase=CreateWindowExW(0,TRACKBAR_CLASSW,L"",WS_CHILD|WS_VISIBLE|TBS_HORZ|TBS_NOTICKS,0,0,150,28,hwnd,(HMENU)ID_PHASE,inst,0);
    SendMessageW(h_phase,TBM_SETRANGE,TRUE,MAKELPARAM(0,100));
    SendMessageW(h_phase,TBM_SETPOS,TRUE,50);
    h_phaseval=CreateWindowW(L"STATIC",L"50",WS_CHILD|WS_VISIBLE|SS_CENTER,0,0,36,22,hwnd,0,inst,0);
    h_rate=CreateWindowExW(0,TRACKBAR_CLASSW,L"",WS_CHILD|WS_VISIBLE|TBS_HORZ|TBS_NOTICKS,0,0,150,28,hwnd,(HMENU)ID_RATE,inst,0);
    SendMessageW(h_rate,TBM_SETRANGE,TRUE,MAKELPARAM(950,1050));
    SendMessageW(h_rate,TBM_SETPOS,TRUE,1000);
    h_rateval=CreateWindowW(L"STATIC",L"1000",WS_CHILD|WS_VISIBLE|SS_CENTER,0,0,44,22,hwnd,0,inst,0);
    h_lin=CreateWindowW(L"STATIC",T(GUI_S_INPUT),WS_CHILD|WS_VISIBLE,0,0,80,20,hwnd,0,inst,0);
    h_lout=CreateWindowW(L"STATIC",T(GUI_S_OUTPUT),WS_CHILD|WS_VISIBLE,0,0,80,20,hwnd,0,inst,0);
    h_lsql=CreateWindowW(L"STATIC",T(GUI_S_SQL),WS_CHILD|WS_VISIBLE,0,0,40,20,hwnd,0,inst,0);
    h_lcontrast=CreateWindowW(L"STATIC",T(GUI_S_WATERFALL),WS_CHILD|WS_VISIBLE,0,0,60,20,hwnd,0,inst,0);
    h_lphase=CreateWindowW(L"STATIC",T(GUI_S_PHASE),WS_CHILD|WS_VISIBLE,0,0,50,20,hwnd,0,inst,0);
    h_lrate=CreateWindowW(L"STATIC",T(GUI_S_RATE),WS_CHILD|WS_VISIBLE,0,0,50,20,hwnd,0,inst,0);
    refresh_ui_fonts();
    sync_bw();
    layout_ui();
    apply_ui_language();
    SetTimer(hwnd,1,250,0);
    ShowWindow(hwnd,show);
    UpdateWindow(hwnd);
    MSG msg;
    while (GetMessageW(&msg,0,0,0)>0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}

int main(int argc,char **argv) {
    argv=utf8_args(&argc);
    if (argc>1) return run(argc,argv);
    FreeConsole();
    return gui_main(GetModuleHandleW(0),SW_SHOW);
}
