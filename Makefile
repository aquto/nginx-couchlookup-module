NAME = nginx-couchlookup-module
VERSION = $(shell cat build.number)

FILES = config $(wildcard *.h *.c lib/*.h lib/*.c)
TARBALL = $(NAME)-$(VERSION).tar.gz

all: archive

archive:
	mkdir $(NAME) && cp $(FILES) $(NAME)/
	tar -cvf $(TARBALL) $(NAME)/
	$(RM) -r $(NAME)/

clean:
	$(RM) $(TARBALL)
