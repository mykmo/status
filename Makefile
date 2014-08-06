PROGNAME=$(shell pwd | xargs basename)

$(PROGNAME): main.o
	gcc -s -o $@ $<

%.o: %.c
	gcc -c -pedantic -Wall -std=c99 -o $@ $<

clean:
	@echo cleaning...
	@rm -f *.o *~ core.*
