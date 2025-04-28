

#include "SimpleMsg.h"

void gotMsg(const std::string & msg)
{
	printf("got msg:%s\n", msg.c_str());
}

int main(int argc, char* argv[])
{
	// 初始化套接字动态库
	SimpleMsg msgr(MsgrType::SVR, 9988);
	//msgr.setHandler(gotMsg);
	char buf[1000] = { 0 };
	while (msgr.available())
	{
		std::string rm;
		while (msgr.recvMsg(rm) <= 0)
		{
			_sleep(0);
		}
		gotMsg(rm);
		fgets(buf, sizeof(buf), stdin);
		msgr.sendMsg(buf);
		_sleep(0);
	}
	return 0;
}
