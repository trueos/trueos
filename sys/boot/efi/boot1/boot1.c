/*-
 * Copyright (c) 1998 Robert Nordier
 * All rights reserved.
 * Copyright (c) 2001 Robert Drehmel
 * All rights reserved.
 * Copyright (c) 2014 Nathan Whitehorn
 * All rights reserved.
 * Copyright (c) 2015 Eric McCorkle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are freely
 * permitted provided that the above copyright notice and this
 * paragraph and the following disclaimer are duplicated in all
 * such forms.
 *
 * This software is provided "AS IS" and without any express or
 * implied warranties, including, without limitation, the implied
 * warranties of merchantability and fitness for a particular
 * purpose.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <machine/elf.h>
#include <machine/stdarg.h>

#include <efi.h>
#include <eficonsctl.h>

#include "boot_module.h"

#define _PATH_LOADER	"/boot/loader.efi"

#ifdef EFI_DEBUG
#define DPRINTF(fmt, args...) \
	do { \
		printf(fmt, ##args) \
	} while (0)
#else
#define DPRINTF(fmt, args...) {}
#endif

typedef int putc_func_t(char c, void *arg);

struct sp_data {
	char	*sp_buf;
	u_int	sp_len;
	u_int	sp_size;
};

static const boot_module_t *boot_modules[] =
{
#ifdef EFI_ZFS_BOOT
	&zfs_module,
#endif
#ifdef EFI_UFS_BOOT
	&ufs_module
#endif
};

#define NUM_BOOT_MODULES (sizeof(boot_modules) / sizeof(boot_module_t*))

static const char digits[] = "0123456789abcdef";

static int __printf(const char *fmt, putc_func_t *putc, void *arg, va_list ap);
static int __puts(const char *s, putc_func_t *putc, void *arg);
static int __sputc(char c, void *arg);
static char *__uitoa(char *buf, u_int val, int base);
static char *__ultoa(char *buf, u_long val, int base);
static int putchr(char c, void *arg);

static void try_load(const boot_module_t* mod);
static EFI_STATUS probe_handle(EFI_HANDLE h);

EFI_SYSTEM_TABLE *systab;
EFI_BOOT_SERVICES *bs;
static EFI_HANDLE *image;

void *
Malloc(size_t len, const char *file, int line)
{
	void *out;

	if (bs->AllocatePool(EfiLoaderData, len, &out) == EFI_SUCCESS)
		return (out);

	return (NULL);
}

char *
strcpy(char * __restrict to, const char * __restrict from)
{
        char *save = to;

        for (; (*to = *from) != 0; ++from, ++to);

        return (save);
}

char *
strchr(const char *s, int c)
{
	int i;

	for (i = 0; s[i]; i++)
		if (s[i] == c)
			return ((char*)(s + i));

	return (NULL);
}

int
strncmp(const char *s1, const char *s2, size_t n)
{

	if (n == 0)
		return (0);
	do {
		if (*s1 != *s2++)
			return (*(const unsigned char *)s1 -
				*(const unsigned char *)(s2 - 1));
		if (*s1++ == '\0')
			break;
	} while (--n != 0);
	return (0);
}

size_t
strlen(const char *s)
{
	size_t len = 0;

	for (; *s != '\0'; s++, len++)
		;

	return (len);
}

char *
strdup(const char *s)
{
	size_t len;
	char *out;

	len = strlen(s) + 1;
	out = malloc(len);
	if (out == NULL)
		return (NULL);

	bcopy(s, out, len);

	return (out);
}

int
bcmp(const void *a, const void *b, size_t len)
{
	const char *sa = a;
	const char *sb = b;
	int i;

	for (i = 0; i < len; i++)
		if (sa[i] != sb[i])
			return (1);

	return (0);
}

int
memcmp(const void *a, const void *b, size_t len)
{

	return bcmp(a, b, len);
}

void
bcopy(const void *src, void *dst, size_t len)
{
	const char *s = src;
	char *d = dst;

	while (len-- != 0)
		*d++ = *s++;
}

void *
memcpy(void *dst, const void *src, size_t len)
{

	bcopy(src, dst, len);

	return (dst);
}

void *
memset(void *b, int val, size_t len)
{
	char *p = b;

	while (len-- != 0)
		*p++ = val;

	return (b);
}

int
strcmp(const char *s1, const char *s2)
{

	for (; *s1 == *s2 && *s1; s1++, s2++)
		;
	return ((u_char)*s1 - (u_char)*s2);
}

static int
putchr(char c, void *arg)
{
	CHAR16 buf[2];

	if (c == '\n')
		putchr('\r', arg);

	buf[0] = c;
	buf[1] = 0;
	systab->ConOut->OutputString(systab->ConOut, buf);

	return (1);
}

static EFI_GUID BlockIoProtocolGUID = BLOCK_IO_PROTOCOL;
static EFI_GUID DevicePathGUID = DEVICE_PATH_PROTOCOL;
static EFI_GUID LoadedImageGUID = LOADED_IMAGE_PROTOCOL;
static EFI_GUID ConsoleControlGUID = EFI_CONSOLE_CONTROL_PROTOCOL_GUID;

/*
 * This function only returns if it fails to load the kernel. If it
 * succeeds, it simply boots the kernel.
 */
