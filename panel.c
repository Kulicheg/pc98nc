#include "pc98.h"

#include <dos.h>
#include <string.h>

static unsigned char g_frame[2];
static unsigned char g_path_ok[2];
static char g_path_c[2][PATH_MAX];
static unsigned char g_row_ok[2][PANEL_ROWS];
static unsigned short g_row_attr[2][PANEL_ROWS];
static char g_row_txt[2][PANEL_ROWS][PANEL_INNER];

static int panel_id(const struct panel *p)
{
	return p == &g_pan[1];
}

void panel_damage(void)
{
	memset(g_frame, 0, sizeof(g_frame));
	memset(g_path_ok, 0, sizeof(g_path_ok));
	memset(g_row_ok, 0, sizeof(g_row_ok));
}

static void draw_frame(int x)
{
	char hdr[PANEL_INNER + 1];
	int y;
	int right;
	int gap;

	vid_box(x, 0, PANEL_W, PANEL_H, COL_WHITE);
	right = x + PANEL_W;
	if (right >= VID_COLS)
		right = VID_COLS - 1;
	memset(hdr, ' ', PANEL_INNER);
	hdr[PANEL_INNER] = 0;
	memcpy(hdr + 1, "Name", 4);
	memcpy(hdr + 14, "Date", 4);
	memcpy(hdr + 31, "Size", 4);
	/* Underline sits on the bottom of the cell, the vertical on its left
	   edge, so the border cell itself has to carry both or the line
	   starts a column late. The right-hand border cell stays vertical
	   only, otherwise the line would run past it. */
	vid_cell(x, 2, (unsigned short)(COL_WHITE | ATTR_UL | ATTR_VLINE), ' ');
	vid_cell(x + 1, 2, (unsigned short)(COL_WHITE | ATTR_UL), ' ');
	vid_putpad(x + 2, 2, PANEL_INNER, (unsigned short)(COL_WHITE | ATTR_UL), hdr);
	gap = x + 2 + PANEL_INNER;
	if (gap < right)
		vid_fill(gap, 2, right - gap, (unsigned short)(COL_WHITE | ATTR_UL), ' ');
	/* The screen-edge top cell is not part of the line: the vertical
	   starts on the next row, and a clear used to leave this one blank. */
	if (x + PANEL_W >= VID_COLS)
		vid_cell(VID_COLS - 1, 0, COL_WHITE, ' ');
	for (y = 1; y < PANEL_H - 1; y++) {
		if (y == 2)
			continue;
		vid_cell(x + 1, y, COL_WHITE, ' ');
		if (gap < right)
			vid_fill(gap, y, right - gap, COL_WHITE, ' ');
	}
}

static int is_dotdot(const struct file_ent *e)
{
	return e->name[0] == '.' && e->name[1] == '.' && e->name[2] == 0;
}

static int ent_less(const struct file_ent *a, const struct file_ent *b)
{
	int ad;
	int bd;

	if (is_dotdot(a))
		return 1;
	if (is_dotdot(b))
		return 0;
	ad = (a->attr & FA_DIR) != 0;
	bd = (b->attr & FA_DIR) != 0;
	if (ad != bd)
		return ad > bd;
	return namecmp(a->name, b->name) < 0;
}

static void sort_files(struct panel *p)
{
	unsigned int i;
	unsigned int j;
	struct file_ent tmp;

	for (i = 1; i < p->count; i++) {
		tmp = p->files[i];
		j = i;
		while (j > 0 && ent_less(&tmp, &p->files[j - 1])) {
			p->files[j] = p->files[j - 1];
			j--;
		}
		p->files[j] = tmp;
	}
}

static void add_dotdot(struct panel *p)
{
	struct file_ent *e;

	if (path_is_root(p->path) || p->count >= MAX_FILES)
		return;
	e = &p->files[p->count++];
	memset(e, 0, sizeof(*e));
	e->name[0] = '.';
	e->name[1] = '.';
	e->attr = FA_DIR;
}

static void take_entry(struct panel *p, const struct find_t *ft)
{
	struct file_ent *e;
	const char *name;

	name = ft->name;
	if (name[0] == '.' && name[1] == 0)
		return;
	if (name[0] == '.' && name[1] == '.' && name[2] == 0)
		return;
	if (ft->attrib & FA_VOL)
		return;
	if (p->count >= MAX_FILES)
		return;
	e = &p->files[p->count++];
	memset(e, 0, sizeof(*e));
	strncpy(e->name, name, NAME_LEN - 1);
	e->name[NAME_LEN - 1] = 0;
	e->attr = (unsigned char)ft->attrib;
	e->time = ft->wr_time;
	e->date = ft->wr_date;
	e->size = ft->size;
}

