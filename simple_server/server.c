#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

int setupServer(const char* address, const char* port) {
	int serverSocket = 0;
	struct addrinfo addrInfo;
	struct addrinfo* result;
	struct addrinfo* resNext;
	memset((void*)&addrInfo, 0, sizeof(struct addrinfo));
	addrInfo.ai_family = AF_UNSPEC;
	addrInfo.ai_socktype = SOCK_STREAM;
	int ret = getaddrinfo(address, port, &addrInfo, &result); 
	if (ret != 0) {
		fprintf(stderr, "getaddrinfo failed: %s\n", strerror(errno));
		return -1;
	}
	for (resNext = result; resNext != NULL; resNext = resNext->ai_next) {
		serverSocket = socket(resNext->ai_family, resNext->ai_socktype, resNext->ai_protocol);
		if (serverSocket == -1) {
			continue;
		}
		if (fcntl(serverSocket, F_SETFL, O_ASYNC) == -1) {
			close(serverSocket);
			continue;
		}
		if (bind(serverSocket, resNext->ai_addr, resNext->ai_addrlen) == -1) {
			close(serverSocket);
			continue;
		}
		break;
	}
	freeaddrinfo(result);
	if (resNext == NULL) {
		fprintf(stderr, "failed to setup server\n");
		return -1;
	}
	if (listen(serverSocket, 0x1000) == -1) {
		close(serverSocket);
		fprintf(stderr, "failed to listen on server: %s\n", strerror(errno));
		return -1;
	}
	return serverSocket;
}

struct EventData {
	int networkFd;
	int fileFd;
	unsigned short fileNameLength;
	size_t bytesRead;
	char* fileName;
};

struct EventData* buildEventData(int networkFd, int fileFd) { 
	struct EventData* eventData = (struct EventData*)malloc(sizeof(struct EventData));
	if (eventData == NULL) {
		fprintf(stderr, "failed to allocate EventData: %s\n", strerror(errno));
		return NULL;
	}
	memset(eventData, 0, sizeof(struct EventData));
	eventData->networkFd = networkFd;
	eventData->fileFd = fileFd;
	return eventData;
}