void
try_load(const boot_module_t *mod)
{
	size_t bufsize;
	void *buf;
	dev_info_t *dev;
	EFI_HANDLE loaderhandle;
	EFI_LOADED_IMAGE *loaded_image;
	EFI_STATUS status;

	status = mod->load(_PATH_LOADER, &dev, &buf, &bufsize);
	if (status == EFI_NOT_FOUND)
		return;

	if (status != EFI_SUCCESS) {
		printf("%s failed to load %s (%lu)\n", mod->name, _PATH_LOADER,
		    status);
		return;
	}

	if ((status = bs->LoadImage(TRUE, image, dev->devpath, buf, bufsize,
	    &loaderhandle)) != EFI_SUCCESS) {
		printf("Failed to load image provided by %s (%lu)\n", mod->name,
		    status);
		return;
	}

	if ((status = bs->HandleProtocol(loaderhandle, &LoadedImageGUID,
	    (VOID**)&loaded_image)) != EFI_SUCCESS) {
		printf("Failed to query LoadedImage provided by %s (%lu)\n",
		    mod->name, status);
		return;
	}

	loaded_image->DeviceHandle = dev->devhandle;

	if ((status = bs->StartImage(loaderhandle, NULL, NULL)) !=
	    EFI_SUCCESS) {
		printf("Failed start image provided by %s (%lu)\n", mod->name,
			status);
		return;
	}
}

void
efi_main(EFI_HANDLE Ximage, EFI_SYSTEM_TABLE *Xsystab)
{
	EFI_HANDLE *handles;
	EFI_STATUS status;
	EFI_CONSOLE_CONTROL_PROTOCOL *ConsoleControl = NULL;
	SIMPLE_TEXT_OUTPUT_INTERFACE *conout = NULL;
	UINTN i, max_dim, best_mode, cols, rows, hsize, nhandles;

	/* Basic initialization*/
	systab = Xsystab;
	image = Ximage;
	bs = Xsystab->BootServices;

	/* Set up the console, so printf works. */
	status = bs->LocateProtocol(&ConsoleControlGUID, NULL,
	    (VOID **)&ConsoleControl);
	if (status == EFI_SUCCESS)
		(void)ConsoleControl->SetMode(ConsoleControl,
		    EfiConsoleControlScreenText);
	/*
	 * Reset the console and find the best text mode.
	 */
	conout = systab->ConOut;
	conout->Reset(conout, TRUE);
	max_dim = best_mode = 0;
	for (i = 0; ; i++) {
		status = conout->QueryMode(conout, i, &cols, &rows);
		if (EFI_ERROR(status))
			break;
		if (cols * rows > max_dim) {
			max_dim = cols * rows;
			best_mode = i;
		}
	}
	if (max_dim > 0)
		conout->SetMode(conout, best_mode);
	conout->EnableCursor(conout, TRUE);
	conout->ClearScreen(conout);

	printf("\n>> FreeBSD EFI boot block\n");
	printf("   Loader path: %s\n\n", _PATH_LOADER);
	printf("   Initializing modules:");
	for (i = 0; i < NUM_BOOT_MODULES; i++) {
		if (boot_modules[i] == NULL)
			continue;

		printf(" %s", boot_modules[i]->name);
		if (boot_modules[i]->init != NULL)
			boot_modules[i]->init();
	}
	putchr('\n', NULL);

	/* Get all the device handles */
	hsize = sizeof(EFI_HANDLE) * 24;
	if (bs->AllocatePool(EfiLoaderData, hsize, (void **)&handles) != EFI_SUCCESS)
		panic("Failed to allocate handles");

	status = bs->LocateHandle(ByProtocol, &BlockIoProtocolGUID, NULL,
	    &hsize, handles);
	switch (status) {
	case EFI_SUCCESS:
		break;
	case EFI_BUFFER_TOO_SMALL:
		(void)bs->FreePool(handles);
		if ((status = bs->AllocatePool(EfiLoaderData, hsize,
		    (void **)&handles) != EFI_SUCCESS)) {
			panic("Failed to allocate %lu handles (%lu)", hsize /
			    sizeof(*handles), status);
		}
		status = bs->LocateHandle(ByProtocol, &BlockIoProtocolGUID,
		    NULL, &hsize, handles);
		if (status != EFI_SUCCESS)
			panic("Failed to get device handles (%lu)\n", status);
		break;
	default:
		panic("Failed to get device handles (%lu)", status);
	}

	/* Scan all partitions, probing with all modules. */
	nhandles = hsize / sizeof(*handles);
	printf("   Probing %lu block devices...", nhandles);
	for (i = 0; i < nhandles; i++) {
		status = probe_handle(handles[i]);
		switch (status) {
		case EFI_UNSUPPORTED:
			printf(".");
			break;
		case EFI_SUCCESS:
			printf("+");
			break;
		default:
			printf("x");
			break;
		}
	}
	printf(" done\n");

	/* Status summary. */
	for (i = 0; i < NUM_BOOT_MODULES; i++) {
		if (boot_modules[i] != NULL) {
			printf("    ");
			boot_modules[i]->status();
		}
	}

	/* Select a partition to boot by trying each module in order. */
	for (i = 0; i < NUM_BOOT_MODULES; i++)
		if (boot_modules[i] != NULL)
			try_load(boot_modules[i]);

	/* If we get here, we're out of luck... */
	panic("No bootable partitions found!");
}