void panel_reload(struct panel *p)
{
	struct find_t ft;
	char spec[PATH_MAX];
	char keep[NAME_LEN];
	unsigned err;
	unsigned int i;
	unsigned char drive1;

	keep[0] = 0;
	if (p->count > 0 && p->cursor < p->count)
		strcpy(keep, p->files[p->cursor].name);
	p->count = 0;
	p->cursor = 0;
	p->top = 0;
	p->freeok = 0;
	if (path_join(spec, p->path, "*.*") != 0) {
		strcpy(g_msg, "Path too long");
		return;
	}
	err = _dos_findfirst(spec, FA_ANY, &ft);
	if (crit_hit) {
		crit_hit = 0;
		if (!err)
			err = 21;
	}
	if (err) {
		strcpy(g_msg, dos_errmsg((int)err));
		add_dotdot(p);
		return;
	}
	take_entry(p, &ft);
	for (;;) {
		err = _dos_findnext(&ft);
		if (crit_hit) {
			crit_hit = 0;
			if (!err || err == 18)
				err = 21;
		}
		if (err) {
			if (err != 18)
				strcpy(g_msg, dos_errmsg((int)err));
			break;
		}
		if (p->count >= MAX_FILES - 1) {
			strcpy(g_msg, "Too many files");
			break;
		}
		take_entry(p, &ft);
	}
	add_dotdot(p);
	sort_files(p);
	if (keep[0]) {
		for (i = 0; i < p->count; i++) {
			if (strcmp(p->files[i].name, keep) == 0) {
				p->cursor = i;
				break;
			}
		}
	}
	if (p->cursor >= PANEL_ROWS)
		p->top = p->cursor - (PANEL_ROWS - 1);
	drive1 = 1;
	if (p->path[0] >= 'A' && p->path[0] <= 'Z')
		drive1 = (unsigned char)(p->path[0] - 'A' + 1);
	else if (p->path[0] >= 'a' && p->path[0] <= 'z')
		drive1 = (unsigned char)(p->path[0] - 'a' + 1);
	if (dos_free(drive1, &p->freeb) == 0)
		p->freeok = 1;
}

void panel_init(void)
{
	char rest[66];
	unsigned char d;
	int i;

	d = dos_getdrive();
	rest[0] = 0;
	if (dos_getcwd(0, rest) != 0)
		rest[0] = 0;
	g_pan[0].path[0] = (char)('A' + d);
	g_pan[0].path[1] = ':';
	g_pan[0].path[2] = '\\';
	g_pan[0].path[3] = 0;
	if (rest[0])
		strcat(g_pan[0].path, rest);
	strcpy(g_pan[1].path, g_pan[0].path);
	for (i = 0; i < 2; i++) {
		g_pan[i].count = 0;
		g_pan[i].cursor = 0;
		g_pan[i].top = 0;
		g_pan[i].freeok = 0;
		panel_reload(&g_pan[i]);
	}
	g_msg[0] = 0;
}

void panel_move(struct panel *p, int delta)
{
	int c;
	int n;

	n = (int)p->count;
	if (n <= 0)
		return;
	c = (int)p->cursor + delta;
	if (c < 0)
		c = 0;
	if (c >= n)
		c = n - 1;
	p->cursor = (unsigned)c;
	if (p->cursor < p->top)
		p->top = p->cursor;
	if (p->cursor >= p->top + PANEL_ROWS)
		p->top = p->cursor - (PANEL_ROWS - 1);
}

void panel_parent(struct panel *p)
{
	char leaf[NAME_LEN];
	int i;
	int n;
	int len;
	unsigned int k;

	if (path_is_root(p->path))
		return;
	/* The folder we are leaving, so the parent list can land on it. */
	n = (int)strlen(p->path);
	if (n > 3 && p->path[n - 1] == '\\')
		n--;
	i = n - 1;
	while (i > 2 && p->path[i] != '\\')
		i--;
	if (p->path[i] == '\\')
		i++;
	leaf[0] = 0;
	if (i < n) {
		len = n - i;
		if (len > NAME_LEN - 1)
			len = NAME_LEN - 1;
		memcpy(leaf, p->path + i, (unsigned)len);
		leaf[len] = 0;
	}
	path_parent(p->path);
	p->cursor = 0;
	p->top = 0;
	panel_reload(p);
	for (k = 0; k < p->count; k++) {
		if (namecmp(p->files[k].name, leaf) == 0) {
			p->cursor = k;
			break;
		}
	}
	if (p->cursor < PANEL_ROWS)
		p->top = 0;
	else
		p->top = p->cursor - (PANEL_ROWS - 1);
}

