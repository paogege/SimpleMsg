#include <stdio.h>
#include <thread>
#include <cstring>

#include "SimpleMsg.h"

#define BUF_SIZE 6400  //  ��������С
#define LIST_SIZE 300

#ifdef WIN32
#include <Winsock2.h> //Socket�ĺ������á�
#include <windows.h>
#define _SOCKET SOCKET
#define _CLOSESOCKET closesocket
#define _SOCKETCLN	
//#define _SOCKETCLN	WSACleanup()
#define _SLEEP Sleep
#define _SOCKADDR_IN SOCKADDR_IN
#define _INVALID_SOCKET INVALID_SOCKET
#define _SOCKET_ERROR SOCKET_ERROR
#pragma comment (lib, "ws2_32")     // ʹ��WINSOCK2.Hʱ������Ҫ���ļ�WS2_32.LIB
#else
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#define _SOCKET int
#define _CLOSESOCKET close
#define _SOCKETCLN	;
#define _SLEEP sleep
#define _SOCKADDR_IN sockaddr_in
#define _INVALID_SOCKET -1
#define _SOCKET_ERROR -1
#endif

#include "shareMemCP.h"

// ��ȡ��ǰʱ����ַ�����ʾ
std::string getCurrentTime() {
	std::time_t now = std::time(nullptr);
	char buf[80];
	std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
	return std::string(buf);
}

// д��־����
void writeLog(const std::string& message, const std::string& logFile) {
	std::ofstream logStream(logFile, std::ios::app); // ��׷��ģʽ���ļ�
	if (logStream.is_open()) {
		logStream << "[" << getCurrentTime() << "] " << message << std::endl;
		logStream.close();
	}
	else {
		std::cerr << "�޷�����־�ļ�: " << logFile << std::endl;
	}
}






SimpleMsg::SimpleMsg(MsgrType mt)
{
#ifdef WIN32
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		//printf("winsock load faild!\n");
		return;
	}
#endif

	//m_port = port;
	m_type = mt;

	if (mt == MsgrType::SVR)
	{
		m_pMemCP = new SvrMem();
		std::string portStr;
		if (getAvailableListenPort(portStr))
		{
			m_pMemCP->writeContent((char*)portStr.c_str());
			m_port = atoi(portStr.c_str());
		}
		else
		{
			return;
		}



		//  ����������׽���
		_SOCKET sServer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sServer == _INVALID_SOCKET) {
			//printf("socket faild!\n");
			_SOCKETCLN;
			return;
		}
		m_localSocket = sServer;

		//  ����˵�ַ
		sockaddr_in addrServ;

		addrServ.sin_family = AF_INET;
		addrServ.sin_port = htons(m_port);
		addrServ.sin_addr.s_addr = htonl(INADDR_ANY);

		//  ���׽���
		if (bind(sServer, (const struct sockaddr*)&addrServ, sizeof(addrServ)) == _SOCKET_ERROR) {
			//printf("bind faild!\n");
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
		m_pClnMem = new ClnMem();
		char * portStr = m_pClnMem->readContent();
		

		m_port = atoi(portStr);

		delete m_pClnMem;
		m_pClnMem = nullptr;

		//  �������׽���
		_SOCKET sHost = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sHost == _INVALID_SOCKET) {
			//printf("socket faild!\n");
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
			_SLEEP(100);
			//continue;
		}
	}
	if (m_type == MsgrType::SVR)
	{
		delete m_pMemCP;
	}

	m_over = true;
	//use sleep to avoid sendind failures
	_SLEEP(300);
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
			std::string msgPref;
			std::string msg;
			if (m_type == MsgrType::SVR)
			{
				msgPref = "SVR ";
			}
			else
			{
				msgPref = "CLN ";
			}

			msg = msgPref + "before recv.";

			writeLog(msg, "svr.log");
