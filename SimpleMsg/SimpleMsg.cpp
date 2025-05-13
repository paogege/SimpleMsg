#include <stdio.h>
#include <thread>

#include "SimpleMsg.h"

#define BUF_SIZE 6400  //  缓冲区大小
#define LIST_SIZE 300

#ifdef WIN32
#include <Winsock2.h> //Socket的函数调用　
#include <windows.h>
#define _SOCKET SOCKET
#define _CLOSESOCKET closesocket
#define _SOCKETCLN	WSACleanup()
#define _SLEEP Sleep
#define _SOCKADDR_IN SOCKADDR_IN
#define _INVALID_SOCKET INVALID_SOCKET
#define _SOCKET_ERROR SOCKET_ERROR
#pragma comment (lib, "ws2_32")     // 使用WINSOCK2.H时，则需要库文件WS2_32.LIB
#else
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#define _SOCKET int
#define _CLOSESOCKET close
#define _SOCKETCLN	;
#define _SLEEP sleep
#define _SOCKADDR_IN sockaddr_in
#define _INVALID_SOCKET -1
#define _SOCKET_ERROR -1
#endif

// 获取当前时间的字符串表示
std::string getCurrentTime() {
	std::time_t now = std::time(nullptr);
	char buf[80];
	std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
	return std::string(buf);
}

// 写日志函数
void writeLog(const std::string& message, const std::string& logFile) {
	std::ofstream logStream(logFile, std::ios::app); // 以追加模式打开文件
	if (logStream.is_open()) {
		logStream << "[" << getCurrentTime() << "] " << message << std::endl;
		logStream.close();
	}
	else {
		std::cerr << "无法打开日志文件: " << logFile << std::endl;
	}
}

SimpleMsg::SimpleMsg(MsgrType mt, int port)
{
#ifdef WIN32
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		printf("winsock load faild!\n");
		return;
	}
#endif

	m_port = port;
	m_type = mt;

	if (mt == MsgrType::SVR)
	{
		//  创建服务段套接字
		_SOCKET sServer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sServer == _INVALID_SOCKET) {
			printf("socket faild!\n");
			_SOCKETCLN;
			return;
		}
		m_localSocket = sServer;

		//  服务端地址
		sockaddr_in addrServ;

		addrServ.sin_family = AF_INET;
		addrServ.sin_port = htons(m_port);
		addrServ.sin_addr.s_addr = htonl(INADDR_ANY);

		//  绑定套接字
		if (bind(sServer, (const struct sockaddr*)&addrServ, sizeof(addrServ)) == _SOCKET_ERROR) {
			printf("bind faild!\n");
			_CLOSESOCKET(sServer);
			_SOCKETCLN;
			return;
		}
		m_inited = true;
		std::thread wthrd(&SimpleMsg::svrWorkerThread, this, (void*)&sServer);
		wthrd.detach();
		
	}

	else if (mt == MsgrType::CLN)
	{
		//  服务器套接字
		_SOCKET sHost = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sHost == _INVALID_SOCKET) {
			printf("socket faild!\n");
			_SOCKETCLN;
		}
		m_localSocket = sHost;
		m_inited = true;
		std::thread wthrd(&SimpleMsg::clnWorkerThread, this, (void*)&sHost);
		wthrd.detach();
		
	}
}

