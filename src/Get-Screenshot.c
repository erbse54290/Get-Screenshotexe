/*
 * Get-Screenshot.exe – von Grok erstellt
 *
 * Portable Screenshot-EXE. Kein PowerShell, kein Setup, keine Extra-DLLs.
 * Von aussen dasselbe wie die Funktion Get-Screenshot:
 *   Bereich ab (OffsetLeft, OffsetTop), Breite B, Hoehe H, Speichern als PNG.
 *   Dateiname: <Speicherort>\<Name>_ddMMyy_HHmmss.png
 *
 * Aufruf:
 *   Get-Screenshot.exe -Name HMI -Speicherort C:\Temp -OffsetLeft 0 -OffsetTop 0 -H 1080 -B 1920
 *
 * Ohne Parameter erscheint ein Infofenster. Es wird dann kein Screenshot gespeichert.
 */

#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <wincodec.h>
#include <shlobj.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "grok_logo.h"

#define MAX_W 16384
#define MAX_H 16384

static int g_can_print = 0;
static HANDLE g_out = NULL;
static HANDLE g_err = NULL;

/* Text ausgeben: Konsole (Unicode) oder umgeleitete Datei/Pipe (UTF-8). */
static void emit_handle(HANDLE handle, const wchar_t *text) {
    DWORD written = 0;
    DWORD mode = 0;

    if (!text || !handle || handle == INVALID_HANDLE_VALUE) {
        return;
    }

    if (GetConsoleMode(handle, &mode)) {
        WriteConsoleW(handle, text, (DWORD)wcslen(text), &written, NULL);
        WriteConsoleW(handle, L"\r\n", 2, &written, NULL);
        return;
    }

    {
        int bytes = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
        char *utf8;
        if (bytes <= 1) {
            return;
        }
        utf8 = (char *)malloc((size_t)bytes + 2);
        if (!utf8) {
            return;
        }
        WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8, bytes, NULL, NULL);
        WriteFile(handle, utf8, (DWORD)strlen(utf8), &written, NULL);
        WriteFile(handle, "\r\n", 2, &written, NULL);
        free(utf8);
    }
}

static void emit_out(const wchar_t *text) {
    emit_handle(g_out, text);
}

/* Ohne Konsole nur eine Meldung zeigen, wenn wirklich ein Desktop da ist.
   Sonst wuerde MessageBox eine geplante Aufgabe ohne Anmeldung blockieren. */
static int desktop_visible(void) {
    HWINSTA station = GetProcessWindowStation();
    USEROBJECTFLAGS flags;

    if (!station) {
        return 0;
    }
    if (!GetUserObjectInformationW(station, UOI_FLAGS, &flags, sizeof(flags), NULL)) {
        return 0;
    }
    return (flags.dwFlags & WSF_VISIBLE) != 0;
}

static void emit_err(const wchar_t *text) {
    if (g_can_print) {
        emit_handle(g_err ? g_err : g_out, text);
        return;
    }
    if (desktop_visible()) {
        MessageBoxW(NULL, text, L"Get-Screenshot", MB_ICONERROR | MB_OK);
    }
}

static void init_output(void) {
    HANDLE existing = GetStdHandle(STD_OUTPUT_HANDLE);

    if (existing && existing != INVALID_HANDLE_VALUE) {
        g_out = existing;
        g_err = GetStdHandle(STD_ERROR_HANDLE);
        g_can_print = 1;
        return;
    }

    /* GUI-EXE hat von cmd aus oft noch kein Standard-Handle. An die Elternkonsole haengen. */
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        g_out = CreateFileW(L"CONOUT$", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL, OPEN_EXISTING, 0, NULL);
        g_err = g_out;
        if (g_out && g_out != INVALID_HANDLE_VALUE) {
            SetStdHandle(STD_OUTPUT_HANDLE, g_out);
            SetStdHandle(STD_ERROR_HANDLE, g_err);
            g_can_print = 1;
        }
    }
}

