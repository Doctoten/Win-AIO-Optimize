#include "theme.h"
#include <dwmapi.h>
#include <uxtheme.h>
#include <windowsx.h>

AppTheme g_theme;
static LRESULT CALLBACK themed_proc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
static LRESULT CALLBACK report_heading_proc(HWND, UINT, WPARAM, LPARAM);

static void colors(void) {
    DWORD light = 1, size = sizeof(light);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &light, &size);
    HIGHCONTRASTW contrast = {sizeof(contrast), 0, NULL};
    SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0);
    g_theme.high_contrast = (contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
    g_theme.dark = g_theme.mode == 2 || (g_theme.mode == 0 && !light);
    g_theme.background = g_theme.dark ? RGB(32, 32, 32) : RGB(243, 243, 243);
    g_theme.sidebar_bg = g_theme.dark ? RGB(26, 26, 26) : RGB(238, 238, 238);
    g_theme.surface = g_theme.dark ? RGB(44, 44, 44) : RGB(255, 255, 255);
    g_theme.text = g_theme.dark ? RGB(248, 248, 248) : RGB(26, 26, 26);
    g_theme.muted = g_theme.dark ? RGB(160, 160, 160) : RGB(105, 105, 105);
    g_theme.accent = g_theme.dark ? RGB(96, 205, 255) : RGB(0, 103, 192);
    g_theme.accent_hover = g_theme.dark ? RGB(118, 215, 255) : RGB(24, 118, 204);
    g_theme.border = g_theme.dark ? RGB(58, 58, 58) : RGB(228, 228, 228);
    g_theme.sidebar_border = g_theme.dark ? RGB(45, 45, 45) : RGB(225, 225, 225);
    g_theme.hover = g_theme.dark ? RGB(52, 52, 52) : RGB(246, 246, 246);
    if (g_theme.high_contrast) {
        g_theme.background = g_theme.surface = g_theme.sidebar_bg = GetSysColor(COLOR_WINDOW);
        g_theme.text = GetSysColor(COLOR_WINDOWTEXT);
        g_theme.muted = GetSysColor(COLOR_WINDOWTEXT);
        g_theme.accent = g_theme.accent_hover = GetSysColor(COLOR_HIGHLIGHT);
        g_theme.border = g_theme.sidebar_border = GetSysColor(COLOR_WINDOWTEXT);
        g_theme.hover = GetSysColor(COLOR_BTNFACE);
    }
    if (g_theme.background_brush) DeleteObject(g_theme.background_brush);
    if (g_theme.surface_brush) DeleteObject(g_theme.surface_brush);
    if (g_theme.sidebar_brush) DeleteObject(g_theme.sidebar_brush);
    g_theme.background_brush = CreateSolidBrush(g_theme.background);
    g_theme.surface_brush = CreateSolidBrush(g_theme.surface);
    g_theme.sidebar_brush = CreateSolidBrush(g_theme.sidebar_bg);
}

void theme_card_r(HDC dc, const RECT *rect, int radius) {
    HPEN pen = CreatePen(PS_SOLID, 1, g_theme.high_contrast ? g_theme.border : g_theme.surface);
    HGDIOBJ previous_pen = SelectObject(dc, pen), previous_brush = SelectObject(dc, g_theme.surface_brush);
    RoundRect(dc, rect->left, rect->top, rect->right, rect->bottom, radius, radius);
    SelectObject(dc, previous_pen); SelectObject(dc, previous_brush); DeleteObject(pen);
}

void theme_card(HDC dc, const RECT *rect) {
    theme_card_r(dc, rect, 16);
}

void theme_move(HWND control, int x, int y, int width, int height) {
    if (control) SetWindowPos(control,NULL,x,y,max(0,width),max(0,height),
        SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW|SWP_NOCOPYBITS);
}

void theme_report_layout(HWND list, int x, int y, int width, int height) {
    HWND heading=(HWND)GetPropW(list,L"WAIOReportHeading");
    if (heading) {
        theme_move(heading,x,y,width,36);
        theme_move(list,x,y+36,width,max(0,height-36));
    } else theme_move(list,x,y,width,height);
}

void theme_repaint(HWND window) {
    if (window) RedrawWindow(window,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_FRAME|RDW_ALLCHILDREN);
}

/* Read-only regression invariant: rows contain their text and report columns fit. */
BOOL theme_list_readable(HWND list, int columns) {
    RECT client, row; GetClientRect(list, &client);
    int total = 0;
    for (int i = 0; i < columns; ++i) total += ListView_GetColumnWidth(list, i);
    if (total > client.right || !ListView_GetItemRect(list, 0, &row, LVIR_BOUNDS)) return FALSE;
    HDC dc = GetDC(list); HGDIOBJ font = SelectObject(dc, g_font);
    TEXTMETRICW metric; BOOL measured = GetTextMetricsW(dc, &metric);
    SelectObject(dc, font); ReleaseDC(list, dc);
    HWND heading = (HWND)GetPropW(list, L"WAIOReportHeading");
    RECT header, table; GetWindowRect(heading, &header); GetWindowRect(list, &table);
    return measured && IsWindow(heading) && row.bottom - row.top >= metric.tmHeight + 8 &&
        row.top >= 0 && header.bottom == table.top;
}

