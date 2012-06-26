if HAVE_SYSTEMD
udevdir = $(libdir)/udev/rules.d
udev_DATA = 99-hdapsd.rules
systemdsystemunit_DATA = \
	hdapsd@.service
EXTRA_DIST = \
	99-hdapsd.rules \
	hdapsd@.service
endif