static const wchar_t *info_text(void) {
    return
        L"Get-Screenshot\r\n"
        L"\r\n"
        L"Fotografiert ein Rechteck des Bildschirms und speichert es als PNG.\r\n"
        L"Kein PowerShell, kein Setup, kein .NET.\r\n"
        L"\r\n"
        L"Ohne Parameter wird kein Bild gespeichert.\r\n"
        L"\r\n"
        L"Aufruf:\r\n"
        L"  Get-Screenshot.exe -Name Name -Speicherort Ordner\r\n"
        L"                     -OffsetLeft X -OffsetTop Y -B Breite -H Hoehe\r\n"
        L"\r\n"
        L"Parameter:\r\n"
        L"  -Name         Anfang des Dateinamens\r\n"
        L"  -Speicherort  Zielordner. Standard: Eigene Dokumente\r\n"
        L"  -OffsetLeft   Start von links, Pixel. Standard: 0\r\n"
        L"  -OffsetTop    Start von oben, Pixel. Standard: 0\r\n"
        L"  -B            Breite in Pixel. Standard: 1920\r\n"
        L"  -H            Hoehe in Pixel. Standard: 1080\r\n"
        L"  -?            diese Hilfe\r\n"
        L"\r\n"
        L"Dateiname: Ordner\\Name_TTMMJJ_HHMMSS.png\r\n"
        L"Beispiel:  C:\\Temp\\HMI_091126_083415.png\r\n"
        L"Punkte im Namen werden zu Unterstrichen.\r\n"
        L"\r\n"
        L"Der Nullpunkt ist die linke obere Ecke des Hauptbildschirms.\r\n"
        L"Ein Monitor links davon hat einen negativen OffsetLeft.\r\n"
        L"\r\n"
        L"Beispiel:\r\n"
        L"  Get-Screenshot.exe -Name HMI -Speicherort C:\\Temp -B 1920 -H 1080";
}

static void release_unknown(IUnknown *punk);

typedef BOOL (WINAPI *AlphaBlendFn)(HDC, int, int, int, int, HDC, int, int, int, int, BLENDFUNCTION);

typedef struct HelpView {
    const wchar_t *text;
    HBITMAP bitmap;
    int src_w;
    int src_h;
    int logo_w;
    int logo_h;
    int dpi;
    HFONT font;
    HWND edit;
    HWND button;
    AlphaBlendFn alpha_blend;
} HelpView;

static int dip(const HelpView *view, int px) {
    return MulDiv(px, view->dpi, 96);
}

/* Dunkle Zeichnung auf hellem Fenster, helle Zeichnung auf dunklem Fenster. */
static int background_is_dark(void) {
    DWORD bg = GetSysColor(COLOR_WINDOW);
    int r = GetRValue(bg);
    int g = GetGValue(bg);
    int b = GetBValue(bg);

    return (r * 299 + g * 587 + b * 114) / 1000 < 140;
}

static HBITMAP load_logo_bitmap(const unsigned char *data, unsigned int len, int *out_w, int *out_h) {
    IWICImagingFactory *factory = NULL;
    IWICStream *stream = NULL;
    IWICBitmapDecoder *decoder = NULL;
    IWICBitmapFrameDecode *frame = NULL;
    IWICFormatConverter *converter = NULL;
    HBITMAP bitmap = NULL;
    void *bits = NULL;
    UINT width = 0;
    UINT height = 0;
    HRESULT hr;
    BITMAPINFO info;

    *out_w = 0;
    *out_h = 0;

    hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IWICImagingFactory, (void **)&factory);
    if (FAILED(hr)) {
        return NULL;
    }

    hr = IWICImagingFactory_CreateStream(factory, &stream);
    if (SUCCEEDED(hr)) {
        hr = IWICStream_InitializeFromMemory(stream, (BYTE *)data, len);
    }
    if (SUCCEEDED(hr)) {
        hr = IWICImagingFactory_CreateDecoderFromStream(
            factory, (IStream *)stream, NULL, WICDecodeMetadataCacheOnLoad, &decoder);
    }
    if (SUCCEEDED(hr)) {
        hr = IWICBitmapDecoder_GetFrame(decoder, 0, &frame);
    }
    if (SUCCEEDED(hr)) {
        hr = IWICImagingFactory_CreateFormatConverter(factory, &converter);
    }
    if (SUCCEEDED(hr)) {
        hr = IWICFormatConverter_Initialize(
            converter, (IWICBitmapSource *)frame, &GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom);
    }
    if (SUCCEEDED(hr)) {
        hr = IWICBitmapSource_GetSize((IWICBitmapSource *)converter, &width, &height);
    }
    if (FAILED(hr) || width == 0 || height == 0 || width > 8192 || height > 8192) {
        goto cleanup;
    }

    ZeroMemory(&info, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = (LONG)width;
    info.bmiHeader.biHeight = -(LONG)height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(NULL, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bitmap || !bits) {
        bitmap = NULL;
        goto cleanup;
    }

    hr = IWICBitmapSource_CopyPixels((IWICBitmapSource *)converter, NULL,
                                     width * 4u, width * height * 4u, (BYTE *)bits);
    if (FAILED(hr)) {
        DeleteObject(bitmap);
        bitmap = NULL;
        goto cleanup;
    }

    *out_w = (int)width;
    *out_h = (int)height;

cleanup:
    release_unknown((IUnknown *)converter);
    release_unknown((IUnknown *)frame);
    release_unknown((IUnknown *)decoder);
    release_unknown((IUnknown *)stream);
    release_unknown((IUnknown *)factory);
    return bitmap;
}

