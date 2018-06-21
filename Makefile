server:
	gcc kcamserver.c -o kcamserver -I/opt/EDTpdv /opt/EDTpdv/libpdv.a -lm -lpthread -ldl

acquire:
	gcc kcam_acquire.c cred1struct.c -o kcam_acquire -I/opt/EDTpdv -I/home/scexao/src/cacao/src/ImageStreamIO -I/home/scexao/src/cacao/src /home/scexao/src/cacao/src/ImageStreamIO/ImageStreamIO.c /opt/EDTpdv/libpdv.a -lm -lpthread -ldl 

clean:
	rm -rf *~
	rm -f kcamserver
	rm -f *.o
