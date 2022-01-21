#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma comment(lib,"Ws2_32.lib")

#include <winsock2.h>
#include <process.h>
#include <stdio.h>

#define BUFSIZE 1024

// ���� ���� ������ ���� ����ü
struct SOCKETINFO
{
	SOCKET sock;
	char buf[BUFSIZE];
	int recvbytes;
	int sendbytes;
};

SOCKET listenSock; //��� ���� �ڵ�
int nTotalSockets = 0; //Ŀ�� �̺�Ʈ Ȯ���� ���� ���� ����
// ���� ���� ���� �迭
SOCKETINFO* SocketInfoArray[WSA_MAXIMUM_WAIT_EVENTS];
// ���� ������ ����Ǵ� Ŀ�� �̺�Ʈ �迭
WSAEVENT EventArray[WSA_MAXIMUM_WAIT_EVENTS];

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
// ���� ������ �߰��Ѵ�.
BOOL AddSocketInfo(SOCKET sock)
{
	if (nTotalSockets >= WSA_MAXIMUM_WAIT_EVENTS) {
		printf("[����] ���� ������ �߰��� �� �����ϴ�!\n");
		return FALSE;
	}

	SOCKETINFO* ptr = new SOCKETINFO;
	if (ptr == NULL) {
		printf("[����] �޸𸮰� �����մϴ�!\n");
		return FALSE;
	}

	WSAEVENT hEvent = WSACreateEvent();
	if (hEvent == WSA_INVALID_EVENT) {
		DisplayMessage();
		return FALSE;
	}

	ptr->sock = sock;
	ptr->recvbytes = 0;
	ptr->sendbytes = 0;
	SocketInfoArray[nTotalSockets] = ptr;
	EventArray[nTotalSockets] = hEvent;
	nTotalSockets++;

	return TRUE;
}

