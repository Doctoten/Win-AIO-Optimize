#include "app.h"
#include "theme.h"
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <uxtheme.h>
#include <stdio.h>
#include <stdarg.h>

HINSTANCE g_instance;
HFONT g_font;
HFONT g_font_semibold;
HFONT g_font_title;
HFONT g_font_subtitle;
BOOL g_vi = TRUE;
static HWND main_window, log_window, progress_window, timer_status;
static HFONT title_font;
static HWND active_page, confirmation_window;
static BOOL dashboard_advanced, closing_main;
static void layout_main(HWND window);
static LONG running_job;
static CRITICAL_SECTION log_lock;
static wchar_t log_path[MAX_PATH];
static BOOL log_ready;
static BOOL smoke_mode;
static BOOL smoke_failed;
static DWORD windows_build;

enum {
    ID_SETUP = 101, ID_NETWORK, ID_BLOAT, ID_WIFI, ID_DRIVERS, ID_RESTORE,
    ID_DELAY, ID_UNIT, ID_SCHEDULE, ID_CANCEL, ID_LANGUAGE, ID_TITLE, ID_SUBTITLE,
    ID_ACCOUNT, ID_TIMER_LABEL, ID_LOG_LABEL, ID_OPEN_LOG, ID_SYSTEM_INFO, ID_THEME,
    ID_PC_SUMMARY, ID_PC_LABEL, ID_NAV_LABEL, ID_BACKUPS, ID_PROTECTION_NOTE, ID_BRAND,
    ID_SYSTEM_PROTECTION, ID_DETAILS, ID_QUICK_SETUP, ID_QUICK_NETWORK,
    ID_QUICK_BLOAT, ID_QUICK_LABEL, ID_SAFETY_LABEL, ID_BACK, ID_ADVANCED, ID_APP_ICON, ID_HOME
};

const wchar_t *tr(const wchar_t *vi, const wchar_t *en) { return g_vi ? vi : en; }
BOOL app_is_smoke_test(void) { return smoke_mode; }
HWND app_main_window(void) { return main_window; }

static void dashboard_visibility(void) {
    const int basic[] = {ID_TITLE,ID_SUBTITLE,ID_PC_LABEL,ID_PC_SUMMARY,ID_DETAILS,
        ID_QUICK_LABEL,ID_QUICK_SETUP,ID_QUICK_NETWORK,ID_QUICK_BLOAT,ID_TIMER_LABEL,
        ID_DELAY,ID_UNIT,ID_SCHEDULE,ID_CANCEL,ID_ADVANCED};
    const int advanced[] = {ID_ACCOUNT,ID_LOG_LABEL,ID_OPEN_LOG,ID_SYSTEM_PROTECTION};
    BOOL home = !active_page && !confirmation_window;
    int selected=active_page?(int)(INT_PTR)GetPropW(active_page,L"WAIONav"):ID_HOME;
    const int nav[]={ID_HOME,ID_SYSTEM_INFO,ID_SETUP,ID_NETWORK,ID_BLOAT};
    for (unsigned i=0;i<ARRAYSIZE(nav);++i) {
        HWND control=GetDlgItem(main_window,nav[i]);
        if (nav[i]==selected) SetPropW(control,L"WAIOActive",(HANDLE)1);
        else RemovePropW(control,L"WAIOActive");
        InvalidateRect(control,NULL,FALSE);
    }
    for (unsigned i=0;i<ARRAYSIZE(basic);++i) ShowWindow(GetDlgItem(main_window,basic[i]),home?SW_SHOW:SW_HIDE);
    for (unsigned i=0;i<ARRAYSIZE(advanced);++i) ShowWindow(GetDlgItem(main_window,advanced[i]),home&&dashboard_advanced?SW_SHOW:SW_HIDE);
    ShowWindow(timer_status,home?SW_SHOW:SW_HIDE);
    ShowWindow(log_window,home&&dashboard_advanced?SW_SHOW:SW_HIDE);
    ShowWindow(GetDlgItem(main_window,ID_BACK),active_page&&!confirmation_window?SW_SHOW:SW_HIDE);
    ShowWindow(GetDlgItem(main_window,ID_SAFETY_LABEL),SW_HIDE);
    ShowWindow(GetDlgItem(main_window,ID_PROTECTION_NOTE),SW_HIDE);
    ShowWindow(GetDlgItem(main_window,ID_NAV_LABEL),SW_HIDE);
}
void app_show_page(HWND page) {
    if (!main_window || !IsWindow(page) || GetParent(page)!=main_window ||
        app_busy(NULL) || confirmation_window) return;
    if (IsWindow(active_page) && active_page!=page) ShowWindow(active_page,SW_HIDE);
    active_page=page;
    dashboard_visibility(); layout_main(main_window);
    ShowWindow(page,SW_SHOW);
    SetWindowPos(page,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    SetFocus(GetNextDlgTabItem(page,NULL,FALSE));
}
void app_home(void) {
    if (!main_window || closing_main || app_busy(NULL) || confirmation_window) return;
    if (IsWindow(active_page)) ShowWindow(active_page,SW_HIDE);
    active_page=NULL;
    dashboard_visibility(); layout_main(main_window);
    SetFocus(GetDlgItem(main_window,ID_HOME));
}
void app_page_destroyed(HWND page) {
    if (active_page==page) { active_page=NULL; if (!closing_main) app_home(); }
}

static const wchar_t language_key[] = L"Software\\Doctoten\\WinAIOOptimize";
static void language_load(void) {
    DWORD value = 0, size = sizeof(value);
    g_vi = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_VIETNAMESE;
    if (RegGetValueW(HKEY_CURRENT_USER, language_key, L"Vietnamese", RRF_RT_REG_DWORD,
                     NULL, &value, &size) == ERROR_SUCCESS && value <= 1)
        g_vi = value != 0;
}
static void language_save(HWND owner) {
    if (smoke_mode) return;
    HKEY key;
    DWORD value = g_vi ? 1 : 0;
    LSTATUS error = RegCreateKeyExW(HKEY_CURRENT_USER, language_key, 0, NULL, 0,
                                   KEY_SET_VALUE, NULL, &key, NULL);
    if (error == ERROR_SUCCESS) {
        error = RegSetValueExW(key, L"Vietnamese", 0, REG_DWORD, (BYTE *)&value, sizeof(value));
        RegCloseKey(key);
    }
    if (error != ERROR_SUCCESS)
        app_error(owner, tr(L"Không lưu được ngôn ngữ", L"Could not save language preference"), (DWORD)error);
}
static void language_thread(void) {
    SetThreadUILanguage(g_vi ? MAKELANGID(LANG_VIETNAMESE, SUBLANG_DEFAULT)
                            : MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
}

HWND app_control(HWND parent, const wchar_t *cls, const wchar_t *text, DWORD style,
                 int x, int y, int w, int h, int id) {
    if (!lstrcmpiW(cls, L"COMBOBOX")) style |= CBS_OWNERDRAWFIXED | CBS_HASSTRINGS;
    if (!lstrcmpiW(cls, L"STATIC")) style |= SS_NOPREFIX;
    if (!lstrcmpiW(cls, L"EDIT") || !wcscmp(cls, WC_LISTVIEWW)) style &= ~WS_BORDER;
    DWORD extended = 0;
    HWND control = CreateWindowExW(extended, cls, text, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | style,
                                  x, y, w, h, parent, (HMENU)(INT_PTR)id, g_instance, NULL);
    if (control) {
        SendMessageW(control, WM_SETFONT, (WPARAM)g_font, TRUE);
        SetWindowTheme(control, L"Explorer", NULL);
        theme_control(control);
        if (!lstrcmpiW(cls, L"COMBOBOX")) {
            SendMessageW(control, CB_SETITEMHEIGHT, (WPARAM)-1, 26);
            SendMessageW(control, CB_SETITEMHEIGHT, 0, 26);
        }
    }
    return control;
}

BOOL app_data_path(wchar_t *path, size_t capacity, const wchar_t *filename) {
    if (smoke_mode) return FALSE;
    wchar_t base[MAX_PATH];
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, base))) return FALSE;
    if (FAILED(StringCchPrintfW(path, capacity, L"%s\\WinAIOOptimize", base))) return FALSE;
    if (!CreateDirectoryW(path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) return FALSE;
    if (filename && *filename) {
        if (FAILED(StringCchCatW(path, capacity, L"\\")) ||
            FAILED(StringCchCatW(path, capacity, filename))) return FALSE;
    }
    return TRUE;
}

void app_log(const wchar_t *format, ...) {
    wchar_t message[8192], line[8300];
    va_list args;
    SYSTEMTIME now;
    va_start(args, format);
    StringCchVPrintfW(message, ARRAYSIZE(message), format, args);
    va_end(args);
    GetLocalTime(&now);
    StringCchPrintfW(line, ARRAYSIZE(line), L"[%02u:%02u:%02u] %s\r\n",
                     now.wHour, now.wMinute, now.wSecond, message);
    if (log_ready) {
        EnterCriticalSection(&log_lock);
        HANDLE file = CreateFileW(log_path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL, NULL);
        if (file != INVALID_HANDLE_VALUE) {
            char encoded[33200];
            int bytes = WideCharToMultiByte(CP_UTF8, 0, line, -1, encoded, sizeof(encoded), NULL, NULL);
            DWORD written;
            if (bytes > 1) WriteFile(file, encoded, (DWORD)bytes - 1, &written, NULL);
            CloseHandle(file);
        }
        LeaveCriticalSection(&log_lock);
    }
    if (main_window) {
        size_t bytes = (wcslen(line) + 1) * sizeof(wchar_t);
        wchar_t *copy = malloc(bytes);
        if (copy) {
            memcpy(copy, line, bytes);
            if (!PostMessageW(main_window, WM_APP_LOG, 0, (LPARAM)copy)) free(copy);
        }
    }
}

