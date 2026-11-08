#include "pc98.h"

#include <dos.h>
#include <string.h>

/*
 * Shift-JIS -> text VRAM. Same arithmetic as the published CONVJIS notes:
 * AH in = lead, AL in = trail, AH out = JIS high, AL out = JIS low.
 * Then the high JIS byte loses 0x20 (that is the ku), and the right-hand
 * cell sets bit 7 of the ten byte. Cat (94 4C) lands as 27 2D / 27 AD.
 */
static void sjis_cells(unsigned char hi, unsigned char lo,
		       unsigned short *left, unsigned short *right)
{
	unsigned char ah;
	unsigned char al;
	unsigned char ku;
	unsigned char ten20;

	ah = hi;
	al = lo;
	if (ah >= 0xE0)
		ah = (unsigned char)(ah - 0x40);
	ah = (unsigned char)(ah - 0x71);
	if (al >= 0x80)
		al = (unsigned char)(al - 1);
	if (al < 0x9E) {
		ah = (unsigned char)((ah << 1) + 1);
	} else {
		al = (unsigned char)(al - 0x5E);
		ah = (unsigned char)(ah + 1);
		ah = (unsigned char)(ah << 1);
	}
	al = (unsigned char)(al - 0x1F);
	ku = (unsigned char)(ah - 0x20);
	ten20 = al;
	*left = (unsigned short)ku | ((unsigned short)ten20 << 8);
	*right = (unsigned short)ku | ((unsigned short)(ten20 | 0x80) << 8);
}

int vid_puts(int x, int y, int xend, unsigned short attr, const char *s);

static void bios18(unsigned short ax, unsigned short dx)
{
	union REGS r;

	memset(&r, 0, sizeof(r));
	r.x.ax = ax;
	r.x.dx = dx;
	int86(0x18, &r, &r);
}

/* Last word written to each cell. A repeat is not sent to VRAM, so a
 * redraw of an unchanged row does not blink on the real machine. */
static unsigned short s_code[VID_ROWS * VID_COLS];
static unsigned char s_attr[VID_ROWS * VID_COLS];

void vid_cell(int x, int y, unsigned short attr, unsigned short code)
{
	unsigned i;
	unsigned off;
	unsigned char a;
	volatile unsigned __far *cp;
	volatile unsigned __far *ap;

	if (x < 0 || y < 0 || x >= VID_COLS || y >= VID_ROWS)
		return;
	/* Bit 0 is /ST: 1 shows the glyph. White is E1h, not E0h. */
	a = (unsigned char)(attr | 1);
	i = (unsigned)y * (unsigned)VID_COLS + (unsigned)x;
	if (s_code[i] == code && s_attr[i] == a)
		return;
	s_code[i] = code;
	s_attr[i] = a;
	off = (unsigned)y * 160u + (unsigned)x * 2u;
	cp = (volatile unsigned __far *)__libi86_MK_FP(0xA000u, off);
	ap = (volatile unsigned __far *)__libi86_MK_FP(0xA200u, off);
	*cp = code;
	*ap = a;
}

void vid_putpad(int x, int y, int w, unsigned short attr, const char *s)
{
	char buf[81];
	int n;

	if (w <= 0)
		return;
	if (w > 80)
		w = 80;
	memset(buf, ' ', (unsigned)w);
	buf[w] = 0;
	n = 0;
	if (s) {
		while (s[n] && n < w) {
			buf[n] = s[n];
			n++;
		}
	}
	vid_puts(x, y, x + w, attr, buf);
}

void vid_fill(int x, int y, int n, unsigned short attr, unsigned char ch)
{
	while (n > 0 && x < VID_COLS) {
		vid_cell(x, y, attr, (unsigned short)ch);
		x++;
		n--;
	}
}

void vid_clear(unsigned short attr)
{
	int y;

	for (y = 0; y < VID_ROWS; y++)
		vid_fill(0, y, VID_COLS, attr, ' ');
}

/* A DOS program writes VRAM on its own. Drop the shadow so the next
   paint is stored for real instead of being skipped as "already there". */
void vid_forget(void)
{
	memset(s_code, 0, sizeof(s_code));
	memset(s_attr, 0, sizeof(s_attr));
}

void vid_handoff(void)
{
	vid_init();
	vid_forget();
	vid_clear(COL_WHITE);
	bios18(0x1300, 0);
	bios18(0x1100, 0);
}

void vid_fw(int x, int y, unsigned short attr, unsigned char ku, unsigned char ten)
{
	unsigned char ten20;
	unsigned short left;
	unsigned short right;

	ten20 = (unsigned char)(ten + 0x20);
	left = (unsigned short)ku | ((unsigned short)ten20 << 8);
	right = (unsigned short)ku | ((unsigned short)(ten20 | 0x80) << 8);
	vid_cell(x, y, attr, left);
	vid_cell(x + 1, y, attr, right);
}

void vid_box(int x, int y, int w, int h, unsigned short attr)
{
	int i;
	int r;
	int right;
	int edge;
	unsigned short horiz;
	unsigned short vert;
	unsigned short leftb;

	/* Underline is the bottom of the cell, the vertical line is its left
	   edge. A vertical on the top row sticks up above the line, and an
	   underline on the right-hand cell runs past that vertical. */
	if (w < 2 || h < 2 || x < 0 || y < 0)
		return;
	right = x + w;
	edge = 0;
	if (right >= VID_COLS) {
		right = VID_COLS - 1;
		edge = 1;
	}
	horiz = (unsigned short)(attr | ATTR_UL);
	vert = (unsigned short)(attr | ATTR_VLINE);
	leftb = (unsigned short)(horiz | ATTR_VLINE);
	for (i = x; i < right; i++)
		vid_cell(i, y, horiz, ' ');
	/* Underline on the right column continues past its left edge, so the
	   corner becomes a T. A blank cell there meets the side vertical in an L.
	   The bottom cell keeps the vertical only, for the same reason. */
	if (!edge)
		vid_cell(right, y, attr, ' ');
	for (r = y + 1; r < y + h - 1; r++) {
		vid_cell(x, r, vert, ' ');
		vid_cell(right, r, vert, ' ');
	}
	for (i = x; i < right; i++)
		vid_cell(i, y + h - 1, (i == x) ? leftb : horiz, ' ');
	vid_cell(right, y + h - 1, vert, ' ');
}

int vid_puts(int x, int y, int xend, unsigned short attr, const char *s)
{
	const unsigned char *p;

	p = (const unsigned char *)s;
	while (*p && x < xend) {
		unsigned char c;

		c = *p;
		if (c == '\n' || c == '\r')
			break;
		if (is_sjis_lead(c) && is_sjis_trail(p[1])) {
			unsigned short left;
			unsigned short right;

			if (x + 2 > xend)
				break;
			sjis_cells(c, p[1], &left, &right);
			vid_cell(x, y, attr, left);
			vid_cell(x + 1, y, attr, right);
			x += 2;
			p += 2;
			continue;
		}
		if (c < 0x20)
			c = ' ';
		vid_cell(x, y, attr, (unsigned short)c);
		x++;
		p++;
	}
	return x;
}

void vid_init(void)
{
	bios18(0x0C00, 0);
	bios18(0x4100, 0);
	bios18(0x0E00, 0);
	bios18(0x1200, 0);
}

void vid_done(void)
{
	vid_clear(COL_WHITE);
	bios18(0x1300, 0);
	bios18(0x1100, 0);
}