void panel_enter_dir(struct panel *p)
{
	struct file_ent *e;
	char next[PATH_MAX];

	e = panel_cur(p);
	if (!e || (e->attr & FA_DIR) == 0)
		return;
	if (is_dotdot(e)) {
		panel_parent(p);
		return;
	}
	if (path_join(next, p->path, e->name) != 0) {
		strcpy(g_msg, "Path too long");
		return;
	}
	strcpy(p->path, next);
	p->cursor = 0;
	p->top = 0;
	panel_reload(p);
}

void panel_mark(struct panel *p)
{
	struct file_ent *e;

	e = panel_cur(p);
	if (!e || is_dotdot(e)) {
		panel_move(p, 1);
		return;
	}
	e->mark = (unsigned char)!e->mark;
	panel_move(p, 1);
}

void panel_invert(struct panel *p)
{
	unsigned int i;

	for (i = 0; i < p->count; i++) {
		if (is_dotdot(&p->files[i]))
			continue;
		p->files[i].mark = (unsigned char)!p->files[i].mark;
	}
}

void panel_setdrive(struct panel *p, unsigned char drive0)
{
	char rest[66];

	g_msg[0] = 0;
	p->path[0] = (char)('A' + drive0);
	p->path[1] = ':';
	p->path[2] = '\\';
	p->path[3] = 0;
	rest[0] = 0;
	if (dos_getcwd((unsigned char)(drive0 + 1), rest) == 0 && rest[0])
		strcat(p->path, rest);
	p->cursor = 0;
	p->top = 0;
	panel_reload(p);
}

struct file_ent *panel_cur(struct panel *p)
{
	if (p->count == 0 || p->cursor >= p->count)
		return 0;
	return &p->files[p->cursor];
}

static void format_row(char *line, const struct file_ent *e)
{
	char sz[16];
	char dt[12];
	int n;
	int k;

	memset(line, ' ', PANEL_INNER);
	line[PANEL_INNER] = 0;
	line[0] = e->mark ? '*' : ' ';
	n = 0;
	while (e->name[n] && n < 12) {
		line[1 + n] = e->name[n];
		n++;
	}
	if (e->date) {
		fmt_date(dt, e->date);
		memcpy(line + 14, dt, 8);
	}
	if (e->attr & FA_DIR) {
		memcpy(line + 30, "<DIR>", 5);
	} else {
		fmt_ulong(sz, e->size);
		k = (int)strlen(sz);
		if (k > 8)
			k = 8;
		memcpy(line + PANEL_INNER - k, sz, (unsigned)k);
	}
}

void panel_draw(const struct panel *p, int x, int active)
{
	char line[PANEL_INNER + 1];
	int id;
	int row;

	id = panel_id(p);
	if (!g_frame[id]) {
		draw_frame(x);
		g_frame[id] = 1;
	}
	if (!g_path_ok[id] || strcmp(g_path_c[id], p->path) != 0) {
		vid_putpad(x + 2, 1, PANEL_INNER, COL_YELLOW, p->path);
		strncpy(g_path_c[id], p->path, PATH_MAX - 1);
		g_path_c[id][PATH_MAX - 1] = 0;
		g_path_ok[id] = 1;
	}
	for (row = 0; row < PANEL_ROWS; row++) {
		int y;
		int idx;
		unsigned short attr;
		const struct file_ent *e;
		int on;

		y = PANEL_FILE_Y + row;
		idx = (int)p->top + row;
		on = active && idx >= 0 && (unsigned)idx < p->count &&
		     (unsigned)idx == p->cursor;
		attr = COL_WHITE;
		if (idx >= 0 && (unsigned)idx < p->count) {
			e = &p->files[idx];
			if (e->attr & FA_DIR)
				attr = COL_YELLOW;
			else if (is_exec(e->name))
				attr = COL_GREEN;
			if (e->mark)
				attr = COL_RED;
			if (on)
				attr = (unsigned short)(ATTR_REV |
					(e->mark ? COL_YELLOW : COL_CYAN));
			format_row(line, e);
		} else {
			memset(line, ' ', PANEL_INNER);
			line[PANEL_INNER] = 0;
			if (on)
				attr = (unsigned short)(ATTR_REV | COL_WHITE);
		}
		if (g_row_ok[id][row] && g_row_attr[id][row] == attr &&
		    memcmp(g_row_txt[id][row], line, PANEL_INNER) == 0)
			continue;
		vid_putpad(x + 2, y, PANEL_INNER, attr, line);
		memcpy(g_row_txt[id][row], line, PANEL_INNER);
		g_row_attr[id][row] = attr;
		g_row_ok[id][row] = 1;
	}
}
