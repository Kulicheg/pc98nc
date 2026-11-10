#include "pc98.h"

#include <string.h>

struct panel g_pan[2];
int g_act;
char g_cmd[CMD_MAX];
int g_cmdlen;
char g_msg[80];

static char g_comspec[80];
static char g_editor[48];

static void cwd_full(char *dst)
{
	char rest[66];
	unsigned char d;

	d = dos_getdrive();
	rest[0] = 0;
	dos_getcwd(0, rest);
	dst[0] = (char)('A' + d);
	dst[1] = ':';
	dst[2] = '\\';
	dst[3] = 0;
	if (rest[0] && strlen(rest) + 4 < PATH_MAX)
		strcat(dst, rest);
}

static void ini_line(char *s)
{
	char *val;
	char *end;

	while (*s == ' ' || *s == '\t')
		s++;
	if (*s == 0 || *s == ';' || *s == '#')
		return;
	val = s;
	while (*val && *val != '=')
		val++;
	if (*val != '=')
		return;
	*val++ = 0;
	end = s + strlen(s);
	while (end > s && (end[-1] == ' ' || end[-1] == '\t'))
		end--;
	*end = 0;
	while (*val == ' ' || *val == '\t')
		val++;
	end = val + strlen(val);
	while (end > val && (end[-1] == ' ' || end[-1] == '\t'))
		end--;
	*end = 0;
	if (namecmp(s, "editor") == 0 && val[0]) {
		strncpy(g_editor, val, sizeof(g_editor) - 1);
		g_editor[sizeof(g_editor) - 1] = 0;
	}
}

static void ini_parse(char *buf)
{
	char *s;

	s = buf;
	while (*s) {
		char *eol;
		char cut;

		eol = s;
		while (*eol && *eol != '\n' && *eol != '\r')
			eol++;
		cut = *eol;
		*eol = 0;
		ini_line(s);
		if (cut == 0)
			break;
		s = eol + 1;
		if (cut == '\r' && *s == '\n')
			s++;
	}
}

static int ini_read(const char *path)
{
	static char buf[512];
	unsigned short got;
	int h;

	h = dos_open(path, 0);
	if (h < 0)
		return -1;
	got = 0;
	if (dos_read(h, buf, 511, &got) != 0) {
		dos_close(h);
		return -1;
	}
	dos_close(h);
	buf[got] = 0;
	ini_parse(buf);
	return 0;
}

static void ini_from_exe(char *dst)
{
	char exe[PATH_MAX];
	const char *s;
	const char *sep;
	int n;

	dst[0] = 0;
	if (dos_exepath(exe, (int)sizeof(exe)) != 0)
		return;
	sep = 0;
	s = exe;
	while (*s) {
		if (is_sjis_lead((unsigned char)*s) && s[1]) {
			s += 2;
			continue;
		}
		if (*s == '\\' || *s == '/')
			sep = s;
		s++;
	}
	if (!sep)
		return;
	n = (int)(sep - exe) + 1;
	if (n + 6 >= PATH_MAX)
		return;
	memcpy(dst, exe, (unsigned)n);
	dst[n] = 0;
	strcat(dst, "NC.INI");
}

static void ini_load(void)
{
	char path[PATH_MAX];
	int got;

	strcpy(g_editor, "sedit");
	ini_from_exe(path);
	got = -1;
	if (path[0])
		got = ini_read(path);
	if (got != 0)
		ini_read("NC.INI");
}

static void wait_back(const char *err)
{
	struct keyev ev;

	vid_init();
	vid_forget();
	if (err && err[0])
		vid_putpad(0, 23, VID_COLS, COL_YELLOW, err);
	vid_putpad(0, 24, VID_COLS, (unsigned short)(ATTR_REV | COL_CYAN),
		   " Press any key to return ");
	while (kbd_poll(&ev))
		kbd_read(&ev);
	kbd_read(&ev);
}

