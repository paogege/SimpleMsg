#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#elif __linux
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#endif
#include "shareMemCP.h"

using namespace std;

#ifdef _WIN32
#define SMNAME "SMCPSimpleMsg"
#elif __linux
#define SMNAME "/home/SMCPSimpleMsg"
#endif







SvrMem::SvrMem(char* memName)
{
#ifdef _WIN32
	const char *shared_file_name = SMNAME;
	char* tarName = (char*)shared_file_name;
	if (memName)
	{
		tarName = memName;
	}
	strcpy(memFile, tarName);

	// create shared memory file
	hdr1 = CreateFile(tarName,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_ALWAYS, // open exist or create new, overwrite file
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (hdr1 == INVALID_HANDLE_VALUE)
		return;

	hdr2 = CreateFileMapping(
		hdr1, // Use paging file - shared memory
		NULL,                 // Default security attributes
		PAGE_READWRITE,       // Allow read and write access
		0,                    // High-order DWORD of file mapping max size
		buff_size,            // Low-order DWORD of file mapping max size
		tarName);    // Name of the file mapping object

	if (hdr2)
	{
		// map memory file view, get pointer to the shared memory
		lp = MapViewOfFile(
			hdr2,  // Handle of the map object
			FILE_MAP_ALL_ACCESS,  // Read and write access
			0,                    // High-order DWORD of the file offset
			0,                    // Low-order DWORD of the file offset
			buff_size);           // The number of bytes to map to view	
	}

#elif __linux

	const char *shared_file_name = SMNAME;	

	// create mmap file
	fd = open(shared_file_name, O_CREAT | O_RDWR | O_TRUNC, 00777);
	if (fd < 0)
	{
		std::cout << "fd open failed." << std::endl;
		return;
	}

	size_t write_size = BUFSIZE;
	ftruncate(fd, write_size); // extend file size
	// map memory to file
	p = mmap(NULL, write_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

#endif
}

SvrMem::~SvrMem()
{
#ifdef _WIN32
	UnmapViewOfFile(lp);
	CloseHandle(hdr2);
	CloseHandle(hdr1);
	std::remove(memFile);
#elif __linux
	munmap(p, BUFSIZE);
	close(fd);
#endif
}

void SvrMem::writeContent(char * content)
{
#ifdef _WIN32
	if (lp)
	{
		memcpy(lp, content, strlen(content) + 1);
		FlushViewOfFile(lp, buff_size); // can choose save to file or not
	}
#elif __linux
	if (p)
	{
		char buf[BUFSIZE] = { 0 };
		memcpy(buf, content, strlen(content));
		memcpy(p, buf, BUFSIZE);
	}
#endif
}



ClnMem::ClnMem(char* memName)
{
#ifdef _WIN32
	const char *shared_file_name = SMNAME;
	char* tarName = (char*)shared_file_name;
	if (memName)
	{
		tarName = memName;
	}

	// open shared memory file
	hdr = OpenFileMapping(
		FILE_MAP_ALL_ACCESS,
		NULL,
		tarName);

	if (hdr)
	{
		lp = MapViewOfFile(
			hdr,
			FILE_MAP_ALL_ACCESS,
			0,
			0,
			0);				
	}
#elif __linux
	const char *shared_file_name = SMNAME;

	// open mmap file
	int fd = open(shared_file_name, O_RDONLY, 00777);
	if (fd < 0)
		return;
	
	size_t read_size = BUFSIZE;
	// map file to memory
	p = mmap(NULL, read_size, PROT_READ, MAP_SHARED, fd, 0);
#endif
	
}

ClnMem::~ClnMem()
{
#ifdef _WIN32
	// close share memory file
	UnmapViewOfFile(lp);
	CloseHandle(hdr);
#elif __linux
	munmap(p, BUFSIZE);
	close(fd);
#endif
}

char * ClnMem::readContent()
{
#ifdef _WIN32	
	char *share_buffer = (char *)lp;
	return share_buffer;
#elif __linux
	char *share_buffer = (char *)p;
	return share_buffer;
#endif
}
