OBJS = GSML1FEC.o GSMTDMA.o BitVector.o GSM610Tables.o GSM660Tables.o GSM690Tables.o # Threads.o
.cpp.o:
	c++ -g -I. -c -o $@ $<

libopenbts.a: $(OBJS) glue1.o
	rm -f libopenbts.a
	ar -r libopenbts.a $(OBJS) glue1.o

all: libopenbts.a

clean:
	rm -f *.o *.a