static int run_prog(const char *path, const char *args, int via_shell)
{
	char saved[PATH_MAX];
	char tail[CMD_MAX + 8];
	int err;

	cwd_full(saved);
	dos_chdir(g_pan[g_act].path);
	err = 0;
	if (via_shell) {
		if (g_comspec[0] == 0) {
			ui_status("No COMSPEC");
			dos_chdir(saved);
			return -1;
		}
		strcpy(tail, "/C ");
		strncat(tail, args, CMD_MAX);
		tail[sizeof(tail) - 1] = 0;
		vid_handoff();
		err = dos_exec(g_comspec, tail);
	} else {
		vid_handoff();
		err = dos_exec(path, args ? args : "");
	}
	dos_chdir(saved);
	if (err) {
		strncpy(g_msg, dos_errmsg(err), 79);
		g_msg[79] = 0;
		wait_back(g_msg);
	} else {
		g_msg[0] = 0;
		wait_back(0);
	}
	panel_reload(&g_pan[0]);
	panel_reload(&g_pan[1]);
	draw_damage();
	draw_all();
	return err;
}

static int ext_is(const char *name, const char *ext)
{
	const char *dot;
	const char *p;

	dot = 0;
	p = name;
	while (*p) {
		if (*p == '.')
			dot = p;
		p++;
	}
	if (!dot || dot[1] == 0)
		return 0;
	return namecmp(dot + 1, ext) == 0;
}

void do_run_cursor(void)
{
	struct panel *p;
	struct file_ent *e;
	char path[PATH_MAX];

	p = &g_pan[g_act];
	e = panel_cur(p);
	if (!e)
		return;
	if (e->attr & FA_DIR) {
		panel_enter_dir(p);
		g_msg[0] = 0;
		draw_all();
		return;
	}
	if (path_join(path, p->path, e->name) != 0) {
		ui_status("Path too long");
		draw_all();
		return;
	}
	if (ext_is(e->name, "BAT")) {
		run_prog(path, path, 1);
		return;
	}
	if (ext_is(e->name, "COM") || ext_is(e->name, "EXE")) {
		run_prog(path, "", 0);
		return;
	}
	ui_status("Press F3 to view");
	draw_all();
}

void do_run_cmd(void)
{
	if (g_cmdlen <= 0)
		return;
	run_prog("", g_cmd, 1);
	g_cmdlen = 0;
	g_cmd[0] = 0;
	draw_all();
}

static void do_edit(void)
{
	struct panel *p;
	struct file_ent *e;
	char args[CMD_MAX];
	int n;
	int m;

	p = &g_pan[g_act];
	e = panel_cur(p);
	if (!e) {
		ui_status("No file");
		return;
	}
	if (e->attr & FA_DIR) {
		ui_status("Cannot edit a directory");
		return;
	}
	n = (int)strlen(g_editor);
	m = (int)strlen(e->name);
	if (n <= 0 || n + 1 + m >= CMD_MAX) {
		ui_status("Editor command too long");
		return;
	}
	strcpy(args, g_editor);
	strcat(args, " ");
	strcat(args, e->name);
	run_prog("", args, 1);
}

static char c_path[PATH_MAX];
static char c_cmd[CMD_MAX];
static int c_act;
static int c_ok;
static char s_left[62];
static char s_right[20];
static unsigned short s_col;
static int s_ok;
static int g_chrome;
static int g_force;

void draw_damage(void)
{
	g_force = 1;
}

static void draw_cmd(void)
{
	char prompt[PATH_MAX + 2];
	char tail[81];
	int plen;
	int n;

	if (c_ok && c_act == g_act && strcmp(c_path, g_pan[g_act].path) == 0 &&
	    strcmp(c_cmd, g_cmd) == 0)
		return;
	strcpy(prompt, g_pan[g_act].path);
	strcat(prompt, ">");
	plen = (int)strlen(prompt);
	if (plen > 48)
		plen = 48;
	vid_putpad(0, ROW_CMD, plen, COL_CYAN, prompt);
	memset(tail, ' ', 80);
	tail[80] = 0;
	n = 0;
	if (plen < 48)
		n = 1;
	{
		int k;

		k = 0;
		while (g_cmd[k] && n + k < 80) {
			tail[n + k] = g_cmd[k];
			k++;
		}
	}
	vid_putpad(plen, ROW_CMD, VID_COLS - plen, COL_WHITE, tail);
	strncpy(c_path, g_pan[g_act].path, PATH_MAX - 1);
	c_path[PATH_MAX - 1] = 0;
	strncpy(c_cmd, g_cmd, CMD_MAX - 1);
	c_cmd[CMD_MAX - 1] = 0;
	c_act = g_act;
	c_ok = 1;
}

