LD = ld
LDFLAGS = -s
ASM = yasm
ASMFLAGS = -f elf64

OBJS = *.o
EXEC = tmt70init tmt70cut tmt70test

all : $(EXEC)

strip : $(EXEC)
	-strip $<

.PHONY : clean
clean :
	-rm -f $(OBJS) $(EXEC)

%.o : %.asm
	$(ASM) $(ASMFLAGS) -o $@ $<

tmt70init : tmt70init.o
	$(LD) $(LDFLAGS) -o $@ $<

tmt70cut : tmt70cut.o
	$(LD) $(LDFLAGS) -o $@ $<

tmt70test : tmt70test.o
	$(LD) $(LDFLAGS) -o $@ $<
