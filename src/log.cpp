#include <stdarg.h>
#include <stdio.h>

#include "log.h"

void Log::write(const char *fmt, ...) {
	// calculate length of formatting
	va_list va;
	va_start(va, fmt);
	const int length = vsnprintf(nullptr, 0, fmt, va);
	va_end(va);

	// allocate and format into buffer
	va_start(va, fmt);
	std::string buffer;
	buffer.resize(length + 1, 0);
	vsnprintf(&buffer[0], buffer.size(), fmt, va);
	va_end(va);

	// move string into line
	instance().m_lines.push_back(std::move(buffer));

	printf("%s\n", instance().m_lines.back().c_str());
}

Log& Log::instance() {
	static Log log;
	return log;
}