void app_error(HWND owner, const wchar_t *context, DWORD error) {
    if (smoke_mode) {
        fprintf(stderr, "UI test error: %lu\n", error); fflush(stderr);
        smoke_failed = TRUE;
        return;
    }
    wchar_t detail[1024] = L"", message[2048];
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error,
                   0, detail, ARRAYSIZE(detail), NULL);
    StringCchPrintfW(message, ARRAYSIZE(message), L"%s\n\n%lu (0x%08lX): %s", context, error, error, detail);
    app_log(L"%s", message);
    MessageBoxW(owner, message, tr(L"Không thể hoàn tất", L"Unable to complete"), MB_OK | MB_ICONERROR);
}

/* A scrollable review is necessary for long lists of selected tweaks/packages. */
enum { ID_PROTECT_STATUS = 703 };
typedef struct {
    const wchar_t *details;
    BOOL accepted, protect, registry, restore, working;
    DWORD error;
} Confirmation;
static void confirmation_prepare(void *argument) {
    Confirmation *state = argument;
    state->error = protection_prepare(state->registry, state->restore);
}
static void confirmation_layout(HWND window, BOOL protect) {
    RECT r; GetClientRect(window,&r);
    int width=r.right-48, bottom=r.bottom-24;
    int detail_height=max(96,r.bottom-64-(protect?230:92));
    theme_move(GetDlgItem(window,701),24,18,width,32);
    theme_move(GetDlgItem(window,10),24,64,width,detail_height);
    theme_move(GetDlgItem(window,702),24,76+detail_height,width,80);
    theme_move(GetDlgItem(window,ID_PROTECT_STATUS),24,164+detail_height,width,48);
    theme_move(GetDlgItem(window,IDCANCEL),r.right-320,bottom-40,130,40);
    theme_move(GetDlgItem(window,IDOK),r.right-174,bottom-40,150,40);
    theme_repaint(window);
}
static LRESULT CALLBACK confirm_proc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    Confirmation *state = (Confirmation *)GetWindowLongPtrW(window, GWLP_USERDATA);
    switch (message) {
    case WM_CREATE: {
        state = ((CREATESTRUCTW *)lp)->lpCreateParams;
        SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)state);
        theme_attach(window);
        HWND title = app_control(window, L"STATIC", tr(L"Xem lại thay đổi trước khi đồng ý", L"Review the changes before you agree"),
                    0, 24, 18, 744, 30, 701);
        SendMessageW(title, WM_SETFONT, (WPARAM)g_font_semibold, TRUE);
        HWND details = app_control(window, L"EDIT", L"",
                    ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL | WS_TABSTOP,
                    24, 58, 744, 315, 10);
        if (!details) return -1; /* Never accept changes if their review cannot be displayed. */
        SendMessageW(details, EM_SETLIMITTEXT, 131072, 0);
        SetWindowTextW(details,state->details);
        if ((size_t)GetWindowTextLengthW(details)!=wcslen(state->details)) return -1;
        SendMessageW(details, EM_SETSEL, 0, 0);
        if (state->protect) {
            app_control(window, L"STATIC", tr(
                L"Registry: lưu SYSTEM, SOFTWARE và HKCU vào thư mục dữ liệu ứng dụng.\r\n"
                L"System Protection phải được bật; Windows có thể dùng lại điểm khôi phục gần đây.\r\n"
                L"Bảo vệ đã chọn phải hoàn tất trước khi thay đổi. Không thay thế sao lưu file cá nhân.",
                L"Registry: save SYSTEM, SOFTWARE and HKCU in the app data folder.\r\n"
                L"System Protection must be enabled; Windows may reuse a recent restore point.\r\n"
                L"Selected protection must succeed before changes. This does not back up personal files."),
                0, 24, 391, 744, 78, 702);
            app_control(window, L"STATIC", L"", 0, 24, 482, 744, 70, ID_PROTECT_STATUS);
        }
        app_control(window, L"BUTTON", tr(L"Hủy", L"Cancel"), WS_TABSTOP | BS_DEFPUSHBUTTON,
                    476, state->protect ? 568 : 394, 130, 40, IDCANCEL);
        HWND ok_btn = app_control(window, L"BUTTON", tr(L"Tôi đồng ý", L"I agree"), WS_TABSTOP,
                    618, state->protect ? 568 : 394, 150, 40, IDOK);
        SetPropW(ok_btn, L"WAIOPrimary", (HANDLE)1);
        SetFocus(GetDlgItem(window, IDCANCEL));
        return 0;
    }
    case WM_COMMAND:
        if (!state || state->working) return 0;
        if (LOWORD(wp) == IDOK || LOWORD(wp) == IDCANCEL) {
            if (LOWORD(wp) == IDOK && state->protect) {
                if (state->registry || state->restore) {
                    state->working = TRUE;
                    SetDlgItemTextW(window, ID_PROTECT_STATUS, tr(L"Đang tạo bản bảo vệ… Chưa áp dụng thay đổi. Vui lòng chờ.",
                        L"Preparing recovery options… No changes applied. Please wait."));
                    if (!app_start_job(window, confirmation_prepare, state)) {
                        state->working = FALSE;
                        SetDlgItemTextW(window, ID_PROTECT_STATUS, tr(
                            L"Không bắt đầu được bước bảo vệ. Chưa áp dụng thay đổi. Thử lại hoặc Hủy.",
                            L"Could not start recovery preparation. No changes applied. Retry or Cancel."));
                    }
                    return 0;
                }
            }
            state->accepted = LOWORD(wp) == IDOK;
            DestroyWindow(window);
        }
        return 0;
    case WM_APP_JOB_DONE:
        state->working = FALSE; app_finish_job(window);
        if (!state->error) { state->accepted = TRUE; DestroyWindow(window); }
        else {
            wchar_t text[512];
            StringCchPrintfW(text, ARRAYSIZE(text), tr(
                L"Bảo vệ thất bại (mã %lu). CHƯA áp dụng thay đổi. Kiểm tra nhật ký hoặc System Protection, rồi thử lại. Chọn Hủy để về Windows Setup và điều chỉnh lựa chọn bảo vệ.",
                L"Protection failed (code %lu). NO changes applied. Check the log or System Protection, then retry. Choose Cancel to return to Windows Setup and adjust recovery options."), state->error);
            SetDlgItemTextW(window, ID_PROTECT_STATUS, text); app_log(L"%s", text);
            SetFocus(GetDlgItem(window, IDCANCEL));
        }
        return 0;
    case WM_CLOSE: if (!state || !state->working) DestroyWindow(window); return 0;
    case WM_SIZE: if (state) confirmation_layout(window,state->protect); return 0;
    case DM_GETDEFID: return MAKELONG(IDCANCEL, DC_HASDEFID);
    }
    return DefWindowProcW(window, message, wp, lp);
}

static BOOL confirm_dialog(HWND owner, const wchar_t *details, BOOL registry, BOOL restore) {
    if (smoke_mode) return FALSE;
    WNDCLASSW cls = {0};
    cls.hInstance = g_instance; cls.lpfnWndProc = confirm_proc;
    cls.lpszClassName = L"WAIO.Confirm"; cls.hCursor = LoadCursorW(NULL, IDC_ARROW);
    RegisterClassW(&cls);
    Confirmation state = {0}; state.details = details; state.protect = registry || restore;
    state.registry = registry; state.restore = restore;
    /* Review is a child page, so the application keeps a single top-level window. */
    HWND dialog = CreateWindowExW(WS_EX_CONTROLPARENT, cls.lpszClassName,
                    tr(L"Xác nhận thay đổi", L"Confirm changes"), WS_CHILD | WS_CLIPCHILDREN,
                    224, 72, 812, 660, main_window, NULL, g_instance, &state);
    if (!dialog) return FALSE;
    HWND disabled[128]; unsigned disabled_count=0;
    for (HWND child=GetWindow(main_window,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)) {
        if (child!=dialog && IsWindowEnabled(child)) {
            if (disabled_count==ARRAYSIZE(disabled)) { DestroyWindow(dialog); return FALSE; }
            disabled[disabled_count++]=child;
        }
    }
    for (unsigned i=0;i<disabled_count;++i) EnableWindow(disabled[i],FALSE);
    confirmation_window=dialog;
    if (IsWindow(active_page)) ShowWindow(active_page,SW_HIDE);
    layout_main(main_window);
    ShowWindow(dialog,SW_SHOW);
    SetWindowPos(dialog,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    SetFocus(GetDlgItem(dialog,IDCANCEL));
    MSG msg;
    BOOL quit = FALSE;
    while (IsWindow(dialog)) {
        int result = (int)GetMessageW(&msg, NULL, 0, 0);
        if (result <= 0) {
            quit = TRUE;
            if (state.working) continue; /* Drain the completion before this stack context can go away. */
            break;
        }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE &&
            (msg.hwnd == dialog || IsChild(dialog, msg.hwnd))) {
            SendMessageW(dialog, WM_COMMAND, IDCANCEL, 0);
        } else if (!IsDialogMessageW(dialog, &msg)) {
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }
    }
    if (IsWindow(dialog)) DestroyWindow(dialog);
    confirmation_window=NULL;
    for (unsigned i=0;i<disabled_count;++i) if (IsWindow(disabled[i])) EnableWindow(disabled[i],TRUE);
    if (IsWindow(main_window)) {
        if (IsWindow(active_page)) ShowWindow(active_page,SW_SHOW);
        layout_main(main_window);
        SetForegroundWindow(main_window);
        SetFocus(IsWindow(active_page)?GetNextDlgTabItem(active_page,NULL,FALSE):GetDlgItem(main_window,ID_HOME));
    }
    if (quit) PostQuitMessage(0);
    return state.accepted && IsWindow(owner) && !quit;
}
BOOL app_confirm(HWND owner, const wchar_t *details) { return confirm_dialog(owner, details, FALSE, FALSE); }
BOOL app_confirm_setup(HWND owner, const wchar_t *details, BOOL registry, BOOL restore) {
    return confirm_dialog(owner, details, registry, restore);
}

