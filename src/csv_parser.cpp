#include "main.h"
#include "csv_parser.h"

#include <Windows.h>

#include <vector>


static bool FileReadAll(const char* filename, char** out_buffer, uint32_t* out_size) {
        char path[MAX_PATH];
        GetPathInDllDir(path, filename);

        DEBUG("Filename: %s", path);
        const auto hfile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hfile == INVALID_HANDLE_VALUE) {
                return NULL;
        }

        const auto file_size = GetFileSize(hfile, NULL);
        if (file_size == INVALID_FILE_SIZE) {
                return NULL;
        }

        const auto allocation_size  = (file_size + 4096) & ~DWORD{4095};
        const auto file_buffer = (char*)malloc(allocation_size);
        if (file_buffer == NULL) {
                return NULL;
        }
        file_buffer[file_size] = '\0';

        if (!ReadFile(hfile, file_buffer, file_size, NULL, NULL)) {
                free(file_buffer);
                return NULL;
        }

        CloseHandle(hfile);

        *out_buffer = file_buffer;
        *out_size = file_size;

        return true;
}


struct CSVFile {
        char* file_buffer;
        uint32_t* cells;

        uint32_t buffer_size;
        uint32_t line_count;
        uint32_t column_count;
        uint32_t cell_count;
};


static CSVFile* CSV_Load(const char* filename) {
        CSVFile *ret = (CSVFile*) malloc(sizeof(*ret));
        ASSERT(ret != NULL);
        memset(ret, 0, sizeof(*ret));

        if (!FileReadAll(filename, &ret->file_buffer, &ret->buffer_size)) {
                free(ret);
                return NULL;
        }

        uint32_t lines = 0;
        uint32_t columns = 0;
        bool in_quotes = false;
        bool cell_start = true;
        const auto start = ret->file_buffer;
        const auto end = ret->file_buffer + ret->buffer_size;
        char* csv = ret->file_buffer;
        std::vector<uint32_t> cells;
        while (csv < end) {
                if (*csv == '\r' && !in_quotes) {
                        if (*(csv + 1) == '\n') {
                                //DEBUG("unquoted CRLF");
                                *csv = '\0';
                                ++csv;
                        }
                        else {
                                //DEBUG("unquoted CR");
                        }
                }

                if (*csv == '\n') {
                        if (!in_quotes) {
                                *csv = '\0';
                                if (columns) {
                                        ++lines;
                                        ++columns;
                                        if (!ret->column_count) {
                                                ret->column_count = columns;
                                        }
                                        //DEBUG("unquoted newline with %u columns", columns);
                                        if (columns != ret->column_count) {
                                                DEBUG("Column count mismatch: %u != %u", columns, ret->column_count);
                                                goto PARSE_ERROR;
                                        }
                                        columns = 0;
                                        cell_start = true;
                                }
                                else {
                                        //DEBUG("Unquoted newline with no columns");
                                }
                        }
                        else {
                                //DEBUG("Quoted newline");
                        }
                }
                else if (cell_start) {
                        const auto pos = (uint32_t)(csv - start);
                        //DEBUG("Cell Start at %u (%c)", pos, *csv);
                        cell_start = false;
                        cells.push_back(pos);
                }
                else if (*csv == ',') {
                        if (!in_quotes) {
                                //DEBUG("Unquoted comma");
                                *csv = '\0';
                                ++columns;
                                cell_start = true;
                        }
                        else {
                                //DEBUG("Quoted Comma");
                        }
                }
                else if (*csv == '"') {
                        if (in_quotes) {
                                if (*(csv + 1) == '"') {
                                        //DEBUG("Escaped quote");
                                        ++csv;
                                }
                                else {
                                        //DEBUG("Closing quote");
                                        in_quotes = false;
                                }
                        }
                        else {
                                //DEBUG("Starting Quote");
                                in_quotes = true;
                        }
                }
                else {
                        if (in_quotes) {
                                //DEBUG("in_quotes: (%c)", *csv);
                        }
                        else {
                                //DEBUG("Other Unquoted: (%c)", *csv);
                        }
                }

                ++csv;
        }
        if (in_quotes) {
                DEBUG("Unterminated quote");
                goto PARSE_ERROR;
        }

        // handle if the file did not end in a newline
        if (columns) {
                ++lines;
        }

        ret->line_count = lines;
        ret->cell_count = lines * ret->column_count;
        ASSERT(cells.size() == ret->cell_count);
        ret->cells = (uint32_t*)malloc(ret->cell_count * sizeof(uint32_t));
        ASSERT(ret->cells != NULL);
        memcpy(ret->cells, &cells[0], ret->cell_count * sizeof(uint32_t));

        return ret;

PARSE_ERROR:
        free(ret->file_buffer);
        memset(csv, 0, sizeof(*csv));
        free(csv);
        return NULL;
}


static void CSV_Close(CSVFile* csv) {
        free(csv->file_buffer);
        free(csv->cells);
        memset(csv, 0, sizeof(*csv));
        free(csv);
}


static void CSV_Info(const CSVFile* csv, uint32_t* out_line_count, uint32_t* out_column_count) {
        if (out_line_count) {
                *out_line_count = csv->line_count;
        }
        if (out_column_count) {
                *out_column_count = csv->column_count;
        }
}


static const char* CSV_ReadCell(const CSVFile* csv, uint32_t row, uint32_t column) {
        ASSERT(row < csv->line_count);
        ASSERT(column < csv->column_count);
        return &csv->file_buffer[csv->cells[row * csv->column_count + column]];
}


extern const struct csv_api_t* GetCSVAPI() {
        static struct csv_api_t CSVAPI = {
                CSV_Load,
                CSV_Close,
                CSV_Info,
                CSV_ReadCell,
        };
        return &CSVAPI;
}