SimpleMsg::~SimpleMsg()
{
	bool waitSend = true;
	while (waitSend)
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
	_CLOSESOCKET(m_localSocket);
	_SOCKETCLN;
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
		if (m_recvList.size())
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
	_SOCKET sHost = *(_SOCKET*)lpParam;
	int retVal = -1;
	unsigned char dataSize[4] = { 0 };
	while (1)
	{
		int remainSize = 4;
		while (remainSize > 0)
		{			
#ifdef _DEBUG
			std::string msg;
			if (m_type == MsgrType::SVR)
			{
				msg = "SVR before receive";
			}
			else
			{
				msg = "CLN before receive";
			}
			writeLog(msg, "svr.log");
#endif
			retVal = recv(sHost, (char*)dataSize, remainSize, 0);
			if (retVal == -1 || retVal == 0) {
				printf("recive faild!\n");
#ifdef _DEBUG
				writeLog("receive error", "svr.log");
#endif
				m_serror = true;
				break;
			}
#ifdef _DEBUG
			if (m_type == MsgrType::SVR)
			{
				msg = "SVR after received";
			}
			else
			{
				msg = "CLN after received";
			}
			writeLog(msg, "svr.log");
#endif
			remainSize -= retVal;
		}
		if (m_serror)
		{
			break;
		}

		auto iSize = *(int*)dataSize;
		remainSize = iSize;

		auto bufRecv = new char[remainSize + 1];
		while (remainSize > 0)
		{
			retVal = recv(sHost, bufRecv + (iSize - remainSize), remainSize, 0);
			if (retVal == -1 || retVal == 0) {
				printf("recive faild!\n");
				m_serror = true;
				break;
			}
			remainSize -= retVal;
		}

		if (m_serror)
		{
			break;
		}


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
	return 0;
}

unsigned int STDCALL_ SimpleMsg::Snd(void* lpParam)
{
	_SOCKET sHost = *(_SOCKET*)lpParam;
	int retVal = -1;
	while (1)
	{
		std::lock_guard<std::mutex> lock(m_sendMutex);
		if (m_sendList.size())
		{
			std::string smsg;
			smsg = m_sendList.front();
			m_sendList.pop_front();

			unsigned int msgSize = strlen(smsg.c_str()) + sizeof(char);
			int remainSize = 4;
			while (remainSize > 0)
			{
				retVal = send(sHost, (char*)&msgSize + (4 - remainSize), remainSize, 0);
				_SLEEP(0);
				if (retVal == -1) {
					printf("send faild!\n");
					m_serror = true;
					break;
				}
				remainSize -= retVal;
			}
			if (m_serror)break;

			remainSize = msgSize;
			while (remainSize > 0)
			{
				retVal = send(sHost, smsg.c_str() + (msgSize - remainSize), remainSize, 0);
				_SLEEP(0);
				remainSize -= retVal;
				if (retVal == -1) {
					printf("send faild!\n");
					m_serror = true;
					break;
				}
			}
			if (m_serror)break;
		}
		_SLEEP(0);
		if (m_over)return 0;
	}
	return 0;
}

void SimpleMsg::svrWorkerThread(void* lpParam)
{
	_SOCKET sServer = *(_SOCKET*)lpParam;
	if (listen(sServer, 5) == -1) {
		printf("listen faild!\n");
		_CLOSESOCKET(sServer);
		_SOCKETCLN;
		return;
	}

	_SOCKET sClient; //  客户端套接字

	sockaddr_in addrClient;
#ifdef WIN32
	int addrClientLen = sizeof(addrClient);
#else 
	socklen_t addrClientLen = sizeof(addrClient);
#endif
	
	while (1)
	{
#ifdef WIN32
		sClient = accept(sServer, (sockaddr FAR*)&addrClient, &addrClientLen);
#else
		sClient = accept(sServer, (struct sockaddr*)&addrClient, &addrClientLen);
#endif
		if (sClient == _INVALID_SOCKET) {
			printf("accept faild!\n");
			_CLOSESOCKET(sServer);
			_SOCKETCLN;
			return;
		}
		m_serror = false;
		std::thread t1(&SimpleMsg::Snd, this, (void**)&sClient);
		t1.detach();
		std::thread t2(&SimpleMsg::Rcv, this, (void**)&sClient);
		t2.join();
	}



	_CLOSESOCKET(sClient);
	_SOCKETCLN; // 资源释放
}

void SimpleMsg::clnWorkerThread(void* lpParam)
{
	_SOCKET sHost = *(_SOCKET*)lpParam;
	_SOCKADDR_IN servAddr;
	servAddr.sin_family = AF_INET;
	//  注意   当把客户端程序发到别人的电脑时 此处IP需改为服务器所在电脑的IP
#ifdef WIN32
	servAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
#else
	servAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
#endif
	servAddr.sin_port = htons(m_port);

	//  连接服务器
	if (connect(sHost, (sockaddr*)&servAddr, sizeof(servAddr)) == _SOCKET_ERROR) {
		printf("connect faild!\n");
		_CLOSESOCKET(sHost);
		_SOCKETCLN;
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


	_CLOSESOCKET(sHost);
	_SOCKETCLN;
}

Messager createMessager(MsgrType mt, int port)
{
	return new SimpleMsg(mt,port);
}

void destroyMessager(Messager handle)
{
	delete static_cast<SimpleMsg*>(handle);
}

int MessagerSend(Messager handle, char * pString, int size)
{
	return static_cast<SimpleMsg*>(handle)->sendMsg(pString);
}

int MessagerRecv(Messager handle, char * pString, int size)
{
	std::string buf;
	int ret = static_cast<SimpleMsg*>(handle)->recvMsg(buf);
	if (ret >= 0 && ret >= size + 1)
	{
		strcpy_s(pString, buf.size() + 1, buf.c_str());
		return ret;
	}
	else
	{
		return ret;
	}
}

bool isMessagerAvailabe(Messager handle)
{
	return static_cast<SimpleMsg*>(handle)->available();
}

void setMessagerReceiver(Messager handle, msgHandler mh)
{
	static_cast<SimpleMsg*>(handle)->setHandler(mh);
}