static void fmt_grouped(char *dst, unsigned long n)
{
	char tmp[12];
	int i;
	int j;

	if (n == 0) {
		dst[0] = '0';
		dst[1] = 0;
		return;
	}
	i = 0;
	while (n && i < 11) {
		tmp[i++] = (char)('0' + (unsigned)(n % 10ul));
		n /= 10ul;
	}
	j = 0;
	while (i > 0) {
		dst[j++] = tmp[--i];
		if (i > 0 && (i % 3) == 0)
			dst[j++] = ',';
	}
	dst[j] = 0;
}

static void draw_status(void)
{
	struct panel *p;
	struct file_ent *e;
	char left[62];
	char right[20];
	unsigned short col;

	p = &g_pan[g_act];
	memset(left, 0, sizeof(left));
	memset(right, 0, sizeof(right));
	if (g_msg[0]) {
		strncpy(left, g_msg, 61);
		col = COL_YELLOW;
	} else {
		e = panel_cur(p);
		col = COL_WHITE;
		if (e) {
			strcpy(left, e->name);
			if (e->attr & FA_DIR) {
				strcat(left, "  <DIR>");
			} else {
				strcat(left, "  ");
				fmt_ulong(left + strlen(left), e->size);
				if (is_exec(e->name))
					col = COL_GREEN;
			}
		}
	}
	left[61] = 0;
	if (p->freeok) {
		fmt_grouped(right, p->freeb);
		strcat(right, " free");
	}
	if (s_ok && s_col == col && strcmp(s_left, left) == 0 &&
	    strcmp(s_right, right) == 0)
		return;
	vid_putpad(0, ROW_STAT, 62, col, left);
	vid_putpad(62, ROW_STAT, 18, COL_CYAN, right);
	strcpy(s_left, left);
	strcpy(s_right, right);
	s_col = col;
	s_ok = 1;
}

static void draw_hint(void)
{
	vid_putpad(0, ROW_HINT, VID_COLS, COL_GREEN,
		   "Tab panel   Ins mark   * invert   Left parent   Enter open   F10 quit");
}

static void draw_fkeys(void)
{
	static const char *lab[10] = {
		" 1Help  ", " 2Drive ", " 3View  ", " 4Edit  ", " 5Copy  ",
		" 6Move  ", " 7MkDir ", " 8Del   ", " 9      ", "10Quit  "
	};
	int i;
	int c;

	for (i = 0; i < 10; i++) {
		for (c = 0; c < 8; c++) {
			unsigned char ch;
			unsigned short attr;

			ch = (unsigned char)lab[i][c];
			if (ch >= '0' && ch <= '9')
				attr = COL_YELLOW;
			else
				attr = (unsigned short)(ATTR_REV | COL_CYAN);
			vid_cell(i * 8 + c, ROW_KEYS, attr, ch);
		}
	}
}

void draw_all(void)
{
	if (g_force) {
		panel_damage();
		c_ok = 0;
		s_ok = 0;
		g_chrome = 0;
		g_force = 0;
	}
	panel_draw(&g_pan[0], 0, g_act == 0);
	panel_draw(&g_pan[1], 40, g_act == 1);
	draw_cmd();
	draw_status();
	if (!g_chrome) {
		draw_hint();
		draw_fkeys();
		g_chrome = 1;
	}
}

void ui_status(const char *msg)
{
	strncpy(g_msg, msg, 79);
	g_msg[79] = 0;
	draw_status();
}

int ui_esc(void)
{
	struct keyev ev;

	if (!kbd_poll(&ev))
		return 0;
	if (ev.scan != SCAN_ESC)
		return 0;
	kbd_read(&ev);
	return 1;
}

static void dialog_frame(int x, int y, int w, int h, const char *title)
{
	int r;
	int right;

	/* The box covers panel rows. The next full draw has to paint them
	   again; until then only the dialog itself is touched. */
	draw_damage();
	vid_box(x, y, w, h, COL_WHITE);
	right = x + w;
	if (right >= VID_COLS)
		right = VID_COLS - 1;
	for (r = y + 1; r < y + h - 1; r++)
		vid_fill(x + 1, r, right - (x + 1), COL_WHITE, ' ');
	vid_puts(x + 2, y + 1, x + w - 2, COL_YELLOW, title);
}

