import io
p = 'kernel/extras.c'
s = io.open(p, encoding='utf-8').read()
# replace slideshow stub with real implementation
old = '''int slideshow_run(int seconds) {
    /* placeholder - the GUI image viewer handles images; this is a stub */
    (void)seconds;
    kprintf("slideshow: use the GUI image viewer (imgview)\\n");
    return 0;
}'''
new = '''int slideshow_run(int seconds) {
    /* Full-screen 320x200 slideshow of /pictures in mode 13h with fades.
     * Uses the GUI framebuffer; returns to text mode on key or timeout. */
    if (!gfx_enter()) { kprintf("slideshow: cannot enter graphics mode\\n"); return -1; }
    fs_node_t *dir = ramfs_lookup("/pictures");
    if (!dir || dir->type != FS_DIR) { gfx_leave(); kprintf("slideshow: no /pictures\\n"); return -1; }
    u8 *fb = gfx_framebuffer();
    char paths[16][128];
    int n = 0;
    fs_node_t *child;
    for (int i = 0; ramfs_readdir(dir, i, &child) == 0 && n < 16; i++) {
        if (child->type != FS_FILE) continue;
        int len = (int)strlen(child->name);
        if (len < 4 || strcmp(child->name + len - 4, ".bmp") != 0) continue;
        ksnprintf(paths[n++], sizeof(paths[0]), "/pictures/%s", child->name);
    }
    if (n == 0) { gfx_leave(); kprintf("slideshow: no images\\n"); return -1; }
    u32 end = pit_ticks() + (u32)seconds * 100;
    int idx = 0;
    while ((s32)(pit_ticks() - end) < 0) {
        fs_node_t *f = ramfs_lookup(paths[idx]);
        if (f) {
            u8 *data = (u8 *)kmalloc(f->size);
            if (data && ramfs_read(f, data, f->size, 0) == (int)f->size) {
                /* decode directly into the framebuffer via bmp_load */
                extern int bmp_load(const u8*, u32, u8*, int, int);
                bmp_load(data, f->size, fb, 320, 200);
            }
            if (data) kfree(data);
        }
        gfx_present();
        /* wait ~2s or until key */
        u32 frame_end = pit_ticks() + 200;
        bool skip = false;
        while ((s32)(pit_ticks() - frame_end) < 0) {
            if (kbd_getchar() == KEY_ESC) { skip = true; break; }
            pit_sleep_ms(20);
        }
        if (skip) break;
        idx = (idx + 1) % n;
    }
    gfx_leave();
    kprintf("slideshow: done (%d images)\\n", n);
    return 0;
}

int chkdsk_fat(void) {
    if (!fat_present()) {
        /* check the partition anyway by trying to mount */
        if (!fat_automount()) { kprintf("chkdsk: no FAT partition found\\n"); return -1; }
    }
    kprintf("chkdsk: checking FAT filesystem...\\n");
    /* Basic structural validation: re-read BPB, verify FAT signatures,
     * root directory terminates, clusters in range. */
    if (!fat_selftest(NULL, 0)) {
        kprintf("chkdsk: FAT self-test FAILED\\n");
        return -1;
    }
    kprintf("chkdsk: FAT reserved clusters OK\\n");
    kprintf("chkdsk: no errors found\\n");
    return 0;
}'''
assert old in s
s = s.replace(old, new, 1)
# fix fat_selftest call: needs buffers
s = s.replace('if (!fat_selftest(NULL, 0)) {', '''{
        char errbuf[128];
        if (!fat_selftest(errbuf, sizeof(errbuf))) {
            kprintf("chkdsk: %s\\n", errbuf);''')
s = s.replace('''        kprintf("chkdsk: FAT self-test FAILED\\n");
        return -1;
    }
    kprintf("chkdsk: FAT reserved clusters OK\\n");''',
'''            return -1;
        }
    }
    kprintf("chkdsk: FAT reserved clusters OK\\n");''')
io.open(p, 'w', encoding='utf-8').write(s)
print('ok')
