#include "pc98.h"

#include <dos.h>
#include <string.h>

#define COPY_BUF 4096

static unsigned char g_buf[COPY_BUF];
static int g_all;
static int g_stop;

struct job {
	char src[PATH_MAX];
	char dst[PATH_MAX];
	struct find_t ft;
	unsigned char isdir;
	unsigned char started;
};

static struct job g_job[OP_DEPTH];
static int g_sp;

static int is_dot(const char *name)
{
	if (name[0] != '.')
		return 0;
	if (name[1] == 0)
		return 1;
	return name[1] == '.' && name[2] == 0;
}

static void stop_msg(const char *text)
{
	ui_status(text);
	g_stop = 1;
}

static int ensure_dir(const char *path)
{
	unsigned char attr;
	int err;

	if (dos_exists(path, &attr)) {
		if (attr & FA_DIR)
			return 0;
		stop_msg("Destination is a file");
		return -1;
	}
	err = dos_mkdir(path);
	if (err) {
		stop_msg(dos_errmsg(err));
		return -1;
	}
	return 0;
}

static int copy_file(const char *src, const char *dst)
{
	unsigned char attr;
	unsigned short date;
	unsigned short time;
	int in;
	int out;
	int err;
	int created;
	unsigned long copied;

	if (g_stop)
		return -1;
	if (ui_esc()) {
		stop_msg("Cancelled");
		return -1;
	}
	ui_progress("Copy", src, dst);
	in = dos_open(src, 0);
	if (in < 0) {
		stop_msg(dos_errmsg(-in));
		return -1;
	}
	date = 0;
	time = 0;
	dos_gettime(in, &time, &date);
	if (dos_exists(dst, &attr)) {
		int yes;

		if (attr & FA_DIR) {
			dos_close(in);
			stop_msg("Destination is a directory");
			return -1;
		}
		yes = 0;
		if (g_all)
			yes = 1;
		else {
			int a;

			a = ui_ask("Overwrite", dst, 1);
			if (a == 3) {
				dos_close(in);
				stop_msg("Cancelled");
				return -1;
			}
			if (a == 2) {
				g_all = 1;
				yes = 1;
			} else {
				yes = a == 1;
			}
		}
		if (!yes) {
			dos_close(in);
			return 1;
		}
		err = dos_unlink(dst);
		if (err) {
			dos_close(in);
			stop_msg(dos_errmsg(err));
			return -1;
		}
	}
	out = dos_create(dst, FA_ARCH);
	if (out < 0) {
		dos_close(in);
		stop_msg(dos_errmsg(-out));
		return -1;
	}
	created = 1;
	copied = 0;
	ui_progress("Copy", src, dst);
	for (;;) {
		unsigned short got;
		unsigned short put;

		if (ui_esc()) {
			err = -1;
			stop_msg("Cancelled");
			break;
		}
		err = dos_read(in, g_buf, COPY_BUF, &got);
		if (err || got == 0)
			break;
		err = dos_write(out, g_buf, got, &put);
		if (err || put != got) {
			if (!err)
				err = 39;
			break;
		}
		copied += put;
	}
	if (!err)
		dos_settime(out, time, date);
	dos_close(out);
	dos_close(in);
	if (err) {
		/*
		 * A failed write often leaves a 0-byte name behind (disk full
		 * on a 2048-byte cluster). Drop that empty file so the panel
		 * does not show a copy that never received any bytes.
		 */
		if (created && copied == 0)
			dos_unlink(dst);
		if (!g_stop)
			stop_msg(dos_errmsg(err));
		return -1;
	}
	return 0;
}

static int push_job(const char *src, const char *dst, int isdir)
{
	struct job *j;

	if (g_sp >= OP_DEPTH) {
		stop_msg("Directory too deep");
		return -1;
	}
	j = &g_job[g_sp++];
	memset(j, 0, sizeof(*j));
	strcpy(j->src, src);
	strcpy(j->dst, dst);
	j->isdir = (unsigned char)(isdir ? 1 : 0);
	return 0;
}

