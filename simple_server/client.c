#include <sys/types.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

off_t getFileSize(int fd) {
	struct stat statInfo;
	if (fstat(fd, &statInfo) == -1) {
		return -1;
	}
	return statInfo.st_size;
}

int setupClient(const char* address, const char* port) {
	int clientSocket = 0;
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
		clientSocket = socket(resNext->ai_family, resNext->ai_socktype, resNext->ai_protocol);
		if (clientSocket == -1) {
			continue;
		}
		if (connect(clientSocket, resNext->ai_addr, resNext->ai_addrlen) == -1) {
			close(clientSocket);
			continue;
		}
		break;
	}
	freeaddrinfo(result);
	if (resNext == NULL) {
		fprintf(stderr, "failed to setup client\n");
		return -1;
	}
	return clientSocket;
}

char* getNameInDir(char* fileName) {
	int fileNameLength = strlen(fileName);
	char* end = fileName + strlen(fileName) - 1;
	int i = 0;
	for (i = 0; i < fileNameLength; ++i) {
		if (*end == '/') {
			return ++end;
		}
		--end;
	}
	return fileName;
}

int main(int argc, char** argv) {
	if (argc != 4) {
		fprintf(stderr, "Usage: ./client host port file\n");
		return -1;
	}
	int clientSocket = setupClient(argv[1], argv[2]);
	if (clientSocket == -1) {
		return -1;
	}
	int fileToSend = open(argv[3], O_RDONLY);
	if (fileToSend == -1) {
		fprintf(stderr, "failed to open file %s\n", argv[3]);
		close(clientSocket);
		return -1;	
	}
	char* nameToSend = getNameInDir(argv[3]);
	off_t fileToSendLength = getFileSize(fileToSend);
	unsigned short nameToSendLength = strlen(nameToSend);
	if (write(clientSocket, &nameToSendLength, sizeof(nameToSendLength)) == -1) {
		fprintf(stderr, "failed to write to server: %s\n", strerror(errno));
		close(clientSocket);
		close(fileToSend);
		return -1;
	}
	if (write(clientSocket, nameToSend, nameToSendLength) == -1) {
		fprintf(stderr, "failed to write to server: %s\n", strerror(errno));
		close(clientSocket);
		close(fileToSend);
		return -1;
	}
	if (sendfile(clientSocket, fileToSend, NULL, fileToSendLength) == -1) {
		fprintf(stderr, "failed to send file to server: %s\n", strerror(errno));
		close(clientSocket);
		close(fileToSend);
		return -1;
	}
	close(clientSocket);
	close(fileToSend);
	printf("File %s was sent successfully\n", nameToSend);
	return 0;
}