int ui_ask(const char *title, const char *msg, int allow_all)
{
	struct keyev ev;
	int x;
	int y;
	int w;

	w = 48;
	x = (VID_COLS - w) / 2;
	y = 8;
	dialog_frame(x, y, w, 7, title);
	vid_puts(x + 2, y + 3, x + w - 2, COL_WHITE, msg);
	if (allow_all)
		vid_puts(x + 2, y + 5, x + w - 2, COL_CYAN, "Y yes   N skip   A all   Esc stop");
	else
		vid_puts(x + 2, y + 5, x + w - 2, COL_CYAN, "Enter/Y yes   Esc/N no");
	for (;;) {
		kbd_read(&ev);
		if (ev.scan == SCAN_ESC)
			return allow_all ? 3 : 0;
		if (ev.scan == SCAN_ENTER || ev.ascii == 13)
			return 1;
		if (ev.ascii == 'y' || ev.ascii == 'Y')
			return 1;
		if (ev.ascii == 'n' || ev.ascii == 'N')
			return 0;
		if (allow_all && (ev.ascii == 'a' || ev.ascii == 'A'))
			return 2;
	}
}

int ui_confirm(const char *title, const char *line1, const char *line2)
{
	struct keyev ev;
	int x;
	int y;
	int w;

	w = 52;
	x = (VID_COLS - w) / 2;
	y = 8;
	dialog_frame(x, y, w, 7, title);
	vid_puts(x + 2, y + 2, x + w - 2, COL_WHITE, line1);
	if (line2 && line2[0])
		vid_puts(x + 2, y + 3, x + w - 2, COL_CYAN, line2);
	vid_puts(x + 2, y + 5, x + w - 2, COL_CYAN, "Enter start    Esc cancel");
	for (;;) {
		kbd_read(&ev);
		if (ev.scan == SCAN_ESC)
			return 0;
		if (ev.scan == SCAN_ENTER || ev.ascii == 13)
			return 1;
		if (ev.ascii == 'y' || ev.ascii == 'Y')
			return 1;
		if (ev.ascii == 'n' || ev.ascii == 'N')
			return 0;
	}
}

void ui_progress(const char *title, const char *line1, const char *line2)
{
	char st[16];
	int x;
	int y;
	int w;
	int n;

	w = 56;
	x = (VID_COLS - w) / 2;
	y = 8;
	dialog_frame(x, y, w, 7, title);
	if (line1)
		vid_puts(x + 2, y + 2, x + w - 2, COL_WHITE, line1);
	if (line2 && line2[0])
		vid_puts(x + 2, y + 3, x + w - 2, COL_CYAN, line2);
	vid_puts(x + 2, y + 5, x + w - 2, COL_CYAN, "Esc cancel all");
	n = 0;
	while (title[n] && n < 12) {
		st[n] = title[n];
		n++;
	}
	st[n++] = '.';
	st[n++] = '.';
	st[n++] = '.';
	st[n] = 0;
	ui_status(st);
}

static void edit_bs(char *buf, int *len)
{
	if (*len <= 0)
		return;
	if (*len >= 2 &&
	    is_sjis_lead((unsigned char)buf[*len - 2]) &&
	    is_sjis_trail((unsigned char)buf[*len - 1]))
		*len -= 2;
	else
		*len -= 1;
	buf[*len] = 0;
}

int ui_input(const char *title, const char *prompt, char *buf, int max)
{
	struct keyev ev;
	int len;
	int x;
	int y;
	int w;

	if (max > 60)
		max = 60;
	len = (int)strlen(buf);
	w = 52;
	x = (VID_COLS - w) / 2;
	y = 8;
	for (;;) {
		dialog_frame(x, y, w, 8, title);
		vid_puts(x + 2, y + 3, x + w - 2, COL_WHITE, prompt);
		vid_fill(x + 2, y + 4, w - 4, (unsigned short)(ATTR_REV | COL_CYAN), ' ');
		vid_puts(x + 2, y + 4, x + w - 2, (unsigned short)(ATTR_REV | COL_CYAN), buf);
		vid_puts(x + 2, y + 6, x + w - 2, COL_CYAN, "Enter accept    Esc cancel");
		kbd_read(&ev);
		if (ev.scan == SCAN_ESC)
			return 0;
		if (ev.scan == SCAN_ENTER) {
			if (len <= 0)
				return 0;
			return 1;
		}
		if (ev.scan == SCAN_BS || ev.ascii == 8) {
			edit_bs(buf, &len);
			continue;
		}
		if (ev.ascii >= 0x20 || is_sjis_lead(ev.ascii)) {
			if (len < max) {
				buf[len++] = (char)ev.ascii;
				buf[len] = 0;
			}
		}
	}
}