static EFI_STATUS
probe_handle(EFI_HANDLE h)
{
	dev_info_t *devinfo;
	EFI_BLOCK_IO *blkio;
	EFI_DEVICE_PATH *devpath;
	EFI_STATUS status;
	UINTN i;

	/* Figure out if we're dealing with an actual partition. */
	status = bs->HandleProtocol(h, &DevicePathGUID, (void **)&devpath);
	if (status == EFI_UNSUPPORTED)
		return (status);

	if (status != EFI_SUCCESS) {
		DPRINTF("\nFailed to query DevicePath (%lu)\n", status);
		return (status);
	}

	while (!IsDevicePathEnd(NextDevicePathNode(devpath)))
		devpath = NextDevicePathNode(devpath);

	status = bs->HandleProtocol(h, &BlockIoProtocolGUID, (void **)&blkio);
	if (status == EFI_UNSUPPORTED)
		return (status);

	if (status != EFI_SUCCESS) {
		DPRINTF("\nFailed to query BlockIoProtocol (%lu)\n", status);
		return (status);
	}

	if (!blkio->Media->LogicalPartition)
		return (EFI_UNSUPPORTED);

	/* Run through each module, see if it can load this partition */
	for (i = 0; i < NUM_BOOT_MODULES; i++ ) {
		if (boot_modules[i] == NULL)
			continue;

		if ((status = bs->AllocatePool(EfiLoaderData,
		    sizeof(*devinfo), (void **)&devinfo)) !=
		    EFI_SUCCESS) {
			DPRINTF("\nFailed to allocate devinfo (%lu)\n",
			    status);
			continue;
		}
		devinfo->dev = blkio;
		devinfo->devpath = devpath;
		devinfo->devhandle = h;
		devinfo->devdata = NULL;
		devinfo->next = NULL;

		status = boot_modules[i]->probe(devinfo);
		if (status == EFI_SUCCESS)
			return (EFI_SUCCESS);
		(void)bs->FreePool(devinfo);
	}

	return (EFI_UNSUPPORTED);
}

void
add_device(dev_info_t **devinfop, dev_info_t *devinfo)
{
	dev_info_t *dev;
	
	if (*devinfop == NULL) {
		*devinfop = devinfo;
		return;
	}

	for (dev = *devinfop; dev->next != NULL; dev = dev->next)
		;

	dev->next = devinfo;
}

void
panic(const char *fmt, ...)
{
	char buf[128];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	printf("panic: %s\n", buf);
	va_end(ap);

	while (1) {}
}

