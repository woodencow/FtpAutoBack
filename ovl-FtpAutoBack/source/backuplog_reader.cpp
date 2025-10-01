#include "backuplog_reader.h"
#include <cstring>
#include <algorithm>

// 静态常量定义
const char* BackuplogReader::LOG_FILE_PATH = "/AutoBack/backuplog.txt";

BackuplogReader::BackuplogReader() {
    // 构造函数
}

BackuplogReader::~BackuplogReader() {
    // 析构函数
}

std::vector<BackupLogEntry> BackuplogReader::read_last_entries(int max_entries) {
    std::vector<BackupLogEntry> result;
    
    // 打开文件
    FILE* fp = fopen(LOG_FILE_PATH, "rb");
    if (!fp) {
        return result;  // 返回空向量
    }
    
    // 获取文件大小
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return result;  // 返回空向量
    }
    
    long file_size = ftell(fp);
    if (file_size <= 0) {
        // 空文件或获取大小失败
        fclose(fp);
        return result;  // 返回空向量
    }
    
    // 查找第max_entries个换行符的位置
    long start_offset = find_nth_newline_from_end(fp, max_entries);
    if (start_offset < 0) {
        // 文件行数不足max_entries，从文件开头读取
        start_offset = 0;
    }
    
    // 从找到的位置开始解析条目内容
    parse_entries_from_offset(fp, start_offset, result);
    
    fclose(fp);

    // 逆转顺序，使最后一行变成第一个
    std::reverse(result.begin(), result.end());
    
    return result;
}

long BackuplogReader::find_nth_newline_from_end(FILE* fp, int n) {
    // 获取文件大小
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    
    if (file_size <= 0) {
        return -1;
    }
    
    char buffer[SCAN_BUFFER_SIZE];
    long current_pos = file_size;
    int newline_count = 0;
    
    // 从文件末尾开始，分块向前扫描
    while (current_pos > 0 && newline_count < n) {
        // 计算本次读取的大小
        long read_size = std::min((long)SCAN_BUFFER_SIZE, current_pos);
        current_pos -= read_size;
        
        // 定位到读取位置
        if (fseek(fp, current_pos, SEEK_SET) != 0) {
            return -1;
        }
        
        // 读取数据
        size_t bytes_read = fread(buffer, 1, read_size, fp);
        if (bytes_read != (size_t)read_size) {
            return -1;
        }
        
        // 从后向前扫描换行符
        for (long i = bytes_read - 1; i >= 0; i--) {
            if (buffer[i] == '\n') {
                newline_count++;
                if (newline_count == n) {
                    // 找到第n个换行符，返回其后一个字符的位置
                    return current_pos + i + 1;
                }
            }
        }
    }
    
    // 没有找到足够的换行符，返回-1表示从文件开头读取
    return -1;
}

void BackuplogReader::parse_entries_from_offset(FILE* fp, long start_offset, std::vector<BackupLogEntry>& result) {
    // 定位到开始位置
    if (fseek(fp, start_offset, SEEK_SET) != 0) {
        return;  // 失败直接返回
    }
    
    std::string current_line;
    int ch;
    
    // 逐字符读取，构建行内容
    while ((ch = fgetc(fp)) != EOF) {
        if (ch == '\n') {
            // 遇到换行符，处理当前行
            clean_line_ending(current_line);
            
            // 解析当前行为结构体
            BackupLogEntry entry;
            if (parse_log_line(current_line, entry)) {
                result.push_back(entry);
            }
            
            current_line.clear();
        } else if (ch != '\r') {
            // 忽略\r字符，添加其他字符到当前行
            if (current_line.length() < MAX_LINE_LENGTH - 1) {
                current_line += (char)ch;
            }
            // 如果行太长，忽略超出部分
        }
    }
    
    // 处理文件末尾没有换行符的情况
    if (!current_line.empty()) {
        clean_line_ending(current_line);
        BackupLogEntry entry;
        if (parse_log_line(current_line, entry)) {
            result.push_back(entry);
        }
    }
}

bool BackuplogReader::parse_log_line(const std::string& line, BackupLogEntry& entry) {
    // 初始化结构体所有字段为空字符串
    memset(&entry, 0, sizeof(BackupLogEntry));
    
    // 查找分隔符位置
    size_t pos1 = line.find('|');
    if (pos1 == std::string::npos) return false;
    
    size_t pos2 = line.find('|', pos1 + 1);
    if (pos2 == std::string::npos) return false;
    
    size_t pos3 = line.find('|', pos2 + 1);
    if (pos3 == std::string::npos) return false;

    // 提取各个字段
    strncpy(entry.result_L, line.c_str(), pos1);
    entry.result_L[pos1] = '\0';

    // 然后根据result_L字段设置result_S
    if (strcmp(entry.result_L, "本地备份成功") == 0 || strcmp(entry.result_L, "上传备份失败，仅备份至本地") == 0)
        strcpy(entry.result_S, "仅备份");
    else if (strcmp(entry.result_L, "本地备份失败") == 0)
        strcpy(entry.result_S, "备份失败");
    else if (strcmp(entry.result_L, "上传备份成功") == 0)
        strcpy(entry.result_S, "已上传");
    else strcpy(entry.result_S, "未知");
    
    strncpy(entry.game_name, line.c_str() + pos1 + 1, pos2 - pos1 - 1);
    entry.game_name[pos2 - pos1 - 1] = '\0';
    
    strncpy(entry.username, line.c_str() + pos2 + 1, pos3 - pos2 - 1);
    entry.username[pos3 - pos2 - 1] = '\0';
    
    // 处理timestamp字段，用@分割日期和时间
    const char* timestamp_start = line.c_str() + pos3 + 1;
    const char* at_pos = strchr(timestamp_start, '@');
    
    strncpy(entry.date, timestamp_start, at_pos - timestamp_start);
    entry.date[at_pos - timestamp_start] = '\0';
    
    strcpy(entry.time, at_pos + 1);
    
    return true;
}

void BackuplogReader::clean_line_ending(std::string& line) {
    // 去除行尾的\r和\n字符
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
        line.pop_back();
    }
}