int acceptEvent(int epollDevice, int serverSocket) {
	int newFd = accept(serverSocket, NULL, NULL);
	if (newFd == -1) {
		fprintf(stderr, "failed to accept connection: %s\n", strerror(errno));
		return -1;
	}
	struct epoll_event connectionEvent;
	connectionEvent.events = EPOLLIN;
	connectionEvent.data.ptr = buildEventData(newFd, -1);
	if (connectionEvent.data.ptr == NULL) {
		return -1;
	}
	if (epoll_ctl(epollDevice, EPOLL_CTL_ADD, newFd, &connectionEvent) == -1) {
		close(epollDevice);
		fprintf(stderr, "failed to add descriptor to epoll device: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

enum {DONE, AGAIN, ERROR, FILEEND};

int readFileNameSize(struct EventData* eventData) {
	int networkFd = eventData->networkFd;
	size_t bytesRead = eventData->bytesRead;
	if (bytesRead < sizeof(eventData->fileNameLength)) {
		off_t bytes = read(networkFd, ((char*)&eventData->fileNameLength) + bytesRead, 
				sizeof(eventData->fileNameLength) - bytesRead);
		if (bytes == -1) {
			if (errno == EAGAIN) return AGAIN;
			else return ERROR;
		}
		else if (bytes == 0) {
			return FILEEND;
		}
		eventData->bytesRead += bytes;
	}
	return DONE;
}

int readFileName(struct EventData* eventData) {
	int networkFd = eventData->networkFd;
	size_t bytesRead = eventData->bytesRead - sizeof(eventData->fileNameLength);
	if (bytesRead < eventData->fileNameLength) {
		
		eventData->fileName = malloc(eventData->fileNameLength);
		if (eventData->fileName == NULL) {
			fprintf(stderr, "failed to allocate fileaname: %s %d\n", 
					strerror(errno), eventData->fileNameLength);
			return ERROR;
		}
		off_t bytes = read(networkFd, eventData->fileName + bytesRead, 
				eventData->fileNameLength - bytesRead);
		if (bytes == -1) {
			if (errno == EAGAIN) return AGAIN;
			else return ERROR;
		}
		else if (bytes == 0) {
			return FILEEND;
		}
		eventData->bytesRead += bytes;
	}
	return DONE;
}

int openFile(struct EventData* eventData) {
	if (eventData->fileFd < 0) {
		eventData->fileFd = open(eventData->fileName, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0777);
		if (eventData->fileFd == -1) {
			fprintf(stderr, "failed to open file %s: %s\n", eventData->fileName, strerror(errno));
			return ERROR;
		}
	}
	return DONE;
}

int writeFile(struct EventData* eventData) {
	const int bufferSize = 0x1000;
	char readBuffer[bufferSize];
	off_t bytes = read(eventData->networkFd, readBuffer, bufferSize);
	if (bytes == -1) {
		if (errno == EAGAIN) return AGAIN;
		else return ERROR;
	}
	else if (bytes == 0) {
		return FILEEND;
	}
	else {
		if (write(eventData->fileFd, readBuffer, bytes) != bytes) {
			fprintf(stderr, "failed to write to file: %s\n", strerror(errno));
			return ERROR;
		}
	}
	return DONE;
}

int readEvent(int epollDevice, struct EventData* eventData) {
	int ret = readFileNameSize(eventData);
	if (ret != DONE) { 
		return ret;
	}
	ret = readFileName(eventData);
	if (ret != DONE) {
		return ret;
	}
	ret = openFile(eventData);
	if (ret != DONE) {
		return ret;
	}
	return writeFile(eventData);
}

void cleanup(struct EventData* eventData) {
	if (eventData == NULL) return;
	if (eventData->networkFd != -1) {
		close(eventData->networkFd);
	}
	if (eventData->fileFd != -1) {
		close(eventData->fileFd);
	}
	if (eventData->fileName != NULL) {
		free(eventData->fileName);
	}
	free(eventData);
}

void processEvent(int epollDevice, int serverSocket, struct epoll_event event) {
	struct EventData* eventData = event.data.ptr;
	if (eventData->networkFd == serverSocket) {
		acceptEvent(epollDevice, serverSocket);
		return;
	}
	if (event.events & EPOLLERR) {
		cleanup(eventData);
		return;
	}
	if (event.events & EPOLLHUP) {
		cleanup(eventData);
		return;
	}
	if (event.events & EPOLLIN) {
		switch (readEvent(epollDevice, eventData)) {
			case AGAIN:
			case DONE: 
				return;
			case ERROR:
			case FILEEND:
				cleanup(eventData);
				return;
		}
	}
}

int runServer(int serverSocket) {
	const int maxEvents = 100;
	int epollDevice = epoll_create(0x1000);
	if (epollDevice == -1) {
		fprintf(stderr, "failed to create epoll device: %s\n", strerror(errno));
		return -1;
	}
	struct epoll_event serverEvent;
	serverEvent.events = EPOLLIN;
	serverEvent.data.ptr = buildEventData(serverSocket, -1);
	if (serverEvent.data.ptr == NULL) {
		return -1;
	}
	if (epoll_ctl(epollDevice, EPOLL_CTL_ADD, serverSocket, &serverEvent) == -1) {
		close(epollDevice);
		fprintf(stderr, "failed to add server to epoll device: %s\n", strerror(errno));
		return -1;
	}
	struct epoll_event events[maxEvents];
	struct epoll_event* eventPointer;
	int i;
	for (;;) {
		int eventsCount = epoll_wait(epollDevice, events, maxEvents, -1);
		for (i = 0; i < eventsCount; ++i) {
			processEvent(epollDevice, serverSocket, events[i]);
		}
	}
}

int main(int argc, char** argv) {
	if (argc != 3) {
		fprintf(stderr, "Usage : ./server host port\n");
		return -1;
	}
	int serverSocket = setupServer(argv[1], argv[2]);
	if (serverSocket == -1) {
		return -1;
	}
	return runServer(serverSocket);
}