static BOOL confirmation_ui_test(HWND owner) {
    WNDCLASSW cls = {0}; cls.hInstance = g_instance; cls.lpfnWndProc = confirm_proc;
    cls.lpszClassName = L"WAIO.Confirm"; cls.hCursor = LoadCursorW(NULL, IDC_ARROW); RegisterClassW(&cls);
    Confirmation state = {0}; state.protect = TRUE; state.registry = TRUE; state.restore = TRUE;
    state.details = L"Read-only consent test";
    HWND window = CreateWindowExW(WS_EX_CONTROLPARENT, cls.lpszClassName, L"", WS_CHILD | WS_CLIPCHILDREN,
        0, 0, 812, 660, owner, NULL, g_instance, &state);
    if (!window) return FALSE;
    BOOL ok = state.registry && state.restore && LOWORD(SendMessageW(window, DM_GETDEFID, 0, 0)) == IDCANCEL;
    /* A failed preparatory job must leave the review open and unaccepted. */
    state.working = TRUE; state.error = ERROR_SERVICE_DISABLED;
    SendMessageW(window, WM_APP_JOB_DONE, 0, 0);
    ok = ok && IsWindow(window) && !state.working && !state.accepted;
    SendMessageW(window, WM_COMMAND, IDCANCEL, 0);
    return ok && !state.accepted && !state.working && !app_busy(NULL);
}

BOOL app_pick_folder(HWND owner, wchar_t *path, size_t capacity) {
    IFileOpenDialog *dialog = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
                                  &IID_IFileOpenDialog, (void **)&dialog);
    if (FAILED(hr)) { app_error(owner, tr(L"Chọn thư mục", L"Folder picker"), (DWORD)hr); return FALSE; }
    DWORD flags = 0;
    IFileOpenDialog_GetOptions(dialog, &flags);
    IFileOpenDialog_SetOptions(dialog, flags | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    hr = IFileOpenDialog_Show(dialog, owner);
    BOOL result = FALSE;
    if (SUCCEEDED(hr)) {
        IShellItem *item = NULL;
        hr = IFileOpenDialog_GetResult(dialog, &item);
        if (SUCCEEDED(hr)) {
            PWSTR name = NULL;
            hr = IShellItem_GetDisplayName(item, SIGDN_FILESYSPATH, &name);
            if (SUCCEEDED(hr)) {
                result = SUCCEEDED(StringCchCopyW(path, capacity, name));
                CoTaskMemFree(name);
                if (!result) app_error(owner, tr(L"Đường dẫn thư mục", L"Folder path"), ERROR_FILENAME_EXCED_RANGE);
            }
            IShellItem_Release(item);
        }
    } else if (hr != HRESULT_FROM_WIN32(ERROR_CANCELLED)) app_error(owner, tr(L"Chọn thư mục", L"Folder picker"), (DWORD)hr);
    IFileOpenDialog_Release(dialog);
    return result;
}

typedef struct { HWND owner; void (*work)(void *); void *context; } Work;
static DWORD WINAPI worker_proc(void *argument) {
    Work *work = argument;
    work->work(work->context);
    PostMessageW(work->owner, WM_APP_JOB_DONE, 0, 0);
    free(work);
    return 0;
}

BOOL app_start_job(HWND owner, void (*work)(void *), void *context) {
    if (smoke_mode) return FALSE;
    if (InterlockedCompareExchange(&running_job, 1, 0)) {
        MessageBoxW(owner, tr(L"Hãy chờ tác vụ hiện tại hoàn tất.", L"Please wait for the current task to finish."),
                    L"Win AIO Optimize", MB_OK | MB_ICONINFORMATION);
        return FALSE;
    }
    Work *job = malloc(sizeof(*job));
    if (!job) { InterlockedExchange(&running_job, 0); app_error(owner, tr(L"Tác vụ nền", L"Worker"), ERROR_OUTOFMEMORY); return FALSE; }
    *job = (Work){owner, work, context};
    if (!SetPropW(owner, L"WAIOBusy", (HANDLE)1)) {
        free(job); InterlockedExchange(&running_job, 0); return FALSE;
    }
    EnableWindow(owner, FALSE);
    HANDLE thread = CreateThread(NULL, 0, worker_proc, job, 0, NULL);
    if (!thread) {
        DWORD error = GetLastError(); free(job); app_finish_job(owner); app_error(owner, tr(L"Tác vụ nền", L"Worker"), error); return FALSE;
    }
    CloseHandle(thread);
    if (progress_window) {
        SetWindowPos(progress_window,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);
        SendMessageW(progress_window, PBM_SETMARQUEE, TRUE, 40);
    }
    return TRUE;
}

void app_finish_job(HWND owner) {
    RemovePropW(owner, L"WAIOBusy");
    InterlockedExchange(&running_job, 0);
    EnableWindow(owner, TRUE);
    if (progress_window) { SendMessageW(progress_window, PBM_SETMARQUEE, FALSE, 0); ShowWindow(progress_window, SW_HIDE); }
    SetForegroundWindow(GetAncestor(owner, GA_ROOT));
}

BOOL app_busy(HWND owner) {
    return owner ? GetPropW(owner, L"WAIOBusy") != NULL : InterlockedCompareExchange(&running_job, 0, 0) != 0;
}

DWORD app_run_tool(const wchar_t *exe_name, const wchar_t *arguments) {
    if (smoke_mode) return ERROR_ACCESS_DENIED;
    if (!exe_name || !arguments) return ERROR_INVALID_PARAMETER;
    static const wchar_t *const allowed[] = {L"netsh.exe", L"ipconfig.exe", L"dism.exe", L"powercfg.exe", L"shutdown.exe"};
    BOOL found = FALSE;
    for (size_t i = 0; i < ARRAYSIZE(allowed); ++i) if (!_wcsicmp(exe_name, allowed[i])) found = TRUE;
    if (!found) return ERROR_ACCESS_DENIED;
    wchar_t path[MAX_PATH], command[32768];
    UINT length = GetSystemDirectoryW(path, ARRAYSIZE(path));
    if (!length || length >= ARRAYSIZE(path)) return ERROR_INSUFFICIENT_BUFFER;
    if (FAILED(StringCchCatW(path, ARRAYSIZE(path), L"\\")) ||
        FAILED(StringCchCatW(path, ARRAYSIZE(path), exe_name)) ||
        FAILED(StringCchPrintfW(command, ARRAYSIZE(command), L"\"%s\" %s", path, arguments))) return ERROR_INSUFFICIENT_BUFFER;
    SECURITY_ATTRIBUTES security = {sizeof(security), NULL, TRUE};
    HANDLE read_pipe = NULL, write_pipe = NULL, input = INVALID_HANDLE_VALUE;
    STARTUPINFOEXW start = {0};
    PROCESS_INFORMATION process = {0};
    DWORD code = ERROR_GEN_FAILURE;
    BOOL attributes_initialized = FALSE;
    if (!CreatePipe(&read_pipe, &write_pipe, &security, 0)) return GetLastError();
    if (!SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0)) { code = GetLastError(); goto cleanup; }
    input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0, NULL);
    if (input == INVALID_HANDLE_VALUE) { code = GetLastError(); goto cleanup; }
    SIZE_T bytes = 0;
    InitializeProcThreadAttributeList(NULL, 1, 0, &bytes);
    start.lpAttributeList = malloc(bytes);
    if (!start.lpAttributeList) { code = ERROR_OUTOFMEMORY; goto cleanup; }
    if (!InitializeProcThreadAttributeList(start.lpAttributeList, 1, 0, &bytes)) { code = GetLastError(); goto cleanup; }
    attributes_initialized = TRUE;
    HANDLE inherited[] = {write_pipe, input};
    if (!UpdateProcThreadAttribute(start.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                   inherited, sizeof(inherited), NULL, NULL)) { code = GetLastError(); goto cleanup; }
    start.StartupInfo.cb = sizeof(start);
    start.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    start.StartupInfo.hStdInput = input;
    start.StartupInfo.hStdOutput = start.StartupInfo.hStdError = write_pipe;
    app_log(L"%s %s", exe_name, arguments);
    if (!CreateProcessW(path, command, NULL, NULL, TRUE, CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT,
                        NULL, NULL, &start.StartupInfo, &process)) { code = GetLastError(); goto cleanup; }
    CloseHandle(write_pipe); write_pipe = NULL;
    char raw[2048];
    DWORD read = 0;
    while (ReadFile(read_pipe, raw, sizeof(raw), &read, NULL) && read) {
        wchar_t output[4096];
        int count = MultiByteToWideChar(CP_OEMCP, 0, raw, (int)read, output, ARRAYSIZE(output) - 1);
        if (count > 0) { output[count] = L'\0'; app_log(L"%s", output); }
    }
    WaitForSingleObject(process.hProcess, INFINITE);
    if (!GetExitCodeProcess(process.hProcess, &code)) code = GetLastError();
    CloseHandle(process.hThread); CloseHandle(process.hProcess);
