include globals.mk

# Makefile.am

SOURCES=

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src ringtones man doc bin test

EXTRA_DIST = m4/libtool.m4 \
			 m4/lt~obsolete.m4 \
			 m4/ltoptions.m4 \
			 m4/ltsugar.m4 \
			 m4/ltversion.m4 \
			 contrib/bootstrap \
			 contrib/src

coverage-html:
	$(AM_V_at)lcov --no-external --capture --initial --directory $(top_builddir) --output-file jami-coverage-base.info
	$(AM_V_at)lcov --no-external --capture --directory $(top_builddir) --output-file jami-coverage-tests.info
	$(AM_V_at)lcov --add-tracefile jami-coverage-base.info --add-tracefile jami-coverage-tests.info --output-file jami-coverage.info
	$(AM_V_at)lcov --remove jami-coverage.info "$(abs_top_builddir)/contrib/*" "$(abs_top_builddir)/bin/dbus/*"  --output-file jami-coverage-filtered.info
	$(MKDIR_P) html-coverage-output
	genhtml -o html-coverage-output jami-coverage-filtered.info

coverage-%:
	gcovr --filter src --filter test --$*-pretty --$* --output $*-coverage-output.$*

coverage-clean:
	$(AM_V_at)lcov --directory $(top_builddir) --zerocounters
	$(AM_V_at)rm -f *.info
	$(AM_V_at)rm -rf *-coverage-output

coverage-cleaner: coverage-clean
	$(AM_V_at)find -name '*.gcda' -delete -or -name "*.gcno" -delete

all-coverage: coverage-clean coverage-html coverage-xml coverage-json

.PHONY: all-coverage coverage-html coverage-json coverage-xml coverage-clean coverage-cleaner