static void copy_tree(const char *src, const char *dst, int isdir)
{
	g_sp = 0;
	if (path_inside(src, dst)) {
		stop_msg("Cannot copy a directory into itself");
		return;
	}
	if (push_job(src, dst, isdir) != 0)
		return;
	while (g_sp > 0 && !g_stop) {
		struct job *j;
		unsigned err;
		char child_s[PATH_MAX];
		char child_d[PATH_MAX];
		char wild[PATH_MAX];

		j = &g_job[g_sp - 1];
		if (!j->isdir) {
			copy_file(j->src, j->dst);
			g_sp--;
			continue;
		}
		if (!j->started) {
			if (path_inside(j->src, j->dst)) {
				stop_msg("Cannot copy a directory into itself");
				g_sp--;
				continue;
			}
			if (ensure_dir(j->dst) != 0) {
				g_sp--;
				continue;
			}
			if (path_join(wild, j->src, "*.*") != 0) {
				stop_msg("Path too long");
				g_sp--;
				continue;
			}
			err = _dos_findfirst(wild, FA_ANY, &j->ft);
			j->started = 1;
			if (err) {
				g_sp--;
				continue;
			}
		} else {
			err = _dos_findnext(&j->ft);
			if (err) {
				g_sp--;
				continue;
			}
		}
		if (is_dot(j->ft.name))
			continue;
		if (path_join(child_s, j->src, j->ft.name) != 0 ||
		    path_join(child_d, j->dst, j->ft.name) != 0) {
			stop_msg("Path too long");
			continue;
		}
		if (j->ft.attrib & FA_DIR)
			push_job(child_s, child_d, 1);
		else
			copy_file(child_s, child_d);
	}
}

static void delete_tree(const char *path, int isdir)
{
	g_sp = 0;
	g_stop = 0;
	if (push_job(path, path, isdir) != 0)
		return;
	while (g_sp > 0 && !g_stop) {
		struct job *j;
		unsigned err;
		char child[PATH_MAX];
		char wild[PATH_MAX];

		j = &g_job[g_sp - 1];
		if (ui_esc()) {
			stop_msg("Cancelled");
			break;
		}
		if (!j->isdir) {
			ui_progress("Delete", j->src, 0);
			err = (unsigned)dos_unlink(j->src);
			if (err)
				stop_msg(dos_errmsg((int)err));
			g_sp--;
			continue;
		}
		if (!j->started) {
			if (path_join(wild, j->src, "*.*") != 0) {
				stop_msg("Path too long");
				g_sp--;
				continue;
			}
			err = _dos_findfirst(wild, FA_ANY, &j->ft);
			j->started = 1;
			if (err) {
				err = (unsigned)dos_rmdir(j->src);
				if (err)
					stop_msg(dos_errmsg((int)err));
				g_sp--;
				continue;
			}
		} else {
			err = _dos_findnext(&j->ft);
			if (err) {
				err = (unsigned)dos_rmdir(j->src);
				if (err)
					stop_msg(dos_errmsg((int)err));
				g_sp--;
				continue;
			}
		}
		if (is_dot(j->ft.name))
			continue;
		if (path_join(child, j->src, j->ft.name) != 0) {
			stop_msg("Path too long");
			continue;
		}
		if (j->ft.attrib & FA_DIR) {
			push_job(child, child, 1);
		} else {
			ui_progress("Delete", child, 0);
			err = (unsigned)dos_unlink(child);
			if (err)
				stop_msg(dos_errmsg((int)err));
		}
	}
}

static int collect(struct panel *p, unsigned short *sel, int max)
{
	unsigned int i;
	int any;
	int n;

	any = 0;
	n = 0;
	for (i = 0; i < p->count; i++) {
		if (p->files[i].mark)
			any = 1;
	}
	if (any) {
		for (i = 0; i < p->count && n < max; i++) {
			if (!p->files[i].mark)
				continue;
			if (p->files[i].name[0] == '.' &&
			    p->files[i].name[1] == '.' &&
			    p->files[i].name[2] == 0)
				continue;
			sel[n++] = (unsigned short)i;
		}
		return n;
	}
	if (p->count == 0 || p->cursor >= p->count)
		return 0;
	if (p->files[p->cursor].name[0] == '.' &&
	    p->files[p->cursor].name[1] == '.' &&
	    p->files[p->cursor].name[2] == 0)
		return 0;
	sel[0] = (unsigned short)p->cursor;
	return 1;
}

