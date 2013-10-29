all: main

main: main.o lwt.o
	gcc main.o lwt.o -o main

main.o: main.c
	gcc -c main.c 

lwt.o: lwt.c
	gcc -c lwt.c 

clean:
	rm main lwt.o main.o

run:
	./main