static void help_screen(void)
{
	static const char *lines[] = {
		"Tab            switch panel",
		"Up / Down      move the cursor",
		"Left           parent directory",
		"Right / Enter  open directory or run COM EXE BAT",
		"Roll Up/Down   page",
		"Home / HELP    first file / last file",
		"Ins            mark, and move down",
		"*              invert marks",
		"F2             drive of the active panel",
		"F3             view a file",
		"F4             edit, program is set in NC.INI",
		"F5             copy to the other panel",
		"F6             move, or rename if both panels match",
		"GRPH/CTRL+F6   rename the file under the cursor",
		"F7             make a directory",
		"F8 / Del       delete",
		"F10            quit",
		"Type           command line, Enter runs COMSPEC /C",
		0
	};
	struct keyev ev;
	int i;

	vid_clear(COL_WHITE);
	vid_fill(0, 0, VID_COLS, (unsigned short)(ATTR_REV | COL_CYAN), ' ');
	vid_puts(1, 0, 78, (unsigned short)(ATTR_REV | COL_CYAN), "PC98NC");
	for (i = 0; lines[i]; i++)
		vid_puts(2, 2 + i, 78, COL_WHITE, lines[i]);
	vid_puts(2, 22, 78, COL_YELLOW, "Any key returns");
	kbd_read(&ev);
	(void)ev;
	draw_damage();
	draw_all();
}

static int pick_drive(void)
{
	unsigned char list[26];
	struct keyev ev;
	int n;
	int cur;
	int i;
	unsigned char d;

	n = 0;
	for (d = 1; d <= 26; d++) {
		if (dos_drive_ok(d))
			list[n++] = (unsigned char)(d - 1);
	}
	if (n <= 0) {
		ui_status("No drives");
		draw_all();
		return 0;
	}
	cur = 0;
	for (;;) {
		int x;
		int y;

		x = 16;
		y = 8;
		dialog_frame(x, y, 48, 8, "Drive");
		vid_puts(x + 2, y + 3, x + 46, COL_WHITE, "Select a drive");
		for (i = 0; i < n; i++) {
			unsigned short attr;
			char letter[2];

			attr = (i == cur) ? (unsigned short)(ATTR_REV | COL_CYAN)
					  : COL_WHITE;
			letter[0] = (char)('A' + list[i]);
			letter[1] = 0;
			vid_puts(x + 2 + (i % 13) * 3, y + 4 + (i / 13),
				 x + 46, attr, letter);
		}
		vid_puts(x + 2, y + 6, x + 46, COL_CYAN, "Enter select    Esc cancel");
		kbd_read(&ev);
		if (ev.scan == SCAN_ESC) {
			draw_all();
			return 0;
		}
		if (ev.scan == SCAN_ENTER) {
			panel_setdrive(&g_pan[g_act], list[cur]);
			draw_all();
			return 1;
		}
		if (ev.scan == SCAN_LEFT)
			cur = (cur + n - 1) % n;
		else if (ev.scan == SCAN_RIGHT)
			cur = (cur + 1) % n;
		else if (ev.scan == SCAN_UP && cur >= 13)
			cur -= 13;
		else if (ev.scan == SCAN_DOWN && cur + 13 < n)
			cur += 13;
		else if (ev.ascii >= 'a' && ev.ascii <= 'z')
			ev.ascii = (unsigned char)(ev.ascii - 32);
		if (ev.ascii >= 'A' && ev.ascii <= 'Z') {
			for (i = 0; i < n; i++) {
				if (list[i] == (unsigned char)(ev.ascii - 'A')) {
					cur = i;
					break;
				}
			}
		}
	}
}

static void cmd_bs(void)
{
	if (g_cmdlen <= 0)
		return;
	if (g_cmdlen >= 2 &&
	    is_sjis_lead((unsigned char)g_cmd[g_cmdlen - 2]) &&
	    is_sjis_trail((unsigned char)g_cmd[g_cmdlen - 1]))
		g_cmdlen -= 2;
	else
		g_cmdlen--;
	g_cmd[g_cmdlen] = 0;
}

static void cmd_add(unsigned char c)
{
	if (g_cmdlen + 1 >= CMD_MAX)
		return;
	g_cmd[g_cmdlen++] = (char)c;
	g_cmd[g_cmdlen] = 0;
}

