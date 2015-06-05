-include Makefile.conf


SRCDIRS := src test
SUBDIRS := $(SRCDIRS) user-doc developer-doc regtest

SUBDIRSCLEAN:=$(addsuffix .clean,$(SUBDIRS))

     
.PHONY: all lib clean $(SRCDIRS) doc docclean check cppcheck

# if machine dependent configuration has been found:
ifdef GCCDEP
all:
	$(MAKE) lib

lib:
	$(MAKE)	-C src

install:
	$(MAKE) -C src install

uninstall:
	$(MAKE) -C src uninstall

$(SRCDIRS):
	$(MAKE) -C $@
     
# compile plumed before tests:
test: src

# doxygen
doc:
	$(MAKE) -C user-doc
	$(MAKE) -C developer-doc

docs:
	$(MAKE) doc

# regtests
check: src test
	$(MAKE) -C regtest

else

all:
	@echo No configuration available
	@echo First run ./configure
endif

# these targets are available also without configuration

clean: $(SUBDIRSCLEAN)
	rm -f *~ */*~ */*/*~

$(SUBDIRSCLEAN): %.clean:
	$(MAKE) -C $* clean

fullclean:
	make clean
	rm -f Makefile.conf
	rm -f sourceme.sh
	rm -f config.log 


docclean:
	cd user-doc && make clean
	cd developer-doc && make clean

cppcheck:
	$(MAKE) -C src cppcheck