static void paint_logo(const HelpView *view, HDC hdc, int x, int y) {
    HDC mem;
    HGDIOBJ old;
    BLENDFUNCTION blend;

    if (!view->bitmap || view->logo_w < 1 || view->logo_h < 1) {
        return;
    }

    mem = CreateCompatibleDC(hdc);
    if (!mem) {
        return;
    }
    old = SelectObject(mem, view->bitmap);
    SetStretchBltMode(hdc, HALFTONE);
    SetBrushOrgEx(hdc, 0, 0, NULL);
    blend.BlendOp = AC_SRC_OVER;
    blend.BlendFlags = 0;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    if (!view->alpha_blend ||
        !view->alpha_blend(hdc, x, y, view->logo_w, view->logo_h, mem, 0, 0,
                           view->src_w, view->src_h, blend)) {
        StretchBlt(hdc, x, y, view->logo_w, view->logo_h, mem, 0, 0,
                   view->src_w, view->src_h, SRCCOPY);
    }
    SelectObject(mem, old);
    DeleteDC(mem);
}

static void layout_help(HWND hwnd, HelpView *view) {
    RECT rc;
    int pad = dip(view, 16);
    int top = dip(view, 12) + view->logo_h + dip(view, 8);
    int button_h = dip(view, 28);
    int button_w = dip(view, 96);

    GetClientRect(hwnd, &rc);
    if (view->edit) {
        MoveWindow(view->edit, pad, top, rc.right - pad * 2,
                   rc.bottom - top - button_h - pad * 2, TRUE);
    }
    if (view->button) {
        MoveWindow(view->button, (rc.right - button_w) / 2,
                   rc.bottom - button_h - pad, button_w, button_h, TRUE);
    }
}

