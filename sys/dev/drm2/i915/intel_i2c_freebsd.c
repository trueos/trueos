static int
intel_gmbus_probe(device_t dev)
{

	return (BUS_PROBE_SPECIFIC);
}

static int
intel_gmbus_attach(device_t idev)
{
	struct intel_iic_softc *sc;
	struct drm_device *dev;
	struct drm_i915_private *dev_priv;
	int pin, port;

	sc = device_get_softc(idev);
	pin = device_get_unit(idev);
	port = pin + 1; /* +1 to map gmbus index to pin pair */

	snprintf(sc->name, sizeof(sc->name), "i915 gmbus %s",
	    intel_gmbus_is_port_valid(port) ? gmbus_ports[pin].name :
	    "reserved");
	device_set_desc(idev, sc->name);

	dev = device_get_softc(device_get_parent(idev));
	dev_priv = dev->dev_private;
	sc->bus = &dev_priv->gmbus[pin];

	/* add bus interface device */
	sc->iic_dev = device_add_child(idev, "iicbus", -1);
	if (sc->iic_dev == NULL)
		return (ENXIO);
	device_quiet(sc->iic_dev);
	bus_generic_attach(idev);

	return (0);
}

static int
intel_gmbus_detach(device_t idev)
{

	bus_generic_detach(idev);
	device_delete_children(idev);

	return (0);
}

static device_method_t intel_gmbus_methods[] = {
	DEVMETHOD(device_probe,		intel_gmbus_probe),
	DEVMETHOD(device_attach,	intel_gmbus_attach),
	DEVMETHOD(device_detach,	intel_gmbus_detach),
	DEVMETHOD(iicbus_reset,		intel_iicbus_reset),
	DEVMETHOD(iicbus_transfer,	gmbus_xfer),
	DEVMETHOD_END
};
static driver_t intel_gmbus_driver = {
	"intel_gmbus",
	intel_gmbus_methods,
	sizeof(struct intel_iic_softc)
};
static devclass_t intel_gmbus_devclass;
DRIVER_MODULE_ORDERED(intel_gmbus, drmn, intel_gmbus_driver,
    intel_gmbus_devclass, 0, 0, SI_ORDER_FIRST);
DRIVER_MODULE(iicbus, intel_gmbus, iicbus_driver, iicbus_devclass, 0, 0);

static int
intel_iicbb_probe(device_t dev)
{

	return (BUS_PROBE_DEFAULT);
}

static int
intel_iicbb_attach(device_t idev)
{
	struct intel_iic_softc *sc;
	struct drm_device *dev;
	struct drm_i915_private *dev_priv;
	int pin, port;

	sc = device_get_softc(idev);
	pin = device_get_unit(idev);
	port = pin + 1;

	snprintf(sc->name, sizeof(sc->name), "i915 iicbb %s",
	    intel_gmbus_is_port_valid(port) ? gmbus_ports[pin].name :
	    "reserved");
	device_set_desc(idev, sc->name);

	dev = device_get_softc(device_get_parent(idev));
	dev_priv = dev->dev_private;
	sc->bus = &dev_priv->gmbus[pin];

	/* add generic bit-banging code */
	sc->iic_dev = device_add_child(idev, "iicbb", -1);
	if (sc->iic_dev == NULL)
		return (ENXIO);
	device_quiet(sc->iic_dev);
	bus_generic_attach(idev);
	iicbus_set_nostop(idev, true);

	return (0);
}

static int
intel_iicbb_detach(device_t idev)
{

	bus_generic_detach(idev);
	device_delete_children(idev);

	return (0);
}


