
#include "SimpleMsg.h"

void gotMsg(const std::string & msg)
{
	printf("got msg:%s", msg.c_str());
}

int main(int argc, char* argv[])
{
	SimpleMsg msgr(MsgrType::CLN, 9988);
	msgr.setHandler(gotMsg);
	char buf[1000] = { 0 };
	while (msgr.available())
	{
		fgets(buf, sizeof(buf), stdin);
		msgr.sendMsg(buf);
		_sleep(0);
	}
	return 0;
}