static void op_line(char *dst, const char *verb, int n, const char *name, int ask)
{
	char num[12];

	strcpy(dst, verb);
	strcat(dst, " ");
	if (n == 1) {
		strncat(dst, name, 24);
	} else {
		fmt_ulong(num, (unsigned long)n);
		strcat(dst, num);
		strcat(dst, " items");
	}
	if (ask)
		strcat(dst, "?");
	else
		strcat(dst, " to");
}

static int sel_has(const unsigned short *sel, int n, unsigned int idx)
{
	int i;

	for (i = 0; i < n; i++) {
		if ((unsigned int)sel[i] == idx)
			return 1;
	}
	return 0;
}

/* After a delete, stay on the next surviving row. The last row has no next,
 * so step back to the previous one instead of the top of the list. */
static void place_after_delete(struct panel *p, const unsigned short *sel, int removed)
{
	int k;
	int place;
	int cur;

	cur = (int)p->cursor;
	place = -1;
	for (k = cur; k < (int)p->count; k++) {
		if (!sel_has(sel, removed, (unsigned int)k)) {
			place = k;
			break;
		}
	}
	if (place < 0) {
		for (k = cur - 1; k >= 0; k--) {
			if (!sel_has(sel, removed, (unsigned int)k)) {
				place = k;
				break;
			}
		}
	}
	if (place >= 0)
		p->cursor = (unsigned int)place;
}

static void finish(struct panel *a, struct panel *b)
{
	panel_reload(a);
	if (b && b != a)
		panel_reload(b);
	draw_all();
}

void do_copy(void)
{
	struct panel *src;
	struct panel *dst;
	unsigned short sel[MAX_FILES];
	int n;
	int i;
	char from[PATH_MAX];
	char to[PATH_MAX];
	char line[48];

	src = &g_pan[g_act];
	dst = &g_pan[1 - g_act];
	n = collect(src, sel, MAX_FILES);
	if (n <= 0) {
		ui_status("Nothing to copy");
		draw_all();
		return;
	}
	op_line(line, "Copy", n, src->files[sel[0]].name, 0);
	if (!ui_confirm("Copy", line, dst->path)) {
		ui_status("Cancelled");
		draw_all();
		return;
	}
	g_all = 0;
	g_stop = 0;
	for (i = 0; i < n && !g_stop; i++) {
		struct file_ent *e;

		e = &src->files[sel[i]];
		if (path_join(from, src->path, e->name) != 0 ||
		    path_join(to, dst->path, e->name) != 0) {
			ui_status("Path too long");
			break;
		}
		copy_tree(from, to, (e->attr & FA_DIR) != 0);
	}
	if (i == n && !g_stop)
		ui_status("Copied");
	finish(dst, 0);
}

void do_delete(void)
{
	struct panel *p;
	unsigned short sel[MAX_FILES];
	int n;
	int i;
	char path[PATH_MAX];
	char line[48];

	p = &g_pan[g_act];
	n = collect(p, sel, MAX_FILES);
	if (n <= 0) {
		ui_status("Nothing to delete");
		draw_all();
		return;
	}
	op_line(line, "Delete", n, p->files[sel[0]].name, 1);
	if (!ui_confirm("Delete", line, 0)) {
		ui_status("Cancelled");
		draw_all();
		return;
	}
	for (i = 0; i < n; i++) {
		struct file_ent *e;

		e = &p->files[sel[i]];
		if (path_join(path, p->path, e->name) != 0) {
			ui_status("Path too long");
			break;
		}
		delete_tree(path, (e->attr & FA_DIR) != 0);
		if (g_stop)
			break;
	}
	place_after_delete(p, sel, i);
	if (i == n && !g_stop)
		ui_status("Deleted");
	finish(p, 0);
}

static int same_drive(const char *a, const char *b)
{
	unsigned char ca;
	unsigned char cb;

	ca = (unsigned char)a[0];
	cb = (unsigned char)b[0];
	if (ca >= 'a' && ca <= 'z')
		ca = (unsigned char)(ca - 32);
	if (cb >= 'a' && cb <= 'z')
		cb = (unsigned char)(cb - 32);
	if (ca < 'A' || ca > 'Z')
		return 0;
	return ca == cb;
}

static void status_move(const char *path)
{
	ui_progress("Move", path, 0);
}

/* Move one file. On the same drive this is a rename and needs no extra
 * space. Across drives the file is copied and removed before the caller
 * goes on to the next name. Returns 1 if the user skipped an overwrite. */
