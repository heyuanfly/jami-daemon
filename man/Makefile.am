SECTION="1"

TEMPLATES=jamid.pod

man_MANS=jamid.1

POD2MAN=pod2man

EXTRA_DIST=$(man_MANS) $(TEMPLATES)

all: $(man_MANS)

SUFFIXES=.pod .1

.pod.1:
	$(POD2MAN) --section=$(SECTION) --release=@VERSION@ --center "" $< > $@

clean-local:
	rm -rf *.1
