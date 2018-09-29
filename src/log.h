#ifndef LOG_H
#define LOG_H
#include <vector>
#include <string>

// Singleton logger
class Log {
public:
	static void write(const char *fmt, ...);
private:
	static Log& instance();
	std::vector<std::string> m_lines;
};
#endif