
BINS = test 
CFLAGS+= -m64 -pthread -I../src -g
LDFLAGS+= -m64 -lrt -lpthread -lpmu_sched -L../lib -g 

.PHONY:	all clean

all:	$(BINS)

%.o:	%.c
	$(CC) $(CFLAGS) $(DEFINES) -c -o $@ $<

$(BINS):	%:	%.o $(TMLIB)
	$(CC) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(BINS) *.o
