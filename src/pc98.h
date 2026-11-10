#ifndef PC98_H
#define PC98_H

/*
 * PC-9801 MS-DOS commander, machine notes.
 *
 * DOS calls are INT 21h and match IBM PC-DOS / MS-DOS 2+. NEC MS-DOS 3.3
 * through 6.2 implement them. File names are 8.3 bytes in Shift-JIS.
 *
 * What is NOT IBM, and lives only in vid.c / kbd.c:
 *
 * Text VRAM, 80 columns, stride 160 bytes (one word per cell):
 *   segment A000h  character code
 *   segment A200h  attribute
 *   offset = y * 160 + x * 2
 * Halfwidth (ASCII, JIS X 0201 katakana A1h-DFh): low byte = code, high = 0.
 * Kanji, two cells. Shift-JIS is converted to JIS, then:
 *   left  bytes: [ku] [ten+20h]
 *   right bytes: [ku] [(ten+20h) | 80h]
 * Example: U+732B cat, Shift-JIS 94 4C -> cells 27 2D and 27 AD.
 * Box drawing is JIS X 0208 row 8 (fullwidth), not CP866.
 *
 * Attribute byte. Bit 0 must be 1 or the cell is hidden (PC98.INC, /ST).
 *   bit 1 blink, bit 2 reverse, bit 3 underline, bit 4 vertical line
 *   bits 7-5 colour: 0 black, 1 blue, 2 red, 3 purple,
 *                    4 green, 5 cyan, 6 yellow, 7 white
 * Reverse swaps the colour onto the background and draws black text.
 * High byte of the attribute word stays 0 (8-colour machines).
 *
 * Keyboard is INT 18h, not INT 16h.
 *   AH=00h wait, AX = scan in AH, character in AL. Removes the key.
 *   AH=01h peek, BH=0 if the buffer is empty. Does not remove.
 *   AH=02h live modifiers. AH=07h is the key plus modifiers held with it.
 *   Bits: bit0 SHIFT, bit1 CAPS, bit2 KANA, bit3 GRPH, bit4 CTRL.
 *   F1-F10 stay 62h-6Bh. Shift does not make another function-key code.
 * Scans (hex), not IBM scan codes:
 *   Esc 00, BS 0E, Tab 0F, Enter 1C
 *   Roll Up 36, Roll Down 37, Ins 38, Del 39
 *   Up 3A, Left 3B, Right 3C, Down 3D, Home 3E, HELP 3F
 *   F1-F10 are 62-6B. Keypad * is 45.
 *
 * Other INT 18h used here:
 *   AH=0Ch text layer on, AH=0Eh DX=0 home the hardware scroll,
 *   AH=11h cursor on, AH=12h cursor off, AH=13h DX=0 cursor home,
 *   AH=41h graphics layer off.
 *
 * This is the normal 640x400 text mode (25 rows). High-resolution
 * PC-98XA/XL text at E0000h is out of scope. Cyrillic is not in the
 * font ROM; the chrome is English. Japanese names are displayed.
 */

#define VID_COLS 80
#define VID_ROWS 25

#define PANEL_W 40
#define PANEL_H 21
#define PANEL_INNER 36
#define PANEL_ROWS 17
#define PANEL_FILE_Y 3
#define ROW_CMD 21
#define ROW_STAT 22
#define ROW_HINT 23
#define ROW_KEYS 24

#define MAX_FILES 512
#define NAME_LEN 13
#define PATH_MAX 80
#define CMD_MAX 66
#define OP_DEPTH 12

#define ATTR_BLINK 0x02
#define ATTR_REV   0x04
#define ATTR_UL    0x08
#define ATTR_VLINE 0x10
#define COL_BLACK  0x00
#define COL_BLUE   0x20
#define COL_RED    0x40
#define COL_PURPLE 0x60
#define COL_GREEN  0x80
#define COL_CYAN   0xA0
#define COL_YELLOW 0xC0
#define COL_WHITE  0xE0

#define SCAN_ESC    0x00
#define SCAN_BS     0x0E
#define SCAN_TAB    0x0F
#define SCAN_ENTER  0x1C
#define SCAN_STAR   0x45
#define SCAN_ROLLUP 0x36
#define SCAN_ROLLDN 0x37
#define SCAN_INS    0x38
#define SCAN_DEL    0x39
#define SCAN_UP     0x3A
#define SCAN_LEFT   0x3B
#define SCAN_RIGHT  0x3C
#define SCAN_DOWN   0x3D
#define SCAN_HOME   0x3E
#define SCAN_HELP   0x3F
#define SCAN_F1     0x62
#define SCAN_F2     0x63
#define SCAN_F3     0x64
#define SCAN_F4     0x65
#define SCAN_F5     0x66
#define SCAN_F6     0x67
#define MOD_SHIFT   0x01
#define MOD_GRPH    0x08
#define MOD_CTRL    0x10
#define SCAN_F7     0x68
#define SCAN_F8     0x69
#define SCAN_F9     0x6A
#define SCAN_F10    0x6B