int
printf(const char *fmt, ...)
{
	va_list ap;
	int ret;

	/* Don't annoy the user as we probe for partitions */
	if (strcmp(fmt,"Not ufs\n") == 0)
		return 0;

	va_start(ap, fmt);
	ret = __printf(fmt, putchr, 0, ap);
	va_end(ap);
	return (ret);
}

void vprintf(const char *fmt, va_list ap)
{
	__printf(fmt, putchr, 0, ap);
}

int vsnprintf(char *str, size_t sz, const char *fmt, va_list ap)
{
	struct sp_data sp;
	int ret;

	sp.sp_buf = str;
	sp.sp_len = 0;
	sp.sp_size = sz;
	ret = __printf(fmt, __sputc, &sp, ap);
	return (ret);
}

static int
__printf(const char *fmt, putc_func_t *putc, void *arg, va_list ap)
{
	char buf[(sizeof(long) * 8) + 1];
	char *nbuf, *s;
	u_long ul;
	u_int ui;
	int lflag, sflag, pad, ret, c;

	nbuf = &buf[sizeof buf - 1];
	ret = 0;
	while ((c = *fmt++) != 0) {
		if (c != '%') {
			ret += putc(c, arg);
			continue;
		}
		lflag = 0;
		sflag = 0;
		pad = 0;
reswitch:	c = *fmt++;
		switch (c) {
		case '#':
			sflag = 1;
			goto reswitch;
		case '%':
			ret += putc('%', arg);
			break;
		case 'c':
			c = va_arg(ap, int);
			ret += putc(c, arg);
			break;
		case 'd':
			if (lflag == 0) {
				ui = (u_int)va_arg(ap, int);
				if (ui < (int)ui) {
					ui = -ui;
					ret += putc('-', arg);
				}
				s = __uitoa(nbuf, ui, 10);
			} else {
				ul = (u_long)va_arg(ap, long);
				if (ul < (long)ul) {
					ul = -ul;
					ret += putc('-', arg);
				}
				s = __ultoa(nbuf, ul, 10);
			}
			ret += __puts(s, putc, arg);
			break;
		case 'l':
			lflag = 1;
			goto reswitch;
		case 'o':
			if (lflag == 0) {
				ui = (u_int)va_arg(ap, u_int);
				s = __uitoa(nbuf, ui, 8);
			} else {
				ul = (u_long)va_arg(ap, u_long);
				s = __ultoa(nbuf, ul, 8);
			}
			ret += __puts(s, putc, arg);
			break;
		case 'p':
			ul = (u_long)va_arg(ap, void *);
			s = __ultoa(nbuf, ul, 16);
			ret += __puts("0x", putc, arg);
			ret += __puts(s, putc, arg);
			break;
		case 's':
			s = va_arg(ap, char *);
			ret += __puts(s, putc, arg);
			break;
		case 'u':
			if (lflag == 0) {
				ui = va_arg(ap, u_int);
				s = __uitoa(nbuf, ui, 10);
			} else {
				ul = va_arg(ap, u_long);
				s = __ultoa(nbuf, ul, 10);
			}
			ret += __puts(s, putc, arg);
			break;
		case 'x':
			if (lflag == 0) {
				ui = va_arg(ap, u_int);
				s = __uitoa(nbuf, ui, 16);
			} else {
				ul = va_arg(ap, u_long);
				s = __ultoa(nbuf, ul, 16);
			}
			if (sflag)
				ret += __puts("0x", putc, arg);
			ret += __puts(s, putc, arg);
			break;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			pad = pad * 10 + c - '0';
			goto reswitch;
		default:
			break;
		}
	}
	return (ret);
}

static int
__sputc(char c, void *arg)
{
	struct sp_data *sp;

	sp = arg;
	if (sp->sp_len < sp->sp_size)
		sp->sp_buf[sp->sp_len++] = c;
	sp->sp_buf[sp->sp_len] = '\0';
	return (1);
}

static int
__puts(const char *s, putc_func_t *putc, void *arg)
{
	const char *p;
	int ret;

	ret = 0;
	for (p = s; *p != '\0'; p++)
		ret += putc(*p, arg);
	return (ret);
}

static char *
__uitoa(char *buf, u_int ui, int base)
{
	char *p;

	p = buf;
	*p = '\0';
	do
		*--p = digits[ui % base];
	while ((ui /= base) != 0);
	return (p);
}

static char *
__ultoa(char *buf, u_long ul, int base)
{
	char *p;

	p = buf;
	*p = '\0';
	do
		*--p = digits[ul % base];
	while ((ul /= base) != 0);
	return (p);
}
