#ifndef WAIO_APP_H
#define WAIO_APP_H
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <commctrl.h>
#include <strsafe.h>
#include <stdint.h>
#include <wchar.h>
#include <stdlib.h>

#define APP_VERSION L"6.0.1"
#define WM_APP_JOB_DONE (WM_APP + 1)
#define WM_APP_LOG (WM_APP + 2)
#define WM_APP_LANGUAGE (WM_APP + 3)
extern HINSTANCE g_instance;
extern HFONT g_font;
extern HFONT g_font_semibold;
extern HFONT g_font_title;
extern HFONT g_font_subtitle;
extern BOOL g_vi;
const wchar_t *tr(const wchar_t *vi, const wchar_t *en);
HWND app_control(HWND parent, const wchar_t *cls, const wchar_t *text, DWORD style,
                 int x, int y, int w, int h, int id);
void app_log(const wchar_t *format, ...);
void app_error(HWND owner, const wchar_t *context, DWORD error);
BOOL app_confirm(HWND owner, const wchar_t *details);
BOOL app_pick_folder(HWND owner, wchar_t *path, size_t capacity);
BOOL app_start_job(HWND owner, void (*work)(void *), void *context);
void app_finish_job(HWND owner);
BOOL app_busy(HWND owner);
/* Executes only an absolute System32 executable; arguments must be quoted by caller.
   Returns the actual process exit code, or a Win32 error on failure. Output is logged. */
DWORD app_run_tool(const wchar_t *exe_name, const wchar_t *arguments);
BOOL app_data_path(wchar_t *path, size_t capacity, const wchar_t *filename);
DWORD app_windows_build(void);
void network_show(HWND owner);
void optimizer_show(HWND owner);
void bloatware_show(HWND owner);
void backup_wifi(HWND owner);
void restore_wifi(HWND owner);
void backup_drivers(HWND owner);
/* Validation shared with read-only self-tests. */
BOOL network_valid_dns(const wchar_t *address, int family);
BOOL network_self_test(void);
BOOL backup_valid_delay(const wchar_t *text, BOOL hours, DWORD *seconds);
void shutdown_schedule(HWND owner, const wchar_t *value, BOOL hours);
void shutdown_cancel(HWND owner);
void shutdown_status(wchar_t *text, size_t capacity);
BOOL optimizer_self_test(void);
BOOL app_is_smoke_test(void);
/* Feature pages stay inside the main window. Close/back returns to the dashboard. */
HWND app_main_window(void);
void app_show_page(HWND page);
void app_home(void);
void app_page_destroyed(HWND page);
BOOL network_ui_test(HWND owner);
BOOL optimizer_ui_test(HWND owner);
BOOL app_confirm_setup(HWND owner, const wchar_t *details, BOOL registry, BOOL restore);
void optimizer_preview(HWND owner);
void network_preview(HWND owner);
void system_info_preview(HWND owner);
HRESULT protection_com_security(void);
DWORD protection_prepare(BOOL registry, BOOL restore);
BOOL protection_self_test(void);
void system_info_show(HWND owner);
void system_summary(wchar_t *text, size_t capacity);
BOOL system_info_self_test(void);
BOOL system_info_ui_test(HWND owner);
int system_info_inventory_test(void);
#endif
