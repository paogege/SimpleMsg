#include "SimpleMsg.h"


extern "C" void gotMsg(char* msg)
{
	printf("got msg:%s", msg);
}

int main2(int argc, char* argv[])
{
	int * p;
	try {
		
		delete p;
		printf("hello");
		/*if (*p > 100)
		{
			printf("bigger than 100");
		}
		else
		{
			printf("smaller than 100");
		}*/
	}
	catch (...)
	{

	}
	system("pause");
	return 0;
}

int main(int argc, char* argv[])
{
#ifdef USECLASS
	SimpleMsg msgr(MsgrType::CLN, MSGPORT);
	msgr.setHandler(gotMsg);
	char buf[1000] = { 0 };
	while (msgr.available())
	{
		fgets(buf, sizeof(buf), stdin);
		msgr.sendMsg(buf);
		_sleep(0);
	}
#else
	auto msgr = createMessager(MsgrType::CLN, MSGPORT);
	setMessagerReceiver(msgr, gotMsg);
	char buf[1000] = { 0 };
	while (isMessagerAvailabe(msgr))
	{
		fgets(buf, sizeof(buf), stdin);
		MessagerSend(msgr, buf, strlen(buf));
		_sleep(0);
	}
#endif
	return 0;
}
