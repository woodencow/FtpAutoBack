# 设置CMake最低版本要求
cmake_minimum_required(VERSION 3.13)

# 定义项目版本号
set(FTPSRV_VERSION_MAJOR 1)  # 主版本号
set(FTPSRV_VERSION_MINOR 2)  # 次版本号
set(FTPSRV_VERSION_PATCH 2)  # 补丁版本号
set(FTPSRV_VERSION ${FTPSRV_VERSION_MAJOR}.${FTPSRV_VERSION_MINOR}.${FTPSRV_VERSION_PATCH})

# 设置GCC编译器警告标志
set(gcc_warning_flags
    -Wall      # 启用所有常见警告
    -Wextra    # 启用额外警告

    # 不确定是否应该默认启用这些，因为可能在编译器更新时导致构建失败
    # -Werror
    # -Wfatal-errors

    # 禁用符号比较警告，因为nx使用s64作为大小和偏移量，而STL使用size_t，会产生大量警告
    -Wno-sign-compare
    # 禁用未使用参数警告，因为许多重写的方法不使用参数
    -Wno-unused-parameter
    # 禁用缺少字段初始化器警告，缺少的字段会被设置为0
    -Wno-missing-field-initializers
    # 禁用格式截断警告，因为文件系统强制执行最大路径长度，不会溢出
    -Wno-format-truncation

    # 以下警告标志来自GBA模拟器项目，效果很好
    -Wformat-overflow=2      # 检查格式字符串溢出
    -Wundef                  # 检查未定义的宏
    -Wmissing-include-dirs   # 检查缺少的包含目录
    -fstrict-aliasing        # 启用严格别名优化
    -Wstrict-overflow=2      # 检查严格溢出
    -Walloca                 # 检查alloca使用
    -Wduplicated-cond        # 检查重复条件
    -Wwrite-strings          # 检查字符串写入
    -Wdate-time              # 检查日期时间宏
    -Wlogical-op             # 检查逻辑操作
    -Wpacked                 # 检查packed结构
    -Wcast-qual              # 检查类型转换限定符
    -Wcast-align             # 检查类型转换对齐
    -Wimplicit-fallthrough=5 # 检查隐式fallthrough
    -Wsuggest-final-types    # 建议final类型
    -Wuninitialized          # 检查未初始化变量
)

# 定义项目信息
project(ftpsrv
    LANGUAGES C                           # 使用C语言
    VERSION ${FTPSRV_VERSION}            # 项目版本
    DESCRIPTION "small and fast ftp server"  # 项目描述：小型快速FTP服务器
)

# 启用通过set()设置cmake选项的功能
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

