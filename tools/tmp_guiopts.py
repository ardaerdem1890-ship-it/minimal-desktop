import io
p='gui/gui.c'; s=io.open(p,encoding='utf-8').read()

# --- settings state ---
s=s.replace('static int maximized_win = -1;',
'''static int maximized_win = -1;
/* settings */
static int cursor_style = 0;     /* 0=arrow 1=crosshair 2=block */
static int mouse_sensitivity = 1;/* 0..3 multiplier */
static bool show_clock = true;
static int resize_win = -1;
static int resize_mode = 0;      /* bitmask: 1=left 2=right 4=top 8=bottom */''')

# --- draw_cursor with styles ---
old='''static void draw_cursor(int mx, int my) {    for (int i = 0; i < 10; i++) {
        int w = i < 7 ? i + 1 : 10 - i;
        gfx_hline(mx, my + i, w, GUI_CURSOR);
        gfx_hline(mx + 1, my + i, w, GUI_BLACK);
    }
    gfx_vline(mx, my, 10, GUI_CURSOR);
}'''
new='''static void draw_cursor(int mx, int my) {
    if (cursor_style == 1) {      /* crosshair */
        gfx_hline(mx - 6, my, 13, GUI_CURSOR);
        gfx_vline(mx, my - 6, 13, GUI_CURSOR);
        gfx_fill_rect(mx - 1, my - 1, 3, 3, GUI_WHITE);
        return;
    }
    if (cursor_style == 2) {      /* block */
        gfx_fill_rect(mx, my, 8, 8, GUI_CURSOR);
        gfx_frame(mx, my, 8, 8, GUI_BLACK);
        return;
    }
    for (int i = 0; i < 10; i++) {
        int w = i < 7 ? i + 1 : 10 - i;
        gfx_hline(mx, my + i, w, GUI_CURSOR);
        gfx_hline(mx + 1, my + i, w, GUI_BLACK);
    }
    gfx_vline(mx, my, 10, GUI_CURSOR);
}'''
assert old in s
s=s.replace(old,new,1)

# --- resize helper: detect edge/corner of a window ---
old='''static void draw_taskbar(void) {'''
add='''/* Return resize bitmask if (mx,my) is on a window edge/corner. */
static int resize_hit(gui_win_t *w, int mx, int my) {
    const int GRIP = 4;
    int mask = 0;
    if (mx >= w->x - GRIP && mx <= w->x + GRIP) mask |= 1;
    if (mx >= w->x + w->w - GRIP && mx <= w->x + w->w + GRIP) mask |= 2;
    if (my >= w->y - GRIP && my <= w->y + GRIP) mask |= 4;
    if (my >= w->y + w->h - GRIP && my <= w->y + w->h + GRIP) mask |= 8;
    return mask;
}

static void apply_resize(gui_win_t *w, int mx, int my) {
    int minw = 80, minh = 50;
    if (resize_mode & 1) { int nx = mx; if (nx < 0) nx = 0; if (w->x + w->w - nx >= minw) { w->w += w->x - nx; w->x = nx; } }
    if (resize_mode & 2) { int nw = mx - w->x; if (nw >= minw && w->x + nw <= GFX_WIDTH) w->w = nw; }
    if (resize_mode & 4) { int ny = my; if (ny < 0) ny = 0; if (w->y + w->h - ny >= minh) { w->h += w->y - ny; w->y = ny; } }
    if (resize_mode & 8) { int nh = my - w->y; if (nh >= minh && w->y + nh <= TASKBAR_Y) w->h = nh; }
}

'''
assert old in s
s=s.replace(old,add+old,1)

# --- handle_click: check resize before drag ---
old='''        if (my < w->y + TITLE_H) { drag_win = idx; drag_dx = mx - w->x; drag_dy = my - w->y; }
        return;'''
new='''        int rmask = resize_hit(w, mx, my);
        if (rmask && my >= w->y + TITLE_H) {
            resize_win = idx; resize_mode = rmask;
            return;
        }
        if (my < w->y + TITLE_H) { drag_win = idx; drag_dx = mx - w->x; drag_dy = my - w->y; }
        return;'''
assert old in s
s=s.replace(old,new,1)

# --- gui_run: handle resize on mouse move ---
old='''        if (drag_win >= 0) {
            if (left) {
                gui_win_t *w = &wins[drag_win];'''
new='''        if (resize_win >= 0) {
            if (left) {
                apply_resize(&wins[resize_win], m.x, m.y);
            } else {
                resize_win = -1; resize_mode = 0;
            }
        }
        if (drag_win >= 0) {
            if (left) {
                gui_win_t *w = &wins[drag_win];'''
assert old in s
s=s.replace(old,new,1)

io.open(p,'w',encoding='utf-8').write(s)
print('ok')
