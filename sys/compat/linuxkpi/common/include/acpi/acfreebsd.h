#ifndef __ACPI_ACFREEBSD_H_
#define __ACPI_ACFREEBSD_H_
MALLOC_DECLARE(M_ACPICA);

static inline void *
acpi_os_allocate(long Size)
{
    return (malloc(Size, M_ACPICA, M_NOWAIT));
}

static inline void
acpi_os_free(void *Memory)
{
    free(Memory, M_ACPICA);
}
#endif