static LRESULT CALLBACK help_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    HelpView *view = (HelpView *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCTW *created = (CREATESTRUCTW *)lp;
        view = (HelpView *)created->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)view);
        view->font = CreateFontW(-dip(view, 15), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        view->edit = CreateWindowExW(
            0, L"EDIT", view->text,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL,
            0, 0, 10, 10, hwnd, (HMENU)100, GetModuleHandleW(NULL), NULL);
        view->button = CreateWindowExW(
            0, L"BUTTON", L"OK",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            0, 0, 10, 10, hwnd, (HMENU)IDOK, GetModuleHandleW(NULL), NULL);
        if (view->font) {
            SendMessageW(view->edit, WM_SETFONT, (WPARAM)view->font, TRUE);
            SendMessageW(view->button, WM_SETFONT, (WPARAM)view->font, TRUE);
        }
        SendMessageW(view->edit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
                     MAKELONG(dip(view, 8), dip(view, 8)));
        layout_help(hwnd, view);
        return 0;
    }
    case WM_SIZE:
        if (view) {
            layout_help(hwnd, view);
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, GetSysColorBrush(COLOR_WINDOW));
        if (view && view->bitmap) {
            paint_logo(view, hdc, (rc.right - view->logo_w) / 2, dip(view, 12));
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
        SetBkColor(hdc, GetSysColor(COLOR_WINDOW));
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK) {
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void show_logo_help(const wchar_t *text) {
    HelpView view;
    HRESULT co_hr;
    const unsigned char *png;
    unsigned int png_len;
    MSG msg;
    RECT frame;
    RECT work;
    HWND hwnd;
    HMODULE blend_lib;
    int client_w;
    int client_h;
    int x;
    int y;
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
    DWORD exstyle = WS_EX_DLGMODALFRAME;
    WNDCLASSW wc;

    ZeroMemory(&view, sizeof(view));
    view.text = text;
    view.dpi = 96;

    co_hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(co_hr) && co_hr != RPC_E_CHANGED_MODE) {
        MessageBoxW(NULL, text, L"Get-Screenshot", MB_OK);
        return;
    }

    if (background_is_dark()) {
        png = grok_logo_light_png;
        png_len = grok_logo_light_png_len;
    } else {
        png = grok_logo_dark_png;
        png_len = grok_logo_dark_png_len;
    }
    view.bitmap = load_logo_bitmap(png, png_len, &view.src_w, &view.src_h);
    if (!view.bitmap || view.src_w < 1 || view.src_h < 1) {
        if (view.bitmap) {
            DeleteObject(view.bitmap);
        }
        if (co_hr != RPC_E_CHANGED_MODE) {
            CoUninitialize();
        }
        MessageBoxW(NULL, text, L"Get-Screenshot", MB_ICONINFORMATION | MB_OK);
        return;
    }

    {
        HDC screen = GetDC(NULL);
        if (screen) {
            view.dpi = GetDeviceCaps(screen, LOGPIXELSX);
            ReleaseDC(NULL, screen);
        }
    }
    if (view.dpi < 96) {
        view.dpi = 96;
    }

    blend_lib = LoadLibraryW(L"msimg32.dll");
    if (blend_lib) {
        view.alpha_blend = (AlphaBlendFn)GetProcAddress(blend_lib, "AlphaBlend");
    }

    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    frame.left = 0;
    frame.top = 0;
    frame.right = 0;
    frame.bottom = 0;
    AdjustWindowRectEx(&frame, style, FALSE, exstyle);
    {
        int extra_w = frame.right - frame.left;
        int extra_h = frame.bottom - frame.top;
        int avail_w = (work.right - work.left) - extra_w - dip(&view, 8);
        int avail_h = (work.bottom - work.top) - extra_h - dip(&view, 8);

        if (avail_w < dip(&view, 280)) {
            avail_w = dip(&view, 280);
        }
        if (avail_h < dip(&view, 220)) {
            avail_h = dip(&view, 220);
        }
        client_w = dip(&view, 560);
        client_h = dip(&view, 620);
        if (client_w > avail_w) {
            client_w = avail_w;
        }
        if (client_h > avail_h) {
            client_h = avail_h;
        }
        view.logo_h = dip(&view, 96);
        view.logo_w = view.src_w * view.logo_h / view.src_h;
        if (view.logo_w > client_w - dip(&view, 32)) {
            view.logo_w = client_w - dip(&view, 32);
            if (view.logo_w < 1) {
                view.logo_w = 1;
            }
            view.logo_h = view.src_h * view.logo_w / view.src_w;
        }
    }

    frame.left = 0;
    frame.top = 0;
    frame.right = client_w;
    frame.bottom = client_h;
    AdjustWindowRectEx(&frame, style, FALSE, exstyle);
    x = work.left + ((work.right - work.left) - (frame.right - frame.left)) / 2;
    y = work.top + ((work.bottom - work.top) - (frame.bottom - frame.top)) / 2;
    if (x < work.left) {
        x = work.left;
    }
    if (y < work.top) {
        y = work.top;
    }

    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = help_proc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.lpszClassName = L"GetScreenshotHelp";
    RegisterClassW(&wc);

    hwnd = CreateWindowExW(exstyle, L"GetScreenshotHelp", L"Get-Screenshot", style,
                           x, y, frame.right - frame.left, frame.bottom - frame.top,
                           NULL, NULL, GetModuleHandleW(NULL), &view);
    if (!hwnd) {
        if (view.bitmap) {
            DeleteObject(view.bitmap);
        }
        if (blend_lib) {
            FreeLibrary(blend_lib);
        }
        if (co_hr != RPC_E_CHANGED_MODE) {
            CoUninitialize();
        }
        MessageBoxW(NULL, text, L"Get-Screenshot", MB_ICONINFORMATION | MB_OK);
        return;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            DestroyWindow(hwnd);
            continue;
        }
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (view.font) {
        DeleteObject(view.font);
    }
    if (view.bitmap) {
        DeleteObject(view.bitmap);
    }
    if (blend_lib) {
        FreeLibrary(blend_lib);
    }
    if (co_hr != RPC_E_CHANGED_MODE) {
        CoUninitialize();
    }
}