static int move_file(const char *src, const char *dst)
{
	unsigned char attr;
	int err;
	int yes;

	if (g_stop)
		return -1;
	if (ui_esc()) {
		stop_msg("Cancelled");
		return -1;
	}
	status_move(src);
	if (same_drive(src, dst)) {
		if (dos_exists(dst, &attr)) {
			if (attr & FA_DIR) {
				stop_msg("Destination is a directory");
				return -1;
			}
			yes = g_all;
			if (!yes) {
				int a;

				a = ui_ask("Overwrite", dst, 1);
				if (a == 3) {
					stop_msg("Cancelled");
					return -1;
				}
				if (a == 2) {
					g_all = 1;
					yes = 1;
				} else {
					yes = a == 1;
				}
			}
			if (!yes)
				return 1;
			err = dos_unlink(dst);
			if (err) {
				stop_msg(dos_errmsg(err));
				return -1;
			}
		}
		err = dos_rename(src, dst);
		if (err) {
			stop_msg(dos_errmsg(err));
			return -1;
		}
		return 0;
	}
	err = copy_file(src, dst);
	if (err < 0 || g_stop)
		return -1;
	if (err > 0)
		return 1;
	err = dos_unlink(src);
	if (err) {
		stop_msg(dos_errmsg(err));
		return -1;
	}
	return 0;
}

/* A source directory that still holds skipped files cannot be removed. */
static void rmdir_src(const char *path)
{
	int err;

	err = dos_rmdir(path);
	if (err && err != 16)
		stop_msg(dos_errmsg(err));
}

static void move_tree(const char *src, const char *dst, int isdir)
{
	unsigned char attr;

	if (path_same(src, dst))
		return;
	if (path_inside(src, dst)) {
		stop_msg("Cannot move a directory into itself");
		return;
	}
	if (!isdir) {
		move_file(src, dst);
		return;
	}
	/* Same drive: one rename moves the whole tree and uses no extra space. */
	if (same_drive(src, dst) && !dos_exists(dst, &attr)) {
		status_move(src);
		if (dos_rename(src, dst) == 0)
			return;
	}
	if (dos_exists(dst, &attr) && !(attr & FA_DIR)) {
		stop_msg("Destination is a file");
		return;
	}
	g_sp = 0;
	if (push_job(src, dst, 1) != 0)
		return;
	while (g_sp > 0 && !g_stop) {
		struct job *j;
		unsigned err;
		char child_s[PATH_MAX];
		char child_d[PATH_MAX];
		char wild[PATH_MAX];

		j = &g_job[g_sp - 1];
		if (!j->started) {
			if (path_inside(j->src, j->dst)) {
				stop_msg("Cannot move a directory into itself");
				g_sp--;
				continue;
			}
			/* A free name on the same drive is renamed whole, including
			 * every file under it. Nothing is copied. */
			if (same_drive(j->src, j->dst) && !dos_exists(j->dst, &attr)) {
				status_move(j->src);
				if (dos_rename(j->src, j->dst) == 0) {
					g_sp--;
					continue;
				}
			}
			if (dos_exists(j->dst, &attr) && !(attr & FA_DIR)) {
				stop_msg("Destination is a file");
				g_sp--;
				continue;
			}
			if (ensure_dir(j->dst) != 0) {
				g_sp--;
				continue;
			}
			if (path_join(wild, j->src, "*.*") != 0) {
				stop_msg("Path too long");
				g_sp--;
				continue;
			}
			err = _dos_findfirst(wild, FA_ANY, &j->ft);
			j->started = 1;
			if (err) {
				if (err == 18)
					rmdir_src(j->src);
				else
					stop_msg(dos_errmsg((int)err));
				g_sp--;
				continue;
			}
		} else {
			err = _dos_findnext(&j->ft);
			if (err) {
				if (err == 18)
					rmdir_src(j->src);
				else
					stop_msg(dos_errmsg((int)err));
				g_sp--;
				continue;
			}
		}
		if (is_dot(j->ft.name))
			continue;
		if (j->ft.attrib & FA_VOL)
			continue;
		if (path_join(child_s, j->src, j->ft.name) != 0 ||
		    path_join(child_d, j->dst, j->ft.name) != 0) {
			stop_msg("Path too long");
			continue;
		}
		if (j->ft.attrib & FA_DIR)
			push_job(child_s, child_d, 1);
		else
			move_file(child_s, child_d);
	}
}

