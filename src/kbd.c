#include "pc98.h"

#include <dos.h>
#include <string.h>

static unsigned char kbd_mods(void)
{
	union REGS r;

	memset(&r, 0, sizeof(r));
	r.h.ah = 0x02;
	int86(0x18, &r, &r);
	return r.h.al;
}

void kbd_read(struct keyev *ev)
{
	union REGS r;

	memset(&r, 0, sizeof(r));
	r.h.ah = 0x00;
	int86(0x18, &r, &r);
	ev->scan = r.h.ah;
	ev->ascii = r.h.al;
	/* F6 stays 67h with GRPH or CTRL held. Those are bits, not a new scan. */
	ev->shift = kbd_mods();
}

int kbd_poll(struct keyev *ev)
{
	union REGS r;

	memset(&r, 0, sizeof(r));
	r.h.ah = 0x01;
	int86(0x18, &r, &r);
	/* Anex86 and the VC hook treat BX==0 as an empty buffer. */
	if (r.x.bx == 0) {
		ev->shift = 0;
		return 0;
	}
	ev->scan = r.h.ah;
	ev->ascii = r.h.al;
	ev->shift = kbd_mods();
	return 1;
}