// ���� ���� ����
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
	WSACloseEvent(EventArray[nIndex]);

	for (int i = nIndex; i < nTotalSockets; i++) {
		SocketInfoArray[i] = SocketInfoArray[i + 1];
		EventArray[i] = EventArray[i + 1];
	}
	nTotalSockets--;
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

	// ��� ������ SocketInfoArray�� �߰��Ѵ�.
	if (AddSocketInfo(listenSock) == FALSE)
		return false;

	// ��� ���ϰ� Ŀ�� �̺�Ʈ ��ü�� �����Ѵ�.
	// FD_ACCEPT�� FD_CLOSE ��Ʈ��ũ �̺�Ʈ�� �߻��ϸ� Ŀ�� �̺�Ʈ ��ü�� ��ȣ���°� �ȴ�.
	retval = WSAEventSelect(listenSock, EventArray[nTotalSockets - 1],
		FD_ACCEPT | FD_CLOSE);
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
unsigned int WINAPI WorkerThread(void* pParam)
{
	int retval;
	int index;
	WSANETWORKEVENTS NetworkEvents;
	SOCKET client_sock;
	SOCKADDR_IN clientaddr;
	int addrlen;

	while (1) {
		// �̺�Ʈ ��ü�� ��ȣ���°� �� ������ ����Ѵ�.
		index = WSAWaitForMultipleEvents(nTotalSockets, EventArray,
			FALSE, WSA_INFINITE, FALSE);
		if (index == WSA_WAIT_FAILED) {
			DisplayMessage();
			continue;
		}
		index -= WSA_WAIT_EVENT_0;

		// ��Ʈ��ũ �̺�Ʈ�� �߻��� ������ ������ Ȯ���Ѵ�.
		// ��ȣ ������ ���� �̺�Ʈ�� ���ȣ ���·� �����Ѵ�.
		retval = WSAEnumNetworkEvents(SocketInfoArray[index]->sock,
			EventArray[index], &NetworkEvents);
		if (retval == SOCKET_ERROR) {
			DisplayMessage();
			continue;
		}

		// FD_ACCEPT �̺�Ʈ ó���� ó���Ѵ�.
		if (NetworkEvents.lNetworkEvents & FD_ACCEPT) {
			if (NetworkEvents.iErrorCode[FD_ACCEPT_BIT] != 0) {
				DisplayMessage();
				continue;
			}

			addrlen = sizeof(clientaddr);
			client_sock = accept(SocketInfoArray[index]->sock,
				(SOCKADDR*)&clientaddr, &addrlen);
			if (client_sock == INVALID_SOCKET) {
				DisplayMessage();
				continue;
			}
			printf("[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
				inet_ntoa(clientaddr.sin_addr),
				ntohs(clientaddr.sin_port));

			if (nTotalSockets >= WSA_MAXIMUM_WAIT_EVENTS) {
				printf("[����] �� �̻� ������ �޾Ƶ��� �� �����ϴ�!\n");
				closesocket(client_sock);
				continue;
			}

			if (AddSocketInfo(client_sock) == FALSE)
				continue;

			retval = WSAEventSelect(client_sock,
				EventArray[nTotalSockets - 1],
				FD_READ | FD_WRITE | FD_CLOSE);
			if (retval == SOCKET_ERROR)
			{
				DisplayMessage();
				break;
			}
		}

		// FD_READ, FD_WRITE �̺�Ʈ ó���� ó���Ѵ�.
		if (NetworkEvents.lNetworkEvents & FD_READ
			|| NetworkEvents.lNetworkEvents & FD_WRITE)
		{
			if (NetworkEvents.lNetworkEvents & FD_READ
				&& NetworkEvents.iErrorCode[FD_READ_BIT] != 0)
			{
				DisplayMessage();
				continue;
			}
			if (NetworkEvents.lNetworkEvents & FD_WRITE
				&& NetworkEvents.iErrorCode[FD_WRITE_BIT] != 0)
			{
				DisplayMessage();
				continue;
			}

			SOCKETINFO* ptr = SocketInfoArray[index];

			if (ptr->recvbytes == 0) {
				// ������ �ޱ�
				retval = recv(ptr->sock, ptr->buf, BUFSIZE, 0);
				if (retval == SOCKET_ERROR) {
					if (WSAGetLastError() != WSAEWOULDBLOCK) {
						DisplayMessage();
						RemoveSocketInfo(index);
					}
					continue;
				}
				ptr->recvbytes = retval;
				// ���� ������ ���
				ptr->buf[retval] = '\0';
				addrlen = sizeof(clientaddr);
				getpeername(ptr->sock, (SOCKADDR*)&clientaddr,
					&addrlen);
				printf("[TCP/%s:%d] %s\n",
					inet_ntoa(clientaddr.sin_addr),
					ntohs(clientaddr.sin_port), ptr->buf);
			}

			if (ptr->recvbytes > ptr->sendbytes) {
				// ������ ������
				retval = send(ptr->sock, ptr->buf + ptr->sendbytes,
					ptr->recvbytes - ptr->sendbytes, 0);
				if (retval == SOCKET_ERROR) {
					if (WSAGetLastError() != WSAEWOULDBLOCK) {
						DisplayMessage();
						RemoveSocketInfo(index);
					}
					continue;
				}
				ptr->sendbytes += retval;
				// ���� �����͸� ��� ���´��� üũ
				if (ptr->recvbytes == ptr->sendbytes)
					ptr->recvbytes = ptr->sendbytes = 0;
			}
		}

		// FD_CLOSE �̺�Ʈ ó���� ó���Ѵ�.
		if (NetworkEvents.lNetworkEvents & FD_CLOSE) {
			if (NetworkEvents.iErrorCode[FD_CLOSE_BIT] != 0)
				DisplayMessage();
			RemoveSocketInfo(index);
		}
	}

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
	WaitForSingleObject((HANDLE)_beginthreadex(0, 0, WorkerThread, 0, 0,
		&threadID), INFINITE);

	WSACleanup();
	return 0;
}
