/*
*	�����Լ�
*	1) �ɵ� : socket, bind, listen , connect, send, CloseHandle
*	2) ���� : accept, recv
* --> accept, recv �Լ� ������ ��������, 1�� �� ����� �Ҹ�Ť� 
* --> ������(Thread)�� ó���ؾ���
* --> �������� �Լ����� "�̸� ȣ��" �߱� �����̴�.
* -----------------------------------------
* [] ���� ����� �𵨵�..]
* 
 1) ���� ��Ʈ��ũ �̺�Ʈ�� ����
 2) �ش� �̺�Ʈ�� ó���� �����Լ��� "�� �Ŀ� ȣ����

 ex) ���ŷ ����
 -> ���� ��Ʈ��ũ �̺�Ʈ �߻���
 -> recv() ȣ�� --> �ȱ�ٸ��� �ٷ� ������
ex) ���ŷ ����
-> ���� ��Ʈ��ũ �̺�Ʈ �߻���
-> recv() ȣ�� --> �����ؼ� �ٷ� ���ƿ� 
 */
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma comment(lib,"Ws2_32.lib")

#include <winsock2.h>
#include <process.h>
#include <stdio.h>

#define BUFFERSIZE 1024
// ���� ���� ������ ���� ����ü

struct SOCKETINFO
{
	SOCKET sock;
	char buf[BUFFERSIZE];
	int recvbytes;
	int sendbytes;
};

SOCKET listenSock; //��� ���� �ڵ�
int nTotalSockets = 0; // ��� ������ ����
SOCKETINFO* SocketInfoArray[FD_SETSIZE];

// ���� �Լ� ���� ���
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

	// ��� ���� ����
	listenSock = socket(AF_INET, SOCK_STREAM, 0);
	if (listenSock == INVALID_SOCKET)
	{
		DisplayMessage();
		return false;
	}

	// �ͺ��ŷ �������� �ٲ۴�.
	u_long on = 1;
	retval = ioctlsocket(listenSock, FIONBIO, &on);
	if (retval == SOCKET_ERROR)
	{
		DisplayMessage();
		return false;
	}

	// ��� ������ ���� �ּ�, ��Ʈ ����
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

	// ��� ������ ���� ��� ť ���� �� Ŭ���̾�Ʈ ���� ���
	retval = listen(listenSock, SOMAXCONN);
	if (retval == SOCKET_ERROR)
	{
		DisplayMessage();
		return false;
	}

	return true;
}
// ���� ������ �߰��Ѵ�.
BOOL AddSocketInfo(SOCKET sock)
{
	// FD_SETSIZE - ���� ��� ����
	if (nTotalSockets >= (FD_SETSIZE - 1)) {
		printf("[����] ���� ������ �߰��� �� �����ϴ�!\n");
		return FALSE;
	}

	SOCKETINFO* ptr = new SOCKETINFO;
	if (ptr == NULL) {
		printf("[����] �޸𸮰� �����մϴ�!\n");
		return FALSE;
	}

	ptr->sock = sock;
	ptr->recvbytes = 0;
	ptr->sendbytes = 0;
	SocketInfoArray[nTotalSockets++] = ptr;

	return TRUE;
}
// ���� ������ �����Ѵ�.
void RemoveSocketInfo(int nIndex)
{
	SOCKETINFO* ptr = SocketInfoArray[nIndex];

	// Ŭ���̾�Ʈ ���� ���
	SOCKADDR_IN clientaddr;
	int addrlen = sizeof(clientaddr);
	getpeername(ptr->sock, (SOCKADDR*)&clientaddr, &addrlen);
	printf("[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
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
		// ���� �� �ʱ�ȭ
		// 1. ���� �� �ʱ�ȭ 
		FD_ZERO(&rset);
		FD_ZERO(&wset);

		// 2. ������ ������ �濡 ���� : FD_SET
		FD_SET(listenSock, &rset); //��� ������ rset�� ���
		for (int i = 0; i < nTotalSockets; i++)
		{
			if (SocketInfoArray[i]->recvbytes >
				SocketInfoArray[i]->sendbytes)
				FD_SET(SocketInfoArray[i]->sock, &wset);
			else
				FD_SET(SocketInfoArray[i]->sock, &rset); //
		}

		// select() ���
		retval = select(0, &rset, &wset, NULL, NULL);
		if (retval == SOCKET_ERROR)
		{
			DisplayMessage();
			break;
		}

		// ��� ���Ͽ� Ŭ���̾�Ʈ ������ ��û�Ǿ���?	(accept ���⼭)
		if (FD_ISSET(listenSock, &rset)) {
			addrlen = sizeof(clientaddr);
			clientSock = accept(listenSock, (SOCKADDR*)&clientaddr,
				&addrlen);
			if (clientSock == INVALID_SOCKET) {
				if (WSAGetLastError() != WSAEWOULDBLOCK)
					DisplayMessage();
			}
			else {
				printf("\n[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ = % d\n",
					inet_ntoa(clientaddr.sin_addr),
					ntohs(clientaddr.sin_port));
				// ���� ���� �߰�
				if (AddSocketInfo(clientSock) == FALSE) {
					printf("[TCP ����] Ŭ���̾�Ʈ ������ �����մϴ�!\n");
						closesocket(clientSock);
				}
			}
		}

		// ������ ��,���� �����Ѱ�?
		for (int i = 0; i < nTotalSockets; i++)
		{
			SOCKETINFO* ptr = SocketInfoArray[i];
			// �����Ͱ� ���� ���ۿ� �����ߴ�. 
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
				// ���� ������ ���
				addrlen = sizeof(clientaddr);
				getpeername(ptr->sock, (SOCKADDR*)&clientaddr,
					&addrlen);
				ptr->buf[retval] = '\0';

				printf("[TCP ����] IP=%s, Port=%d�� �޽���:%s\n",
					inet_ntoa(clientaddr.sin_addr),
					ntohs(clientaddr.sin_port),
					ptr->buf);
			}
			// �۽� ���� ���� ������ ���� �����͸� ���� �غ� �Ǿ���.
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
	// ��� ���� �ݱ�
	closesocket(listenSock);

	return 0;
}
int main(int argc, char* argv[])
{
	WSADATA wsa;


	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		printf("������ ���� �ʱ�ȭ ����!\n");
		return -1;
	}

	// ��� ���� �ʱ�ȭ(socket()+bind()+listen())
	if (!CreateListenSocket())
	{
		printf("��� ���� ���� ����!\n");
		return -1;
	}

	// ��� ������ ���Ḧ ��ٸ�.
	unsigned int threadID;
	WaitForSingleObject((HANDLE)_beginthreadex(0, 0, WorkerThread, 0, 0, &threadID), INFINITE);

	WSACleanup();
	return 0;
}
