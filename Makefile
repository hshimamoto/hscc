CFLAGS = -Wall -g -O2 -std=c11
objs = main.o utils.o

hscc: $(objs)
	$(CC) -o $@ $(objs)

%.o: %.c
	$(CC) -o $@ $(CFLAGS) -c $<

test: hscc
	./test.sh

clean:
	rm -f hscc *.o tmp*
