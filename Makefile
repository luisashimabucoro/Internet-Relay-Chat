all:
	g++ client.cpp -Wall -pthread -g -o client
	g++ server.cpp -Wall -pthread -g -o server
	@stty -icanon

rm:
	rm client
	rm server

runClient:
	./client 52547 

runServer:
	./server 52547

finish:
	@stty icanon