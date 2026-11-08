#include "pc98.h"

#include <string.h>

int is_sjis_lead(unsigned char c)
{
	return (c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC);
}

int is_sjis_trail(unsigned char c)
{
	return (c >= 0x40 && c <= 0x7E) || (c >= 0x80 && c <= 0xFC);
}

int namecmp(const char *a, const char *b)
{
	const unsigned char *pa;
	const unsigned char *pb;

	pa = (const unsigned char *)a;
	pb = (const unsigned char *)b;
	for (;;) {
		unsigned char ca;
		unsigned char cb;

		ca = *pa;
		cb = *pb;
		if (is_sjis_lead(ca) && is_sjis_trail(pa[1]) &&
		    is_sjis_lead(cb) && is_sjis_trail(pb[1])) {
			if (ca != cb)
				return (int)ca - (int)cb;
			if (pa[1] != pb[1])
				return (int)pa[1] - (int)pb[1];
			pa += 2;
			pb += 2;
			continue;
		}
		if (ca >= 'a' && ca <= 'z')
			ca = (unsigned char)(ca - 32);
		if (cb >= 'a' && cb <= 'z')
			cb = (unsigned char)(cb - 32);
		if (ca != cb || ca == 0)
			return (int)ca - (int)cb;
		pa++;
		pb++;
	}
}

int path_is_root(const char *p)
{
	return p[0] && p[1] == ':' && p[2] == '\\' && p[3] == 0;
}

void path_parent(char *p)
{
	int i;
	int n;

	if (path_is_root(p))
		return;
	n = (int)strlen(p);
	if (n > 3 && p[n - 1] == '\\') {
		p[n - 1] = 0;
		n--;
	}
	i = n - 1;
	while (i > 2 && p[i] != '\\')
		i--;
	if (i <= 2)
		p[3] = 0;
	else
		p[i] = 0;
}

int path_join(char *dst, const char *dir, const char *name)
{
	int n;
	int m;

	n = (int)strlen(dir);
	m = (int)strlen(name);
	if (path_is_root(dir)) {
		if (n + m + 1 > PATH_MAX)
			return -1;
		strcpy(dst, dir);
		strcat(dst, name);
		return 0;
	}
	if (n + 1 + m + 1 > PATH_MAX)
		return -1;
	strcpy(dst, dir);
	strcat(dst, "\\");
	strcat(dst, name);
	return 0;
}

int path_same(const char *a, const char *b)
{
	return namecmp(a, b) == 0;
}

int path_inside(const char *dir, const char *child)
{
	char tmp[PATH_MAX];
	int n;
	int m;

	if (path_same(dir, child))
		return 1;
	n = (int)strlen(dir);
	m = (int)strlen(child);
	if (n >= PATH_MAX || m < n)
		return 0;
	memcpy(tmp, child, (unsigned)n);
	tmp[n] = 0;
	if (namecmp(tmp, dir) != 0)
		return 0;
	if (path_is_root(dir))
		return child[n] != 0;
	return child[n] == '\\';
}

void fmt_ulong(char *dst, unsigned long n)
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
	while (i > 0)
		dst[j++] = tmp[--i];
	dst[j] = 0;
}

void fmt_date(char *dst, unsigned short dos)
{
	unsigned mon;
	unsigned day;
	unsigned year;

	day = (unsigned)(dos & 31);
	mon = (unsigned)((dos >> 5) & 15);
	year = (unsigned)(((dos >> 9) + 1980) % 100);
	dst[0] = (char)('0' + year / 10);
	dst[1] = (char)('0' + year % 10);
	dst[2] = '-';
	dst[3] = (char)('0' + mon / 10);
	dst[4] = (char)('0' + mon % 10);
	dst[5] = '-';
	dst[6] = (char)('0' + day / 10);
	dst[7] = (char)('0' + day % 10);
	dst[8] = 0;
}

int is_exec(const char *name)
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
	p = dot + 1;
	return namecmp(p, "EXE") == 0 || namecmp(p, "COM") == 0 ||
	       namecmp(p, "BAT") == 0;
}
