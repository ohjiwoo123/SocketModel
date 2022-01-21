#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma comment(lib,"Ws2_32.lib")

#include <winsock2.h>
#include <stdio.h>

#define BUFSIZE 1024
#define WM_SOCKET (WM_USER+1)

// 소켓 정보 저장을 위한 구조체
struct SOCKETINFO
{
	SOCKET sock;
	char buf[BUFSIZE];
	int recvbytes;
	int sendbytes;
	BOOL recvdelayed;
};

HWND hWnd; //윈도우의 핸들
SOCKET listenSock; //대기 소켓 핸들
int nTotalSockets = 0; // 통신 소켓의 개수
SOCKETINFO* SocketInfoArray[FD_SETSIZE];

// 윈도우 프로시저
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
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

	//대기 소켓과 사용자 정의 메시지 WM_SOCKET을 연결한다.
	//넌블로킹 소켓으로 변경하는 일도 같이 한다.
	retval = WSAAsyncSelect(listenSock, hWnd,
		WM_SOCKET, FD_ACCEPT | FD_CLOSE);
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

int GetSocketInfo(SOCKET sock)
{
	for (int i = 0; i < nTotalSockets; ++i)
	{
		if (SocketInfoArray[i]->sock == sock)
			return i;
	}

	return -1; //클라이언트에서 오류 처리 해야 함.
}

int main(int argc, char* argv[])
{
	// 윈도우 클래스 등록
	WNDCLASS wndclass;
	wndclass.cbClsExtra = 0;
	wndclass.cbWndExtra = 0;
	wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wndclass.hInstance = NULL;
	wndclass.lpfnWndProc = (WNDPROC)WndProc;
	wndclass.lpszClassName = "MyWindowClass";
	wndclass.lpszMenuName = NULL;
	wndclass.style = CS_HREDRAW | CS_VREDRAW;
	if (!RegisterClass(&wndclass)) return -1;

	// 윈도우 생성
	hWnd = CreateWindow("MyWindowClass", "TCP 서버",
		WS_OVERLAPPEDWINDOW, 0, 0, 600, 300,
		NULL, (HMENU)NULL, NULL, NULL);
	if (hWnd == NULL) return -1;
	ShowWindow(hWnd, SW_SHOWNORMAL);
	UpdateWindow(hWnd);

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return -1;

	// 대기 소켓 초기화(socket()+bind()+listen())
	if (!CreateListenSocket())
	{
		printf("대기 소켓 생성 실패!\n");
		return -1;
	}

	// 메시지 루프
	MSG msg;
	while (GetMessage(&msg, 0, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// 윈속 종료
	WSACleanup();
	return msg.wParam;
}

// 윈도우 메시지 처리
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg,
	WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_SOCKET: // 소켓 관련 윈도우 메시지
	{
		// 데이터 통신에 사용할 변수
		SOCKETINFO* ptr;
		SOCKET client_sock;
		SOCKADDR_IN clientaddr;
		int addrlen;
		int retval;

		// 오류 발생 여부 확인
		if (WSAGETSELECTERROR(lParam)) {
			RemoveSocketInfo(GetSocketInfo(wParam));
			break;
		}

		// 메시지 처리
		switch (WSAGETSELECTEVENT(lParam)) {
		case FD_ACCEPT:
			addrlen = sizeof(clientaddr);
			client_sock = accept(wParam, (SOCKADDR*)&clientaddr,
				&addrlen);
			if (client_sock == INVALID_SOCKET) {
				if (WSAGetLastError() != WSAEWOULDBLOCK)
					DisplayMessage();
				break;
			}
			printf("[TCP 서버] 클라이언트 접속: IP 주소=%s, 포트 번호=%d\n",
				inet_ntoa(clientaddr.sin_addr),
				ntohs(clientaddr.sin_port));
			AddSocketInfo(client_sock);
			retval = WSAAsyncSelect(client_sock, hWnd,
				WM_SOCKET, FD_READ | FD_WRITE | FD_CLOSE);
			if (retval == SOCKET_ERROR) {
				DisplayMessage();
				RemoveSocketInfo(GetSocketInfo(client_sock));
			}
			break;
		case FD_READ:
			ptr = SocketInfoArray[GetSocketInfo(wParam)];
			if (ptr->recvbytes > 0) {
				ptr->recvdelayed = TRUE;
				break;
			}
			// 데이터 받기
			retval = recv(ptr->sock, ptr->buf, BUFSIZE, 0);
			if (retval == SOCKET_ERROR) {
				if (WSAGetLastError() != WSAEWOULDBLOCK) {
					DisplayMessage();
					RemoveSocketInfo(GetSocketInfo(wParam));
				}
				break;
			}
			ptr->recvbytes = retval;
			// 받은 데이터 출력
			ptr->buf[retval] = '\0';
			addrlen = sizeof(clientaddr);
			getpeername(wParam, (SOCKADDR*)&clientaddr, &addrlen);
			printf("[TCP/%s:%d] %s\n", inet_ntoa(clientaddr.sin_addr),
				ntohs(clientaddr.sin_port), ptr->buf);
		case FD_WRITE:
			ptr = SocketInfoArray[GetSocketInfo(wParam)];
			if (ptr->recvbytes <= ptr->sendbytes)
				break;
			// 데이터 보내기
			retval = send(ptr->sock, ptr->buf + ptr->sendbytes,
				ptr->recvbytes - ptr->sendbytes, 0);
			if (retval == SOCKET_ERROR) {
				if (WSAGetLastError() != WSAEWOULDBLOCK) {
					DisplayMessage();
					RemoveSocketInfo(wParam);
				}
				break;
			}
			ptr->sendbytes += retval;
			// 받은 데이터를 모두 보냈는지 체크
			if (ptr->recvbytes == ptr->sendbytes) {
				ptr->recvbytes = ptr->sendbytes = 0;
				if (ptr->recvdelayed) {
					ptr->recvdelayed = FALSE;
					PostMessage(hWnd, WM_SOCKET, wParam,
						FD_READ);
				}
			}
			break;
		case FD_CLOSE:
			RemoveSocketInfo(GetSocketInfo(wParam));
			break;
		}
	}
	return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
