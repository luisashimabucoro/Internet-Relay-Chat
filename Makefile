all:
	g++ client.cpp -Wall -pthread -g -o client
	g++ server.cpp -Wall -pthread -g -o server
	@stty -icanon

rm:
	rm client
	rm server

runClient:
	./client 

runServer:
	./server 55555

finish:
	@stty icanon
