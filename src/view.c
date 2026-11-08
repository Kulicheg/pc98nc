#include "pc98.h"

#include <string.h>

#define VIEW_BUF 8192
#define VIEW_HIST 24

static unsigned char g_vbuf[VIEW_BUF];
static unsigned long g_hist[VIEW_HIST];
static int g_hn;
static unsigned long g_off;
static int g_h;

static unsigned int view_paint(const char *title)
{
	unsigned short got;
	unsigned int i;
	unsigned int n;
	int x;
	int y;
	int err;

	vid_clear(COL_WHITE);
	vid_fill(0, 0, VID_COLS, (unsigned short)(ATTR_REV | COL_CYAN), ' ');
	vid_puts(1, 0, 78, (unsigned short)(ATTR_REV | COL_CYAN), title);
	vid_fill(0, 24, VID_COLS, (unsigned short)(ATTR_REV | COL_CYAN), ' ');
	vid_puts(1, 24, 79, (unsigned short)(ATTR_REV | COL_CYAN),
		 "Esc close    Roll Up / Roll Down page");
	err = dos_seek(g_h, g_off);
	got = 0;
	if (!err)
		err = dos_read(g_h, g_vbuf, VIEW_BUF, &got);
	if (err) {
		vid_puts(1, 2, 78, COL_RED, dos_errmsg(err));
		return 0;
	}
	if (got == 0) {
		vid_puts(1, 2, 78, COL_WHITE, "(empty)");
		return 0;
	}
	n = got;
	i = 0;
	x = 0;
	y = 1;
	while (i < n && y < 23) {
		unsigned char c;

		c = g_vbuf[i];
		if (c == '\n') {
			y++;
			x = 0;
			i++;
			continue;
		}
		if (c == '\r') {
			i++;
			continue;
		}
		if (is_sjis_lead(c) && i + 1 < n && is_sjis_trail(g_vbuf[i + 1])) {
			char pair[3];

			if (x + 2 > 78) {
				y++;
				x = 0;
				if (y >= 23)
					break;
			}
			pair[0] = (char)c;
			pair[1] = (char)g_vbuf[i + 1];
			pair[2] = 0;
			vid_puts(x, y, 80, COL_WHITE, pair);
			x += 2;
			i += 2;
			continue;
		}
		if (c == '\t') {
			int ns;

			ns = 8 - (x & 7);
			while (ns > 0 && x < 78) {
				vid_cell(x, y, COL_WHITE, ' ');
				x++;
				ns--;
			}
			i++;
			continue;
		}
		if (c < 0x20 || c == 0x7F)
			c = '.';
		if (x >= 78) {
			y++;
			x = 0;
			if (y >= 23)
				break;
		}
		vid_cell(x, y, COL_WHITE, c);
		x++;
		i++;
	}
	return i;
}

void do_view(void)
{
	struct panel *p;
	struct file_ent *e;
	char path[PATH_MAX];
	char title[PATH_MAX];
	struct keyev ev;
	unsigned int used;

	p = &g_pan[g_act];
	e = panel_cur(p);
	if (!e || (e->attr & FA_DIR)) {
		ui_status("Not a file");
		draw_all();
		return;
	}
	if (path_join(path, p->path, e->name) != 0) {
		ui_status("Path too long");
		draw_all();
		return;
	}
	g_h = dos_open(path, 0);
	if (g_h < 0) {
		ui_status(dos_errmsg(-g_h));
		draw_all();
		return;
	}
	strcpy(title, e->name);
	g_off = 0;
	g_hn = 0;
	for (;;) {
		used = view_paint(title);
		kbd_read(&ev);
		if (ev.scan == SCAN_ESC)
			break;
		if (ev.scan == SCAN_ROLLDN || ev.scan == SCAN_DOWN) {
			if (used == 0)
				continue;
			if (g_hn < VIEW_HIST)
				g_hist[g_hn++] = g_off;
			else {
				memmove(g_hist, g_hist + 1,
					(VIEW_HIST - 1) * sizeof(g_hist[0]));
				g_hist[VIEW_HIST - 1] = g_off;
			}
			g_off += used;
			continue;
		}
		if (ev.scan == SCAN_ROLLUP || ev.scan == SCAN_UP) {
			if (g_hn > 0)
				g_off = g_hist[--g_hn];
			continue;
		}
		if (ev.scan == SCAN_HOME) {
			g_off = 0;
			g_hn = 0;
		}
	}
	dos_close(g_h);
	g_h = -1;
	vid_init();
	draw_damage();
	draw_all();
}
