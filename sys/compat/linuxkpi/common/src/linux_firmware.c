#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/malloc.h>
#include <sys/firmware.h>

#include <linux/firmware.h>
#undef firmware

MALLOC_DEFINE(M_LKPI_FW, "lkpifw", "linux kpi firmware");

int
request_firmware(const struct linux_firmware **lkfwp, const char *name,
		     struct device *device)
{
	struct linux_firmware *lkfw;
	const struct firmware *fw;

	if ((lkfw = malloc(sizeof(*lkfw), M_LKPI_FW, M_NOWAIT)) == NULL)
		return (-ENOMEM);

	if ((fw = firmware_get(name)) == NULL) {
		free(lkfw, M_LKPI_FW);
		return (-ENOENT);
	}
	lkfw->priv = __DECONST(void *, fw);
	lkfw->size = fw->datasize;
	lkfw->data = fw->data;
	lkfw->pages = NULL;
	*lkfwp = lkfw;
	return (0);
}

void
release_firmware(const struct linux_firmware *lkfw)
{
	struct firmware *fw;	

	fw = lkfw->priv;
	free(__DECONST(void *, lkfw), M_LKPI_FW);
	firmware_put(fw, 0);
}