cleanup:
    if (attributes_initialized) DeleteProcThreadAttributeList(start.lpAttributeList);
    free(start.lpAttributeList);
    if (read_pipe) CloseHandle(read_pipe);
    if (write_pipe) CloseHandle(write_pipe);
    if (input != INVALID_HANDLE_VALUE) CloseHandle(input);
    app_log(tr(L"%s: mã thoát %lu", L"%s: exit code %lu"), exe_name, code);
    return code;
}

DWORD app_windows_build(void) {
    if (!windows_build) {
        typedef LONG (WINAPI *VersionFn)(OSVERSIONINFOW *);
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        VersionFn version = NULL;
        FARPROC address = ntdll ? GetProcAddress(ntdll, "RtlGetVersion") : NULL;
        memcpy(&version, &address, sizeof(version));
        OSVERSIONINFOW info = {0}; info.dwOSVersionInfoSize = sizeof(info);
        if (version && version(&info) == 0 && info.dwMajorVersion >= 10) windows_build = info.dwBuildNumber;
    }
    return windows_build;
}

static BOOL is_admin(void) {
    SID_IDENTIFIER_AUTHORITY authority = SECURITY_NT_AUTHORITY;
    PSID group = NULL; BOOL result = FALSE;
    if (AllocateAndInitializeSid(&authority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
                                 0, 0, 0, 0, 0, 0, &group)) {
        CheckTokenMembership(NULL, group, &result); FreeSid(group);
    }
    return result;
}

static void update_labels(HWND window) {
    SetWindowTextW(window, L"Win AIO Optimize — Doctoten");
    SetDlgItemTextW(window, ID_TITLE, tr(L"Tổng quan", L"Dashboard"));
    SetDlgItemTextW(window, ID_SUBTITLE, tr(L"Chọn công cụ bạn cần để bắt đầu.", L"Choose a tool to get started."));
    SetDlgItemTextW(window, ID_BRAND, L"Win AIO Optimize");
    SetDlgItemTextW(window, ID_NAV_LABEL, tr(L"Không gian làm việc", L"Workspace"));
    SetDlgItemTextW(window, ID_SYSTEM_INFO, tr(L"Thông tin máy", L"System information"));
    SetDlgItemTextW(window, ID_DETAILS, tr(L"Thông tin máy", L"Specifications"));
    SetDlgItemTextW(window, ID_BACK, tr(L"‹  Tổng quan", L"‹  Dashboard"));
    SetDlgItemTextW(window, ID_HOME, tr(L"Tổng quan", L"Dashboard"));
    SetDlgItemTextW(window, ID_ADVANCED, dashboard_advanced ? tr(L"Ẩn nâng cao", L"Hide advanced") : tr(L"Nâng cao", L"Advanced"));
    SetDlgItemTextW(window, ID_PC_LABEL, tr(L"Máy tính của bạn", L"Your computer"));
    SetDlgItemTextW(window, ID_BACKUPS, tr(L"Mở thư mục sao lưu", L"Open backups"));
    SetDlgItemTextW(window, ID_SYSTEM_PROTECTION, tr(L"Bảo vệ hệ thống…", L"System Protection…"));
    SetDlgItemTextW(window, ID_QUICK_LABEL, tr(L"Bạn muốn làm gì?", L"What would you like to do?"));
    SetDlgItemTextW(window, ID_QUICK_SETUP, tr(L"Tối ưu Windows\nQuyền riêng tư và trải nghiệm.",
        L"Optimize Windows\nPrivacy and Windows experience."));
    SetDlgItemTextW(window, ID_QUICK_NETWORK, tr(L"Mạng & DNS\nKết nối và máy chủ DNS.",
        L"Network & DNS\nConnections and DNS providers."));
    SetDlgItemTextW(window, ID_QUICK_BLOAT, tr(L"Ứng dụng tùy chọn\nChọn ứng dụng muốn gỡ.",
        L"Optional apps\nChoose apps to remove."));
    SetDlgItemTextW(window, ID_SAFETY_LABEL, tr(L"Trước khi thay đổi", L"Before making changes"));
    SetDlgItemTextW(window, ID_PROTECTION_NOTE, tr(
        L"Bạn luôn được xem lại và xác nhận. Tùy chọn sao lưu Registry và điểm khôi phục có trong Thiết lập Windows.",
        L"Review and confirm every change. Registry backup and restore point options are in Windows Setup."));
    wchar_t summary[768]; system_summary(summary, ARRAYSIZE(summary)); SetDlgItemTextW(window, ID_PC_SUMMARY, summary);
    HWND theme = GetDlgItem(window, ID_THEME);
    SendMessageW(theme, CB_RESETCONTENT, 0, 0);
    SendMessageW(theme, CB_ADDSTRING, 0, (LPARAM)tr(L"Theo Windows", L"Windows theme"));
    SendMessageW(theme, CB_ADDSTRING, 0, (LPARAM)tr(L"Sáng", L"Light"));
    SendMessageW(theme, CB_ADDSTRING, 0, (LPARAM)tr(L"Tối", L"Dark"));
    SendMessageW(theme, CB_SETCURSEL, g_theme.mode, 0);
    SetDlgItemTextW(window, ID_SETUP, tr(L"Thiết lập Windows", L"Windows Setup"));
    SetDlgItemTextW(window, ID_NETWORK, tr(L"Mạng & đổi DNS", L"Network & DNS"));
    SetDlgItemTextW(window, ID_BLOAT, tr(L"Gỡ ứng dụng cài sẵn", L"Remove bloatware"));
    SetDlgItemTextW(window, ID_WIFI, tr(L"Sao lưu WiFi", L"Back up WiFi"));
    SetDlgItemTextW(window, ID_RESTORE, tr(L"Khôi phục WiFi", L"Restore WiFi"));
    SetDlgItemTextW(window, ID_DRIVERS, tr(L"Sao lưu driver", L"Back up drivers"));
    SetDlgItemTextW(window, ID_TIMER_LABEL, tr(L"Hẹn giờ tắt máy", L"Shutdown timer"));
    SetDlgItemTextW(window, ID_SCHEDULE, tr(L"Đặt lịch…", L"Schedule…"));
    SetDlgItemTextW(window, ID_CANCEL, tr(L"Hủy lịch…", L"Cancel timer…"));
    SetDlgItemTextW(window, ID_LOG_LABEL, tr(L"Nhật ký hoạt động", L"Activity log"));
    SetDlgItemTextW(window, ID_OPEN_LOG, tr(L"Mở nhật ký", L"Open log"));
    HWND units = GetDlgItem(window, ID_UNIT);
    LRESULT selected = SendMessageW(units, CB_GETCURSEL, 0, 0);
    SendMessageW(units, CB_RESETCONTENT, 0, 0);
    SendMessageW(units, CB_ADDSTRING, 0, (LPARAM)tr(L"Phút", L"Minutes"));
    SendMessageW(units, CB_ADDSTRING, 0, (LPARAM)tr(L"Giờ", L"Hours"));
    SendMessageW(units, CB_SETCURSEL, selected == 1 ? 1 : 0, 0);
    wchar_t name[256] = L"?", account[512], status[512]; DWORD capacity = ARRAYSIZE(name);
    GetUserNameW(name, &capacity);
    StringCchPrintfW(account, ARRAYSIZE(account), tr(L"Tài khoản: %s  •  Windows build %lu",
                     L"Account: %s  •  Windows build %lu"), smoke_mode ? L"Preview" : name, app_windows_build());
    SetDlgItemTextW(window, ID_ACCOUNT, account);
    if (smoke_mode) StringCchCopyW(status, ARRAYSIZE(status), tr(L"Kiểm tra giao diện chỉ đọc", L"Read-only UI smoke test"));
    else shutdown_status(status, ARRAYSIZE(status));
    SetWindowTextW(timer_status, status);
}

typedef struct {
    int x, width, tile_top, tile_height, log_top;
    RECT computer, timer, safety;
} DashboardLayout;

static DashboardLayout dashboard_layout(HWND window) {
    RECT r; GetClientRect(window, &r);
    DashboardLayout d = {0};
    d.width = min(1120, max(600, r.right - 224 - 56));
    d.x = 224 + (r.right - 224 - d.width) / 2;
    BOOL compact = r.bottom < 800;
    SetRect(&d.computer, d.x, 108, d.x + d.width, 218);
    d.tile_top = d.computer.bottom + 46;
    d.tile_height = compact ? 112 : 132;
    int utility_top = d.tile_top + d.tile_height + 16;
    int timer_width = d.width;
    SetRect(&d.timer, d.x, utility_top, d.x + timer_width, utility_top + 120);
    SetRect(&d.safety, d.timer.right + 16, utility_top, d.x + d.width, utility_top + 120);
    d.log_top = d.timer.bottom + 100;
    return d;
}

