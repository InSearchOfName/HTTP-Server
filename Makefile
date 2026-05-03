all:
	gcc -o server.out main.c server.c

clean:
	rm server.out