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

static void show_help(void) {
    static const wchar_t *help =
        L"Get-Screenshot.exe - von Grok erstellt\r\n"
        L"Portable EXE, kein PowerShell, kein Setup.\r\n"
        L"\r\n"
        L"Get-Screenshot.exe [-Name Name] [-Speicherort Ordner]\r\n"
        L"                   [-OffsetLeft X] [-OffsetTop Y] [-H Hoehe] [-B Breite]\r\n"
        L"\r\n"
        L"Standard:\r\n"
        L"  Name         (leer)\r\n"
        L"  Speicherort  Eigene Dokumente\r\n"
        L"  OffsetLeft   0\r\n"
        L"  OffsetTop    0\r\n"
        L"  H            1080\r\n"
        L"  B            1920\r\n"
        L"\r\n"
        L"Datei: <Speicherort>\\<Name>_ddMMyy_HHmmss.png\r\n"
        L"Punkte im Namen werden zu Unterstrichen.\r\n"
        L"\r\n"
        L"Beispiel:\r\n"
        L"  Get-Screenshot.exe -Name HMI -Speicherort C:\\Temp -OffsetLeft 100 -OffsetTop 50 -H 1080 -B 1920";

    if (g_can_print) {
        emit_out(help);
    } else if (desktop_visible()) {
        MessageBoxW(NULL, help, L"Get-Screenshot", MB_OK);
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
        ((src[0] == L'\' && src[n - 1] == L'\'') ||
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
