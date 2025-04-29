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
	//�������������ַ���
	int sendMsg(const std::string& msg);
	 //���ý����ַ����Ĵ���ص�
	void setHandler(msgHandler mh);
	//���������ӽ����б����ȡ��ǰ�˵��ַ��������ص�ǰ�����б��ȣ�����Ѿ����ûص��򷵻�-1
	int recvMsg(std::string& msg);
	//�������Ƿ���Ч
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