#endif
			retVal = recv(sHost, (char*)dataSize, remainSize, 0);
			if (retVal == -1 || retVal == 0) {
				//printf("recive faild!\n");
#ifdef _DEBUG
				
				writeLog(msgPref + "receive faild", "svr.log");
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
				//printf("recive faild!\n");
				m_serror = true;
				break;
			}
			remainSize -= retVal;
		}

		if (m_serror)
		{
			break;
		}


		//printf("�յ���������Ϣ��%s\n", bufRecv);
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
					//printf("send faild!\n");
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
					//printf("send faild!\n");
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
		//printf("listen faild!\n");
		auto en = WSAGetLastError();
		_CLOSESOCKET(sServer);
		_SOCKETCLN;
		return;
	}

	_SOCKET sClient; //  �ͻ����׽���

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
			//printf("accept faild!\n");
			_CLOSESOCKET(sServer);
			_SOCKETCLN;
			return;
		}
		m_serror = false;
		auto newClient = sClient;
		std::thread t1(&SimpleMsg::Snd, this, (void**)&newClient);
		t1.detach();
		std::thread t2(&SimpleMsg::Rcv, this, (void**)&newClient);
		t2.detach();/**/
	}



	_CLOSESOCKET(sServer);
	_SOCKETCLN; // ��Դ�ͷ�
}

void SimpleMsg::clnWorkerThread(void* lpParam)
{
	_SOCKET sHost = *(_SOCKET*)lpParam;
	_SOCKADDR_IN servAddr;
	servAddr.sin_family = AF_INET;
	//  ע��   ���ѿͻ��˳��򷢵����˵ĵ���ʱ �˴�IP���Ϊ���������ڵ��Ե�IP
#ifdef WIN32
	servAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
#else
	servAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
#endif
	servAddr.sin_port = htons(m_port);

	//  ���ӷ�����
	if (connect(sHost, (sockaddr*)&servAddr, sizeof(servAddr)) == _SOCKET_ERROR) {
		//printf("connect faild!\n");
		auto en = WSAGetLastError();
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
			//printf("SOCKET is invalid.");
			break;
		}
	}


	_CLOSESOCKET(sHost);
	_SOCKETCLN;
}

bool SimpleMsg::getAvailableListenPort(std::string &port)
{
	bool result = true;

#ifdef WIN32
	WSADATA wsa;
	/*��ʼ��socket��Դ*/
	if (WSAStartup(MAKEWORD(1, 1), &wsa) != 0)
	{
		return false;   //����ʧ��
	}
#endif

	// 1. ����һ��socket
	int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// 2. ����һ��sockaddr���������Ķ˿ں���Ϊ0
	struct sockaddr_in addrto;
	memset(&addrto, 0, sizeof(struct sockaddr_in));
	addrto.sin_family = AF_INET;
	addrto.sin_addr.s_addr = inet_addr("127.0.0.1");
	addrto.sin_port = 0;

	// 3. ��
	int ret = ::bind(sock, (struct sockaddr *)&(addrto), sizeof(struct sockaddr_in));
	if (0 != ret)
	{
		return false;
	}

	// 4. ����getsockname��ȡ
	struct sockaddr_in connAddr;
	memset(&connAddr, 0, sizeof(struct sockaddr_in));
#ifdef WIN32
	int len = sizeof(connAddr);
#else
	unsigned int len = sizeof(connAddr);
#endif
	ret = ::getsockname(sock, (sockaddr*)&connAddr, &len);

	if (0 != ret)
	{
		return false;
	}

	port = std::to_string(ntohs(connAddr.sin_port)); // ��ȡ�˿ں�

#ifdef WIN32
	if (0 != closesocket(sock))
#else
	if (0 != close(sock))
#endif
	{
		result = false;
	}
#ifdef _WIN32
	WSACleanup();
#endif
	return result;
}

Messager createMessager(MsgrType mt)
{
	return new SimpleMsg(mt);
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
		 strncpy(pString,  buf.c_str(), buf.size() + 1);
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
