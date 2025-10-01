#pragma once

#include <vector>
#include <string>
#include <cstdio>

/**
 * 备份日志条目结构体
 * 格式：结果|游戏名|用户名|时间戳
 */
struct BackupLogEntry {
    char result_S[24];      // 短结果字段
    char result_L[45];      // 长结果字段
    char game_name[64];   // 游戏名字段  
    char username[24];    // 用户名字段
    char date[16];        // 日期字段
    char time[16];        // 时间字段
};

/**
 * 备份日志读取器
 * 用于读取 /AutoBack/backuplog.txt 文件的最后 N 行并解析为结构体
 */
class BackuplogReader {
public:
    // 构造函数
    BackuplogReader();
    
    // 析构函数
    ~BackuplogReader();
    
    /**
     * 读取日志文件的最后 N 个条目
     * @param max_entries 最大条目数，默认30个
     * @return 包含最后N个日志条目的向量，失败返回空向量
     */
    std::vector<BackupLogEntry> read_last_entries(int max_entries = 30);

private:
    static const char* LOG_FILE_PATH;  // 日志文件路径常量
    static const int MAX_LINE_LENGTH = 128;  // 每行最大长度
    static const int SCAN_BUFFER_SIZE = 1024;  // 扫描缓冲区大小
    
    /**
     * 从文件末尾开始查找第N个换行符的位置
     * @param fp 文件指针
     * @param n 要查找的换行符数量
     * @return 第N个换行符的文件偏移位置，-1表示失败
     */
    long find_nth_newline_from_end(FILE* fp, int n);
    
    /**
     * 从指定偏移位置开始解析行内容为结构体
     * @param fp 文件指针
     * @param start_offset 开始读取的文件偏移
     * @param result 输出结果向量
     */
    void parse_entries_from_offset(FILE* fp, long start_offset, std::vector<BackupLogEntry>& result);
    
    /**
     * 解析单行日志为结构体
     * @param line 日志行内容
     * @param entry 输出的日志条目结构体
     * @return 解析成功返回true，失败返回false
     */
    bool parse_log_line(const std::string& line, BackupLogEntry& entry);
    
    /**
     * 清理行内容（去除行尾的\r\n字符）
     * @param line 要清理的行内容
     */
    void clean_line_ending(std::string& line);
};