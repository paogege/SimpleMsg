
#include "SimpleMsg.h"


extern "C" void gotMsg(char* msg)
{
	printf("got msg:%s\n", msg);
}

int main(int argc, char* argv[])
{
#ifdef USECLASS
	SimpleMsg msgr(MsgrType::SVR, MSGPORT);
	msgr.setHandler(gotMsg);
	char buf[1000] = { 0 };
	while (msgr.available())
	{
		fgets(buf, sizeof(buf), stdin);
		msgr.sendMsg(buf);
		_sleep(0);
	}
#else
	auto msgr = createMessager(MsgrType::SVR, MSGPORT);
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