#define FA_RDONLY 0x01
#define FA_HIDDEN 0x02
#define FA_SYSTEM 0x04
#define FA_VOL    0x08
#define FA_DIR    0x10
#define FA_ARCH   0x20
#define FA_ANY    0x37

/* JIS X 0208 row 8, 1-based ten. Fullwidth, two text cells each. */
#define KEI_H  1
#define KEI_V  2
#define KEI_TL 3
#define KEI_TR 4
#define KEI_BL 5
#define KEI_BR 6

struct keyev {
	unsigned char scan;
	unsigned char ascii;
	unsigned char shift;
};

struct file_ent {
	char name[NAME_LEN];
	unsigned char attr;
	unsigned char mark;
	unsigned short time;
	unsigned short date;
	unsigned long size;
};

struct panel {
	char path[PATH_MAX];
	struct file_ent files[MAX_FILES];
	unsigned int count;
	unsigned int cursor;
	unsigned int top;
	unsigned long freeb;
	int freeok;
};

extern struct panel g_pan[2];
extern int g_act;
extern char g_cmd[CMD_MAX];
extern int g_cmdlen;
extern char g_msg[80];

void vid_init(void);
void vid_done(void);
void vid_cell(int x, int y, unsigned short attr, unsigned short code);
void vid_putpad(int x, int y, int w, unsigned short attr, const char *s);
void vid_fill(int x, int y, int n, unsigned short attr, unsigned char ch);
void vid_clear(unsigned short attr);
void vid_forget(void);
void vid_handoff(void);
void vid_fw(int x, int y, unsigned short attr, unsigned char ku, unsigned char ten);
void vid_box(int x, int y, int w, int h, unsigned short attr);
int vid_puts(int x, int y, int xend, unsigned short attr, const char *s);

void kbd_read(struct keyev *ev);
int kbd_poll(struct keyev *ev);

int dos_setdta(void *dta);
void dos_dta_main(void);
unsigned char *dos_dta(void);
int dos_findfirst(const char *path, unsigned short attr);
int dos_findnext(void);
int dos_exists(const char *path, unsigned char *attr);
int dos_getcwd(unsigned char dl, char *buf);
int dos_chdir(const char *path);
unsigned char dos_getdrive(void);
int dos_mkdir(const char *path);
int dos_rmdir(const char *path);
int dos_unlink(const char *path);
int dos_rename(const char *oldn, const char *newn);
int dos_create(const char *path, unsigned short attr);
int dos_open(const char *path, unsigned char mode);
int dos_close(int h);
int dos_read(int h, void *buf, unsigned short n, unsigned short *got);
int dos_write(int h, const void *buf, unsigned short n, unsigned short *put);
int dos_seek(int h, unsigned long pos);
int dos_gettime(int h, unsigned short *time, unsigned short *date);
int dos_settime(int h, unsigned short time, unsigned short date);
int dos_free(unsigned char drive1, unsigned long *bytes);
int dos_drive_ok(unsigned char drive1);
int dos_exec(const char *path, const char *args);
int dos_comspec(char *dst, int max);
int dos_exepath(char *dst, int max);
void dos_crit_on(void);
void dos_crit_off(void);
extern unsigned char crit_hit;
const char *dos_errmsg(int err);

int path_is_root(const char *p);
void path_parent(char *p);
int path_join(char *dst, const char *dir, const char *name);
int path_same(const char *a, const char *b);
int path_inside(const char *dir, const char *child);
void fmt_ulong(char *dst, unsigned long n);
void fmt_date(char *dst, unsigned short dos);
int is_sjis_lead(unsigned char c);
int is_sjis_trail(unsigned char c);
int namecmp(const char *a, const char *b);
int is_exec(const char *name);

void panel_init(void);
void panel_reload(struct panel *p);
void panel_draw(const struct panel *p, int x, int active);
void panel_damage(void);
void panel_move(struct panel *p, int delta);
void panel_parent(struct panel *p);
void panel_enter_dir(struct panel *p);
void panel_mark(struct panel *p);
void panel_invert(struct panel *p);
void panel_setdrive(struct panel *p, unsigned char drive0);
struct file_ent *panel_cur(struct panel *p);

void draw_all(void);
void draw_damage(void);
void ui_status(const char *msg);
int ui_ask(const char *title, const char *msg, int allow_all);
int ui_confirm(const char *title, const char *line1, const char *line2);
void ui_progress(const char *title, const char *line1, const char *line2);
int ui_input(const char *title, const char *prompt, char *buf, int max);
int ui_esc(void);

void do_copy(void);
void do_move(void);
void do_rename(void);
void do_delete(void);
void do_mkdir(void);
void do_view(void);
void do_run_cursor(void);
void do_run_cmd(void);

#endif
