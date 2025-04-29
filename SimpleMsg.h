#include <mutex>
#include <list>
#include <string>
#ifdef WIN32
#define STDCALL_ __stdcall
#else
#define STDCALL_ __attribute__((__stdcall__))
#endif


enum class MsgrType 
{
	SVR,
	CLN,
};

typedef void(*msgHandler)(const std::string & recvMsg);

class SimpleMsg
{
public :
	SimpleMsg(MsgrType mt, int port);
	~SimpleMsg();
	//非阻塞，发送字符串
	int sendMsg(const std::string& msg);
	 //设置接收字符串的处理回调
	void setHandler(msgHandler mh);
	//非阻塞，从接收列表里获取最前端的字符串，返回当前接收列表长度，如果已经设置回调则返回-1
	int recvMsg(std::string& msg);
	//返回类是否有效
	bool available();
private:
	unsigned int STDCALL_ Rcv(void* lpParam);
	unsigned int STDCALL_ Snd(void* lpParam);
	void svrWorkerThread(void* lpParam);
	void clnWorkerThread(void* lpParam);
	bool m_inited = false;
	bool m_over = false;
	int m_port = 0;
	bool m_serror = false;
	mutable std::mutex m_sendMutex;
	mutable std::mutex m_recvMutex;
	std::list <std::string> m_sendList;
	std::list <std::string> m_recvList;
	MsgrType m_type;
	msgHandler m_hdr = 0;
};