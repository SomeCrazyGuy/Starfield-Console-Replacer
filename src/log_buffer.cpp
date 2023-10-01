#include "log_buffer.h"

#include <cstring>

#include <string>
#include <vector>


struct LogBuffer {
	std::string buffer;
	std::vector<uint32_t> lines;
	const char* name{nullptr};
	FILE* logfile{nullptr};
};


static std::vector<LogBuffer> Logs{};


static LogBufferHandle LogBufferCreate(const char* name, const char* path) {
	LogBuffer log;
	log.name = name;
	log.logfile = NULL;
	if (path) {
		fopen_s(&log.logfile, path, "ab");
	}
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
	if (l.logfile) {
		fputs(line, l.logfile);
		fputc('\n', l.logfile);
	}
}


static void LogBufferSave(LogBufferHandle handle, const char* filename) {
	FILE* f = nullptr;
	fopen_s(&f, filename, "wb");
	if (f == nullptr) return;

	for (uint32_t line = 0; line < LogBufferGetLineCount(handle); ++line) {
		fputs(LogBufferGetLine(handle, line), f);
		fputc('\n', f);
	}
	fclose(f);
}


// close the file handle for a logbuffer
static void LogBufferCloseFile(LogBufferHandle handle) {
	auto& l = Logs.at(handle);
	if (!l.logfile) return;
	fclose(l.logfile);
	l.logfile = NULL;
}


static LogBufferHandle LogBufferRestore(const char* name, const char* filename) {
	auto ret = LogBufferCreate(name, NULL);

	char max_line[4096];
	FILE* f = nullptr;

	fopen_s(&f, filename, "r+b");
	if (f == nullptr) return LogBufferCreate(name, filename);

	while(fgets(max_line, sizeof(max_line), f)) {
		auto end = strlen(max_line);
		--end;
		if (max_line[end] == '\n') {
			max_line[end] = '\0';
		}
		LogBufferAppend(ret, max_line);
	}
	
	// quick hack to add a file* to a log
	Logs[ret].logfile = f;

	return ret;
}


static constexpr struct log_buffer_api_t LogBufferAPI {
	&LogBufferCreate,
	&LogBufferGetName,
	&LogBufferGetSize,
	&LogBufferGetLineCount,
	&LogBufferGetLine,
	&LogBufferClear,
	&LogBufferAppend,
	&LogBufferSave,
	&LogBufferRestore,
	&LogBufferCloseFile
};


extern constexpr const struct log_buffer_api_t* GetLogBufferAPI() {
	return &LogBufferAPI;
}