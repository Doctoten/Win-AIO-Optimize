#ifndef WAIO_THEME_H
#define WAIO_THEME_H
#include "app.h"
typedef struct {
    COLORREF background, surface, text, muted, accent, border, hover;
    COLORREF sidebar_bg, sidebar_border, accent_hover;
    HBRUSH background_brush, surface_brush, sidebar_brush;
    HFONT font_icon;
    BOOL dark, high_contrast;
    int mode; /* 0 Windows, 1 light, 2 dark */
} AppTheme;
extern AppTheme g_theme;
void theme_init(void);
void theme_destroy(void);
void theme_set(int mode, BOOL save);
void theme_refresh(void);
void theme_attach(HWND window);
void theme_control(HWND control);
void theme_card_r(HDC dc, const RECT *rect, int radius);
void theme_card(HDC dc, const RECT *rect);
/* Place controls without copying stale pixels; repaint only after layout is complete. */
void theme_move(HWND control, int x, int y, int width, int height);
void theme_report_layout(HWND list, int x, int y, int width, int height);
void theme_repaint(HWND window);
BOOL theme_list_readable(HWND list, int columns);
#endif