static void layout_main(HWND window) {
    RECT r; GetClientRect(window, &r);
    int height = r.bottom;
    DashboardLayout d = dashboard_layout(window);
    int x = d.x, content = d.width;
    theme_move(GetDlgItem(window, ID_APP_ICON), 20, 24, 32, 32);
    theme_move(GetDlgItem(window, ID_BRAND), 62, 29, 156, 26);
    theme_move(GetDlgItem(window, ID_HOME), 12, 84, 200, 40);
    theme_move(GetDlgItem(window, ID_NAV_LABEL), 24, 88, 176, 24);
    int nav_buttons[] = {ID_SYSTEM_INFO, ID_SETUP, ID_NETWORK, ID_BLOAT, ID_WIFI, ID_RESTORE, ID_DRIVERS};
    for (int i = 0; i < 7; ++i) {
        theme_move(GetDlgItem(window, nav_buttons[i]), 12, 130 + i * 46, 200, 40);
    }
    theme_move(GetDlgItem(window, ID_SYSTEM_PROTECTION), 12, height - 98, 200, 38);
    theme_move(GetDlgItem(window, ID_BACKUPS), 12, height - 52, 200, 38);
    theme_move(GetDlgItem(window, ID_TITLE), x, 24, content - 320, 38);
    theme_move(GetDlgItem(window, ID_THEME), x + content - 300, 26, 144, 180);
    theme_move(GetDlgItem(window, ID_LANGUAGE), x + content - 144, 26, 144, 180);
    theme_move(GetDlgItem(window, ID_SUBTITLE), x, 66, content, 22);
    theme_move(GetDlgItem(window, ID_ACCOUNT), x, d.timer.bottom + 66, content, 22);
    theme_move(GetDlgItem(window, ID_ADVANCED), x, d.timer.bottom + 20, 158, 34);
    theme_move(GetDlgItem(window, ID_BACK), x, 24, 180, 36);
    int top = d.computer.top;
    theme_move(GetDlgItem(window, ID_PC_LABEL), x + 20, top + 16, content - 260, 24);
    theme_move(GetDlgItem(window, ID_DETAILS), x + content - 220, top + 16, 200, 34);
    theme_move(GetDlgItem(window, ID_PC_SUMMARY), x + 20, top + 52, content - 40, 46);
    theme_move(GetDlgItem(window, ID_QUICK_LABEL), x, d.computer.bottom + 20, content, 26);
    int tile_width = (content - 32) / 3;
    int quick[] = {ID_QUICK_SETUP, ID_QUICK_NETWORK, ID_QUICK_BLOAT};
    for (int i = 0; i < 3; ++i)
        theme_move(GetDlgItem(window, quick[i]), x + i * (tile_width + 16), d.tile_top, tile_width, d.tile_height);
    int timer_width = d.timer.right - d.timer.left;
    top = d.timer.top;
    theme_move(GetDlgItem(window, ID_TIMER_LABEL), x + 18, top + 16, timer_width - 36, 24);
    theme_move(GetDlgItem(window, ID_DELAY), x + 18, top + 50, 56, 34);
    theme_move(GetDlgItem(window, ID_UNIT), x + 82, top + 50, 112, 160);
    theme_move(GetDlgItem(window, ID_SCHEDULE), x + 204, top + 50, 130, 34);
    theme_move(GetDlgItem(window, ID_CANCEL), x + 344, top + 50, 134, 34);
    theme_move(timer_status, x + 18, top + 98, timer_width - 36, 24);
    int note_x = d.safety.left + 18, note_width = d.safety.right - d.safety.left - 36;
    theme_move(GetDlgItem(window, ID_SAFETY_LABEL), note_x, top + 16, note_width, 24);
    theme_move(GetDlgItem(window, ID_PROTECTION_NOTE), note_x, top + 48, note_width, 72);
    theme_move(GetDlgItem(window, ID_LOG_LABEL), x, d.log_top, content - 150, 26);
    theme_move(GetDlgItem(window, ID_OPEN_LOG), x + content - 134, d.log_top - 4, 134, 30);
    theme_move(log_window, x, d.log_top + 34, content, max(32, min(112, height - d.log_top - 50)));
    theme_move(progress_window, x, height - 8, content, 3);
    HWND page=confirmation_window?confirmation_window:active_page;
    if (IsWindow(page)) theme_move(page,d.x-24,72,d.width+48,height-72);
    dashboard_visibility();
    theme_repaint(window);
}

static void paint_dashboard(HWND window) {
    PAINTSTRUCT paint; HDC dc = BeginPaint(window, &paint);
    RECT r; GetClientRect(window, &r);
    FillRect(dc, &r, g_theme.background_brush);

    int sidebar_w = 224;
    RECT side = {0, 0, sidebar_w, r.bottom};
    FillRect(dc, &side, g_theme.sidebar_brush);

    HPEN pen = CreatePen(PS_SOLID, 1, g_theme.sidebar_border);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    MoveToEx(dc, sidebar_w, 0, NULL); LineTo(dc, sidebar_w, r.bottom);

    // Divider line in sidebar above bottom buttons
    MoveToEx(dc, 16, r.bottom - 110, NULL); LineTo(dc, sidebar_w - 16, r.bottom - 110);

    DashboardLayout d = dashboard_layout(window);
    if (!active_page && !confirmation_window) {
        theme_card(dc, &d.computer);
        theme_card(dc, &d.timer);
    }

    SelectObject(dc, old_pen);
    DeleteObject(pen);
    EndPaint(window, &paint);
}

