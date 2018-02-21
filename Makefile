NAME = nginx-couchlookup-module
VERSION = $(shell cat build.number)

SRC = config lib $(wildcard *.h *.c)
TARBALL = $(NAME)-$(VERSION).tar.gz

all: archive

archive:
	mkdir $(NAME) && cp -R $(SRC) $(NAME)/
	tar -czvf $(TARBALL) $(NAME)/
	$(RM) -r $(NAME)/

clean:
	$(RM) $(TARBALL)
