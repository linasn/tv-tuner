CFLAGS=-Wall -g 
LDLIBS=-lusb-1.0
OBJ = a828_replay.o

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

a828_replay: $(OBJ)
	$(CC) $^ -o $@ $(LDFLAGS) $(LDLIBS)

clean:
	rm a828_replay *.o
