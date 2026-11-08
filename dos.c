#include "pc98.h"

#include <dos.h>
#include <string.h>

static struct find_t g_probe;

static int dos_path(unsigned char ah, const char *path)
{
	union REGS r;

	memset(&r, 0, sizeof(r));
	r.h.ah = ah;
	r.x.dx = (unsigned)path;
	int86(0x21, &r, &r);
	if (r.x.cflag)
		return (int)r.x.ax;
	return 0;
}

const char *dos_errmsg(int err)
{
	static char buf[20];

	switch (err) {
	case 2:
		return "File not found";
	case 3:
		return "Path not found";
	case 5:
		return "Access denied";
	case 8:
		return "Not enough memory";
	case 15:
		return "Invalid drive";
	case 16:
		return "Cannot remove directory";
	case 19:
		return "Write protected";
	case 21:
		return "Drive not ready";
	case 23:
		return "Data error";
	case 27:
		return "Sector not found";
	case 29:
		return "Write fault";
	case 30:
		return "Read fault";
	case 31:
		return "General failure";
	case 39:
		return "Disk full";
	default:
		break;
	}
	strcpy(buf, "DOS error ");
	fmt_ulong(buf + 10, (unsigned long)err);
	return buf;
}

int dos_findfirst(const char *path, unsigned short attr)
{
	/*
	 * Callers that scan keep their own struct find_t. This entry point
	 * is only used by dos_exists(), which must not disturb another scan:
	 * the search state lives inside the find_t, and findnext reloads it.
	 */
	return (int)_dos_findfirst(path, (unsigned)attr, &g_probe);
}

int dos_findnext(void)
{
	return (int)_dos_findnext(&g_probe);
}

unsigned char *dos_dta(void)
{
	return (unsigned char *)&g_probe;
}

void dos_dta_main(void)
{
}

int dos_setdta(void *dta)
{
	(void)dta;
	return 0;
}

int dos_exists(const char *path, unsigned char *attr)
{
	int err;

	err = dos_findfirst(path, FA_ANY);
	if (err)
		return 0;
	if (attr)
		*attr = (unsigned char)g_probe.attrib;
	return 1;
}

int dos_getcwd(unsigned char dl, char *buf)
{
	union REGS r;

	memset(&r, 0, sizeof(r));
	r.h.ah = 0x47;
	r.h.dl = dl;
	r.x.si = (unsigned)buf;
	buf[0] = 0;
	int86(0x21, &r, &r);
	if (r.x.cflag)
		return (int)r.x.ax;
	return 0;
}

int dos_chdir(const char *path)
{
	return dos_path(0x3B, path);
}

unsigned char dos_getdrive(void)
{
	union REGS r;

	memset(&r, 0, sizeof(r));
	r.h.ah = 0x19;
	int86(0x21, &r, &r);
	return r.h.al;
}

int dos_mkdir(const char *path)
{
	return dos_path(0x39, path);
}

int dos_rmdir(const char *path)
{
	return dos_path(0x3A, path);
}

int dos_unlink(const char *path)
{
	return dos_path(0x41, path);
}

int dos_rename(const char *oldn, const char *newn)
{
	union REGS r;
	struct SREGS s;

	memset(&r, 0, sizeof(r));
	segread(&s);
	r.h.ah = 0x56;
	r.x.dx = (unsigned)oldn;
	r.x.di = (unsigned)newn;
	s.es = s.ds;
	int86x(0x21, &r, &r, &s);
	if (r.x.cflag)
		return (int)r.x.ax;
	return 0;
}

int dos_create(const char *path, unsigned short attr)
{
	int h;
	unsigned err;

	h = 0;
	err = _dos_creat(path, (unsigned)attr, &h);
	if (err)
		return -(int)err;
	return h;
}

int dos_open(const char *path, unsigned char mode)
{
	int h;
	unsigned err;

	h = 0;
	err = _dos_open(path, (unsigned)mode, &h);
	if (err)
		return -(int)err;
	return h;
}

int dos_close(int h)
{
	unsigned err;

	err = _dos_close(h);
	return err ? (int)err : 0;
}

int dos_read(int h, void *buf, unsigned short n, unsigned short *got)
{
	unsigned bytes;
	unsigned err;

	bytes = 0;
	err = _dos_read(h, (void __far *)buf, (unsigned)n, &bytes);
	*got = (unsigned short)bytes;
	return err ? (int)err : 0;
}

