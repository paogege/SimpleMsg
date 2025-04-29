#include <stdio.h>
#include <thread>
#ifdef WIN32
#include <Winsock2.h> //Socket的函数调用　
#include <windows.h>
#pragma comment (lib, "ws2_32")     // 使用WINSOCK2.H时，则需要库文件WS2_32.LIB
#else
#endif
#include "SimpleMsg.h"

#define BUF_SIZE 6400  //  缓冲区大小
#define LIST_SIZE 300

SimpleMsg::SimpleMsg(MsgrType mt, int port)
{
#ifdef WIN32
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		printf("winsock load faild!\n");
		return ;
	}
#endif

	m_port = port;
	m_type = mt;

	if (mt == MsgrType::SVR)
	{
		//  创建服务段套接字
		SOCKET sServer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sServer == INVALID_SOCKET) {
			printf("socket faild!\n");
			WSACleanup();
			return;
		}

		//  服务端地址
		sockaddr_in addrServ;

		addrServ.sin_family = AF_INET;
		addrServ.sin_port = htons(m_port);
		addrServ.sin_addr.s_addr = htonl(INADDR_ANY);

		//  绑定套接字
		if (bind(sServer, (const struct sockaddr*)&addrServ, sizeof(addrServ)) == SOCKET_ERROR) {
			printf("bind faild!\n");
			closesocket(sServer);
			WSACleanup();
			return;
		}

		std::thread wthrd(&SimpleMsg::svrWorkerThread, this, (void*)&sServer);
		wthrd.detach();
		m_inited = true;
	}

	else if (mt == MsgrType::CLN)
	{
		//  服务器套接字
		SOCKET sHost = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sHost == INVALID_SOCKET) {
			printf("socket faild!\n");
			WSACleanup();
		}

		std::thread wthrd(&SimpleMsg::clnWorkerThread, this, (void*)&sHost);
		wthrd.detach();
		m_inited = true;
	}
}

SimpleMsg::~SimpleMsg()
{
	bool waitSend = true;
	while(waitSend)
	{
		waitSend = false;
		std::lock_guard<std::mutex> lock(m_sendMutex);
		if (m_sendList.size())
		{
			waitSend = true;
			continue;
		}
	}
	m_over = true;
#ifdef WIN32
	WSACleanup();
#endif
}

int SimpleMsg::sendMsg(const std::string& msg)
{
	if (!m_inited || m_over)return 0;
	std::lock_guard<std::mutex> lock(m_sendMutex);
	while (m_sendList.size() >= LIST_SIZE)
	{
		m_sendList.pop_front();
	}
	m_sendList.push_back(msg);
	return m_sendList.size();
}

void SimpleMsg::setHandler(msgHandler mh)
{
	m_hdr = mh;
}

int SimpleMsg::recvMsg(std::string& msg)
{
	if (!m_inited || m_over)return 0;
	if (m_hdr)
	{
		return -1;
	}
	else
	{
		std::lock_guard<std::mutex> lock(m_recvMutex);
		int ret = m_recvList.size();
		if (m_recvList.size() )
		{
			msg = m_recvList.front();
			m_recvList.pop_front();
		}
		
		return ret;
	}
}

bool SimpleMsg::available()
{
	bool ret = m_inited && ((m_type == MsgrType::CLN && !m_serror) || m_type == MsgrType::SVR);
	return ret;
}

unsigned int STDCALL_ SimpleMsg::Rcv(void* lpParam)
{
	SOCKET sHost = *(SOCKET*)lpParam;
	int retVal = -1;
	char bufRecv[BUF_SIZE];
	memset(bufRecv, 0, sizeof(bufRecv));
	while (1)
	{
		retVal = recv(sHost, bufRecv, BUF_SIZE, 0);
		if (retVal == -1 || retVal == 0) {
			printf("recive faild!\n");
			m_serror = true;
			break;
		}
		else {
			//printf("收到服务器消息：%s\n", bufRecv);
			if (m_hdr)
			{
				m_hdr(bufRecv);
			}
			else
			{
				std::lock_guard<std::mutex> lock(m_recvMutex);
				while (m_recvList.size() >= LIST_SIZE)
				{
					m_recvList.pop_front();
				}
				m_recvList.push_back(bufRecv);
			}
		}
	}
	return 0;
}

unsigned int STDCALL_ SimpleMsg::Snd(void* lpParam)
{
	SOCKET sHost = *(SOCKET*)lpParam;
	int retVal = -1;
	while (1)
	{
		std::lock_guard<std::mutex> lock(m_sendMutex);
		if (m_sendList.size())
		{
			std::string smsg;
			smsg = m_sendList.front();
			m_sendList.pop_front();
			retVal = send(sHost,smsg.c_str() , strlen(smsg.c_str()) + sizeof(char), 0);
			if (retVal == -1) {
				printf("send faild!\n");
				m_serror = true;
				break;
			}
		}		
		Sleep(0);
		if (m_over)return 0;
	}
	return 0;
}

void SimpleMsg::svrWorkerThread(void* lpParam)
{
	SOCKET sServer = *(SOCKET*)lpParam;
	if (listen(sServer, 5) == -1) {
		printf("listen faild!\n");
		closesocket(sServer);
		WSACleanup();
		return;
	}

	SOCKET sClient; //  客户端套接字

	sockaddr_in addrClient;
	int addrClientLen = sizeof(addrClient);	
	

	
	while (1)
	{
		sClient = accept(sServer, (sockaddr FAR*)&addrClient, &addrClientLen);
		if (sClient == INVALID_SOCKET) {
			printf("accept faild!\n");
			closesocket(sServer);
			WSACleanup();
			return;
		}
		std::thread t1(&SimpleMsg::Snd, this, (void**)&sClient);
		t1.detach();
		std::thread t2(&SimpleMsg::Rcv, this, (void**)&sClient);
		t2.join();
	}
	


	closesocket(sClient);
	WSACleanup(); // 资源释放
}

void SimpleMsg::clnWorkerThread(void* lpParam)
{
	SOCKET sHost = *(SOCKET*)lpParam;
	SOCKADDR_IN servAddr;
	servAddr.sin_family = AF_INET;
	//  注意   当把客户端程序发到别人的电脑时 此处IP需改为服务器所在电脑的IP
	servAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	servAddr.sin_port = htons(m_port);

	//  连接服务器
	if (connect(sHost, (LPSOCKADDR)&servAddr, sizeof(servAddr)) == SOCKET_ERROR) {
		printf("connect faild!\n");
		closesocket(sHost);
		WSACleanup();
	}

	while (1)
	{
		std::thread t1(&SimpleMsg::Snd, this, (void**)&sHost);
		t1.detach();
		std::thread t2(&SimpleMsg::Rcv, this, (void**)&sHost);
		t2.join();
		if (m_serror)
		{
			printf("SOCKET is invalid.");
			break;
		}
	}


	closesocket(sHost);
	WSACleanup();
}