static void round_control(HWND window) {
    wchar_t cls[64] = L""; GetClassNameW(window, cls, ARRAYSIZE(cls));
    if (lstrcmpiW(cls, L"EDIT") && lstrcmpiW(cls, L"COMBOBOX") && wcscmp(cls, WC_LISTVIEWW)) return;
    RECT r; GetWindowRect(window, &r);
    if (r.right <= r.left || r.bottom <= r.top) return;
    HRGN region = CreateRoundRectRgn(0, 0, r.right - r.left + 1, r.bottom - r.top + 1, 12, 12);
    if (region && !wcscmp(cls,WC_LISTVIEWW)) {
        /* The separate heading owns the top corners; the report joins it flush. */
        HRGN top=CreateRectRgn(0,0,r.right-r.left+1,6);
        if (top) { CombineRgn(region,region,top,RGN_OR); DeleteObject(top); }
    }
    if (region && !SetWindowRgn(window, region, FALSE)) DeleteObject(region);
}

static void paint_frame(HWND window, HDC dc) {
    RECT bounds; GetWindowRect(window, &bounds);
    RECT r = {0, 0, bounds.right - bounds.left, bounds.bottom - bounds.top};
    LONG_PTR style = GetWindowLongPtrW(window, GWL_STYLE);
    wchar_t cls[64]; GetClassNameW(window,cls,ARRAYSIZE(cls));
    BOOL input=!lstrcmpiW(cls,L"EDIT") && !(style&ES_READONLY);
    if (g_theme.high_contrast || (wcscmp(cls,WC_LISTVIEWW) && GetFocus()==window) || input) {
        HPEN pen = CreatePen(PS_SOLID, 1, GetFocus() == window ? g_theme.accent : g_theme.border);
        HGDIOBJ previous_pen = SelectObject(dc, pen), previous_brush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        RoundRect(dc, 0, 0, r.right, r.bottom, 12, 12);
        SelectObject(dc, previous_pen); SelectObject(dc, previous_brush); DeleteObject(pen);
    }
    for (int axis = 0; axis < 2; ++axis) {
        if (!(style & (axis ? WS_HSCROLL : WS_VSCROLL))) continue;
        SCROLLBARINFO bar = {0}; bar.cbSize = sizeof(bar);
        if (!GetScrollBarInfo(window, axis ? OBJID_HSCROLL : OBJID_VSCROLL, &bar) ||
            (bar.rgstate[0] & STATE_SYSTEM_INVISIBLE)) continue;
        RECT track = bar.rcScrollBar; OffsetRect(&track, -bounds.left, -bounds.top);
        FillRect(dc, &track, g_theme.surface_brush);
        if (bar.xyThumbBottom <= bar.xyThumbTop) continue;
        RECT thumb = track;
        if (axis) {
            thumb.left = track.left + bar.xyThumbTop; thumb.right = track.left + bar.xyThumbBottom;
            thumb.top += 4; thumb.bottom -= 4;
        } else {
            thumb.top = track.top + bar.xyThumbTop; thumb.bottom = track.top + bar.xyThumbBottom;
            thumb.left += 4; thumb.right -= 4;
        }
        HBRUSH brush = CreateSolidBrush(g_theme.muted);
        HGDIOBJ old_brush = SelectObject(dc, brush), old_pen = SelectObject(dc, GetStockObject(NULL_PEN));
        RoundRect(dc, thumb.left, thumb.top, thumb.right, thumb.bottom, 6, 6);
        SelectObject(dc, old_brush); SelectObject(dc, old_pen); DeleteObject(brush);
    }
}

static void paint_combo(HWND window, HDC dc) {
    RECT r; GetClientRect(window, &r);
    FillRect(dc, &r, GetPropW(window, L"WAIOCard") ? g_theme.surface_brush : g_theme.background_brush);

    BOOL focused = (GetFocus() == window);
    BOOL hover = (GetPropW(window, L"WAIOHover") != NULL);
    COLORREF border_color = focused ? g_theme.accent : (hover ? (g_theme.dark ? RGB(100, 100, 100) : RGB(180, 180, 180)) : g_theme.border);
    HPEN pen = CreatePen(PS_SOLID, 1, border_color);
    HGDIOBJ prev_pen = SelectObject(dc, pen);
    HGDIOBJ prev_brush = SelectObject(dc, g_theme.surface_brush);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, 12, 12);
    SelectObject(dc, prev_pen);
    SelectObject(dc, prev_brush);
    DeleteObject(pen);

    HGDIOBJ font = SelectObject(dc, (HFONT)SendMessageW(window, WM_GETFONT, 0, 0));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, IsWindowEnabled(window) ? g_theme.text : g_theme.muted);
    wchar_t text[256] = L"";
    LRESULT selected = SendMessageW(window, CB_GETCURSEL, 0, 0);
    if (selected >= 0 && SendMessageW(window, CB_GETLBTEXTLEN, selected, 0) < 256)
        SendMessageW(window, CB_GETLBTEXT, selected, (LPARAM)text);
    RECT label = r; label.left += 12; label.right -= 28;
    DrawTextW(dc, text, -1, &label, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    HPEN arrow_pen = CreatePen(PS_SOLID, 1, IsWindowEnabled(window) ? g_theme.text : g_theme.muted);
    prev_pen = SelectObject(dc, arrow_pen);
    int x = r.right - 14, y = r.bottom / 2;
    MoveToEx(dc, x - 4, y - 2, NULL); LineTo(dc, x, y + 2); LineTo(dc, x + 4, y - 2);
    SelectObject(dc, prev_pen);
    DeleteObject(arrow_pen);
    SelectObject(dc, font);
}

