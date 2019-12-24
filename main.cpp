#include <stdio.h> // for perror
#include <string.h> // for memset
#include <unistd.h> // for close
#include <arpa/inet.h> // for htons
#include <netinet/in.h> // for sockaddr_in
#include <sys/socket.h> // for socket
#include <list>
#include <vector>
#include <thread>
#include <mutex>
#include <netdb.h>
#include "boyer_moore.h"
using namespace std;

mutex mtx_lock;
list<int> threadlist;
vector<thread> workers;	
const static int BUFSIZE = 4000;
bool chk = true;

void usage() {
	printf("syntax : web_proxy <tcp port> <ssl port>\n");
	printf("sample : web_proxy 8080 4433\n");
}

void dump(unsigned char* buf, int size) {
	int i;
	for (i = 0; i < size; i++) {
		if (i % 16 == 0)
			printf("\n");
		printf("%02x ", buf[i]);
	}
	printf("\n\n");
}

void clitoserv(int clifd, int servfd, int threadnum) {
	char buf[BUFSIZE];
	while (true) {
		ssize_t received = recv(clifd, buf, BUFSIZE - 1, 0);
		if (received == 0 || received == -1) {
			perror("recv failed");
			printf("Connection from client disconnected\n");
			break;
		}
		buf[received] = '\0';
		printf("Client -> Webserver\n");
		for(int i=0; i<received; i++)
			printf("%c", buf[i]);
		printf("\n\n");
		//dump((unsigned char*)buf, received);
		ssize_t sent = send(servfd, buf, received, 0);
		if (sent == 0) {
			perror("send failed");
			break;
		}
	}
	close(clifd);
	close(servfd);
	mtx_lock.lock();
	threadlist.push_back(threadnum);
	mtx_lock.unlock();
}

void servtocli(int clifd, int servfd, int threadnum) {
	char buf[BUFSIZE];
	while (true) {
		ssize_t received = recv(servfd, buf, BUFSIZE - 1, 0);
		if (received == 0 || received == -1) {
			perror("recv failed");
			printf("Connection from webserver disconnected\n");
			break;
		}
		buf[received] = '\0';
		printf("Webserver -> Client\n");
		for(int i=0; i<received; i++)
			printf("%c", buf[i]);
		printf("\n\n");
		//dump((unsigned char*)buf, received);
		ssize_t sent = send(clifd, buf, received, 0);
		if (sent == 0) {
			perror("send failed");
			break;
		}
	}
	close(clifd);
	close(servfd);
	mtx_lock.lock();
	threadlist.push_back(threadnum);
	mtx_lock.unlock();
}

void joinfun() {
	while (chk) {
		mtx_lock.lock();
		for(list<int>::iterator iter = threadlist.begin(); iter != threadlist.end();){
			workers[*iter].join();
			iter = threadlist.erase(iter);
		}
		mtx_lock.unlock();
		this_thread::sleep_for(chrono::duration<int>(3));
	}
}

int jump[300];
char pattern[9] = {0x0d, 0x0a, 'H', 'o', 's', 't', ':', ' ', '\0'};
int patternlen = 8;

int main(int argc, char* argv[]) {
	if (argc < 3) {
		usage();
		return -1;
	}

	computeJump(pattern, patternlen, jump);	

	int tcpport = atoi(argv[1]);
	int sslport = atoi(argv[2]);

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		perror("socket failed");
		return -1;
	}

	int optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,  &optval , sizeof(int));

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(tcpport);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	memset(addr.sin_zero, 0, sizeof(addr.sin_zero));

	int res = bind(sockfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(struct sockaddr));
	if (res == -1) {
		perror("bind failed");
		return -1;
	}

	res = listen(sockfd, 2);
	if (res == -1) {
		perror("listen failed");
		return -1;
	}
	printf("starting proxy\n");
	thread jointhread(joinfun);
	while (true) {
		struct sockaddr_in addr;
		socklen_t clientlen = sizeof(sockaddr);
		int clifd = accept(sockfd, reinterpret_cast<struct sockaddr*>(&addr), &clientlen);
		if (clifd < 0) {
			perror("ERROR on accept");
			break;
		}
		printf("client connected\n");
		
		char buf[BUFSIZE];

		ssize_t received = recv(clifd, buf, BUFSIZE - 1, 0);
		if (received == 0 || received == -1) {
			perror("recv failed");
			break;
		}
		buf[received] = '\0';
		for(int i=0; i<received; i++)
			printf("%c", buf[i]);
		printf("\n\n");
		//dump((unsigned char*)buf, received);

		int hostlen = 0;
		int pos = BoyerMooreHorspool(buf, received, pattern, patternlen, jump);
		if(pos == -1) {
			printf("find Host error\n");
			continue;
		}
		for(int i=pos+8; i<received; i++)
		{
			if(buf[i] == 0x0d && buf[i+1] == 0x0a)
			{
				hostlen = i - (pos + 8);
				break;
			}
		}
		char* host = (char*) malloc (sizeof(char) * (hostlen + 1));
		memcpy(host, buf + pos + 8, hostlen);
		host[hostlen] = '\0';
		struct hostent *host_entry;
		host_entry = gethostbyname(host);
		if (!host_entry) {
			printf("gethostbyname() error\n");
			continue;
		}
		int servfd = socket(AF_INET, SOCK_STREAM, 0);
		if (servfd == -1) {
			perror("socket failed");
			return -1;
		}

		struct sockaddr_in addr2;
		addr2.sin_family = AF_INET;
		addr2.sin_port = htons(80);
		addr2.sin_addr.s_addr = (*(struct in_addr*)host_entry->h_addr_list[0]).s_addr;
		memset(addr2.sin_zero, 0, sizeof(addr.sin_zero));

		int res2 = connect(servfd, reinterpret_cast<struct sockaddr*>(&addr2), sizeof(struct sockaddr));
		if (res2 == -1) {
			perror("connect failed");
			return -1;
		}
		printf("webserver connected\n");
		ssize_t sent = send(servfd, buf, received, 0);
		if (sent == 0) {
			perror("send failed");
			break;
		}
		int len = workers.size();
		workers.push_back(thread(clitoserv, clifd, servfd, len));
		workers.push_back(thread(servtocli, clifd, servfd, len+1));
	}
	chk = false;
	jointhread.join();
	close(sockfd);
}
