all: main

main: main.o lwt.o
	gcc main.o lwt.o -o main -O3 

main.o: main.c
	gcc -c main.c -O3

lwt.o: lwt.c
	gcc -c lwt.c -O3

clean:
	rm main lwt.o main.o

run:
	./main
