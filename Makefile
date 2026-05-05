clean:
	rm server.out

run: build
	./server.out

build:
	gcc -o server.out main.c server.c