void do_rename(void)
{
	struct panel *p;
	struct panel *other;
	struct file_ent *e;
	char renamed[NAME_LEN];
	char from[PATH_MAX];
	char to[PATH_MAX];
	int err;

	p = &g_pan[g_act];
	e = panel_cur(p);
	if (!e || is_dot(e->name)) {
		ui_status("Nothing to rename");
		draw_all();
		return;
	}
	strcpy(renamed, e->name);
	if (!ui_input("Rename", "New name:", renamed, NAME_LEN - 1)) {
		ui_status("Cancelled");
		draw_all();
		return;
	}
	if (namecmp(renamed, e->name) == 0) {
		ui_status("Cancelled");
		draw_all();
		return;
	}
	if (path_join(from, p->path, e->name) != 0 ||
	    path_join(to, p->path, renamed) != 0) {
		ui_status("Path too long");
		draw_all();
		return;
	}
	err = dos_rename(from, to);
	if (err) {
		ui_status(dos_errmsg(err));
	} else {
		ui_status("Renamed");
		strcpy(e->name, renamed);
	}
	other = &g_pan[1 - g_act];
	if (path_same(p->path, other->path))
		finish(p, other);
	else
		finish(p, 0);
}

void do_move(void)
{
	struct panel *src;
	struct panel *dstp;
	unsigned short sel[MAX_FILES];
	int n;
	int i;
	char from[PATH_MAX];
	char to[PATH_MAX];
	char renamed[NAME_LEN];
	char line[48];

	src = &g_pan[g_act];
	dstp = &g_pan[1 - g_act];
	n = collect(src, sel, MAX_FILES);
	if (n <= 0) {
		ui_status("Nothing to move");
		draw_all();
		return;
	}
	g_all = 0;
	g_stop = 0;
	if (path_same(src->path, dstp->path)) {
		struct file_ent *e;

		if (n != 1) {
			ui_status("Same directory");
			draw_all();
			return;
		}
		e = &src->files[sel[0]];
		strcpy(renamed, e->name);
		if (!ui_input("Rename", "New name:", renamed, NAME_LEN - 1)) {
			ui_status("Cancelled");
			draw_all();
			return;
		}
		if (path_join(from, src->path, e->name) != 0 ||
		    path_join(to, src->path, renamed) != 0) {
			ui_status("Path too long");
			draw_all();
			return;
		}
		i = dos_rename(from, to);
		if (i)
			ui_status(dos_errmsg(i));
		else
			ui_status("Renamed");
		finish(src, 0);
		return;
	}
	op_line(line, "Move", n, src->files[sel[0]].name, 0);
	if (!ui_confirm("Move", line, dstp->path)) {
		ui_status("Cancelled");
		draw_all();
		return;
	}
	for (i = 0; i < n && !g_stop; i++) {
		struct file_ent *e;

		e = &src->files[sel[i]];
		if (path_join(from, src->path, e->name) != 0 ||
		    path_join(to, dstp->path, e->name) != 0) {
			ui_status("Path too long");
			break;
		}
		move_tree(from, to, (e->attr & FA_DIR) != 0);
	}
	if (i == n && !g_stop)
		ui_status("Moved");
	finish(src, dstp);
}

void do_mkdir(void)
{
	struct panel *p;
	char name[NAME_LEN];
	char path[PATH_MAX];
	int err;

	p = &g_pan[g_act];
	name[0] = 0;
	if (!ui_input("MkDir", "Name:", name, NAME_LEN - 1)) {
		ui_status("Cancelled");
		draw_all();
		return;
	}
	if (name[0] == 0 || strchr(name, '\\') || strchr(name, '/') ||
	    strchr(name, ':') || strchr(name, '*') || strchr(name, '?')) {
		ui_status("Invalid name");
		draw_all();
		return;
	}
	if (path_join(path, p->path, name) != 0) {
		ui_status("Path too long");
		draw_all();
		return;
	}
	err = dos_mkdir(path);
	if (err)
		ui_status(dos_errmsg(err));
	else
		ui_status("Directory created");
	panel_reload(p);
	draw_all();
}
