CFLAGS = -Wall -g -O2 -std=c11
objs = main.o token.o utils.o

hscc: $(objs)
	$(CC) -o $@ $(objs)

%.o: %.c
	$(CC) -o $@ $(CFLAGS) -c $<

main.o: utils.h token.h
utils.o: utils.h
token.o: token.h

test: hscc
	./test.sh

clean:
	rm -f hscc *.o tmp*
