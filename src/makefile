all: client server

client:
	mkdir -p ../bin
	gcc ftpclient.c ftpcmd.c -o ../bin/ftpclient -lreadline -lpthread
server: 
	mkdir -p ../bin
	gcc ftpserver.c -o ../bin/ftpserver -lpthread
clean:
	rm -f ../bin/ftpserver ../bin/ftpclient