int dos_write(int h, const void *buf, unsigned short n, unsigned short *put)
{
	unsigned bytes;
	unsigned err;

	bytes = 0;
	err = _dos_write(h, (const void __far *)buf, (unsigned)n, &bytes);
	*put = (unsigned short)bytes;
	return err ? (int)err : 0;
}

int dos_seek(int h, unsigned long pos)
{
	union REGS r;

	memset(&r, 0, sizeof(r));
	r.h.ah = 0x42;
	r.h.al = 0;
	r.x.bx = (unsigned)h;
	r.x.cx = (unsigned)(pos >> 16);
	r.x.dx = (unsigned)pos;
	int86(0x21, &r, &r);
	if (r.x.cflag)
		return (int)r.x.ax;
	return 0;
}

int dos_gettime(int h, unsigned short *time, unsigned short *date)
{
	unsigned d;
	unsigned t;
	unsigned err;

	d = 0;
	t = 0;
	err = _dos_getftime(h, &d, &t);
	if (err)
		return (int)err;
	*date = (unsigned short)d;
	*time = (unsigned short)t;
	return 0;
}

int dos_settime(int h, unsigned short time, unsigned short date)
{
	unsigned err;

	err = _dos_setftime(h, (unsigned)date, (unsigned)time);
	return err ? (int)err : 0;
}

int dos_free(unsigned char drive1, unsigned long *bytes)
{
	struct diskfree_t d;
	unsigned err;

	err = _dos_getdiskfree((unsigned)drive1, &d);
	if (err)
		return (int)err;
	*bytes = (unsigned long)d.avail_clusters *
		 (unsigned long)d.sectors_per_cluster *
		 (unsigned long)d.bytes_per_sector;
	return 0;
}

int dos_drive_ok(unsigned char drive1)
{
	union REGS r;

	memset(&r, 0, sizeof(r));
	r.x.ax = 0x4409;
	r.h.bl = drive1;
	int86(0x21, &r, &r);
	return r.x.cflag ? 0 : 1;
}

int dos_exec(const char *path, const char *args)
{
	static char tail[128];
	static char fcb[16];
	union _dosspawn_t sp;
	unsigned err;
	int n;

	if (!args)
		args = "";
	n = (int)strlen(args);
	if (n > 124)
		n = 124;
	tail[0] = (char)(n + 1);
	tail[1] = ' ';
	memcpy(tail + 2, args, (unsigned)n);
	tail[2 + n] = '\r';
	memset(fcb, 0, sizeof(fcb));
	memset(fcb + 1, ' ', 11);
	memset(&sp, 0, sizeof(sp));
	sp._proc_run._env_seg = 0;
	sp._proc_run._argv = tail;
	sp._proc_run._fcb1 = fcb;
	sp._proc_run._fcb2 = fcb;
	err = _dos_spawn(0, path, &sp);
	return err ? (int)err : 0;
}

int dos_comspec(char *dst, int max)
{
	union REGS r;
	unsigned psp;
	unsigned envseg;
	unsigned char __far *env;
	unsigned off;
	const char *key;
	int k;

	dst[0] = 0;
	memset(&r, 0, sizeof(r));
	r.h.ah = 0x62;
	int86(0x21, &r, &r);
	psp = r.x.bx;
	envseg = *(unsigned __far *)__libi86_MK_FP(psp, 0x2C);
	if (envseg == 0)
		return -1;
	env = (unsigned char __far *)__libi86_MK_FP(envseg, 0);
	off = 0;
	key = "COMSPEC=";
	while (env[off] != 0) {
		k = 0;
		while (key[k]) {
			unsigned char c;

			c = env[off + (unsigned)k];
			if (c >= 'a' && c <= 'z')
				c = (unsigned char)(c - 32);
			if (c != (unsigned char)key[k])
				break;
			k++;
		}
		if (key[k] == 0) {
			int n;

			n = 0;
			off = (unsigned)(off + (unsigned)k);
			while (env[off] != 0 && n + 1 < max) {
				dst[n++] = (char)env[off];
				off++;
			}
			dst[n] = 0;
			return n > 0 ? 0 : -1;
		}
		while (env[off] != 0)
			off++;
		off++;
	}
	return -1;
}
