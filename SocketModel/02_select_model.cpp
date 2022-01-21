/*
*	소켓함수
*	1) 능동 : socket, bind, listen , connect, send, CloseHandle
*	2) 수동 : accept, recv
* --> accept, recv 함수 때문에 다중접속, 1대 다 통신을 할며ㅕㄴ 
* --> 쓰레드(Thread)로 처리해야함
* --> 수동적인 함수들을 "미리 호출" 했기 때문이다.
* -----------------------------------------
* [] 소켓 입출력 모델들..]
* 
 1) 먼저 네트워크 이벤트를 수신
 2) 해당 이벤트를 처리할 소켓함수를 "그 후에 호출함

 ex) 블록킹 소켓
 -> 먼저 네트워크 이벤트 발생함
 -> recv() 호출 --> 안기다리고 바로 실행함
ex) 블로킹 소켓
-> 먼저 네트워크 이벤트 발생함
-> recv() 호출 --> 성공해서 바로 돌아옴 
 */
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma comment(lib,"Ws2_32.lib")

#include <winsock2.h>
#include <process.h>
#include <stdio.h>

#define BUFFERSIZE 1024
// 소켓 정보 저장을 위한 구조체

struct SOCKETINFO
{
	SOCKET sock;
	char buf[BUFFERSIZE];
	int recvbytes;
	int sendbytes;
};

SOCKET listenSock; //대기 소켓 핸들
int nTotalSockets = 0; // 통신 소켓의 개수
SOCKETINFO* SocketInfoArray[FD_SETSIZE];

// 소켓 함수 오류 출력
void DisplayMessage()
{
	LPVOID pMsg;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&pMsg,
		0, NULL);

	printf("%s\n", pMsg);

	LocalFree(pMsg);
}
bool CreateListenSocket()
{
	int retval;

	// 대기 소켓 생성
	listenSock = socket(AF_INET, SOCK_STREAM, 0);
	if (listenSock == INVALID_SOCKET)
	{
		DisplayMessage();
		return false;
	}

	// 넌블로킹 소켓으로 바꾼다.
	u_long on = 1;
	retval = ioctlsocket(listenSock, FIONBIO, &on);
	if (retval == SOCKET_ERROR)
	{
		DisplayMessage();
		return false;
	}

	// 대기 소켓의 로컬 주소, 포트 설정
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(9000);
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	retval = bind(listenSock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR)
	{
		DisplayMessage();
		return false;
	}

	// 대기 소켓의 접속 대기 큐 생성 및 클라이언트 접속 대기
	retval = listen(listenSock, SOMAXCONN);
	if (retval == SOCKET_ERROR)
	{
		DisplayMessage();
		return false;
	}

	return true;
}
// 소켓 정보를 추가한다.
BOOL AddSocketInfo(SOCKET sock)
{
	// FD_SETSIZE - 연결 대기 소켓
	if (nTotalSockets >= (FD_SETSIZE - 1)) {
		printf("[오류] 소켓 정보를 추가할 수 없습니다!\n");
		return FALSE;
	}

	SOCKETINFO* ptr = new SOCKETINFO;
	if (ptr == NULL) {
		printf("[오류] 메모리가 부족합니다!\n");
		return FALSE;
	}

	ptr->sock = sock;
	ptr->recvbytes = 0;
	ptr->sendbytes = 0;
	SocketInfoArray[nTotalSockets++] = ptr;

	return TRUE;
}
// 소켓 정보를 삭제한다.
void RemoveSocketInfo(int nIndex)
{
	SOCKETINFO* ptr = SocketInfoArray[nIndex];

	// 클라이언트 정보 얻기
	SOCKADDR_IN clientaddr;
	int addrlen = sizeof(clientaddr);
	getpeername(ptr->sock, (SOCKADDR*)&clientaddr, &addrlen);
	printf("[TCP 서버] 클라이언트 종료: IP 주소=%s, 포트 번호=%d\n",
		inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));

	closesocket(ptr->sock);
	delete ptr;

	for (int i = nIndex; i < nTotalSockets; i++) {
		SocketInfoArray[i] = SocketInfoArray[i + 1];
	}
	nTotalSockets--;
}

