#ifndef SHARE_MEM_CP_H
#define SHARE_MEM_CP_H

#define BUFSIZE 1024

class SvrMem
{

public:
	SvrMem(char * memName = nullptr);
	~SvrMem();
	void writeContent(char* content);

private:
	char memFile[256] = { 0 };
#ifdef _WIN32
	void* hdr1 = nullptr;
	void* hdr2 = nullptr;
	void* lp = nullptr;
	unsigned long buff_size = BUFSIZE;
	
#elif __linux
	void* p = nullptr;
	int fd = 0;
#endif

};

class ClnMem
{
public:
	ClnMem(char* memName = nullptr);
	~ClnMem();
	char* readContent();
	
#ifdef _WIN32
	void* hdr = nullptr;
	void* lp = nullptr;
	unsigned long buff_size = BUFSIZE;
#elif __linux
	void* p = nullptr;
	int fd = 0;
#endif
};

#endif
