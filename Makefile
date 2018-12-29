hscc: main.c
	$(CC) -o $@ $<

test: hscc
	./test.sh

clean:
	rm -f hscc *.o tmp*