int
intel_setup_gmbus_freebsd(struct drm_device *dev)
{

	/*
	 * The Giant there is recursed, most likely.  Normally, the
	 * intel_setup_gmbus() is called from the attach method of the
	 * driver.
	 */
	mtx_lock(&Giant);
	for (i = 0; i < GMBUS_NUM_PORTS; i++) {
		struct intel_gmbus *bus = &dev_priv->gmbus[i];
		u32 port = i + 1; /* +1 to map gmbus index to pin pair */

		bus->dev_priv = dev_priv;

		/* By default use a conservative clock rate */
		bus->reg0 = port | GMBUS_RATE_100KHZ;

		/* gmbus seems to be broken on i830 */
		if (IS_I830(dev))
			bus->force_bit = 1;

		intel_gpio_setup(bus, port);

		/*
		 * bbbus_bridge
		 *
		 * Initialized bbbus_bridge before gmbus_bridge, since
		 * gmbus may decide to force quirk transfer in the
		 * attachment code.
		 */
		bus->bbbus_bridge = device_add_child(dev->dev,
		    "intel_iicbb", i);
		if (bus->bbbus_bridge == NULL) {
			DRM_ERROR("bbbus bridge %d creation failed\n", i);
			ret = -ENXIO;
			goto err;
		}
		device_quiet(bus->bbbus_bridge);
		ret = -device_probe_and_attach(bus->bbbus_bridge);
		if (ret != 0) {
			DRM_ERROR("bbbus bridge %d attach failed, %d\n", i,
			    ret);
			goto err;
		}

		/* bbbus */
		iic_dev = device_find_child(bus->bbbus_bridge,
		    "iicbb", -1);
		if (iic_dev == NULL) {
			DRM_ERROR("bbbus bridge doesn't have iicbb child\n");
			goto err;
		}
		iic_dev = device_find_child(iic_dev, "iicbus", -1);
		if (iic_dev == NULL) {
			DRM_ERROR(
		"bbbus bridge doesn't have iicbus grandchild\n");
			goto err;
		}

		bus->bbbus = iic_dev;

		/* gmbus_bridge */
		bus->gmbus_bridge = device_add_child(dev->dev,
		    "intel_gmbus", i);
		if (bus->gmbus_bridge == NULL) {
			DRM_ERROR("gmbus bridge %d creation failed\n", i);
			ret = -ENXIO;
			goto err;
		}
		device_quiet(bus->gmbus_bridge);
		ret = -device_probe_and_attach(bus->gmbus_bridge);
		if (ret != 0) {
			DRM_ERROR("gmbus bridge %d attach failed, %d\n", i,
			    ret);
			ret = -ENXIO;
			goto err;
		}

		/* gmbus */
		iic_dev = device_find_child(bus->gmbus_bridge,
		    "iicbus", -1);
		if (iic_dev == NULL) {
			DRM_ERROR("gmbus bridge doesn't have iicbus child\n");
			goto err;
		}

		bus->gmbus = iic_dev;
	}
	mtx_unlock(&Giant);

	return (0);
err:
	while (--i) {
		struct intel_gmbus *bus = &dev_priv->gmbus[i];
		if (bus->gmbus_bridge != NULL)
			device_delete_child(dev->dev, bus->gmbus_bridge);
		if (bus->bbbus_bridge != NULL)
			device_delete_child(dev->dev, bus->bbbus_bridge);
	}
	mtx_unlock(&Giant);
	mutex_destroy(&dev_priv->gmbus_mutex);

	return ret;
}

void
i2c_del_adapter(struct i2c_adapter *adapter)
{	
	mtx_lock(&Giant);
	ret = device_delete_child(dev->dev, bus->gmbus_bridge);
	mtx_unlock(&Giant);

	KASSERT(ret == 0, ("unable to detach iic gmbus %s: %d",
			   device_get_desc(bus->gmbus_bridge), ret));	
	mtx_lock(&Giant);
	ret = device_delete_child(dev->dev, bus->bbbus_bridge);
	mtx_unlock(&Giant);

	KASSERT(ret == 0, ("unable to detach iic bbbus %s: %d",
			   device_get_desc(bus->bbbus_bridge), ret));
		
}

static device_method_t intel_iicbb_methods[] =	{
	DEVMETHOD(device_probe,		intel_iicbb_probe),
	DEVMETHOD(device_attach,	intel_iicbb_attach),
	DEVMETHOD(device_detach,	intel_iicbb_detach),

	DEVMETHOD(bus_add_child,	bus_generic_add_child),
	DEVMETHOD(bus_print_child,	bus_generic_print_child),

	DEVMETHOD(iicbb_callback,	iicbus_null_callback),
	DEVMETHOD(iicbb_reset,		intel_iicbus_reset),
	DEVMETHOD(iicbb_setsda,		set_data),
	DEVMETHOD(iicbb_setscl,		set_clock),
	DEVMETHOD(iicbb_getsda,		get_data),
	DEVMETHOD(iicbb_getscl,		get_clock),
	DEVMETHOD(iicbb_pre_xfer,	intel_gpio_pre_xfer),
	DEVMETHOD(iicbb_post_xfer,	intel_gpio_post_xfer),
	DEVMETHOD_END
};
static driver_t intel_iicbb_driver = {
	"intel_iicbb",
	intel_iicbb_methods,
	sizeof(struct intel_iic_softc)
};
static devclass_t intel_iicbb_devclass;
DRIVER_MODULE_ORDERED(intel_iicbb, drmn, intel_iicbb_driver,
    intel_iicbb_devclass, 0, 0, SI_ORDER_FIRST);
DRIVER_MODULE(iicbb, intel_iicbb, iicbb_driver, iicbb_devclass, 0, 0);
