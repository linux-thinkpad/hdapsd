MAN_SUBSTS = \
	-e 's|__VERSION__|@PACKAGE_VERSION@|' \
	-e "s|__DATE__|`date -d @PACKAGE_VERSION@ '+%Y-%m-%d'`|"

CLEANFILES = @PACKAGE_NAME@.8

man_MANS = @PACKAGE_NAME@.8
man_PRE = @PACKAGE_NAME@.man
dist_man_MANS = @PACKAGE_NAME@.man

.man.8:
	sed $(MAN_SUBSTS) < $< > $@
