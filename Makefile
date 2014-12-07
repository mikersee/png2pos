CC := gcc
CFLAGS += -std=c99 -O2 -Wall -pedantic -D_POSIX_C_SOURCE=200112L \
	-DLODEPNG_NO_COMPILE_ANCILLARY_CHUNKS \
	-DLODEPNG_NO_COMPILE_CPP \
	-DLODEPNG_NO_COMPILE_ALLOCATORS \
	-DLODEPNG_NO_COMPILE_ENCODER
LDFLAGS += -O2 -Wall -pedantic
PREFIX := /usr/local

OBJS = lodepng.o png2pos.o
EXEC = png2pos

all : $(EXEC)

man : $(EXEC).1.gz

strip : $(EXEC)
	-strip $<

.PHONY : clean
clean :
	-rm -f $(OBJS) $(EXEC)
	-rm *.pos *.gz

$(EXEC) : $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

%.o : %.c
	$(CC) -c $(CFLAGS) -o $@ $<

%.1.gz : %.1
	gzip -c -9 $< > $@

rpi : CFLAGS += -march=armv6j -mfpu=vfp -mfloat-abi=hard
rpi : strip

debug : CFLAGS += -DDEBUG -DLODEPNG_COMPILE_ENCODER
debug : all

profiled :
	make CFLAGS="$(CFLAGS) -fprofile-generate" $(EXEC)
	find ./samples -type f -exec ./$(EXEC) -o test.pos -c -r -a c {} \;
	-rm *.pos
	make clean
	make CFLAGS="$(CFLAGS) -fprofile-use" strip
	-rm -f $(OBJS) *.gcda *.gcno *.dyn pgopti.dpi pgopti.dpi.lock

install : all man
	mkdir -p $(DESTDIR)$(PREFIX)/bin $(DESTDIR)$(PREFIX)/share/man/man1
	install -m755 $(EXEC) $(DESTDIR)$(PREFIX)/bin
	install -m644 $(EXEC).1.gz $(DESTDIR)$(PREFIX)/share/man/man1

install-strip : strip install

uninstall :
	rm $(DESTDIR)$(PREFIX)/bin/$(EXEC)
	rm $(DESTDIR)$(PREFIX)/share/man/man1/$(EXEC).1.gz