static void paint_quick_action(HWND window, HDC dc) {
    RECT r; GetClientRect(window, &r);
    BOOL hover = GetPropW(window, L"WAIOHover") != NULL;
    BOOL focus = GetFocus() == window;
    FillRect(dc, &r, g_theme.background_brush);
    HPEN pen = CreatePen(PS_SOLID, 1, hover || focus ? g_theme.accent : (g_theme.high_contrast ? g_theme.border : g_theme.surface));
    HBRUSH brush = CreateSolidBrush(hover ? g_theme.hover : g_theme.surface);
    HGDIOBJ old_pen = SelectObject(dc, pen), old_brush = SelectObject(dc, brush);
    RoundRect(dc, 0, 0, r.right, r.bottom, 16, 16);
    SelectObject(dc, old_pen); SelectObject(dc, old_brush);
    DeleteObject(pen); DeleteObject(brush);
    int saved = SaveDC(dc);
    SetBkMode(dc, TRANSPARENT);
    const wchar_t *glyph = (const wchar_t *)GetPropW(window, L"WAIOGlyph");
    RECT icon = {20, 16, 48, 42};
    SelectObject(dc, g_theme.font_icon); SetTextColor(dc, g_theme.accent);
    if (glyph) DrawTextW(dc, glyph, -1, &icon, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    wchar_t text[512]; GetWindowTextW(window, text, ARRAYSIZE(text));
    wchar_t *description = wcschr(text, L'\n');
    if (description) *description++ = L'\0';
    RECT title = {20, 45, r.right - 20, 70};
    SelectObject(dc, g_font_semibold); SetTextColor(dc, g_theme.text);
    DrawTextW(dc, text, -1, &title, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    if (description) {
        RECT detail = {20, 72, r.right - 20, r.bottom - 4};
        SelectObject(dc, g_font); SetTextColor(dc, g_theme.muted);
        DrawTextW(dc, description, -1, &detail, DT_WORDBREAK | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    RECT arrow = {r.right - 38, 18, r.right - 16, 40};
    SelectObject(dc, g_theme.font_icon); SetTextColor(dc, g_theme.muted);
    DrawTextW(dc, L"\uE76C", 1, &arrow, DT_CENTER | DT_SINGLELINE);
    RestoreDC(dc, saved);
}

static void paint_button(HWND window, HDC dc) {
    if (GetPropW(window, L"WAIOQuickAction")) { paint_quick_action(window, dc); return; }
    RECT r; GetClientRect(window, &r);
    wchar_t text[512]; GetWindowTextW(window, text, ARRAYSIZE(text));
    DWORD type = (DWORD)GetWindowLongPtrW(window, GWL_STYLE) & BS_TYPEMASK;
    BOOL check = type == BS_AUTOCHECKBOX || type == BS_CHECKBOX;
    BOOL group = type == BS_GROUPBOX;
    BOOL primary = GetPropW(window, L"WAIOPrimary") && IsWindowEnabled(window);
    BOOL is_sidebar = GetPropW(window, L"WAIOSidebar") != NULL;
    LRESULT state = SendMessageW(window, BM_GETSTATE, 0, 0);
    BOOL hover = GetPropW(window, L"WAIOHover") != NULL;
    BOOL focused = (state & BST_FOCUS) != 0;
    BOOL pushed = (state & BST_PUSHED) != 0;
    BOOL is_card = GetPropW(window, L"WAIOCard") != NULL;

    HBRUSH bg_fill = is_sidebar ? g_theme.sidebar_brush : (is_card ? g_theme.surface_brush : g_theme.background_brush);
    FillRect(dc, &r, bg_fill);

    HGDIOBJ font = SelectObject(dc, (HFONT)SendMessageW(window, WM_GETFONT, 0, 0));
    SetBkMode(dc, TRANSPARENT);

    if (check) {
        int box_size = 18;
        int y = (r.bottom - box_size) / 2;
        int x = 2;
        BOOL is_checked = (SendMessageW(window, BM_GETCHECK, 0, 0) == BST_CHECKED);
        BOOL is_enabled = IsWindowEnabled(window);

        COLORREF box_border = !is_enabled ? g_theme.border : (is_checked ? g_theme.accent : (hover ? (g_theme.dark ? RGB(140, 140, 140) : RGB(120, 120, 120)) : g_theme.muted));
        COLORREF box_fill = !is_enabled ? (g_theme.dark ? RGB(40, 40, 40) : RGB(235, 235, 235)) : (is_checked ? g_theme.accent : (hover ? g_theme.hover : g_theme.surface));

        HPEN pen = CreatePen(PS_SOLID, 1, box_border);
        HBRUSH brush = CreateSolidBrush(box_fill);
        HGDIOBJ old_pen = SelectObject(dc, pen), old_brush = SelectObject(dc, brush);
        RoundRect(dc, x, y, x + box_size, y + box_size, 5, 5);

        if (is_checked) {
            COLORREF tick_color = g_theme.dark ? RGB(0, 0, 0) : RGB(255, 255, 255);
            if (g_theme.high_contrast) tick_color = GetSysColor(COLOR_HIGHLIGHTTEXT);
            HPEN tick = CreatePen(PS_SOLID, 2, tick_color);
            HGDIOBJ old_tick = SelectObject(dc, tick);
            MoveToEx(dc, x + 4, y + 9, NULL);
            LineTo(dc, x + 7, y + 13);
            LineTo(dc, x + 14, y + 5);
            SelectObject(dc, old_tick);
            DeleteObject(tick);
        }
        SelectObject(dc, old_pen);
        SelectObject(dc, old_brush);
        DeleteObject(pen);
        DeleteObject(brush);

        r.left += 28;
        SetTextColor(dc, is_enabled ? g_theme.text : g_theme.muted);
        if (GetWindowLongPtrW(window, GWL_STYLE) & BS_MULTILINE) {
            RECT measured = r;
            DrawTextW(dc, text, -1, &measured, DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
            r.top += max(0, (r.bottom - r.top - (measured.bottom - measured.top)) / 2);
            DrawTextW(dc, text, -1, &r, DT_WORDBREAK | DT_NOPREFIX);
        } else {
            DrawTextW(dc, text, -1, &r, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        }
    } else if (group) {
        theme_card(dc, &r);
        SelectObject(dc, g_font_semibold);
        r.left += 18; r.top = 14; r.bottom = 40;
        SetTextColor(dc, g_theme.text);
        DrawTextW(dc, text, -1, &r, DT_SINGLELINE | DT_NOPREFIX);
    } else if (is_sidebar) {
        BOOL active = focused || GetPropW(window, L"WAIOActive") != NULL;
        if (hover || active) {
            COLORREF btn_bg = pushed ? (g_theme.dark ? RGB(45, 45, 45) : RGB(225, 225, 225)) : g_theme.hover;
            HBRUSH hbr = CreateSolidBrush(btn_bg);
            HPEN hpen = CreatePen(PS_SOLID, 1, btn_bg);
            HGDIOBJ old_p = SelectObject(dc, hpen), old_b = SelectObject(dc, hbr);
            RoundRect(dc, 0, 0, r.right, r.bottom, 6, 6);
            SelectObject(dc, old_p); SelectObject(dc, old_b);
            DeleteObject(hpen); DeleteObject(hbr);
        }

        if (active) {
            HPEN ind_pen = CreatePen(PS_SOLID, 1, g_theme.accent);
            HBRUSH ind_br = CreateSolidBrush(g_theme.accent);
            HGDIOBJ old_p = SelectObject(dc, ind_pen), old_b = SelectObject(dc, ind_br);
            int bar_h = 18;
            int bar_y = (r.bottom - bar_h) / 2;
            RoundRect(dc, 2, bar_y, 5, bar_y + bar_h, 3, 3);
            SelectObject(dc, old_p); SelectObject(dc, old_b);
            DeleteObject(ind_pen); DeleteObject(ind_br);
        }

        int text_x = 16;
        int id = GetDlgCtrlID(window);
        const wchar_t *glyph = NULL;
        switch (id) {
        case 136: glyph = L"\uE80F"; break; /* Dashboard */
        case 118: glyph = L"\uE7F8"; break; /* PC / Monitor */
        case 101: glyph = L"\uE71D"; break; /* Sliders */
        case 102: glyph = L"\uE774"; break; /* Globe */
        case 103: glyph = L"\uE74D"; break; /* Trash */
        case 104: glyph = L"\uE701"; break; /* WiFi */
        case 106: glyph = L"\uE777"; break; /* Restore / Rotate */
        case 105: glyph = L"\uEDA2"; break; /* Hardware chip */
        case 126: glyph = L"\uEA18"; break; /* Shield */
        case 123: glyph = L"\uED25"; break; /* Folder */
        default: break;
        }

        if (glyph && g_theme.font_icon) {
            HGDIOBJ old_f = SelectObject(dc, g_theme.font_icon);
            SetTextColor(dc, active ? g_theme.accent : (IsWindowEnabled(window) ? g_theme.text : g_theme.muted));
            RECT icon_rect = {12, 0, 36, r.bottom};
            DrawTextW(dc, glyph, 1, &icon_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            SelectObject(dc, old_f);
            text_x = 40;
        }

        SetTextColor(dc, IsWindowEnabled(window) ? g_theme.text : g_theme.muted);
        RECT label_rect = r;
        label_rect.left = text_x;
        label_rect.right -= 8;
        DrawTextW(dc, text, -1, &label_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    } else {
        COLORREF btn_bg;
        COLORREF btn_border;
        COLORREF text_color;

        if (primary) {
            btn_bg = (hover || pushed) ? g_theme.accent_hover : g_theme.accent;
            btn_border = btn_bg;
            text_color = g_theme.high_contrast ? GetSysColor(COLOR_HIGHLIGHTTEXT) : (g_theme.dark ? RGB(0, 0, 0) : RGB(255, 255, 255));
        } else {
            btn_bg = (hover || pushed) ? g_theme.hover : (is_card ? g_theme.surface : (g_theme.dark ? RGB(45, 45, 45) : RGB(255, 255, 255)));
            btn_border = focused ? g_theme.accent : (hover ? (g_theme.dark ? RGB(90, 90, 90) : RGB(190, 190, 190)) : g_theme.border);
            text_color = IsWindowEnabled(window) ? g_theme.text : g_theme.muted;
        }

        HPEN pen = CreatePen(PS_SOLID, 1, btn_border);
        HBRUSH brush = CreateSolidBrush(btn_bg);
        HGDIOBJ old_pen = SelectObject(dc, pen), old_brush = SelectObject(dc, brush);
        RoundRect(dc, 0, 0, r.right, r.bottom, 6, 6);
        SelectObject(dc, old_pen); SelectObject(dc, old_brush);
        DeleteObject(pen); DeleteObject(brush);

        SetTextColor(dc, text_color);
        InflateRect(&r, -10, -2);
        DrawTextW(dc, text, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }

    if (focused && !is_sidebar && !(SendMessageW(window, WM_QUERYUISTATE, 0, 0) & UISF_HIDEFOCUS)) {
        if (!primary) {
            RECT focus; GetClientRect(window, &focus); InflateRect(&focus, -3, -3);
            HPEN f_pen = CreatePen(PS_SOLID, 1, g_theme.accent);
            HGDIOBJ old_p = SelectObject(dc, f_pen), old_b = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
            RoundRect(dc, focus.left, focus.top, focus.right, focus.bottom, 4, 4);
            SelectObject(dc, old_p); SelectObject(dc, old_b);
            DeleteObject(f_pen);
        }
    }
    SelectObject(dc, font);
}

static void paint_header(HWND window, HDC dc) {
    RECT client; GetClientRect(window, &client); FillRect(dc, &client, g_theme.background_brush);
    theme_card(dc, &client);
    RECT lower = {0, client.bottom / 2, client.right, client.bottom};
    FillRect(dc, &lower, g_theme.surface_brush);
    HGDIOBJ font = SelectObject(dc, g_font_semibold); SetBkMode(dc, TRANSPARENT); SetTextColor(dc, g_theme.muted);
    HWND list = (HWND)GetWindowLongPtrW(window, GWLP_USERDATA);
    int left = -GetScrollPos(list, SB_HORZ);
    for (int i = 0; i < 32; ++i) {
        wchar_t text[160] = L"";
        LVCOLUMNW column = {0}; column.mask = LVCF_TEXT; column.pszText = text; column.cchTextMax = ARRAYSIZE(text);
        if (!ListView_GetColumn(list, i, &column)) break;
        RECT rect = {left + 10, 0, left + ListView_GetColumnWidth(list, i) - 10, client.bottom};
        DrawTextW(dc, text, -1, &rect, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        left += ListView_GetColumnWidth(list, i);
    }
    HPEN pen = CreatePen(PS_SOLID, 1, g_theme.high_contrast ? g_theme.border : g_theme.surface);
    HGDIOBJ prev_pen = SelectObject(dc, pen);
    MoveToEx(dc, 0, client.bottom - 1, NULL); LineTo(dc, client.right, client.bottom - 1);
    SelectObject(dc, prev_pen);
    DeleteObject(pen);
    SelectObject(dc, font);
}

/* Keep headings outside the scrolling native report client. This avoids the
   header's private cached painting and keeps its language and widths in sync. */
static LRESULT CALLBACK report_heading_proc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    if (message == WM_CREATE) {
        SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCTW *)lp)->lpCreateParams);
        return 0;
    }
    if (message == WM_PAINT || message == WM_PRINT || message == WM_PRINTCLIENT) {
        PAINTSTRUCT paint;
        HDC dc = wp ? (HDC)wp : BeginPaint(window, &paint);
        int saved = SaveDC(dc); paint_header(window, dc); RestoreDC(dc, saved);
        if (!wp) EndPaint(window, &paint);
        return 0;
    }
    if (message == WM_ERASEBKGND) return 1;
    return DefWindowProcW(window, message, wp, lp);
}

void theme_control(HWND control) {
    wchar_t cls[64]; GetClassNameW(control, cls, ARRAYSIZE(cls));
    SetWindowSubclass(control, themed_proc, 1, 0);
    if (!lstrcmpiW(cls, L"EDIT") || !wcscmp(cls, WC_LISTVIEWW)) {
        SetPropW(control, L"WAIOFrame", (HANDLE)1);
    }
    round_control(control);
    if (!wcscmp(cls, WC_LISTVIEWW)) {
        if (!GetPropW(control, L"WAIOReportHeading")) {
            WNDCLASSW heading = {0}; heading.lpfnWndProc = report_heading_proc;
            heading.hInstance = g_instance; heading.hCursor = LoadCursorW(NULL, IDC_ARROW);
            heading.lpszClassName = L"WAIO.ReportHeading";
            RegisterClassW(&heading);
            HWND label = CreateWindowExW(0, heading.lpszClassName, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                0, 0, 0, 0, GetParent(control), NULL, g_instance, control);
            if (label) {
                SetPropW(control, L"WAIOReportHeading", label);
                SetWindowLongPtrW(control, GWL_STYLE, GetWindowLongPtrW(control, GWL_STYLE) | LVS_NOCOLUMNHEADER);
            }
        }
        /* A fixed, shared row metric prevents dense/clipped native report rows.
           The list owns this image list and releases it during destruction. */
        if (!ListView_GetImageList(control, LVSIL_SMALL)) {
            HIMAGELIST spacing = ImageList_Create(1, 34, ILC_COLOR32, 1, 1);
            if (spacing) {
                ImageList_SetImageCount(spacing, 1);
                ListView_SetImageList(control, spacing, LVSIL_SMALL);
            }
        }
        ListView_SetBkColor(control, g_theme.surface);
        ListView_SetTextBkColor(control, g_theme.surface);
        ListView_SetTextColor(control, g_theme.text);
    } else if (!lstrcmpiW(cls, L"COMBOBOX")) {
        SetWindowTheme(control, L"", L"");
    } else if (!lstrcmpiW(cls, L"EDIT")) {
        SetWindowTheme(control, L"", L"");
        SendMessageW(control, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(10, 10));
    } else if (!wcscmp(cls, PROGRESS_CLASSW)) {
        SendMessageW(control, PBM_SETBKCOLOR, 0, g_theme.surface);
        SendMessageW(control, PBM_SETBARCOLOR, 0, g_theme.accent);
    }
    InvalidateRect(control, NULL, TRUE);
}

static BOOL CALLBACK apply_child(HWND window, LPARAM value) { (void)value; theme_control(window); return TRUE; }

void theme_attach(HWND window) {
    SetWindowSubclass(window, themed_proc, 1, 0);
    if (!(GetWindowLongPtrW(window,GWL_STYLE)&WS_CHILD) && app_windows_build() >= 22000) {
        BOOL dark = g_theme.dark && !g_theme.high_contrast;
        DwmSetWindowAttribute(window, 20, &dark, sizeof(dark));
        DWORD rounded = 2; DwmSetWindowAttribute(window, 33, &rounded, sizeof(rounded));
        COLORREF caption = dark ? RGB(32, 32, 32) : RGB(243, 243, 243);
        COLORREF border = dark ? RGB(50, 50, 50) : RGB(225, 225, 225);
        DwmSetWindowAttribute(window, 34, &border, sizeof(border));
        DwmSetWindowAttribute(window, 35, &caption, sizeof(caption));
    }
    EnumChildWindows(window, apply_child, 0);
    RedrawWindow(window, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

static BOOL CALLBACK apply_window(HWND window, LPARAM value) {
    (void)value;
    wchar_t cls[64]; GetClassNameW(window, cls, ARRAYSIZE(cls));
    if (!wcsncmp(cls, L"WAIO", 4)) theme_attach(window);
    return TRUE;
}

void theme_refresh(void) { colors(); EnumThreadWindows(GetCurrentThreadId(), apply_window, 0); }

void theme_init(void) {
    DWORD value = 0, size = sizeof(value);
    if (!app_is_smoke_test() && RegGetValueW(HKEY_CURRENT_USER, L"Software\\Doctoten\\WinAIOOptimize",
        L"Theme", RRF_RT_REG_DWORD, NULL, &value, &size) == ERROR_SUCCESS && value <= 2) g_theme.mode = (int)value;
    g_theme.font_icon = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe MDL2 Assets");
    colors();
}

void theme_set(int mode, BOOL save) {
    if (mode < 0 || mode > 2) return;
    g_theme.mode = mode; theme_refresh();
    if (save && !app_is_smoke_test()) {
        DWORD value = (DWORD)mode;
        HKEY key;
        LSTATUS error = RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Doctoten\\WinAIOOptimize", 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL);
        if (!error) { error = RegSetValueExW(key, L"Theme", 0, REG_DWORD, (BYTE *)&value, sizeof(value)); RegCloseKey(key); }
        if (error) app_log(tr(L"Không lưu được giao diện (mã %lu).", L"Could not save theme (code %lu)."), (DWORD)error);
    }
}

void theme_destroy(void) {
    if (g_theme.font_icon) { DeleteObject(g_theme.font_icon); g_theme.font_icon = NULL; }
    if (g_theme.background_brush) { DeleteObject(g_theme.background_brush); g_theme.background_brush = NULL; }
    if (g_theme.surface_brush) { DeleteObject(g_theme.surface_brush); g_theme.surface_brush = NULL; }
    if (g_theme.sidebar_brush) { DeleteObject(g_theme.sidebar_brush); g_theme.sidebar_brush = NULL; }
}

static LRESULT CALLBACK themed_proc(HWND window, UINT message, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR data) {
    (void)id; (void)data;
    wchar_t cls[64]; GetClassNameW(window, cls, ARRAYSIZE(cls));
    BOOL button = !wcscmp(cls, L"Button");
    switch (message) {
    case WM_SHOWWINDOW:
        if (!wcscmp(cls,WC_LISTVIEWW)) {
            HWND heading=(HWND)GetPropW(window,L"WAIOReportHeading");
            if (heading) ShowWindow(heading,wp?SW_SHOW:SW_HIDE);
        }
        break;
    case WM_NOTIFY: {
        NMHDR *notice = (NMHDR *)lp;
        wchar_t source[64]; GetClassNameW(notice->hwndFrom, source, ARRAYSIZE(source));
        if (notice->code == NM_CUSTOMDRAW && !wcscmp(source, WC_LISTVIEWW)) {
            NMLVCUSTOMDRAW *draw = (NMLVCUSTOMDRAW *)lp;
            BOOL plain_rows=!(ListView_GetExtendedListViewStyle(notice->hwndFrom)&LVS_EX_CHECKBOXES);
            if (draw->nmcd.dwDrawStage == CDDS_PREPAINT)
                return CDRF_NOTIFYITEMDRAW | (plain_rows?CDRF_NOTIFYPOSTPAINT:0);
            if (plain_rows && draw->nmcd.dwDrawStage==CDDS_POSTPAINT) {
                /* Explorer draws column separators in unused report space too. */
                RECT blank,row; GetClientRect(notice->hwndFrom,&blank);
                int count=ListView_GetItemCount(notice->hwndFrom);
                if (count) {
                    if (!ListView_GetItemRect(notice->hwndFrom,count-1,&row,LVIR_BOUNDS)) return CDRF_DODEFAULT;
                    blank.top=max(0,row.bottom);
                }
                if (blank.top<blank.bottom) FillRect(draw->nmcd.hdc,&blank,g_theme.surface_brush);
                return CDRF_DODEFAULT;
            }
            if (draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                draw->clrText = g_theme.text;
                draw->clrTextBk = g_theme.surface;
                if (plain_rows) {
                    HWND list=notice->hwndFrom; HDC dc=draw->nmcd.hdc;
                    int row=(int)draw->nmcd.dwItemSpec;
                    BOOL selected=(ListView_GetItemState(list,row,LVIS_SELECTED)&LVIS_SELECTED)!=0;
                    COLORREF bg=selected?(g_theme.dark?RGB(40,59,72):RGB(226,239,250)):g_theme.surface;
                    COLORREF fg=g_theme.text;
                    if (g_theme.high_contrast && selected) { bg=GetSysColor(COLOR_HIGHLIGHT); fg=GetSysColor(COLOR_HIGHLIGHTTEXT); }
                    int saved=SaveDC(dc);
                    RECT area=draw->nmcd.rc, client; GetClientRect(list,&client);
                    area.right=client.right;
                    SetDCBrushColor(dc,bg); FillRect(dc,&area,(HBRUSH)GetStockObject(DC_BRUSH));
                    SelectObject(dc,g_font); SetBkMode(dc,TRANSPARENT); SetTextColor(dc,fg);
                    int x=-GetScrollPos(list,SB_HORZ);
                    for (int c=0;c<32;++c) {
                        LVCOLUMNW column={0}; column.mask=LVCF_WIDTH;
                        if (!ListView_GetColumn(list,c,&column)) break;
                        if (column.cx>0) {
                            wchar_t value[1600]; ListView_GetItemText(list,row,c,value,ARRAYSIZE(value));
                            RECT cell={x+10,area.top,x+column.cx-10,area.bottom};
                            DrawTextW(dc,value,-1,&cell,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
                        }
                        x+=column.cx;
                    }
                    RestoreDC(dc,saved);
                    return CDRF_SKIPDEFAULT;
                }
                return CDRF_NEWFONT;
            }
        }
        break;
    }
    case WM_SIZE: {
        LRESULT result = DefSubclassProc(window, message, wp, lp);
        round_control(window);
        return result;
    }
    case WM_SETTEXT:
    case WM_ENABLE:
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
    case BM_SETCHECK:
    case BM_SETSTATE:
    case BM_SETSTYLE:
    case CB_SETCURSEL:
    case CB_RESETCONTENT:
    case WM_UPDATEUISTATE: {
        LRESULT result = DefSubclassProc(window, message, wp, lp);
        if (button || !lstrcmpiW(cls, L"COMBOBOX")) RedrawWindow(window, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
        return result;
    }
    case WM_SETTINGCHANGE:
    case WM_SYSCOLORCHANGE:
        if (!(GetWindowLongPtrW(window, GWL_STYLE) & WS_CHILD)) theme_refresh();
        break;
    case WM_ERASEBKGND:
        if (button) return 1;
        if (!wcsncmp(cls, L"WAIO", 4)) { RECT r; GetClientRect(window, &r); FillRect((HDC)wp, &r, g_theme.background_brush); return 1; }
        break;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORBTN: {
        HDC dc = (HDC)wp;
        wchar_t child[64]; GetClassNameW((HWND)lp, child, ARRAYSIZE(child));
        BOOL surface = GetPropW((HWND)lp, L"WAIOCard") || !wcscmp(child, L"Edit") || !wcscmp(child, L"ComboBox") || message == WM_CTLCOLORLISTBOX;
        BOOL is_sidebar = GetPropW((HWND)lp, L"WAIOSidebar") != NULL;
        SetTextColor(dc, GetPropW((HWND)lp, L"WAIOSecondary") ? g_theme.muted : g_theme.text);
        if (is_sidebar) {
            SetBkColor(dc, g_theme.sidebar_bg);
            return (LRESULT)g_theme.sidebar_brush;
        }
        SetBkColor(dc, surface ? g_theme.surface : g_theme.background);
        return (LRESULT)(surface ? g_theme.surface_brush : g_theme.background_brush);
    }
    case WM_MEASUREITEM: {
        MEASUREITEMSTRUCT *item = (MEASUREITEMSTRUCT *)lp;
        if (item->CtlType == ODT_COMBOBOX) { item->itemHeight = 28; return TRUE; }
        break;
    }
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT *item = (DRAWITEMSTRUCT *)lp;
        if (item->CtlType == ODT_COMBOBOX) {
            BOOL selected = (item->itemState & ODS_SELECTED) != 0;
            HBRUSH brush = CreateSolidBrush(selected ? (g_theme.dark ? RGB(55, 55, 55) : RGB(232, 232, 232)) : g_theme.surface);
            FillRect(item->hDC, &item->rcItem, brush); DeleteObject(brush);
            SetTextColor(item->hDC, (item->itemState & ODS_DISABLED) ? g_theme.muted : g_theme.text);
            SetBkMode(item->hDC, TRANSPARENT);
            wchar_t text[256] = L"";
            if (item->itemID != (UINT)-1 && SendMessageW(item->hwndItem, CB_GETLBTEXTLEN, item->itemID, 0) < 256)
                SendMessageW(item->hwndItem, CB_GETLBTEXT, item->itemID, (LPARAM)text);
            RECT r = item->rcItem; r.left += 10; DrawTextW(item->hDC, text, -1, &r, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            if (item->itemState & ODS_FOCUS) DrawFocusRect(item->hDC, &item->rcItem);
            return TRUE;
        }
        break;
    }
    case WM_MOUSEMOVE:
        if ((button || !lstrcmpiW(cls, L"COMBOBOX")) && !GetPropW(window, L"WAIOHover")) {
            SetPropW(window, L"WAIOHover", (HANDLE)1); TRACKMOUSEEVENT track = {sizeof(track), TME_LEAVE, window, 0};
            TrackMouseEvent(&track); InvalidateRect(window, NULL, FALSE);
        }
        break;
    case WM_MOUSELEAVE:
        if (button || !lstrcmpiW(cls, L"COMBOBOX")) { RemovePropW(window, L"WAIOHover"); InvalidateRect(window, NULL, FALSE); }
        break;
    case WM_PAINT:
        if (button) {
            /* Common controls can receive a caller-supplied DC during print.
               BeginPaint here would paint the screen and leave the target stale. */
            PAINTSTRUCT paint; HDC dc = wp ? (HDC)wp : BeginPaint(window, &paint);
            paint_button(window, dc);
            if (!wp) EndPaint(window, &paint);
            return 0;
        }
        if (!lstrcmpiW(cls, L"COMBOBOX")) {
            LRESULT result = DefSubclassProc(window, message, wp, lp);
            HDC dc = wp ? (HDC)wp : GetDC(window);
            int saved = SaveDC(dc); paint_combo(window, dc); RestoreDC(dc, saved);
            if (!wp) ReleaseDC(window, dc);
            return result;
        }
        if (GetPropW(window, L"WAIOFrame") || !wcscmp(cls, L"WAIO.SetupCards")) {
            LRESULT result = DefSubclassProc(window, message, wp, lp);
            HDC dc = GetWindowDC(window); paint_frame(window, dc); ReleaseDC(window, dc);
            if (!wcscmp(cls, WC_LISTVIEWW))
                RedrawWindow((HWND)GetPropW(window, L"WAIOReportHeading"), NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
            return result;
        }
        break;
    case WM_VSCROLL:
    case WM_HSCROLL:
    case WM_MOUSEWHEEL:
    case WM_NCMOUSEMOVE:
    case WM_NCMOUSELEAVE:
    case LVM_SCROLL: {
        LRESULT result = DefSubclassProc(window, message, wp, lp);
        if (!lstrcmpiW(cls, L"EDIT") || !wcscmp(cls, WC_LISTVIEWW) || !wcscmp(cls, L"WAIO.SetupCards")) {
            HDC dc = GetWindowDC(window); paint_frame(window, dc); ReleaseDC(window, dc);
        }
        return result;
    }
    case WM_NCPAINT:
        if (!lstrcmpiW(cls, L"EDIT") || !wcscmp(cls, WC_LISTVIEWW) || !wcscmp(cls, L"WAIO.SetupCards")) {
            LRESULT result = DefSubclassProc(window, message, wp, lp);
            HDC dc = GetWindowDC(window); paint_frame(window, dc); ReleaseDC(window, dc);
            return result;
        }
        break;
    case WM_PRINT:
        if (button) { paint_button(window, (HDC)wp); return 0; }
        if (!lstrcmpiW(cls, L"COMBOBOX")) { paint_combo(window, (HDC)wp); return 0; }
        if (!lstrcmpiW(cls, L"EDIT") || !wcscmp(cls, WC_LISTVIEWW) || !wcscmp(cls, L"WAIO.SetupCards")) {
            int saved = SaveDC((HDC)wp);
            LRESULT result = DefSubclassProc(window, message, wp, lp);
            RestoreDC((HDC)wp, saved);
            paint_frame(window, (HDC)wp);
            return result;
        }
        break;
    case WM_PRINTCLIENT:
        if (button) { paint_button(window, (HDC)wp); return 0; }
        if (!lstrcmpiW(cls, L"COMBOBOX")) { paint_combo(window, (HDC)wp); return 0; }
        if (GetPropW(window, L"WAIOFrame")) {
            int saved = SaveDC((HDC)wp);
            LRESULT result = DefSubclassProc(window, message, wp, lp);
            RestoreDC((HDC)wp, saved);
            paint_frame(window, (HDC)wp);
            return result;
        }
        break;
    case WM_NCDESTROY:
        if (GetPropW(window, L"WAIOReportHeading")) {
            HWND heading = (HWND)RemovePropW(window, L"WAIOReportHeading");
            if (IsWindow(heading)) DestroyWindow(heading);
        }
        RemovePropW(window, L"WAIOHover");
        RemovePropW(window, L"WAIOCard");
        RemovePropW(window, L"WAIOPrimary");
        RemovePropW(window, L"WAIOSidebar");
        RemovePropW(window, L"WAIOFrame");
        RemovePropW(window, L"WAIOSecondary");
        RemovePropW(window, L"WAIOQuickAction");
        RemovePropW(window, L"WAIOGlyph");
        RemovePropW(window, L"WAIOActive");
        RemoveWindowSubclass(window, themed_proc, 1);
        break;
    }
    return DefSubclassProc(window, message, wp, lp);
}
