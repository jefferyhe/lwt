all: main

main: main.o lwt.o channel.o
	gcc main.o lwt.o channel.o -o main -O3

main.o: main.c
	gcc -c main.c -O3

lwt.o: lwt.c
	gcc -c lwt.c -O3

channel.o: channel.c
	gcc -c channel.c -O3
	
clean:
	rm main lwt.o main.o channel.o

run:
	./main
