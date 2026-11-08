#include "pc98.h"

#include <dos.h>
#include <string.h>

void kbd_read(struct keyev *ev)
{
	union REGS r;

	memset(&r, 0, sizeof(r));
	r.h.ah = 0x00;
	int86(0x18, &r, &r);
	ev->scan = r.h.ah;
	ev->ascii = r.h.al;
}

int kbd_poll(struct keyev *ev)
{
	union REGS r;

	memset(&r, 0, sizeof(r));
	r.h.ah = 0x01;
	int86(0x18, &r, &r);
	/* Anex86 and the VC hook treat BX==0 as an empty buffer. */
	if (r.x.bx == 0)
		return 0;
	ev->scan = r.h.ah;
	ev->ascii = r.h.al;
	return 1;
}
