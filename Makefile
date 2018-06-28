server:
	gcc kcamserver.c cred1struct.c -o kcamserver -I/opt/EDTpdv /opt/EDTpdv/libpdv.a -lm -lpthread -ldl

fetch:
	gcc kcamfetch.c cred1struct.c -o kcamfetch -I/opt/EDTpdv -I/home/scexao/src/cacao/src/ImageStreamIO -I/home/scexao/src/cacao/src /home/scexao/src/cacao/src/ImageStreamIO/ImageStreamIO.c /opt/EDTpdv/libpdv.a -lm -lpthread -ldl 

clean:
	rm -rf *~
	rm -f kcamserver
	rm -f *.o
	rm -f kcamfetch
