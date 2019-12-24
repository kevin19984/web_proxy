all : web_proxy

web_proxy: main.o boyer_moore.o
	g++ -std=c++11 -g -o web_proxy main.o boyer_moore.o -pthread

boyer_moore.o: boyer_moore.cpp boyer_moore.h
	g++ -std=c++11 -g -c -o boyer_moore.o boyer_moore.cpp

main.o: main.cpp boyer_moore.h
	g++ -std=c++11 -g -c -o main.o main.cpp -pthread

clean:
	rm -f web_proxy
	rm -f *.o