unsigned int WINAPI WorkerThread(void* pParam)
{
	int retval;
	FD_SET rset;
	FD_SET wset;
	SOCKET clientSock;
	SOCKADDR_IN clientaddr;
	int addrlen;

	while (1)
	{
		// 소켓 셋 초기화
		// 1. 소켓 셋 초기화 
		FD_ZERO(&rset);
		FD_ZERO(&wset);

		// 2. 소켓을 적절한 방에 저장 : FD_SET
		FD_SET(listenSock, &rset); //대기 소켓을 rset에 등록
		for (int i = 0; i < nTotalSockets; i++)
		{
			if (SocketInfoArray[i]->recvbytes >
				SocketInfoArray[i]->sendbytes)
				FD_SET(SocketInfoArray[i]->sock, &wset);
			else
				FD_SET(SocketInfoArray[i]->sock, &rset); //
		}

		// select() 대기
		retval = select(0, &rset, &wset, NULL, NULL);
		if (retval == SOCKET_ERROR)
		{
			DisplayMessage();
			break;
		}

		// 대기 소켓에 클라이언트 접속이 요청되었나?	(accept 여기서)
		if (FD_ISSET(listenSock, &rset)) {
			addrlen = sizeof(clientaddr);
			clientSock = accept(listenSock, (SOCKADDR*)&clientaddr,
				&addrlen);
			if (clientSock == INVALID_SOCKET) {
				if (WSAGetLastError() != WSAEWOULDBLOCK)
					DisplayMessage();
			}
			else {
				printf("\n[TCP 서버] 클라이언트 접속: IP 주소=%s, 포트 번호 = % d\n",
					inet_ntoa(clientaddr.sin_addr),
					ntohs(clientaddr.sin_port));
				// 소켓 정보 추가
				if (AddSocketInfo(clientSock) == FALSE) {
					printf("[TCP 서버] 클라이언트 접속을 해제합니다!\n");
						closesocket(clientSock);
				}
			}
		}

		// 데이터 송,수신 가능한가?
		for (int i = 0; i < nTotalSockets; i++)
		{
			SOCKETINFO* ptr = SocketInfoArray[i];
			// 데이터가 수신 버퍼에 도착했다. 
			if (FD_ISSET(ptr->sock, &rset)) {
				retval = recv(ptr->sock, ptr->buf, BUFFERSIZE, 0);
				if (retval == SOCKET_ERROR) {
					if (WSAGetLastError() != WSAEWOULDBLOCK) {
						DisplayMessage();
						RemoveSocketInfo(i);
					}
					continue;
				}
				else if (retval == 0) {
					RemoveSocketInfo(i);
					continue;
				}
				ptr->recvbytes = retval;
				// 받은 데이터 출력
				addrlen = sizeof(clientaddr);
				getpeername(ptr->sock, (SOCKADDR*)&clientaddr,
					&addrlen);
				ptr->buf[retval] = '\0';

				printf("[TCP 서버] IP=%s, Port=%d의 메시지:%s\n",
					inet_ntoa(clientaddr.sin_addr),
					ntohs(clientaddr.sin_port),
					ptr->buf);
			}
			// 송신 버퍼 여기 공간이 남아 데이터를 보낼 준비가 되었다.
			if (FD_ISSET(ptr->sock, &wset)) {
				retval = send(ptr->sock, ptr->buf + ptr->sendbytes,
					ptr->recvbytes - ptr->sendbytes, 0);
				if (retval == SOCKET_ERROR) {
					if (WSAGetLastError() != WSAEWOULDBLOCK) {
						DisplayMessage();
						RemoveSocketInfo(i);
					}
					continue;
				}
				ptr->sendbytes += retval;
				if (ptr->recvbytes == ptr->sendbytes) {
					ptr->recvbytes = ptr->sendbytes = 0;
				}
			}
		}

	}
	// 대기 소켓 닫기
	closesocket(listenSock);

	return 0;
}
int main(int argc, char* argv[])
{
	WSADATA wsa;


	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		printf("윈도우 소켓 초기화 실패!\n");
		return -1;
	}

	// 대기 소켓 초기화(socket()+bind()+listen())
	if (!CreateListenSocket())
	{
		printf("대기 소켓 생성 실패!\n");
		return -1;
	}

	// 대기 쓰레드 종료를 기다림.
	unsigned int threadID;
	WaitForSingleObject((HANDLE)_beginthreadex(0, 0, WorkerThread, 0, 0, &threadID), INFINITE);

	WSACleanup();
	return 0;
}