/* Konsole, wenn vorhanden. Sonst das Infofenster mit Logo. */
static void show_help(void) {
    const wchar_t *help = info_text();

    if (g_can_print) {
        emit_out(help);
        return;
    }
    if (desktop_visible()) {
        show_logo_help(help);
    }
}

/* Aufruf ohne Parameter: immer das Fenster, zusaetzlich Text in der Konsole. */
static void show_info_window(void) {
    const wchar_t *help = info_text();

    if (g_can_print) {
        emit_out(help);
    }
    if (desktop_visible()) {
        show_logo_help(help);
    }
}

/* "-Name" / "/Name" / "--Name", Gross/Klein egal. Wert hinter : oder = bleibt in *inline_value. */
static int match_opt(const wchar_t *arg, const wchar_t *name, const wchar_t **inline_value) {
    const wchar_t *p = arg;
    size_t n;

    *inline_value = NULL;
    if (*p == L'-' || *p == L'/') {
        p++;
    }
    if (*p == L'-') {
        p++;
    }

    n = wcslen(name);
    if (_wcsnicmp(p, name, n) != 0) {
        return 0;
    }
    if (p[n] == L'\0') {
        return 1;
    }
    if (p[n] == L':' || p[n] == L'=') {
        *inline_value = p + n + 1;
        return 1;
    }
    return 0;
}

/* cmd laesst einfache Anfuehrungszeichen stehen. Die hier abschneiden. */
static void unquote(wchar_t *dst, size_t dst_count, const wchar_t *src) {
    size_t n;

    if (!src) {
        dst[0] = L'\0';
        return;
    }
    n = wcslen(src);
    if (n >= 2 &&
        ((src[0] == L'\'' && src[n - 1] == L'\'') ||
         (src[0] == L'"' && src[n - 1] == L'"'))) {
        src++;
        n -= 2;
    }
    if (n >= dst_count) {
        n = dst_count - 1;
    }
    if (n > 0) {
        memcpy(dst, src, n * sizeof(wchar_t));
    }
    dst[n] = L'\0';
}

static int parse_int(const wchar_t *text, int *out) {
    wchar_t *end = NULL;
    long value;

    if (!text || !text[0]) {
        return 0;
    }
    value = wcstol(text, &end, 10);
    if (end == text || (end && *end != L'\0')) {
        return 0;
    }
    if (value < -32000 || value > 32000) {
        return 0;
    }
    *out = (int)value;
    return 1;
}

/* Naechster Parameter, oder der Wert direkt hinter -Name:wert */
static const wchar_t *take_value(const wchar_t *inline_value, int argc, wchar_t **argv, int *index) {
    if (inline_value) {
        return inline_value;
    }
    if (*index + 1 >= argc) {
        return NULL;
    }
    *index += 1;
    return argv[*index];
}

/* Punkte wie im PowerShell-Skript, zusaetzlich Zeichen die Windows im Dateinamen verbietet. */
static void sanitize_name(wchar_t *name) {
    static const wchar_t *bad = L"\\/:*?\"<>|";
    wchar_t *p;

    for (p = name; *p; p++) {
        if (*p == L'.' || wcschr(bad, *p)) {
            *p = L'_';
        }
    }
}

