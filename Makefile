server:
	gcc kcamserver.c -o kcamserver -I/opt/EDTpdv /opt/EDTpdv/libpdv.a -lm -lpthread -ldl
clean:
	rm -rf *~
	rm -f kcamserver
	rm -f *.o
