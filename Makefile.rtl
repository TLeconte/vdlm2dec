CFLAGS= -Ofast -pthread -D WITH_RTL  -I /usr/local/include/librtlsdr
LDLIBS= -lm -pthread  -lusb-1.0  -L/usr/local/lib -lrtlsdr -lrt

CC=gcc

all:	vdlm2dec

vdlm2dec:	main.o rtl.o d8psk.o vdlm2.o viterbi.o rs.o crc.o out.o outacars.o outxid.o
	$(CC)  main.o rtl.o d8psk.o vdlm2.o viterbi.o rs.o crc.o out.o outacars.o outxid.o -o $@ $(LDLIBS)

outacars.o:	outacars.c acars.h
outxid.o:	outxid.c acars.h
out.o:		out.c
main.o: main.c vdlm2.h
rtl.o:	rtl.c vdlm2.h
d8psk.o: d8psk.c d8psk.h vdlm2.h
vdlm2.o: vdlm2.c vdlm2.h
viterbi.o: viterbi.c vdlm2.h

clean:
	@rm -f *.o vdlm2dec
