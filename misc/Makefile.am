if HAVE_SYSTEMD
udevrulesdir = $(udevdir)/rules.d
udevrules_DATA = hdapsd.rules
systemdsystemunit_DATA = \
	hdapsd@.service
endif

EXTRA_DIST = \
	hdapsd.rules
