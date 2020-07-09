#ifndef _HTTP_H
#define _HTTP_H

#include<stdarg.h>
#include<unordered_map>
#include <sys/uio.h>
#include<string.h>
#include <sys/stat.h>
#include <unistd.h>
#include<sys/epoll.h>
#include<sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/types.h>
#include<sys/mman.h>

#include"../info/info.h"
#include"../mysqlpoll/mysqlpoll.h"
#include"../tools/tools.h"


/*
    定义http响应的一些状态信息
*/
extern const char *ok_200_title;
extern const char *error_400_title ;
extern const char *error_400_form ;
extern const char *error_403_title ;
extern const char *error_403_form ;
extern const char *error_404_title ;
extern const char *error_404_form ;
extern const char *error_500_title ;
extern const char *error_500_form ;

// 定义并行处理模式
extern const int reactor ;
extern const int proactor;
// 定义EPOLL触发模式
extern const int ET ;
extern const int LT ;

static const int FILE_NAME_LEN = 200 ;
static const int READ_BUFFER_SIZE = 2048;
static const int WRITE_BUFFER_SIZE = 2048;


// http 请求方法
    enum HTTP_METHOD {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    // 主状态机状态
    enum CHECK_STATE{
        CHECK_REQUESTLINE = 0,
        CHECK_HEADER,
        CHECK_CONTENT
    };
    // 从状态机状态
    enum LINE_STATE{
        LINE_OK = 0,
        LINE_OPEN,
        LINE_BAD
    };
    // 请求状态
    enum REQUEST_STATE{
        NO_REQUEST = 0,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REUQEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };




class http{

private:
    // 读相关
    char read_buf[READ_BUFFER_SIZE];
    int cur_rd_idx;
    int cur_parse_idx;
    int cur_parseline_head;
    int content_len;
    // 写相关
    char write_buf[WRITE_BUFFER_SIZE];
    int cur_wr_idx;
    int bytes_to_send;
    int bytes_have_send;
    // 状态
    CHECK_STATE master_state;
    HTTP_METHOD  cur_http_method;
    LINE_STATE  line_state;
    //其他
    int sockfd; 
    char* request_url;
    bool KeepAlive;  // linger?
    std::string post_line;
    char native_request_url[FILE_NAME_LEN];
    
    char*  mmap_addr;
    struct stat file_stat;
    struct iovec m_iv[2];
    int m_iv_cnt ;

    static locker m_lock;
    static char* workdir;
    static std::unordered_map<std::string,std::string> users;
    static int epollfd;
    static int actor_model;
    static int epoll_trigger_model;
    
public:
    static int cur_user_cnt;
    //初始化
    void init(int sockfd,int TriggerMode);
    void init();
    friend void init_http_static(char*);
    // 
    void process();
    void close_connection();
    bool isProactor();
    void unmmap();
    // 读取报文 并进行处理  
    bool read_once();
    REQUEST_STATE process_read();
    char* get_line();
    LINE_STATE parse_line();
    REQUEST_STATE parse_requestline(char* line);
    REQUEST_STATE parse_header(char* line);
    REQUEST_STATE parse_content(char*);
    // 处理得到的请求  根据具体请求返回不同的状态
    REQUEST_STATE do_request();
    // 根据报文请求写文件
    bool process_write(REQUEST_STATE);
    bool write_to_socket();
    bool add_line(const char* format, ...);
    bool add_statusline(int status,const char*);
    bool add_header(int);
    bool add_keepalive();
    bool add_content_type();
    bool add_content_len(int size);
    bool add_blank_line();
    bool add_content(const char*);

    int fd(){ return sockfd;}

    // 静态变量接口
    static void set_epoll_fd(int fd);
    static void set_actor_model(int model);
    
};

/*
    非成员函数
*/

void init_http_static(char* );

#endif