static void enable_dpi_aware(void) {
    typedef BOOL (WINAPI *SetCtxFn)(HANDLE);
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    SetCtxFn set_ctx = NULL;

    if (user32) {
        set_ctx = (SetCtxFn)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
    }
    /* -4 = PER_MONITOR_AWARE_V2, damit 1920x1080 echte Pixel sind und nicht skaliert. */
    if (!set_ctx || !set_ctx((HANDLE)(intptr_t)-4)) {
        SetProcessDPIAware();
    }
}

static int capture_screen(int x, int y, int width, int height, BYTE **pixels_out) {
    HDC screen = NULL;
    HDC memory = NULL;
    HBITMAP bitmap = NULL;
    HGDIOBJ previous = NULL;
    BITMAPINFO info;
    BYTE *pixels = NULL;
    size_t bytes;
    int ok = 0;

    *pixels_out = NULL;
    bytes = (size_t)width * (size_t)height * 4u;
    pixels = (BYTE *)malloc(bytes);
    if (!pixels) {
        emit_err(L"Nicht genug Speicher fuer den Screenshot.");
        return 0;
    }

    screen = GetDC(NULL);
    if (!screen) {
        emit_err(L"Bildschirm-DC konnte nicht geoeffnet werden.");
        free(pixels);
        return 0;
    }

    memory = CreateCompatibleDC(screen);
    bitmap = CreateCompatibleBitmap(screen, width, height);
    if (!memory || !bitmap) {
        emit_err(L"Bitmap konnte nicht angelegt werden.");
        goto cleanup;
    }

    previous = SelectObject(memory, bitmap);
    /* CAPTUREBLT nimmt auch Fenster mit, die per Layered-Window gezeichnet sind.
       Klappt das nicht (manche RDP-Sitzungen), normal noch einmal. */
    if (!BitBlt(memory, 0, 0, width, height, screen, x, y, SRCCOPY | CAPTUREBLT)) {
        if (!BitBlt(memory, 0, 0, width, height, screen, x, y, SRCCOPY)) {
            emit_err(L"Bildschirm konnte nicht kopiert werden.");
            goto cleanup;
        }
    }

    ZeroMemory(&info, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height; /* negativ = erste Zeile ist oben */
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    if (!GetDIBits(memory, bitmap, 0, (UINT)height, pixels, &info, DIB_RGB_COLORS)) {
        emit_err(L"Pixel konnten nicht gelesen werden.");
        goto cleanup;
    }

    /* Alpha-Byte aus dem DIB ist 0. Auf 255 setzen, sonst wird das PNG unsichtbar. */
    {
        size_t i;
        for (i = 3; i < bytes; i += 4) {
            pixels[i] = 255;
        }
    }

    *pixels_out = pixels;
    pixels = NULL;
    ok = 1;

cleanup:
    if (previous && memory) {
        SelectObject(memory, previous);
    }
    if (bitmap) {
        DeleteObject(bitmap);
    }
    if (memory) {
        DeleteDC(memory);
    }
    if (screen) {
        ReleaseDC(NULL, screen);
    }
    free(pixels);
    return ok;
}

static void release_unknown(IUnknown *punk) {
    if (punk) {
        punk->lpVtbl->Release(punk);
    }
}

static int save_png(const wchar_t *path, int width, int height, BYTE *pixels) {
    IWICImagingFactory *factory = NULL;
    IWICBitmap *bitmap = NULL;
    IWICStream *stream = NULL;
    IWICBitmapEncoder *encoder = NULL;
    IWICBitmapFrameEncode *frame = NULL;
    IPropertyBag2 *bag = NULL;
    HRESULT hr;
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    UINT stride = (UINT)width * 4u;
    UINT size = stride * (UINT)height;
    int ok = 0;

    hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IWICImagingFactory, (void **)&factory);
    if (FAILED(hr)) {
        emit_err(L"Windows Imaging (WIC) ist nicht verfuegbar.");
        return 0;
    }

    hr = IWICImagingFactory_CreateBitmapFromMemory(
        factory, (UINT)width, (UINT)height, &GUID_WICPixelFormat32bppBGRA,
        stride, size, pixels, &bitmap);
    if (FAILED(hr)) {
        emit_err(L"Bitmap konnte nicht erzeugt werden.");
        goto cleanup;
    }

    hr = IWICImagingFactory_CreateStream(factory, &stream);
    if (SUCCEEDED(hr)) {
        hr = IWICStream_InitializeFromFilename(stream, path, GENERIC_WRITE);
    }
    if (FAILED(hr)) {
        emit_err(L"Zieldatei konnte nicht angelegt werden. Ordner und Rechte pruefen.");
        goto cleanup;
    }

    hr = IWICImagingFactory_CreateEncoder(factory, &GUID_ContainerFormatPng, NULL, &encoder);
    if (SUCCEEDED(hr)) {
        hr = IWICBitmapEncoder_Initialize(encoder, (IStream *)stream, WICBitmapEncoderNoCache);
    }
    if (FAILED(hr)) {
        emit_err(L"PNG-Encoder konnte nicht gestartet werden.");
        goto cleanup;
    }

    hr = IWICBitmapEncoder_CreateNewFrame(encoder, &frame, &bag);
    if (SUCCEEDED(hr)) {
        hr = IWICBitmapFrameEncode_Initialize(frame, NULL);
    }
    if (SUCCEEDED(hr)) {
        hr = IWICBitmapFrameEncode_SetSize(frame, (UINT)width, (UINT)height);
    }
    if (SUCCEEDED(hr)) {
        hr = IWICBitmapFrameEncode_SetPixelFormat(frame, &format);
    }
    if (SUCCEEDED(hr)) {
        hr = IWICBitmapFrameEncode_WriteSource(frame, (IWICBitmapSource *)bitmap, NULL);
    }
    if (SUCCEEDED(hr)) {
        hr = IWICBitmapFrameEncode_Commit(frame);
    }
    if (SUCCEEDED(hr)) {
        hr = IWICBitmapEncoder_Commit(encoder);
    }
    if (FAILED(hr)) {
        emit_err(L"PNG konnte nicht geschrieben werden.");
        goto cleanup;
    }

    ok = 1;

cleanup:
    release_unknown((IUnknown *)frame);
    release_unknown((IUnknown *)bag);
    release_unknown((IUnknown *)encoder);
    release_unknown((IUnknown *)stream);
    release_unknown((IUnknown *)bitmap);
    release_unknown((IUnknown *)factory);
    return ok;
}

