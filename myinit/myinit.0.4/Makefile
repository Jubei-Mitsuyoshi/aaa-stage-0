# By default, paths are relative and all names start with `my'.  This way you
# can test `myinit' without installing it (without even being root) and if you
# install it, it won't replace your current `init'.  Try writing a few test
# initscripts and using them before you install.  When you run the installed
# `mystart', `mystop' etc. programs your current directory must be the root
# directory.
ROOT = 
MY = my

# If you want to replace your `init' program, use these settings AND BE
# EXTREMELY CAREFUL!!!
#ROOT = /
#MY =

CC = gcc

# If you want to compile with `dietlibc', try this:
#CC = diet -Os gcc

################################################################################

CFLAGS = -I. -DHAVE_CONFIG_H -DMY='"$(MY)"' -DROOT='"$(ROOT)"' -g -O2 -Wall -pedantic
LDFLAGS =

FINAL = \
	sbin/$(MY)init \
	sbin/$(MY)start \
	sbin/$(MY)stop \
	sbin/$(MY)halt \
	sbin/$(MY)poweroff \
	sbin/$(MY)reboot \
	sbin/$(MY)initset \

TEMP = ./src/*~ core dev/initctl dev/myinitctl

BUILD = $(BUILD_init) $(BUILD_start) $(BUILD_iecho)

.SUFFIXES :
.SUFFIXES :  .o .c
.c.o :
	$(CC) $(CFLAGS) -o $@ -c $<
.o :
	$(CC) $(LDFLAGS) -o $@ $^

.PHONY : all check depend mostlyclean clean distclean maintainer-clean install use-with-caution
all :

################################################################################

BUILD_init = src/init.o src/servs.o src/procs.o src/util.o
sbin/$(MY)init : $(BUILD_init)
	$(CC) $(LDFLAGS) -o $@ $^

BUILD_start = src/start.o src/util.o
sbin/$(MY)start : $(BUILD_start)
	$(CC) $(LDFLAGS) -o $@ $^
sbin/$(MY)stop : sbin/$(MY)start
	ln -sf $(MY)start $@
sbin/$(MY)halt : sbin/$(MY)start
	ln -sf $(MY)start $@
sbin/$(MY)poweroff : sbin/$(MY)start
	ln -sf $(MY)start $@
sbin/$(MY)reboot : sbin/$(MY)start
	ln -sf $(MY)start $@
sbin/$(MY)initset : sbin/$(MY)start
	ln -sf $(MY)start $@

etc/myinit.boot : etc/init.boot
	sed -e 's#\<\(start\|stop\|initset\|halt\|poweroff\|reboot\)\>#sbin/my\1#g' <$^ >$@
	chmod +x "$@"
etc/myinit : etc/init
	rm -rf etc/myinit
	cd etc && for D in `find init -type d`; do \
		mkdir my"$$D"; \
	done
	cd etc && for F in `find init -type f`; do \
		sed -e 's#\(\(^\|||\|;\)[[:space:]]*\)\<initset\>\([[:space:]]\)#\1myinitset\3#g;s#\(\(^\|||\|;\)[[:space:]]*\)\<stop\>\([[:space:]]\)#\1mystop\3#g;s#\(\(^\|||\|;\)[[:space:]]*\)\<start\>\([[:space:]]\)#\1mystart\3#g;s#^cd etc/init$$#cd etc/myinit#g;' <"$$F" >my"$$F"; \
		test -x "$$F" && chmod +x my"$$F"; \
		true; \
	done
	@echo "****************************************"
	@echo "* BE CAREFUL with changes to etc/init! *"
	@echo "* Make sure etc/myinit is correct!     *"
	@echo "****************************************"

################################################################################

all : $(FINAL)
check : $(FINAL)
	PATH="sbin:$$PATH" gdb -q sbin/$(MY)init
dusting :
	rm -f $(TEMP)
mostlyclean :
	rm -f $(TEMP) $(BUILD)
clean :
	rm -f $(TEMP) $(BUILD) $(FINAL)
distclean :
	rm -f $(TEMP) $(BUILD) $(FINAL)
maintainer-clean :
	rm -f $(TEMP) $(BUILD) $(FINAL)
install :
	@echo "better do this yourself (maybe read the makefile)"

# use this AFTER a successful manual installation, to update files
use-with-caution : all
	@if test -e /etc/$(MY)init.old; then \
		echo "old files are present"; \
		false; else true; fi
	-mv /sbin/$(MY)init /sbin/$(MY)init.old
	cp sbin/$(MY)init /sbin
	cp sbin/$(MY)start /sbin
	cp etc/$(MY)init.boot /etc
	-mv /etc/$(MY)init /etc/$(MY)init.old
	cp -a etc/$(MY)init /etc/
