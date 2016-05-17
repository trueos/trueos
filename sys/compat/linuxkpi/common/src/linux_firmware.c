#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/malloc.h>
#include <sys/firmware.h>

#include <linux/firmware.h>
#include <linux/device.h>
#undef firmware

MALLOC_DEFINE(M_LKPI_FW, "lkpifw", "linux kpi firmware");

int
request_firmware(const struct linux_firmware **lkfwp, const char *name,
		     struct device *device)
{
	struct linux_firmware *lkfw;
	const struct firmware *fw;

	*lkfwp = NULL;
	if ((lkfw = malloc(sizeof(*lkfw), M_LKPI_FW, M_NOWAIT)) == NULL)
		return (-ENOMEM);

	device_printf(device->bsddev, "trying to load firmware image %s\n", name);
	if ((fw = firmware_get(name)) == NULL) {
		device_printf(device->bsddev, "failed to load firmware image %s\n", name);
		pause("CAAAUSE", 2*hz);
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

	if (lkfw == NULL)
		return;

	fw = lkfw->priv;
	free(__DECONST(void *, lkfw), M_LKPI_FW);
	firmware_put(fw, 0);
}