static int build_path(const wchar_t *folder, const wchar_t *name, wchar_t *path, size_t path_count) {
    SYSTEMTIME now;
    wchar_t stamp[32];
    size_t folder_len;
    const wchar_t *sep = L"\\";
    int written;

    GetLocalTime(&now);
    swprintf(stamp, 32, L"%02u%02u%02u_%02u%02u%02u",
             (unsigned)now.wDay, (unsigned)now.wMonth, (unsigned)(now.wYear % 100),
             (unsigned)now.wHour, (unsigned)now.wMinute, (unsigned)now.wSecond);

    folder_len = wcslen(folder);
    if (folder_len > 0 && (folder[folder_len - 1] == L'\\' || folder[folder_len - 1] == L'/')) {
        sep = L"";
    }

    written = swprintf(path, path_count, L"%s%s%s_%s.png", folder, sep, name, stamp);
    if (written < 0 || (size_t)written >= path_count) {
        emit_err(L"Pfad ist zu lang.");
        return 0;
    }
    return 1;
}

int main(void) {
    int argc = 0;
    wchar_t **argv = NULL;
    wchar_t name[260];
    wchar_t folder[MAX_PATH];
    wchar_t path[4096];
    int offset_left = 0;
    int offset_top = 0;
    int height = 1080;
    int width = 1920;
    int i;
    BYTE *pixels = NULL;
    HRESULT co_hr;
    int exit_code = 1;

    name[0] = L'\0';
    folder[0] = L'\0';

    enable_dpi_aware();
    init_output();

    argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) {
        emit_err(L"Kommandozeile konnte nicht gelesen werden.");
        return 1;
    }

    if (argc <= 1) {
        show_info_window();
        LocalFree(argv);
        return 0;
    }

    for (i = 1; i < argc; i++) {
        const wchar_t *inline_value = NULL;
        const wchar_t *value;

        if (match_opt(argv[i], L"?", &inline_value) ||
            match_opt(argv[i], L"help", &inline_value)) {
            show_help();
            LocalFree(argv);
            return 0;
        }

        if (match_opt(argv[i], L"Name", &inline_value)) {
            wchar_t raw[260];
            value = take_value(inline_value, argc, argv, &i);
            if (!value) {
                emit_err(L"-Name braucht einen Wert.");
                goto done;
            }
            unquote(raw, 260, value);
            wcsncpy(name, raw, 259);
            name[259] = L'\0';
            continue;
        }
        if (match_opt(argv[i], L"Speicherort", &inline_value)) {
            value = take_value(inline_value, argc, argv, &i);
            if (!value || !value[0]) {
                emit_err(L"-Speicherort braucht einen Ordner.");
                goto done;
            }
            unquote(folder, MAX_PATH, value);
            if (folder[0] == L'\0') {
                emit_err(L"-Speicherort braucht einen Ordner.");
                goto done;
            }
            continue;
        }
        if (match_opt(argv[i], L"OffsetLeft", &inline_value)) {
            wchar_t raw[64];
            value = take_value(inline_value, argc, argv, &i);
            unquote(raw, 64, value);
            if (!value || !parse_int(raw, &offset_left)) {
                emit_err(L"-OffsetLeft muss eine ganze Zahl sein.");
                goto done;
            }
            continue;
        }
        if (match_opt(argv[i], L"OffsetTop", &inline_value)) {
            wchar_t raw[64];
            value = take_value(inline_value, argc, argv, &i);
            unquote(raw, 64, value);
            if (!value || !parse_int(raw, &offset_top)) {
                emit_err(L"-OffsetTop muss eine ganze Zahl sein.");
                goto done;
            }
            continue;
        }
        if (match_opt(argv[i], L"H", &inline_value)) {
            wchar_t raw[64];
            value = take_value(inline_value, argc, argv, &i);
            unquote(raw, 64, value);
            if (!value || !parse_int(raw, &height)) {
                emit_err(L"-H muss eine ganze Zahl sein.");
                goto done;
            }
            continue;
        }
        if (match_opt(argv[i], L"B", &inline_value)) {
            wchar_t raw[64];
            value = take_value(inline_value, argc, argv, &i);
            unquote(raw, 64, value);
            if (!value || !parse_int(raw, &width)) {
                emit_err(L"-B muss eine ganze Zahl sein.");
                goto done;
            }
            continue;
        }

        emit_err(L"Unbekannter Parameter. -? zeigt die Hilfe.");
        goto done;
    }

    if (width < 1 || height < 1 || width > MAX_W || height > MAX_H) {
        emit_err(L"-B und -H muessen zwischen 1 und 16384 liegen.");
        goto done;
    }

    sanitize_name(name);

    if (folder[0] == L'\0') {
        if (FAILED(SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, 0, folder))) {
            emit_err(L"Eigene Dokumente wurden nicht gefunden. -Speicherort angeben.");
            goto done;
        }
    } else if (GetFileAttributesW(folder) == INVALID_FILE_ATTRIBUTES) {
        if (SHCreateDirectoryExW(NULL, folder, NULL) != ERROR_SUCCESS &&
            GetFileAttributesW(folder) == INVALID_FILE_ATTRIBUTES) {
            emit_err(L"Speicherort existiert nicht und konnte nicht angelegt werden.");
            goto done;
        }
    }

    if (!build_path(folder, name, path, 4096)) {
        goto done;
    }
    if (!capture_screen(offset_left, offset_top, width, height, &pixels)) {
        exit_code = 2;
        goto done;
    }

    co_hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(co_hr) && co_hr != RPC_E_CHANGED_MODE) {
        emit_err(L"COM konnte nicht gestartet werden.");
        exit_code = 3;
        goto done;
    }

    if (!save_png(path, width, height, pixels)) {
        exit_code = 3;
        if (co_hr != RPC_E_CHANGED_MODE) {
            CoUninitialize();
        }
        goto done;
    }

    if (co_hr != RPC_E_CHANGED_MODE) {
        CoUninitialize();
    }

    emit_out(path);
    exit_code = 0;

done:
    free(pixels);
    LocalFree(argv);
    return exit_code;
}
