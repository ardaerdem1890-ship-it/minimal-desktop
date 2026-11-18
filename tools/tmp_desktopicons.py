import io
p='gui/gui.c'; s=io.open(p,encoding='utf-8').read()

# draw_desktop_icons: expand to 9 icons with new labels
old='''static void draw_desktop_icons(void) {
    static const char *names[] = { "Term", "Note", "Tasks", "Net", "Web", "Files", "DOOM" };
    static const int icons[] = { ICON_TERM, ICON_NOTEPAD, ICON_TASKS, ICON_NET,
                                 ICON_BROWSER, ICON_EXPLORER, ICON_DOOM };
    static const u8 colors[] = { GUI_GREEN, GUI_YELLOW, GUI_CYAN, GUI_BLUE, GUI_MAGENTA, GUI_TITLE_LT, GUI_RED };
    for (int i = 0; i < 7; i++) {
        int col = i / 4, row = i % 4;
        int x = 6 + col * 44, y = 8 + row * 40;
        gfx_fill_rect(x + 1, y + 1, 20, 20, GUI_DESK3);
        gfx_frame(x + 1, y + 1, 20, 20, GUI_GREY);
        gfx_icon(x + 3, y + 3, icons[i], colors[i]);
        gfx_text(x + 2, y + 22, names[i], GUI_WHITE);
    }
}'''
new='''static void draw_desktop_icons(void) {
    static const char *names[] = { "Term", "Note", "Tasks", "Net", "Web", "Files",
                                   "Pics", "Set", "DOOM" };
    static const int icons[] = { ICON_TERM, ICON_NOTEPAD, ICON_TASKS, ICON_NET,
                                 ICON_BROWSER, ICON_EXPLORER, ICON_FILE,
                                 ICON_SPEAKER, ICON_DOOM };
    static const u8 colors[] = { GUI_GREEN, GUI_YELLOW, GUI_CYAN, GUI_BLUE,
                                 GUI_MAGENTA, GUI_TITLE_LT, GUI_CYAN, GUI_YELLOW, GUI_RED };
    for (int i = 0; i < 9; i++) {
        int col = i / 4, row = i % 4;
        int x = 6 + col * 44, y = 8 + row * 40;
        gfx_fill_rect(x + 1, y + 1, 20, 20, GUI_DESK3);
        gfx_frame(x + 1, y + 1, 20, 20, GUI_GREY);
        gfx_icon(x + 3, y + 3, icons[i], colors[i]);
        gfx_text(x + 2, y + 22, names[i], GUI_WHITE);
    }
}'''
assert old in s
s=s.replace(old,new,1)

# click dispatch: add cases 7 (Pics), 8 (Set), move DOOM to 8
old='''    for (int i = 0; i < 7; i++) {
        int col = i / 4, row = i % 4;
        int ix = 6 + col * 44, iy = 8 + row * 40;
        if (point_in(mx, my, ix, iy, 30, 32)) {
            switch (i) {
                case 0: open_window(WIN_TERM); break;
                case 1: open_window(WIN_NOTE); break;
                case 2: open_window(WIN_TASKS); break;
                case 3: open_window(WIN_NET); break;
                case 4: open_window(WIN_BROWSER); if (br_url[0]==0) browser_load("about:home"); break;
                case 5: open_window(WIN_EXPLORER); explorer_refresh(); break;
                case 6: launch_req = GUI_LAUNCH_DOOM; running = false; break;
            }
            return;
        }
    }'''
new='''    for (int i = 0; i < 9; i++) {
        int col = i / 4, row = i % 4;
        int ix = 6 + col * 44, iy = 8 + row * 40;
        if (point_in(mx, my, ix, iy, 30, 32)) {
            switch (i) {
                case 0: open_window(WIN_TERM); break;
                case 1: open_window(WIN_NOTE); break;
                case 2: open_window(WIN_TASKS); break;
                case 3: open_window(WIN_NET); break;
                case 4: open_window(WIN_BROWSER); if (br_url[0]==0) browser_load("about:home"); break;
                case 5: open_window(WIN_EXPLORER); explorer_refresh(); break;
                case 6: iv_refresh(); iv_index = 0; iv_load(); open_window(WIN_IMGVIEW); break;
                case 7: open_window(WIN_SETTINGS); break;
                case 8: launch_req = GUI_LAUNCH_DOOM; running = false; break;
            }
            return;
        }
    }'''
assert old in s
s=s.replace(old,new,1)
io.open(p,'w',encoding='utf-8').write(s)
print('ok')
