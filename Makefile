all:
	gcc kcamserver.c -o kcamserver

clean:
	rm -rf *~
	rm -f kcamserver
	rm -f *.o