static BOOL CALLBACK translate_owned(HWND window, LPARAM lp) {
    wchar_t cls[80] = L"";
    GetClassNameW(window, cls, ARRAYSIZE(cls));
    if (window != (HWND)lp && !wcsncmp(cls, L"WAIO", 4)) {
        SendMessageW(window, WM_APP_LANGUAGE, 0, 0);
        RedrawWindow(window, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    }
    return TRUE;
}

typedef struct { HWND root; BOOL active; } ModalSearch;
static BOOL CALLBACK find_owned_modal(HWND window, LPARAM argument) {
    ModalSearch *search = (ModalSearch *)argument;
    wchar_t cls[80] = L"";
    GetClassNameW(window, cls, ARRAYSIZE(cls));
    if (window != search->root && !IsWindowEnabled(window) &&
        (!wcsncmp(cls, L"WAIO", 4) || !wcscmp(cls, L"#32770"))) {
        search->active = TRUE;
        return FALSE;
    }
    return TRUE;
}

/* A common dialog may disable a secondary window while leaving the root enabled.
   Do not destroy its owner, change languages, or start another task in its nested loop. */
static BOOL owned_modal_active(HWND root) {
    if (confirmation_window || !IsWindowEnabled(root) || (active_page && !IsWindowEnabled(active_page))) return TRUE;
    ModalSearch search = {root, FALSE};
    EnumThreadWindows(GetCurrentThreadId(), find_owned_modal, (LPARAM)&search);
    return search.active;
}

static LRESULT CALLBACK main_proc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    switch (message) {
    case WM_CREATE: {
        main_window = window;
        theme_attach(window);
        HWND heading = app_control(window, L"STATIC", L"", 0, 0, 0, 0, 0, ID_TITLE);
        SendMessageW(heading, WM_SETFONT, (WPARAM)g_font_title, TRUE);
        HWND brand = app_control(window, L"STATIC", L"", 0, 0, 0, 0, 0, ID_BRAND);
        SendMessageW(brand, WM_SETFONT, (WPARAM)g_font_semibold, TRUE);
        HWND logo=app_control(window,L"STATIC",L"",SS_ICON,0,0,32,32,ID_APP_ICON);
        SendMessageW(logo,STM_SETICON,(WPARAM)LoadImageW(g_instance,MAKEINTRESOURCEW(1),IMAGE_ICON,32,32,LR_SHARED),0);
        SetPropW(logo,L"WAIOSidebar",(HANDLE)1);
        SetPropW(logo,L"WAIOCard",(HANDLE)1);
        app_control(window,L"BUTTON",L"",WS_TABSTOP,0,0,0,0,ID_BACK);
        app_control(window,L"BUTTON",L"",WS_TABSTOP,0,0,0,0,ID_ADVANCED);
        SetPropW(app_control(window,L"BUTTON",L"",WS_TABSTOP,0,0,0,0,ID_HOME),L"WAIOSidebar",(HANDLE)1);
        SetPropW(brand, L"WAIOCard", (HANDLE)1);
        SetPropW(brand, L"WAIOSidebar", (HANDLE)1);

        HWND nav_label = app_control(window, L"STATIC", L"", 0, 0, 0, 0, 0, ID_NAV_LABEL);
        SendMessageW(nav_label, WM_SETFONT, (WPARAM)g_font_semibold, TRUE);
        SetPropW(nav_label, L"WAIOCard", (HANDLE)1);
        SetPropW(nav_label, L"WAIOSidebar", (HANDLE)1);
        SetPropW(nav_label, L"WAIOSecondary", (HANDLE)1);

        int labels[] = {ID_PC_LABEL, ID_PC_SUMMARY, ID_PROTECTION_NOTE};
        for (unsigned i = 0; i < ARRAYSIZE(labels); ++i)
            SetPropW(app_control(window, L"STATIC", L"", 0, 0, 0, 0, 0, labels[i]), L"WAIOCard", (HANDLE)1);
        SendMessageW(GetDlgItem(window, ID_PC_LABEL), WM_SETFONT, (WPARAM)g_font_semibold, TRUE);
        HWND quick_label = app_control(window, L"STATIC", L"", 0, 0, 0, 0, 0, ID_QUICK_LABEL);
        SendMessageW(quick_label, WM_SETFONT, (WPARAM)g_font_semibold, TRUE);
        HWND safety_label = app_control(window, L"STATIC", L"", 0, 0, 0, 0, 0, ID_SAFETY_LABEL);
        SendMessageW(safety_label, WM_SETFONT, (WPARAM)g_font_semibold, TRUE);
        SetPropW(safety_label, L"WAIOCard", (HANDLE)1);
        SetPropW(GetDlgItem(window, ID_PROTECTION_NOTE), L"WAIOSecondary", (HANDLE)1);
        const int quick_ids[] = {ID_QUICK_SETUP, ID_QUICK_NETWORK, ID_QUICK_BLOAT};
        const wchar_t *glyphs[] = {L"\uE713", L"\uE774", L"\uE74C"};
        for (int i = 0; i < 3; ++i) {
            HWND action = app_control(window, L"BUTTON", L"", WS_TABSTOP, 0, 0, 0, 0, quick_ids[i]);
            SetPropW(action, L"WAIOQuickAction", (HANDLE)1);
            SetPropW(action, L"WAIOGlyph", (HANDLE)glyphs[i]);
        }

        int nav_buttons[] = {ID_SYSTEM_INFO, ID_SETUP, ID_NETWORK, ID_BLOAT, ID_WIFI, ID_RESTORE, ID_DRIVERS};
        for (int i = 0; i < 7; ++i) {
            HWND btn = app_control(window, L"BUTTON", L"", WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 0, 0, nav_buttons[i]);
            SetPropW(btn, L"WAIOSidebar", (HANDLE)1);
        }

        int bottom_buttons[] = {ID_SYSTEM_PROTECTION, ID_BACKUPS};
        for (int i = 0; i < 2; ++i) {
            HWND btn = app_control(window, L"BUTTON", L"", WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 0, 0, bottom_buttons[i]);
            SetPropW(btn, L"WAIOSidebar", (HANDLE)1);
        }

        HWND details_btn = app_control(window, L"BUTTON", L"", WS_TABSTOP, 0, 0, 0, 0, ID_DETAILS);
        SetPropW(details_btn, L"WAIOCard", (HANDLE)1);

        app_control(window, L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST, 0, 0, 0, 0, ID_THEME);
        app_control(window, L"STATIC", L"", 0, 0, 0, 0, 0, ID_SUBTITLE);
        app_control(window, L"STATIC", L"", 0, 0, 0, 0, 0, ID_ACCOUNT);
        SetPropW(GetDlgItem(window, ID_SUBTITLE), L"WAIOSecondary", (HANDLE)1);
        SetPropW(GetDlgItem(window, ID_ACCOUNT), L"WAIOSecondary", (HANDLE)1);

        HWND language = app_control(window, L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST, 0, 0, 0, 0, ID_LANGUAGE);
        SendMessageW(language, CB_ADDSTRING, 0, (LPARAM)L"Tiếng Việt");
        SendMessageW(language, CB_ADDSTRING, 0, (LPARAM)L"English");
        SendMessageW(language, CB_SETCURSEL, g_vi ? 0 : 1, 0);

        HWND timer_lbl = app_control(window, L"STATIC", L"", 0, 0, 0, 0, 0, ID_TIMER_LABEL);
        SendMessageW(timer_lbl, WM_SETFONT, (WPARAM)g_font_semibold, TRUE);

        HWND delay = app_control(window, L"EDIT", L"30", WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL, 0, 0, 0, 0, ID_DELAY);
        SendMessageW(delay, EM_SETLIMITTEXT, 7, 0);
        app_control(window, L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST, 0, 0, 0, 0, ID_UNIT);
        app_control(window, L"BUTTON", L"", WS_TABSTOP, 0, 0, 0, 0, ID_SCHEDULE);
        app_control(window, L"BUTTON", L"", WS_TABSTOP, 0, 0, 0, 0, ID_CANCEL);
        timer_status = app_control(window, L"STATIC", L"", 0, 0, 0, 0, 0, 0);
        SetPropW(timer_status, L"WAIOCard", (HANDLE)1);

        int card[] = {ID_TIMER_LABEL, ID_DELAY, ID_UNIT, ID_SCHEDULE, ID_CANCEL};
        for (unsigned i = 0; i < ARRAYSIZE(card); ++i) SetPropW(GetDlgItem(window, card[i]), L"WAIOCard", (HANDLE)1);

        HWND log_lbl = app_control(window, L"STATIC", L"", 0, 0, 0, 0, 0, ID_LOG_LABEL);
        SendMessageW(log_lbl, WM_SETFONT, (WPARAM)g_font_semibold, TRUE);
        app_control(window, L"BUTTON", L"", WS_TABSTOP, 0, 0, 0, 0, ID_OPEN_LOG);
        log_window = app_control(window, L"EDIT", L"", WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                                 0, 0, 0, 0, 0);
        SendMessageW(log_window, EM_SETLIMITTEXT, 250000, 0);
        progress_window = app_control(window, PROGRESS_CLASSW, L"", PBS_MARQUEE, 0, 0, 0, 0, 0);
        ShowWindow(progress_window, SW_HIDE);
        update_labels(window); layout_main(window);
        if (!smoke_mode) SetTimer(window, 1, 1000, NULL);
        app_log(L"Win AIO Optimize %s. %s", APP_VERSION, tr(L"Sẵn sàng. Chưa áp dụng thay đổi nào.", L"Ready. No changes have been applied."));
        return 0;
    }
    case WM_COMMAND:
        if (smoke_mode && LOWORD(wp)!=ID_THEME && LOWORD(wp)!=ID_LANGUAGE && LOWORD(wp)!=ID_HOME && LOWORD(wp)!=ID_BACK &&
            LOWORD(wp)!=IDCANCEL && LOWORD(wp)!=ID_ADVANCED && LOWORD(wp)!=ID_SETUP && LOWORD(wp)!=ID_NETWORK && LOWORD(wp)!=ID_BLOAT &&
            LOWORD(wp)!=ID_DETAILS && LOWORD(wp)!=ID_SYSTEM_INFO && LOWORD(wp)!=ID_QUICK_SETUP && LOWORD(wp)!=ID_QUICK_NETWORK && LOWORD(wp)!=ID_QUICK_BLOAT) return 0;
        if (owned_modal_active(window)) {
            if (LOWORD(wp) == ID_LANGUAGE && HIWORD(wp) == CBN_SELCHANGE)
                SendDlgItemMessageW(window, ID_LANGUAGE, CB_SETCURSEL, g_vi ? 0 : 1, 0);
            return 0;
        }
        if (app_busy(NULL) && LOWORD(wp)!=ID_LANGUAGE && LOWORD(wp)!=ID_THEME) return 0;
        switch (LOWORD(wp)) {
        case ID_SYSTEM_INFO:
        case ID_DETAILS: system_info_show(window); break;
        case ID_HOME:
        case ID_BACK:
        case IDCANCEL: app_home(); break;
        case ID_ADVANCED:
            dashboard_advanced=!dashboard_advanced; update_labels(window); layout_main(window); break;
        case ID_THEME:
            if (HIWORD(wp) == CBN_SELCHANGE) theme_set((int)SendDlgItemMessageW(window, ID_THEME, CB_GETCURSEL, 0, 0), !smoke_mode);
            break;
        case ID_BACKUPS: {
            wchar_t path[MAX_PATH];
            if (app_data_path(path, ARRAYSIZE(path), NULL)) ShellExecuteW(window, L"open", path, NULL, NULL, SW_SHOWNORMAL);
            break;
        }
        case ID_SYSTEM_PROTECTION: {
            wchar_t path[MAX_PATH];
            UINT n = GetSystemDirectoryW(path, ARRAYSIZE(path));
            if (n && n < ARRAYSIZE(path) && SUCCEEDED(StringCchCatW(path, ARRAYSIZE(path), L"\\SystemPropertiesProtection.exe")))
                if ((INT_PTR)ShellExecuteW(window, L"open", path, NULL, NULL, SW_SHOWNORMAL) <= 32)
                    app_error(window, L"System Protection", ERROR_OPEN_FAILED);
            break;
        }
        case ID_QUICK_SETUP:
        case ID_SETUP: optimizer_show(window); break;
        case ID_QUICK_NETWORK:
        case ID_NETWORK: network_show(window); break;
        case ID_QUICK_BLOAT:
        case ID_BLOAT: bloatware_show(window); break;
        case ID_WIFI: backup_wifi(window); break;
        case ID_RESTORE: restore_wifi(window); break;
        case ID_DRIVERS: backup_drivers(window); break;
        case ID_SCHEDULE: {
            wchar_t value[32]; GetDlgItemTextW(window, ID_DELAY, value, ARRAYSIZE(value));
            shutdown_schedule(window, value, SendDlgItemMessageW(window, ID_UNIT, CB_GETCURSEL, 0, 0) == 1); break;
        }
        case ID_CANCEL: shutdown_cancel(window); break;
        case ID_OPEN_LOG:
            if ((INT_PTR)ShellExecuteW(window, L"open", log_path, NULL, NULL, SW_SHOWNORMAL) <= 32)
                app_error(window, tr(L"Mở nhật ký", L"Open log"), ERROR_OPEN_FAILED);
            break;
        case ID_LANGUAGE:
            if (HIWORD(wp) == CBN_SELCHANGE) {
                if (app_busy(NULL)) {
                    SendDlgItemMessageW(window, ID_LANGUAGE, CB_SETCURSEL, g_vi ? 0 : 1, 0);
                    break;
                }
                g_vi = SendDlgItemMessageW(window, ID_LANGUAGE, CB_GETCURSEL, 0, 0) == 0;
                language_thread();
                update_labels(window);
                EnumChildWindows(window, translate_owned, (LPARAM)window);
                RedrawWindow(window, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
                language_save(window);
            }
            break;
        }
        return 0;
    case WM_APP_LOG: {
        wchar_t *text = (wchar_t *)lp;
        int length = GetWindowTextLengthW(log_window);
        if (length > 200000) { SetWindowTextW(log_window, L""); length = 0; }
        SendMessageW(log_window, EM_SETSEL, length, length);
        SendMessageW(log_window, EM_REPLACESEL, FALSE, (LPARAM)text);
        SendMessageW(log_window, EM_SCROLLCARET, 0, 0);
        free(text); return 0;
    }
    case WM_APP_JOB_DONE: app_finish_job(window); return 0;
    case WM_TIMER: {
        if (wp == 2) { PostMessageW(window, WM_CLOSE, 0, 0); return 0; }
        if (smoke_mode) return 0;
        wchar_t status[512]; shutdown_status(status, ARRAYSIZE(status));
        SetWindowTextW(timer_status, status);
        static unsigned ticks;
        if (++ticks % 30 == 0) { wchar_t summary[768]; system_summary(summary, ARRAYSIZE(summary)); SetDlgItemTextW(window, ID_PC_SUMMARY, summary); }
        return 0;
    }
    case WM_SIZE: if (log_window) layout_main(window); return 0;
    case WM_GETMINMAXINFO:
        ((MINMAXINFO *)lp)->ptMinTrackSize.x = 1040; ((MINMAXINFO *)lp)->ptMinTrackSize.y = 740; return 0;
    case WM_PAINT: paint_dashboard(window); return 0;
    case WM_CLOSE:
        if (app_busy(NULL)) {
            MessageBoxW(window, tr(L"Hãy chờ tác vụ hoàn tất trước khi đóng.", L"Wait for the current task to finish before closing."),
                        L"Win AIO Optimize", MB_OK | MB_ICONINFORMATION); return 0;
        }
        if (owned_modal_active(window)) return 0;
        closing_main=TRUE; DestroyWindow(window); return 0;
    case WM_DESTROY: KillTimer(window, 1); main_window = NULL; PostQuitMessage(smoke_failed ? 1 : 0); return 0;
    }
    return DefWindowProcW(window, message, wp, lp);
}

static BOOL CALLBACK count_app_windows(HWND window, LPARAM argument) {
    wchar_t cls[80]; GetClassNameW(window,cls,ARRAYSIZE(cls));
    if (!wcsncmp(cls,L"WAIO",4)) ++*(unsigned *)argument;
    return TRUE;
}
static BOOL navigation_ui_test(HWND root) {
    void (*pages[])(HWND)={optimizer_show,network_show,bloatware_show,system_info_show};
    BOOL ok=TRUE;
    app_home();
    if (GetWindowLongPtrW(log_window,GWL_STYLE)&WS_VISIBLE) ok=FALSE;
    SendMessageW(root,WM_COMMAND,ID_ADVANCED,0);
    if (!(GetWindowLongPtrW(log_window,GWL_STYLE)&WS_VISIBLE)) ok=FALSE;
    SendMessageW(root,WM_COMMAND,ID_ADVANCED,0);
    for (unsigned i=0;i<ARRAYSIZE(pages);++i) {
        pages[i](root); HWND page=active_page;
        if (!IsWindow(page) || GetParent(page)!=root || GetAncestor(page,GA_ROOT)!=root ||
            (GetWindowLongPtrW(page,GWL_STYLE)&WS_CAPTION)) { ok=FALSE; continue; }
        unsigned top_levels=0;
        EnumThreadWindows(GetCurrentThreadId(),count_app_windows,(LPARAM)&top_levels);
        if (top_levels!=1) ok=FALSE;
        const int sizes[][2]={{1040,740},{1920,1080},{1180,840}};
        for (unsigned s=0;s<ARRAYSIZE(sizes);++s) {
            SetWindowPos(root,NULL,0,0,sizes[s][0],sizes[s][1],SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
            RECT page_rect,back_rect;
            GetWindowRect(page,&page_rect); GetWindowRect(GetDlgItem(root,ID_BACK),&back_rect);
            if (page_rect.left+24!=back_rect.left || page_rect.right-page_rect.left>1168) ok=FALSE;
        }
        InterlockedExchange(&running_job,1);
        app_home(); SendMessageW(page,WM_CLOSE,0,0);
        if (active_page!=page) ok=FALSE;
        InterlockedExchange(&running_job,0);
        SendMessageW(page,WM_CLOSE,0,0);
        if (!IsWindow(root) || !IsWindow(page) || active_page || (GetWindowLongPtrW(page,GWL_STYLE)&WS_VISIBLE)) ok=FALSE;
        pages[i](root);
        if (active_page!=page) ok=FALSE;
        SendMessageW(root,WM_COMMAND,IDCANCEL,0);
        if (!IsWindow(root) || active_page) ok=FALSE;
        DestroyWindow(page);
    }
    return ok;
}

static BOOL elevation_arguments(wchar_t *text, size_t capacity, BOOL vietnamese, int theme) {
    const wchar_t *appearance = theme == 1 ? L" --theme=light" : theme == 2 ? L" --theme=dark" : L"";
    return SUCCEEDED(StringCchPrintfW(text, capacity, L"--lang=%s%s", vietnamese ? L"vi" : L"en", appearance));
}

static int self_test(void) {
    unsigned checks = 0, failed = 0; DWORD seconds = 0;
    wchar_t arguments[64];
#define CHECK(x) do { ++checks; if (!(x)) { ++failed; fprintf(stderr, "FAIL: %s\n", #x); } } while (0)
    CHECK(backup_valid_delay(L"1", FALSE, &seconds) && seconds == 60);
    CHECK(backup_valid_delay(L"2", TRUE, &seconds) && seconds == 7200);
    CHECK(!backup_valid_delay(L"0", FALSE, &seconds));
    CHECK(!backup_valid_delay(L"-1", FALSE, &seconds));
    CHECK(!backup_valid_delay(L"1.5", FALSE, &seconds));
    CHECK(!backup_valid_delay(L"1 & whoami", FALSE, &seconds));
    CHECK(!backup_valid_delay(L"99999999999999999999999", TRUE, &seconds));
    CHECK(!backup_valid_delay(L"", FALSE, &seconds));
    CHECK(backup_valid_delay(L"10080", FALSE, &seconds) && seconds == 604800);
    CHECK(backup_valid_delay(L"168", TRUE, &seconds) && seconds == 604800);
    CHECK(!backup_valid_delay(L"10081", FALSE, &seconds));
    CHECK(!backup_valid_delay(L"169", TRUE, &seconds));
    CHECK(!backup_valid_delay(L" 2", TRUE, &seconds));
    CHECK(!backup_valid_delay(NULL, TRUE, &seconds));
    CHECK(!backup_valid_delay(L"1", FALSE, NULL));
    CHECK(network_valid_dns(L"1.1.1.1", 2));
    CHECK(network_valid_dns(L"2606:4700:4700::1111", 23));
    CHECK(!network_valid_dns(L"300.1.1.1", 2));
    CHECK(!network_valid_dns(L"1.1.1.1 & calc", 2));
    CHECK(!network_valid_dns(L"", 2));
    CHECK(!network_valid_dns(L"2001:::1", 23));
    CHECK(network_self_test());
    CHECK(optimizer_self_test());
    CHECK(protection_self_test());
    CHECK(system_info_self_test());
    CHECK(elevation_arguments(arguments, ARRAYSIZE(arguments), FALSE, 1) && !wcscmp(arguments, L"--lang=en --theme=light"));
    CHECK(elevation_arguments(arguments, ARRAYSIZE(arguments), TRUE, 2) && !wcscmp(arguments, L"--lang=vi --theme=dark"));
    CHECK(elevation_arguments(arguments, ARRAYSIZE(arguments), TRUE, -1) && !wcscmp(arguments, L"--lang=vi"));
    CHECK(!elevation_arguments(arguments, 4, FALSE, 1));
    CHECK(app_run_tool(L"cmd.exe", L"/c exit 0") == ERROR_ACCESS_DENIED);
    CHECK(app_run_tool(L"..\\netsh.exe", L"") == ERROR_ACCESS_DENIED);
    CHECK(app_run_tool(L"netsh.exe", L"interface ipv4 reset") == ERROR_ACCESS_DENIED);
    CHECK(protection_prepare(TRUE, TRUE) == ERROR_ACCESS_DENIED);
#undef CHECK
    printf("Read-only self-test: %u checks, %u failures. No system settings changed.\n", checks, failed);
    return failed ? 1 : 0;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR command, int show) {
    (void)previous; (void)command;
    g_instance = instance;
    int argc = 0; wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    BOOL test = FALSE, smoke = FALSE, inventory = FALSE, preview = FALSE, preview_setup = FALSE;
    BOOL preview_network = FALSE, preview_info = FALSE;
    int language_override = -1;
    int theme_override = -1;
    for (int i = 1; argv && i < argc; ++i) {
        if (!wcscmp(argv[i], L"--self-test")) test = TRUE;
        else if (!wcscmp(argv[i], L"--smoke-test")) smoke = TRUE;
        else if (!wcscmp(argv[i], L"--preview")) { preview = TRUE; smoke = TRUE; }
        else if (!wcscmp(argv[i], L"--preview=setup")) { preview = TRUE; smoke = TRUE; preview_setup = TRUE; }
        else if (!wcscmp(argv[i], L"--preview=network")) { preview = TRUE; smoke = TRUE; preview_network = TRUE; }
        else if (!wcscmp(argv[i], L"--preview=info")) { preview = TRUE; smoke = TRUE; preview_info = TRUE; }
        else if (!wcscmp(argv[i], L"--inventory-test")) inventory = TRUE;
        else if (!wcscmp(argv[i], L"--theme=light")) theme_override = 1;
        else if (!wcscmp(argv[i], L"--theme=dark")) theme_override = 2;
        else if (!wcscmp(argv[i], L"--english") || !wcscmp(argv[i], L"--lang=en")) language_override = 0;
        else if (!wcscmp(argv[i], L"--lang=vi")) language_override = 1;
        else { LocalFree(argv); return ERROR_INVALID_PARAMETER; }
    }
    if (argv) LocalFree(argv);
    if (!test && !smoke && !inventory) language_load();
    if (language_override >= 0) g_vi = language_override != 0;
    language_thread();
    /* Every diagnostic entry point uses the same side-effect guards. */
    smoke_mode = test || smoke || inventory;
    if (test) return self_test();
    if (inventory) {
        HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
        if (FAILED(hr)) return 1;
        hr = protection_com_security();
        int code = FAILED(hr) ? 1 : system_info_inventory_test();
        CoUninitialize(); return code;
    }
    if (app_windows_build() < 10240) {
        MessageBoxW(NULL, tr(L"Ứng dụng yêu cầu Windows 10 hoặc Windows 11.", L"Windows 10 or Windows 11 is required."), L"Win AIO Optimize", MB_OK | MB_ICONERROR); return 1;
    }
    if (!smoke && !is_admin()) {
        wchar_t path[MAX_PATH];
        DWORD length = GetModuleFileNameW(NULL, path, ARRAYSIZE(path));
        if (!length || length >= ARRAYSIZE(path)) return ERROR_FILENAME_EXCED_RANGE;
        wchar_t parameters[64];
        if (!elevation_arguments(parameters, ARRAYSIZE(parameters), g_vi, theme_override)) return ERROR_INSUFFICIENT_BUFFER;
        SHELLEXECUTEINFOW execute = {0}; execute.cbSize = sizeof(execute);
        execute.lpVerb = L"runas"; execute.lpFile = path;
        execute.lpParameters = parameters; execute.nShow = SW_SHOWNORMAL;
        if (!ShellExecuteExW(&execute)) { app_error(NULL, tr(L"Cần quyền quản trị", L"Administrator rights are required"), GetLastError()); return 1; }
        return 0;
    }
    HANDLE mutex = CreateMutexW(NULL, TRUE, smoke ? NULL : L"Local\\Doctoten.WinAIOOptimize.6");
    if (!mutex) return (int)GetLastError();
    if (!smoke && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(L"WAIO.Main", NULL);
        if (existing) { ShowWindow(existing, SW_RESTORE); SetForegroundWindow(existing); }
        CloseHandle(mutex); return 0;
    }
    HRESULT com = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(com)) { CloseHandle(mutex); return 1; }
    HRESULT security = protection_com_security();
    if (FAILED(security)) {
        app_error(NULL, tr(L"Khởi tạo bảo mật COM", L"Initialize COM security"), (DWORD)security);
        CoUninitialize(); CloseHandle(mutex); return 1;
    }
    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS};
    InitCommonControlsEx(&controls);
    /* Manifest GDI scaling keeps fixed-layout utility dialogs readable at high DPI. */
    InitializeCriticalSection(&log_lock);
    if (!smoke) {
        log_ready = app_data_path(log_path, ARRAYSIZE(log_path), L"activity.log");
        WIN32_FILE_ATTRIBUTE_DATA info;
        if (log_ready && GetFileAttributesExW(log_path, GetFileExInfoStandard, &info) &&
            (info.nFileSizeHigh || info.nFileSizeLow > 4 * 1024 * 1024)) {
            wchar_t old[MAX_PATH];
            if (app_data_path(old, ARRAYSIZE(old), L"activity.previous.log")) MoveFileExW(log_path, old, MOVEFILE_REPLACE_EXISTING);
        }
    }
    g_font = CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                         OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    g_font_semibold = CreateFontW(-15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    g_font_title = CreateFontW(-30, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                               OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    g_font_subtitle = CreateFontW(-18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    title_font = g_font_title;
    theme_init();
    if (theme_override >= 0) theme_set(theme_override, FALSE);
    WNDCLASSW cls = {0}; cls.lpfnWndProc = main_proc; cls.hInstance = instance;
    cls.hCursor = LoadCursorW(NULL, IDC_ARROW); cls.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    cls.lpszClassName = L"WAIO.Main";
    RegisterClassW(&cls);
    HWND window = CreateWindowExW(WS_EX_CONTROLPARENT, cls.lpszClassName, L"Win AIO Optimize", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                  CW_USEDEFAULT, CW_USEDEFAULT, 1180, 840, NULL, NULL, instance, NULL);
    int result = 1;
    if (window) {
        ShowWindow(window, smoke && !preview ? SW_HIDE : show); UpdateWindow(window);
        if (preview) SetTimer(window, 2, 60000, NULL);
        if (preview_setup) optimizer_preview(window);
        if (preview_network) network_preview(window);
        if (preview_info) system_info_preview(window);
        if (smoke && !preview) {
            BOOL original = g_vi, ok = TRUE;
            SetDlgItemTextW(window, ID_DELAY, L"42");
            SendDlgItemMessageW(window, ID_UNIT, CB_SETCURSEL, 1, 0);
            for (int lang = 0; lang < 2; ++lang) {
                wchar_t text[80]; g_vi = lang != 0; update_labels(window);
                GetDlgItemTextW(window, ID_SETUP, text, ARRAYSIZE(text));
                if (wcscmp(text, tr(L"Thiết lập Windows", L"Windows Setup"))) ok = FALSE;
                GetDlgItemTextW(window, ID_DELAY, text, ARRAYSIZE(text));
                if (wcscmp(text, L"42") || SendDlgItemMessageW(window, ID_UNIT, CB_GETCURSEL, 0, 0) != 1) ok = FALSE;
            }
            printf("Main window translation/state: %s\n", ok ? "PASS" : "FAIL"); fflush(stdout);
            BOOL network_ok = network_ui_test(window);
            printf("Network translation/state: %s\n", network_ok ? "PASS" : "FAIL"); fflush(stdout);
            BOOL optimizer_ok = optimizer_ui_test(window);
            printf("Optimizer translation/state: %s\n", optimizer_ok ? "PASS" : "FAIL"); fflush(stdout);
            BOOL info_ok = system_info_ui_test(window);
            printf("System information translation/state: %s\n", info_ok ? "PASS" : "FAIL"); fflush(stdout);
            BOOL consent_ok = confirmation_ui_test(window);
            printf("Recovery consent failure/cancel: %s\n", consent_ok ? "PASS" : "FAIL"); fflush(stdout);
            BOOL navigation_ok=navigation_ui_test(window);
            printf("Single-window navigation/close/Advanced/busy guard: %s\n",navigation_ok?"PASS":"FAIL"); fflush(stdout);
            ok = ok && network_ok && optimizer_ok && info_ok && consent_ok && navigation_ok && !smoke_failed;
            g_vi = original; smoke_failed = !ok;
            printf("Read-only UI/language test: %s. No settings changed.\n", ok ? "PASS" : "FAIL");
            PostMessageW(window, WM_CLOSE, 0, 0);
        }
        MSG msg; int status;
        while ((status = (int)GetMessageW(&msg, NULL, 0, 0)) > 0) {
            HWND root = GetAncestor(msg.hwnd, GA_ROOT);
            if (msg.message==WM_KEYDOWN && msg.wParam==VK_ESCAPE && root==main_window && !confirmation_window) {
                app_home();
            } else if (!root || !IsDialogMessageW(root, &msg)) {
                TranslateMessage(&msg); DispatchMessageW(&msg);
            }
        }
        result = status < 0 ? 1 : (int)msg.wParam;
    }
    theme_destroy(); DeleteObject(g_font_subtitle); DeleteObject(g_font_title); DeleteObject(g_font_semibold); DeleteObject(g_font);
    log_ready = FALSE; DeleteCriticalSection(&log_lock); CoUninitialize();
    ReleaseMutex(mutex); CloseHandle(mutex);
    return result;
}
