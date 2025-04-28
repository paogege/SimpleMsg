因为进程间的通信需要考虑到阻塞以及线程同步问题，处理起来稍微复杂，所以整理了一个简单的本地进程间的字符串消息通信类，屏蔽前述复杂细节。
Due to the need to consider blocking and thread synchronization issues in inter process communication, it is slightly more complex to handle. Therefore, a simple local inter process string message communication class has been compiled to mask the aforementioned complex details.
目前Socket部分仍然只支持Windows操作系统，未来有时间考虑兼容Linux.
At present, the Socket part still only supports the Windows operating system, and in the future, we will consider compatibility with Linux.
