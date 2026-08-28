#include <string>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <malloc.h>
#include <math.h>
#include <thread>
#include <iostream>
#include <sys/stat.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <iostream>
#include <locale>
#include <string>
#include <codecvt>
#include <dlfcn.h>


using namespace std;

typedef long ADDRESS;
typedef char PACKAGENAME;
typedef unsigned short UTF16;
typedef char UTF8;


// syscall内存读写
#if defined(__arm__)
int process_vm_readv_syscall = 376;
int process_vm_writev_syscall = 377;
#elif defined(__aarch64__)
int process_vm_readv_syscall = 270;
int process_vm_writev_syscall = 271;
#elif defined(__i386__)
int process_vm_readv_syscall = 347;
int process_vm_writev_syscall = 348;
#else
int process_vm_readv_syscall = 310;
int process_vm_writev_syscall = 311;
#endif

ssize_t process_v1(pid_t __pid, const struct iovec *__local_iov, unsigned long __local_iov_count,
				  const struct iovec *__remote_iov, unsigned long __remote_iov_count,
				  unsigned long __flags, bool iswrite)
{
	return syscall((iswrite ? process_vm_writev_syscall : process_vm_readv_syscall), __pid,
				   __local_iov, __local_iov_count, __remote_iov, __remote_iov_count, __flags);
}

// 进程读写内存
bool pvm1(void *address, void *buffer, size_t size, bool iswrite)
{
	struct iovec local[1];
	struct iovec remote[1];
	local[0].iov_base = buffer;
	local[0].iov_len = size;
	remote[0].iov_base = address;
	remote[0].iov_len = size;
	ssize_t bytes = process_v1(getpid(), local, 1, remote, 1, 0, iswrite);
	return bytes == size;
}

// 读取内存
bool vm_readv(long address, void *buffer, size_t size)
{
	return pvm1(reinterpret_cast < void *>(address), buffer, size, false);
}

// 写入内存
bool vm_writev(long address, void *buffer, size_t size)
{
	return pvm1(reinterpret_cast < void *>(address), buffer, size, true);
}

// 获取F类内存
float getFloat(long addr)
{
	float var = 0;
	vm_readv(addr, &var, 4);
	return (var);
}

// 获取
int getDword(long addr)
{
	int var = 0;
	vm_readv(addr, &var, 4);
	return (var);
}

// 获取
bool getbool(long addr)
{
	bool var = 0;
	vm_readv(addr, &var, 4);
	return (var);
}

// 获取指针
long getPointer(long addr)
{
	long var = 0;
	vm_readv(addr, &var, 4);
	return (var);
}

// 写入F类内存
void WriteFloat(long addr, float data)
{
	vm_writev(addr, &data, 4);
}

// 写入D类内存
void WriteDword(long addr, int data)
{
	vm_writev(addr, &data, 4);
}

// 获取进程
int getProcessID(const char *packageName)
{
	int id = -1;
	DIR *dir;
	FILE *fp;
	char filename[64];
	char cmdline[64];
	struct dirent *entry;
	dir = opendir("/proc");
	while ((entry = readdir(dir)) != NULL)
	{
		id = atoi(entry->d_name);
		if (id != 0)
		{
			sprintf(filename, "/proc/%d/cmdline", id);
			fp = fopen(filename, "r");
			if (fp)
			{
				fgets(cmdline, sizeof(cmdline), fp);
				fclose(fp);
				if (strcmp(packageName, cmdline) == 0)
				{
					return id;
				}
			}
		}
	}
	closedir(dir);
	return -1;
}

// 获取基址
long int get_module_base(const char *module_name)
{
	FILE *fp;
	long addr = 0;
	char *pch;
	char filename[64];
	char line[1024];
	snprintf(filename, sizeof(filename), "/proc/%d/maps", getpid());
	fp = fopen(filename, "r");
	if (fp != NULL)
	{
		while (fgets(line, sizeof(line), fp))
		{
			if (strstr(line, module_name))
			{
				pch = strtok(line, "-");
				addr = strtoul(pch, NULL, 16);
				if (addr == 0x8000)
					addr = 0;
				break;
			}
		}
		fclose(fp);
	}
	return addr;
}

// 读取字符信息
void getUTF8(char * buf, long namepy)
{
	UTF16 buf16[16] = { 0 };
	vm_readv(namepy, buf16, 32);
	UTF16 *pTempUTF16 = buf16;
	UTF8 *pTempUTF8 = buf;
	UTF8 *pUTF8End = pTempUTF8 + 32;
	while (pTempUTF16 < pTempUTF16 + 28)
	{
		if (*pTempUTF16 <= 0x007F && pTempUTF8 + 1 < pUTF8End)
		{
			*pTempUTF8++ = (UTF8) * pTempUTF16;
		}
		else if (*pTempUTF16 >= 0x0080 && *pTempUTF16 <= 0x07FF && pTempUTF8 + 2 < pUTF8End)
		{
			*pTempUTF8++ = (*pTempUTF16 >> 6) | 0xC0;
			*pTempUTF8++ = (*pTempUTF16 & 0x3F) | 0x80;
		}
		else if (*pTempUTF16 >= 0x0800 && *pTempUTF16 <= 0xFFFF && pTempUTF8 + 3 < pUTF8End)
		{
			*pTempUTF8++ = (*pTempUTF16 >> 12) | 0xE0;
			*pTempUTF8++ = ((*pTempUTF16 >> 6) & 0x3F) | 0x80;
			*pTempUTF8++ = (*pTempUTF16 & 0x3F) | 0x80;
		}
		else
		{
			break;
		}
		pTempUTF16++;
	}
}