# 导出编译命令到compile_commands.json文件（用于IDE和代码分析工具）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 启用链接时优化（LTO）
cmake_policy(SET CMP0069 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

# 注释：不再需要FetchContent，因为使用本地库
# include(FetchContent)
# set(FETCHCONTENT_QUIET FALSE)  # 显示下载进度

# 包含编译检查模块
include(CheckCSourceCompiles)  # 检查C源代码是否可编译
include(CheckSymbolExists)     # 检查符号是否存在

# 检查网络相关符号是否存在
check_symbol_exists(IPTOS_THROUGHPUT
    "netinet/ip.h"
    HAVE_IPTOS_THROUGHPUT  # 检查IP服务类型吞吐量标志
)

check_symbol_exists(TCP_NODELAY
    "netinet/tcp.h"
    HAVE_TCP_NODELAY       # 检查TCP无延迟选项
)

check_symbol_exists(SO_KEEPALIVE
    "sys/socket.h"
    HAVE_SO_KEEPALIVE      # 检查套接字保活选项
)

check_symbol_exists(SO_REUSEADDR
    "sys/socket.h"
    HAVE_SO_REUSEADDR      # 检查套接字地址重用选项
)

# 检查系统函数是否可用（仅保留Switch需要的）

check_c_source_compiles("
    #include <poll.h>
    int main(void) { poll(0, 0, 0); }"
HAVE_POLL)  # 检查poll函数（I/O多路复用）

check_c_source_compiles("
    #include <string.h>
    int main(void) { strncasecmp(0, 0, 0); }"
HAVE_STRNCASECMP)  # 检查strncasecmp函数（不区分大小写字符串比较）

check_c_source_compiles("
    #include <time.h>
    int main(void) { localtime_r(0, 0); }"
HAVE_LOCALTIME_R)  # 检查localtime_r函数（线程安全的本地时间转换）

check_c_source_compiles("
    #include <time.h>
    int main(void) { gmtime_r(0, 0); }"
HAVE_GMTIME_R)  # 检查gmtime_r函数（线程安全的GMT时间转换）

# 查找Git工具（用于获取版本信息）
find_package(Git REQUIRED)

# 获取Git提交哈希值（短格式）
execute_process(
    COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# 获取当前Git分支名
execute_process(
    COMMAND "${GIT_EXECUTABLE}" symbolic-ref --short HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# 检查工作目录是否有未提交的更改
execute_process(
    COMMAND "${GIT_EXECUTABLE}" status --porcelain
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_DIRTY
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# 生成版本哈希字符串
if (GIT_DIRTY)
    set(FTPSRV_VERSION_HASH "${FTPSRV_VERSION} ${GIT_COMMIT}-dirty")  # 有未提交更改
else()
    set(FTPSRV_VERSION_HASH "${FTPSRV_VERSION} ${GIT_COMMIT}")  # 干净的工作目录
endif()

# 配置本地minIni库的函数
function(setup_local_minini)
    # 设置本地minIni库路径
    set(MININI_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/minIni-nx")
    
    # 检查本地minIni库是否存在
    if(NOT EXISTS "${MININI_SOURCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "本地minIni库未找到: ${MININI_SOURCE_DIR}")
    endif()
    
    # 配置minIni库的编译选项
    set(MININI_USE_NX ${NINTENDO_SWITCH})      # 启用Nintendo Switch支持
    set(MININI_USE_STDIO ${NINTENDO_SWITCH})   # 启用标准I/O支持
    set(MININI_USE_FLOAT ${NINTENDO_SWITCH})   # 启用浮点数支持
    
    # 添加本地minIni子目录
    add_subdirectory("${MININI_SOURCE_DIR}" minIni)
endfunction()

# 为目标设置FTP服务器编译定义的函数
function(ftp_set_compile_definitions target)
    # 为指定目标添加编译时宏定义
    target_compile_definitions(${target}
        PRIVATE
            HAVE_LSTAT=$<BOOL:${HAVE_LSTAT}>                         # lstat函数支持
            HAVE_READLINK=$<BOOL:${HAVE_READLINK}>                   # readlink函数支持
            HAVE_GETPWUID=$<BOOL:${HAVE_GETPWUID}>                   # getpwuid函数支持
            HAVE_GETGRGID=$<BOOL:${HAVE_GETGRGID}>                   # getgrgid函数支持
            HAVE_STRNCASECMP=$<BOOL:${HAVE_STRNCASECMP}>             # strncasecmp函数支持
            HAVE_LOCALTIME_R=$<BOOL:${HAVE_LOCALTIME_R}>             # localtime_r函数支持
            HAVE_GMTIME_R=$<BOOL:${HAVE_GMTIME_R}>                   # gmtime_r函数支持
            HAVE_POLL=$<BOOL:${HAVE_POLL}>                           # poll函数支持
            HAVE_IPTOS_THROUGHPUT=$<BOOL:${HAVE_IPTOS_THROUGHPUT}>  # IP服务类型吞吐量支持
            HAVE_TCP_NODELAY=$<BOOL:${HAVE_TCP_NODELAY}>             # TCP无延迟选项支持
            HAVE_SO_KEEPALIVE=$<BOOL:${HAVE_SO_KEEPALIVE}>           # 套接字保活选项支持
            HAVE_SO_REUSEADDR=$<BOOL:${HAVE_SO_REUSEADDR}>           # 套接字地址重用支持
        PUBLIC
            FTPSRV_VERSION_MAJOR=${FTPSRV_VERSION_MAJOR}             # FTP服务器主版本号
            FTPSRV_VERSION_MINOR=${FTPSRV_VERSION_MINOR}             # FTP服务器次版本号
            FTPSRV_VERSION_PATCH=${FTPSRV_VERSION_PATCH}             # FTP服务器补丁版本号
            FTPSRV_VERSION_STR="${FTPSRV_VERSION}"                   # FTP服务器版本字符串
            FTPSRV_VERSION_HASH="${FTPSRV_VERSION_HASH}"             # FTP服务器版本哈希
        )
endfunction(ftp_set_compile_definitions)

function(ftp_set_options target path_size sessions buf_size)
    # add base defs
    ftp_set_compile_definitions(${target})
    # add the rest
    target_compile_definitions(${target} PRIVATE
        FTP_PATHNAME_SIZE=${path_size}
        FTP_MAX_SESSIONS=${sessions}
        FTP_FILE_BUFFER_SIZE=${buf_size}
    )
endfunction(ftp_set_options)

function(ftp_add name)
    set_target_properties(${name} PROPERTIES
        C_STANDARD 99
        C_EXTENSIONS ON
    )

    if (NINTENDO_SWITCH)
        set_target_properties(${name} PROPERTIES
            C_STANDARD 11
            C_EXTENSIONS ON
        )
    endif()

    target_compile_options(${name} PRIVATE
        $<$<C_COMPILER_ID:GNU>:
            ${gcc_warning_flags}
        >
    )

    ftp_set_compile_definitions(${name})
endfunction(ftp_add)

add_library(ftpsrv src/ftpsrv.c)
target_include_directories(ftpsrv PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
ftp_add(ftpsrv)
ftp_set_compile_definitions(ftpsrv)

# Nintendo Switch平台特定配置
    if (NINTENDO_SWITCH)
        # 设置虚拟文件系统（VFS）选项的默认值
        if (NOT DEFINED USE_VFS_SAVE)
            set(USE_VFS_SAVE TRUE)      # 启用存档文件系统支持
        endif()
        if (NOT DEFINED USE_VFS_STORAGE)
            set(USE_VFS_STORAGE TRUE)   # 启用存储设备文件系统支持
        endif()
        if (NOT DEFINED USE_VFS_GC)
            set(USE_VFS_GC TRUE)        # 启用游戏卡文件系统支持
        endif()
        if (NOT DEFINED USE_VFS_USBHSFS)
            set(USE_VFS_USBHSFS TRUE)   # 启用USB主机文件系统支持
        endif()

        # 设置FTP服务器选项（路径大小：769，最大会话数：32，缓冲区大小：64KB）
        ftp_set_options(ftpsrv 769 32 1024*64)
        # 配置本地minIni库
        setup_local_minini()

        # 定义Nintendo Switch平台特定的源文件列表
        set(NX_SRC
            src/platform/nx/vfs_nx.c                                   # Switch虚拟文件系统主实现
            src/platform/nx/vfs/vfs_nx_none.c                         # 空文件系统实现
            src/platform/nx/vfs/vfs_nx_root.c                         # 根文件系统实现
            src/platform/nx/vfs/vfs_nx_fs.c                           # 文件系统服务实现
            src/platform/nx/reboot_to_payload/ams_bpc.c               # Atmosphere BPC重启支持
            src/platform/nx/reboot_to_payload/reboot_to_payload.c     # 重启到载荷功能
            src/platform/nx/rtc/max77620-rtc.c                        # MAX77620 RTC时钟支持
            src/platform/nx/custom_commands.c                         # 自定义FTP命令
            src/platform/nx/utils.c                                   # Switch平台工具函数
            src/log/log.c                                              # 日志记录功能
        )

        # 为ftpsrv设置Switch平台特定的编译定义
        target_compile_definitions(ftpsrv PUBLIC
            FTP_VFS_HEADER="${CMAKE_CURRENT_SOURCE_DIR}/src/platform/nx/vfs_nx.h"      # VFS头文件路径
            FTP_SOCKET_HEADER="${CMAKE_CURRENT_SOURCE_DIR}/src/platform/nx/socket_nx.h" # Socket头文件路径
            VFS_NX_BUFFER_WRITES=1                                                      # 启用缓冲写入
        )

        # 根据VFS选项添加相应的源文件
        if (USE_VFS_SAVE)
            list(APPEND NX_SRC src/platform/nx/vfs/vfs_nx_save.c)      # 存档文件系统实现
        endif()
        if (USE_VFS_STORAGE)
            list(APPEND NX_SRC src/platform/nx/vfs/vfs_nx_storage.c)   # 存储设备文件系统实现
        endif()
        if (USE_VFS_GC)
            list(APPEND NX_SRC src/platform/nx/vfs/vfs_nx_gc.c)        # 游戏卡文件系统实现
        endif()



        # 为ftpsrv设置VFS功能的编译定义
        target_compile_definitions(ftpsrv PUBLIC
            USE_VFS_SAVE=$<BOOL:${USE_VFS_SAVE}>        # 存档文件系统支持状态
            USE_VFS_STORAGE=$<BOOL:${USE_VFS_STORAGE}>  # 存储设备文件系统支持状态
            USE_VFS_GC=$<BOOL:${USE_VFS_GC}>            # 游戏卡文件系统支持状态
            USE_VFS_USBHSFS=$<BOOL:${USE_VFS_USBHSFS}>  # USB主机文件系统支持状态
        )

        # USB主机文件系统支持配置
        if (USE_VFS_USBHSFS)
            # 启用GPL许可证支持（用于NTFS和ext4文件系统）
            set(USBHSFS_GPL ON)
            # 设置本地libusbhsfs库路径
            set(libusbhsfs_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/libusbhsfs-0.2.9")
            
            # 检查本地libusbhsfs库是否存在
            if(NOT EXISTS "${libusbhsfs_SOURCE_DIR}/source")
                message(FATAL_ERROR "本地libusbhsfs库未找到: ${libusbhsfs_SOURCE_DIR}")
            endif()
            add_library(libusbhsfs
                ${libusbhsfs_SOURCE_DIR}/source/usbhsfs_drive.c
                ${libusbhsfs_SOURCE_DIR}/source/usbhsfs_log.c
                ${libusbhsfs_SOURCE_DIR}/source/usbhsfs_manager.c
                ${libusbhsfs_SOURCE_DIR}/source/usbhsfs_mount.c
                ${libusbhsfs_SOURCE_DIR}/source/usbhsfs_request.c
                ${libusbhsfs_SOURCE_DIR}/source/usbhsfs_scsi.c
                ${libusbhsfs_SOURCE_DIR}/source/usbhsfs_utils.c
            )

            target_include_directories(libusbhsfs PUBLIC ${libusbhsfs_SOURCE_DIR}/include)

            # fatfs stuff
            target_sources(libusbhsfs PRIVATE
                ${libusbhsfs_SOURCE_DIR}/source/fatfs/diskio.c
                ${libusbhsfs_SOURCE_DIR}/source/fatfs/ff_dev.c
                ${libusbhsfs_SOURCE_DIR}/source/fatfs/ff.c
                ${libusbhsfs_SOURCE_DIR}/source/fatfs/ffsystem.c
                ${libusbhsfs_SOURCE_DIR}/source/fatfs/ffunicode.c
            )

            # sxos stuff
            target_sources(libusbhsfs PRIVATE
                ${libusbhsfs_SOURCE_DIR}/source/sxos/usbfs_dev.c
                ${libusbhsfs_SOURCE_DIR}/source/sxos/usbfs.c
            )

            if (USBHSFS_GPL)
                target_sources(libusbhsfs PRIVATE
                    ${libusbhsfs_SOURCE_DIR}/source/lwext4/ext_dev.c
                    ${libusbhsfs_SOURCE_DIR}/source/lwext4/ext_disk_io.c
                    ${libusbhsfs_SOURCE_DIR}/source/lwext4/ext.c

                    ${libusbhsfs_SOURCE_DIR}/source/ntfs-3g/ntfs_dev.c
                    ${libusbhsfs_SOURCE_DIR}/source/ntfs-3g/ntfs_disk_io.c
                    ${libusbhsfs_SOURCE_DIR}/source/ntfs-3g/ntfs.c
                )

                find_library(ntfs_3g_lib ntfs-3g REQUIRED)
                find_path(ntfs_3g_inc ntfs-3g REQUIRED)

                find_library(lwext4_lib lwext4 REQUIRED)
                find_path(lwext4_inc ext4.h REQUIRED)

                target_link_libraries(libusbhsfs PRIVATE ${ntfs_3g_lib} ${lwext4_lib})
                target_include_directories(libusbhsfs PRIVATE ${ntfs_3g_inc} ${lwext4_inc})
                target_compile_definitions(libusbhsfs PRIVATE GPL_BUILD)
            endif()


        endif()



        # 创建系统模块版本的FTP服务器库
        add_library(ftpsrv_sysmod src/ftpsrv.c)
        target_include_directories(ftpsrv_sysmod PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
        ftp_add(ftpsrv_sysmod)
        # 设置系统模块的FTP选项（路径大小：769，最大会话数：5，缓冲区大小：64KB）
        ftp_set_options(ftpsrv_sysmod 769 5 1024*64)

        # 为系统模块版本设置编译定义
        target_compile_definitions(ftpsrv_sysmod PUBLIC
            FTP_VFS_HEADER="${CMAKE_CURRENT_SOURCE_DIR}/src/platform/nx/vfs_nx.h"      # VFS头文件路径
            FTP_SOCKET_HEADER="${CMAKE_CURRENT_SOURCE_DIR}/src/platform/nx/socket_nx.h" # Socket头文件路径
            VFS_NX_BUFFER_WRITES=0                                                      # 禁用缓冲写入（系统模块）
        )

        # 创建系统模块版本的FTP可执行文件
        add_executable(sysFtpAutoBack
            src/platform/nx/main_sysmod.c  # 系统模块主程序入口
            ${NX_SRC}                      # Switch平台源文件
        )
        # 设置编译选项
        target_compile_options(sysFtpAutoBack PRIVATE ${gcc_warning_flags})
        
        # 查找系统模块所需的外部库
        find_library(CURL_LIB curl)    # libcurl网络库
        find_library(ZLIB_LIB z)       # zlib压缩库
        find_library(XML2_LIB xml2)    # libxml2 XML解析库
        
        # 链接所有必要的库到系统模块
        target_link_libraries(sysFtpAutoBack PRIVATE ftpsrv_sysmod minIni ${CURL_LIB} ${ZLIB_LIB} ${XML2_LIB})
        
        # 添加DevkitPro工具链的包含目录
        target_include_directories(sysFtpAutoBack PRIVATE 
            $ENV{DEVKITPRO}/portlibs/switch/include         # Switch移植库头文件
            $ENV{DEVKITPRO}/portlibs/switch/include/libxml2 # libxml2头文件
        )
        
        # 配置sysFtpAutoBack可执行文件
        ftp_add(sysFtpAutoBack)

        # 为系统模块版本设置VFS功能的编译定义
        target_compile_definitions(ftpsrv_sysmod PUBLIC
            USE_VFS_SAVE=$<BOOL:${USE_VFS_SAVE}>        # 存档文件系统支持状态
            USE_VFS_STORAGE=$<BOOL:${USE_VFS_STORAGE}>  # 存储设备文件系统支持状态
            USE_VFS_GC=$<BOOL:${USE_VFS_GC}>            # 游戏卡文件系统支持状态
        )

        # 创建系统模块所需的目录结构
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/420000000000011B)       # 系统模块目录
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/420000000000011B/flags) # 标志文件目录

        # 配置系统模块的JSON配置文件
        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/platform/nx/sysFtpAutoBack.json ${CMAKE_CURRENT_BINARY_DIR}/sysFtpAutoBack.json)

        # 添加构建后命令：复制配置文件和创建启动标志
        add_custom_command(
            TARGET sysFtpAutoBack POST_BUILD
            # 复制工具箱配置文件
            COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/nx/toolbox.json
                ${CMAKE_CURRENT_BINARY_DIR}/420000000000011B/toolbox.json

            # 创建boot2启动标志文件
            COMMAND ${CMAKE_COMMAND} -E touch
                ${CMAKE_CURRENT_BINARY_DIR}/420000000000011B/flags/boot2.flag
        )

        # 创建系统模块的ExeFS NSP文件
        nx_create_exefs(sysFtpAutoBack
            OUTPUT 420000000000011B/exefs.nsp           # 输出ExeFS文件
            CONFIG ${CMAKE_CURRENT_BINARY_DIR}/sysFtpAutoBack.json  # 使用配置文件
        )
    endif()
