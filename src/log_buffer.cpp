#include "log_buffer.h"

#include <cstring>

#include <string>
#include <vector>


struct LogBuffer {
	std::string buffer;
	std::vector<uint32_t> lines;
	const char *name;
};


static std::vector<LogBuffer> Logs;


static LogBufferHandle LogBufferCreate(const char* name) {
	LogBuffer log;
	log.name = name;
	auto ret = Logs.size();
	Logs.push_back(log);
	return (LogBufferHandle)ret;
}


static const char* LogBufferGetName(LogBufferHandle handle) {
	return Logs.at(handle).name;
}


static uint32_t LogBufferGetSize(LogBufferHandle handle) {
	return (uint32_t)Logs.at(handle).buffer.size();
}


static uint32_t LogBufferGetLineCount(LogBufferHandle handle) {
	return (uint32_t)Logs.at(handle).lines.size();
}


static const char* LogBufferGetLine(LogBufferHandle handle, uint32_t line) {
	const auto& l = Logs.at(handle);
	const auto offset = l.lines.at(line);
	const auto len = l.buffer.size();
	if (len <= offset) return "";
	return &l.buffer[offset];
}


static void LogBufferClear(LogBufferHandle handle) {
	auto& log = Logs.at(handle);
	log.lines.clear();
	log.buffer.clear();
}


static void LogBufferAppend(LogBufferHandle handle, const char* line) {
	auto& l = Logs.at(handle);
	auto offset = l.buffer.size();
	l.buffer.append(line);
	l.buffer += '\0';
	l.lines.push_back((uint32_t)offset);
}


extern const struct log_buffer_api_t* GetLogBufferAPI() {
	static struct log_buffer_api_t ret;
	static bool once = false;
	if (!once) {
		once = true;
		ret.Create = &LogBufferCreate;
		ret.GetName = &LogBufferGetName;
		ret.GetSize = &LogBufferGetSize;
		ret.GetLineCount = &LogBufferGetLineCount;
		ret.GetLine = &LogBufferGetLine;
		ret.Clear = &LogBufferClear;
		ret.Append = &LogBufferAppend;
	}
	return &ret;
}

