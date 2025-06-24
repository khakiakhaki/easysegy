OPT = -g
CFLAG = -Wall -Wextra 
LIBS = -L. -lesegy -lm

test: libesegy.a demo_write demo_read

.PHONY: test clean release

libesegy.a : segy.c segy.h 
	@rm -f libesegy.a demo_write demo_read
	$(CC) $(OPT) $(CFLAG) -c  $< -o $@

demo_write:demo_write.c
	$(CC) $(OPT) $(CFLAG) $< $(LIBS) -o $@

demo_read:demo_read.c
	$(CC) $(OPT) $(CFLAG) $< $(LIBS) -o $@

clean:
	@rm libesegy.a demo_write demo_read *.segy *.bin demo

release:
	tar -czf libsegy.tar.gz *.c *.h Makefile