static int handle(struct keyev *ev)
{
	struct panel *p;

	p = &g_pan[g_act];
	if (ev->scan == SCAN_F10)
		return 1;
	if (ev->scan == SCAN_ESC) {
		if (g_cmdlen > 0) {
			g_cmdlen = 0;
			g_cmd[0] = 0;
		}
		g_msg[0] = 0;
		draw_all();
		return 0;
	}
	if (ev->scan == SCAN_TAB) {
		g_act = 1 - g_act;
		g_msg[0] = 0;
		draw_all();
		return 0;
	}
	if (ev->scan == SCAN_UP) {
		panel_move(p, -1);
		g_msg[0] = 0;
		draw_all();
		return 0;
	}
	if (ev->scan == SCAN_DOWN) {
		panel_move(p, 1);
		g_msg[0] = 0;
		draw_all();
		return 0;
	}
	if (ev->scan == SCAN_ROLLUP) {
		panel_move(p, -PANEL_ROWS);
		g_msg[0] = 0;
		draw_all();
		return 0;
	}
	if (ev->scan == SCAN_ROLLDN) {
		panel_move(p, PANEL_ROWS);
		g_msg[0] = 0;
		draw_all();
		return 0;
	}
	if (ev->scan == SCAN_HOME) {
		panel_move(p, -1000);
		g_msg[0] = 0;
		draw_all();
		return 0;
	}
	if (ev->scan == SCAN_HELP) {
		panel_move(p, 1000);
		g_msg[0] = 0;
		draw_all();
		return 0;
	}
	if (ev->scan == SCAN_LEFT) {
		panel_parent(p);
		g_msg[0] = 0;
		draw_all();
		return 0;
	}
	if (ev->scan == SCAN_RIGHT) {
		panel_enter_dir(p);
		g_msg[0] = 0;
		draw_all();
		return 0;
	}
	if (ev->scan == SCAN_INS) {
		panel_mark(p);
		g_msg[0] = 0;
		draw_all();
		return 0;
	}
	if (ev->scan == SCAN_DEL || ev->scan == SCAN_F8) {
		do_delete();
		return 0;
	}
	if (g_cmdlen == 0 && (ev->ascii == '*' || ev->scan == SCAN_STAR)) {
		panel_invert(p);
		g_msg[0] = 0;
		draw_all();
		return 0;
	}
	if (ev->scan == SCAN_F1) {
		help_screen();
		return 0;
	}
	if (ev->scan == SCAN_F2) {
		pick_drive();
		return 0;
	}
	if (ev->scan == SCAN_F3) {
		do_view();
		return 0;
	}
	if (ev->scan == SCAN_F4) {
		do_edit();
		return 0;
	}
	if (ev->scan == SCAN_F5) {
		do_copy();
		return 0;
	}
	if (ev->scan == SCAN_F6 &&
	    (ev->shift & (MOD_SHIFT | MOD_GRPH | MOD_CTRL))) {
		do_rename();
		return 0;
	}
	if (ev->scan == SCAN_F6) {
		do_move();
		return 0;
	}
	if (ev->scan == SCAN_F7) {
		do_mkdir();
		return 0;
	}
	if (ev->scan == SCAN_ENTER) {
		if (g_cmdlen > 0)
			do_run_cmd();
		else
			do_run_cursor();
		return 0;
	}
	if (ev->scan == SCAN_BS || ev->ascii == 8) {
		cmd_bs();
		g_msg[0] = 0;
		draw_all();
		return 0;
	}
	if (ev->ascii >= 0x20 || is_sjis_lead(ev->ascii)) {
		cmd_add(ev->ascii);
		g_msg[0] = 0;
		draw_all();
	}
	return 0;
}

int main(void)
{
	struct keyev ev;

	g_cmdlen = 0;
	g_cmd[0] = 0;
	g_msg[0] = 0;
	g_act = 0;
	g_comspec[0] = 0;
	dos_comspec(g_comspec, (int)sizeof(g_comspec));
	dos_crit_on();
	ini_load();
	vid_init();
	panel_init();
	vid_clear(COL_WHITE);
	draw_all();
	for (;;) {
		kbd_read(&ev);
		if (handle(&ev))
			break;
	}
	dos_crit_off();
	vid_done();
	return 0;
}
