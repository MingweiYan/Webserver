
#include"../http/http.h"



const char *ok_200_title = "OK";
const char *ok_206_title = "Partial Content";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_416_title = "Out of range";
const char * error_416_form = "Requested Range Not Satisfiable";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

// 定义并行处理模式
const int reactor = 0;
const int proactor = 1;
// 定义EPOLL触发模式
const int ET = 0;
const int LT = 1;

// 初始化静态变量
int http::cur_user_cnt = 0;
char* http::workdir = NULL;
int http::epollfd = 0;
locker http::http::m_lock = locker();
std::unordered_map<std::string,std::string> http::users = std::unordered_map<std::string,std::string>();
//std::unordered_map<std::string,std::string> http::tokens = std::unordered_map<std::string,std::string>();
int http::actor_model = proactor;
int http::epoll_trigger_model = ET;



// 初始化静态变量 从Mysql中获取用户登录信息
void init_http_static(char* rootPath_){

    http::workdir = rootPath_;
   
    //先从连接池中取一个连接
    mysqlconnection mysqlcon;
    MYSQL *mysql = mysqlcon.get_mysql();
    //在user表中检索username，passwd数据，浏览器端输入
    if (mysql_query(mysql, "SELECT username,passwd FROM user")){
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    }
    //从表中检索完整的结果集
    MYSQL_RES *result = mysql_store_result(mysql);
    //返回结果集中的列数
    int num_fields = mysql_num_fields(result);
    //返回所有字段结构的数组
    MYSQL_FIELD *fields = mysql_fetch_fields(result);
    //从结果集中获取下一行，将对应的用户名和密码，存入map中  
    while (MYSQL_ROW row = mysql_fetch_row(result)){
        std::string temp1(row[0]);
        std::string temp2(row[1]);
        http::users[temp1] = temp2;
    }
    
}
//初始化函数
void http::init(int sockfd_,int TriggerMode_){
    sockfd = sockfd_;
    epoll_trigger_model = TriggerMode_;
    // 添加到epoll  读事件
    tools::getInstance()->epoll_add(http::epollfd,sockfd,true,true,epoll_trigger_model == ET);
    tools::getInstance()->setnonblocking(sockfd);
    init();
    http::m_lock.lock();
    ++http::cur_user_cnt;
    http::m_lock.unlock(); 
}
void http::init(){

    memset(read_buf,'\0',READ_BUFFER_SIZE);
    memset(write_buf,'\0',WRITE_BUFFER_SIZE);
    memset(native_request_url,'\0',FILE_NAME_LEN);

    cur_rd_idx = 0;
    cur_parse_idx = 0;
    cur_parseline_head = 0;
    content_len = 0;

    cur_wr_idx = 0;
    bytes_to_send = 0;
    bytes_have_send = 0;

    master_state = CHECK_REQUESTLINE;
    cur_http_method  = GET;
    line_state = LINE_OPEN;

    cookieVerified = false;
    cookieId.clear();
  
    range_request = false;
    
    request_url = NULL;
    KeepAlive = false;
    mmap_addr = NULL;
    m_iv_cnt = 0;
}

// 关闭连接
void http::close_connection(){
    if(sockfd < 0){
        LOG_WARN("%s %d""close a non-exit conncetion who is ",sockfd)
        return ;
    }
    LOG_INFO("%s%d","close http connection and sockfd is ",sockfd)
    tools::getInstance()->epoll_remove(http::epollfd,sockfd);
    close(sockfd);
   // init();
    sockfd = -1;
    http::m_lock.lock();
    --http::cur_user_cnt;
    http::m_lock.unlock();   
}
void http::process(){
    // 处理请求报文 得到请求
    REQUEST_STATE state = process_read();
   // LOG_INFO("process read")
    // 没得到有效的请求  继续读
    if(state == NO_REQUEST){
        LOG_ERROR("%s%d","parse request not get valid request registrer epollin and onshot the sockfd is ",sockfd)
        tools::getInstance()->epoll_mod(http::epollfd,sockfd,EPOLLIN,true,epoll_trigger_model == ET );
        return ;
    }
    // 状态行和首部行放入写缓存  映射文件
    bool ret = process_write(state);
    if(!ret){
        LOG_ERROR("%s%d","write to buf failed the sockfd is ",sockfd)
        close_connection();
    }
   // LOG_INFO("process write")
    // 完成写后注册写事件
  //  LOG_INFO("%s%d","finish process read and write register EPOLLOUT the sockfd is ",sockfd)
    tools::getInstance()->epoll_mod(http::epollfd,sockfd,EPOLLOUT,true,epoll_trigger_model == ET );
}
// 从socket读取信息到读缓存  根据ET和LT选择不同的读取方式
bool http::read_once(){
    // 缓存区已满 直接返回
    if(cur_rd_idx > READ_BUFFER_SIZE){
        LOG_WARN("s%"," http read bufer is full")
        return false;
    }
    int bytes_recv = 0;
    // ET 模式
    if(epoll_trigger_model == ET){
        while(true){
            bytes_recv = recv(sockfd, read_buf+cur_rd_idx, READ_BUFFER_SIZE-cur_rd_idx, 0);
            if(bytes_recv ==-1){
                if(errno == EAGAIN || errno == EWOULDBLOCK){
                    //读取完毕
                    break;
                }
            } 
            else if (bytes_recv == 0){
                // 关闭连接
                 LOG_WARN("s%","read while http is closed")
                return false;
            }
            cur_rd_idx += bytes_recv;
        }
        return true;
    }
    // LT 模式
    else if(epoll_trigger_model == LT) {
        bytes_recv = recv(sockfd, read_buf+cur_rd_idx, READ_BUFFER_SIZE-cur_rd_idx, 0);
        if(bytes_recv <= 0){
            // 返回-1 出错了 0 关闭连接
            return false;
        }
        cur_rd_idx += bytes_recv;
        return true;
    }
}

// 获取新读入数据指针
char* http::get_line(){
    return read_buf + cur_parseline_head;
}
 
/*
    功能：解析一行
    实现： 从解析指针到当前缓存区末尾指针，逐个字符解析直到解析完一行
*/
LINE_STATE http::parse_line(){
    for(; cur_parse_idx < cur_rd_idx; ++cur_parse_idx){
        char temp = read_buf[cur_parse_idx];
        // 第一个元素是\n' 
        if(temp == '\n' && cur_parse_idx == 0){
            return LINE_BAD;
        }
        if(temp == '\n' && read_buf[cur_parse_idx-1]=='\r'){
            read_buf[cur_parse_idx-1] = '\0';
            read_buf[cur_parse_idx++] = '\0';
            return LINE_OK;
        }
    }
    return LINE_OPEN;
}
// 解析请求行 并将url 放入 request_url
REQUEST_STATE  http::parse_requestline(char* line){
    char * idx = NULL;
    // 第一个\t出现的位置 
    idx = strpbrk(line, " \t");
    if(idx == NULL){
        return BAD_REQUEST;
    }
    *idx = '\0';
    ++idx;
    // 得到请求方法
    char * method = line;
    // 忽略大小写比较  
    if(strcasecmp(method, "GET") == 0){
        cur_http_method = GET;
    } else if(strcasecmp(method, "POST") == 0){
         cur_http_method = POST;
    } else{
        return BAD_REQUEST;
    }
    // 处理url 和version
    // 防止多出空格
    idx += strspn(idx, " \t");
    char* version = strpbrk(idx, " \t");
    if(version == NULL){
        return BAD_REQUEST;
    }
    *version = '\0';
    request_url = idx;
    ++version;
    version += strspn(version," \t");
    if(strcasecmp(version, "HTTP/1.1") != 0){
        return BAD_REQUEST;
    }

    if(strncasecmp(request_url, "http://", 7) == 0)
    {
        request_url += 7;
        request_url = strchr(request_url, '/');
    }
    else if(strncasecmp(request_url, "https://", 8) == 0)
    {
        request_url += 8;
        request_url = strchr(request_url, '/');
    }

    if(!request_url || request_url[0] != '/')
        return BAD_REQUEST;
    // url 为  / 显示判断界面
    if (strlen(request_url) == 1)
        strcat(request_url, "judge.html");
    master_state = CHECK_HEADER; 
    return NO_REQUEST;
}
// 解析一行头部信息
REQUEST_STATE http::parse_header(char* line){
   // LOG_INFO(line);
    if(line[0] == '\0'){
        if(content_len != 0){
            master_state = CHECK_CONTENT;
            return NO_REQUEST;
        }
        // 如果是GET  解析完头部信息就OK了
        return GET_REQUEST;
    } 
    else if (strncasecmp(line, "Connection:", 11) == 0){
        line += 11;
        line += strspn(line, " \t");
        if (strcasecmp(line, "keep-alive") == 0){
            KeepAlive = true;
        }
    }
    else if (strncasecmp(line, "Content-length:", 15) == 0){
        line += 15;
        line += strspn(line, " \t");
        content_len = atol(line);
    }
    else if (strncasecmp(line,"Ranges: bytes=",13) == 0){
        range_request = true; 
        line += 13;
        // 找到所有循环
        while (*line != '\0'){
            int range_beg = -1;
            int range_end = -1;
            // 前半部分
            char* mid_pos = strpbrk(line,"-");
            if(mid_pos != line){
                *mid_pos = '\0';
                range_beg = atol(line);
                line = mid_pos;
            } 
            ++line;
            // 后半部分
            char* end_pos = strpbrk(line,",");
            // 已经到了结尾
            if(end_pos == NULL && *line != '\0'){
                range_end = atol(line);
                ranges.push_back({range_beg,range_end});
                break;
            } 
            else if(end_pos != NULL){
                if(*line !=','){
                    *end_pos = '\0';
                    range_end = atol(line);
                    line = end_pos;
                }
                line +=2;
            }
            if(range_end < range_beg){
                return BAD_RANGE;
            }
            ranges.push_back({range_beg,range_end});
        }
    }
    else if (strncasecmp(line, "ETag:", 15) == 0){
        // did nothing
    }
    else if (strncasecmp(line, "Cookie:", 7) == 0){
       // LOG_INFO("begin to parse cookie")
        line += 7;
        char* tmp = strpbrk(line, ";");
        if(tmp){
            line = tmp;
            line += 1;
        }
       // LOG_INFO("state1")
        line += strspn(line, " ");
      //  LOG_INFO("state2")
        std::string cookie = line ;

       // LOG_INFO(cookie.c_str());
        redisClient cli;
       // LOG_INFO("verfiy cookie")
        cookieVerified = cli.verify_cookie(cookie);
        if(cookieVerified){
            cookieId = cookie;
        } 
      //  LOG_INFO("finish verfiy cookie")
    }
    return NO_REQUEST;
}
// 处理POST请求内容 放入post_line 中
REQUEST_STATE http::parse_content(char* line){
    if(cur_rd_idx >= (cur_parse_idx + content_len) ){
        line[content_len] = '\0';
        post_line = line;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}
// 读取报文并得到请求
REQUEST_STATE http::process_read(){
    // 解析请求行和首部行 要先parse line  而解析正文不需要
    while( (master_state == CHECK_CONTENT && line_state == LINE_OK) || ( line_state = parse_line() ) == LINE_OK ){
        char* line = get_line();
        cur_parseline_head = cur_parse_idx;
      //  LOG_INFO("%s", line);
        REQUEST_STATE  ret;
        switch (master_state)
        {
        case CHECK_REQUESTLINE:
            ret = parse_requestline(line);
           // LOG_INFO("parse request line")
            if(ret == BAD_REQUEST){
                return BAD_REQUEST;
            }
            break;
        case CHECK_HEADER:
            ret = parse_header(line);
           // LOG_INFO("parse header")
            if(ret == BAD_REQUEST){
                return BAD_REQUEST;
            } 
            else if (ret == BAD_RANGE){
                return BAD_RANGE;
            }
            else if(ret == GET_REQUEST){
                return do_request();
            }
            break;
        case CHECK_CONTENT:
            ret = parse_content(line);
         //    LOG_INFO("parse content")
            if(ret == GET_REQUEST){
                line_state = LINE_OPEN;
                return do_request();
            }
            break;
        default:
            return INTERNAL_ERROR;
        }
    }
}


// 根据得到的请求内容处理 登录注册 发送文件等
REQUEST_STATE http::do_request(){
    // 首先设置本地路径为根目录
    strcpy(native_request_url,workdir);
    int len = strlen(workdir);
    // 解析url请求的界面
    const char* p = strrchr(request_url,'/');
    //注册页面
    if(*(p+1) == '0'){
        char* tail_url = (char*) malloc(sizeof(char)*200);
        strcpy(tail_url,"/register.html");
        strncpy(native_request_url+len,tail_url,strlen(tail_url));
        free(tail_url);
     //   LOG_INFO("%s","request register page")
    }
    // 登录页面
    else if(*(p+1) == '1'){
        char* tail_url = (char*) malloc(sizeof(char)*200);
        strcpy(tail_url, "/log.html");
        strncpy(native_request_url+len,tail_url,strlen(tail_url));
        free(tail_url);
       // LOG_INFO("%s","request login page")
    }
    // 登录
    else if(*(p+1) == '2'){
        std::string name; std::string password;
        // user=1234&password=123
        int idx = 5;
        for(; post_line[idx]!='&'; ++idx){
            name += post_line[idx];
        }
        idx +=10;
        for(; post_line[idx]!='\0'; ++idx){
            password += post_line[idx];
        }
        if(!name.empty() &&  http::users.count(name) &&  http::users[name] == password){
            cookieVerified = true;
            redisClient cli;
            cli.get_cookie(name,cookieId);
            strcpy(request_url,"/welcome.html");
        } else {
            strcpy(request_url,"/logError.html");
        }
        strncpy(native_request_url + len, request_url, strlen(request_url)); 
     //   LOG_INFO("%s","try to login")
    } 
    // 注册
    else if(*(p+1) == '3'){
        // 
        if(post_line.empty() ){
            strcpy(request_url,"/registerError.html");
            strncpy(native_request_url + len, request_url, strlen(request_url)); 
            LOG_WARN("%s","post empty content ")
        }
        else {
            std::string name; std::string password;
            // user=1234&password=123
            int idx = 5;
            for(; post_line[idx]!='&'; ++idx){
                name += post_line[idx];
            }
            idx +=10;
            for(; post_line[idx]!='\0'; ++idx){
                password += post_line[idx];
            }

            std::string  sql_insert = " INSERT INTO user(username, passwd) VALUES('";
            sql_insert += name;
            sql_insert += "', '";
            sql_insert += password;
            sql_insert += "')";

            mysqlconnection instance;
            MYSQL* mysqlconn = instance.get_mysql() ;

            if(! http::users.count(name)){
                http::m_lock.lock();
                int res = mysql_query(mysqlconn,sql_insert.c_str());
                http::users .insert({name,password});
                http::m_lock.unlock();
                // 插入成功
                if(res == 0){
                    strcpy(request_url,"/welcome.html");
                    time_t cur = time(NULL);
                    cookieId = std::to_string(cur);
                    cookieId += name;
                    redisClient cli;
                    cli.add_cookie(name,cookieId);
                    cookieVerified = true; 
                } 
                // 插入失败 重新注册
                else {
                    strcpy(request_url,"/log.html");   
                }
            } 
            else {
                strcpy(request_url,"/registerError.html");
            }
            strncpy(native_request_url + len, request_url, strlen(request_url)); 
        }

     //   LOG_INFO("%s","request to signup")
    }
    // 请求图片
    else if(*(p+1) == '4'){
        if(!cookieVerified){
            return FORBIDDEN_REUQEST; 
        }
        char* tail_url = (char*) malloc(sizeof(char)*200);
        strcpy(tail_url, "/picture.html");
        strncpy(native_request_url+len,tail_url,strlen(tail_url));
        free(tail_url);
      //  LOG_INFO("%s","request picture page")
    }
    // 请求视频
    else if(*(p+1) == '5'){
        if(!cookieVerified){
            return FORBIDDEN_REUQEST; 
        }
        char* tail_url = (char*) malloc(sizeof(char)*200);
        strcpy(tail_url, "/video.html");
        strncpy(native_request_url+len,tail_url,strlen(tail_url));
        free(tail_url);
     //   LOG_INFO("%s","request movie page")
    }
    // 请求歌曲
    else if(*(p+1) == '6'){
        if(!cookieVerified){
            return FORBIDDEN_REUQEST; 
        }
        char* tail_url = (char*) malloc(sizeof(char)*200);
        strcpy(tail_url, "/music.html");
        strncpy(native_request_url+len,tail_url,strlen(tail_url));
        free(tail_url);
    //    LOG_INFO("%s","request music page")
    }
    // 其他情况  只有/  起始界面 和注册登录完成的界面
    else{
        strncpy(native_request_url + len, request_url, strlen(request_url));
     //   LOG_INFO("%s","request default page")
    }
    
        
    int ret = stat(native_request_url,&file_stat);
    if(ret < 0){
        return NO_RESOURCE;
    } 
    // 其他用户可读
    if( !(file_stat.st_mode && S_IROTH) ){
        return FORBIDDEN_REUQEST;
    }
    // 请求的是一个文件夹
    if( S_ISDIR(file_stat.st_mode) ){
        return BAD_REQUEST;
    }
    // 打开文件  进行mmap映射
    int fd = open(native_request_url,O_RDONLY);
    mmap_addr = (char *)mmap(0, file_stat.st_size, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

// 取消mmap映射
void http::unmmap(){
    if(mmap_addr != NULL){
        munmap(mmap_addr,file_stat.st_size);
        mmap_addr = 0; 
    }
}


//按给定格式写一行内容到写缓存
bool http::add_line(const char* format, ...){
    // 缓存已满
    if(cur_wr_idx >= WRITE_BUFFER_SIZE){
        LOG_WARN("%s%d","add line to buf failure buf is full sockfd is ",sockfd)
        return false;
    }
    va_list va;
    va_start(va,format);
    int len = vsnprintf(write_buf+cur_wr_idx, WRITE_BUFFER_SIZE-cur_wr_idx-1, format, va);
    // 缓存区不够
    if(len >= (WRITE_BUFFER_SIZE-cur_wr_idx-1) ){
        va_end(va);
        LOG_WARN("%s%d","add line to buf failure buf is full sockfd is ",sockfd)
        return false;
    }
    cur_wr_idx += len;
    va_end(va);
   // LOG_INFO("request:%s", write_buf);
    return true;
}
//写状态行
bool http::add_statusline(int status,const char* detail){
    return add_line("%s %d %s\r\n", "HTTP/1.1", status, detail);
}
// 写首部字段
bool http::add_header(int content_len){
    return add_content_len(content_len) && add_keepalive() 
            && add_accpet_rangerequest() && add_ETag() && add_cookie()
            && add_blank_line();
}
//写空白行
bool http::add_blank_line(){
    return add_line("%s", "\r\n");
}
// 写长度
bool http::add_content_len(int size){
    return add_line("Content-Length:%d\r\n", size);
}
// 添加是否长连接
bool http::add_keepalive(){
    return add_line("Connection:%s\r\n", (KeepAlive == true) ? "keep-alive" : "close");
}
// 写内容
bool http::add_content(const char* content){
     return add_line("%s", content);
}
// 支持范围请求
bool http::add_accpet_rangerequest(){
    return add_line("%s\r\n","Accept-Ranges : bytes");
}
// 添加范围写内容
bool http::add_content_range(int beg, int end, int size){
    return add_line("%s%d-%d//%d\r\n","Content-Range: bytes ",beg,end,size);
}
// 添加文件的Etag
bool http::add_ETag(){
    std::string tmp (native_request_url);
    std::string md5_value = tools::getInstance()->get_fileMD5(tmp);
    return add_line("%s%s\"\r\n","ETag:\"",md5_value.c_str());
}
// 添加token
bool http::add_token(){
    // did nothing
}
//添加Cookie
bool http::add_cookie(){
    if(cookieVerified)
        return add_line("Set-Cookie: %s\r\n",cookieId.c_str());
}

// 处理写
bool http::process_write(REQUEST_STATE state){
    bool ret;
    switch (state)
    {
    case INTERNAL_ERROR:
        add_statusline(500,error_500_title);
        add_header(strlen(error_500_form));
        ret = add_content(error_500_form);
        if(!ret) return false;
        break;
    case NO_RESOURCE:
        add_statusline(404,error_404_title);
        add_header(strlen(error_404_form));
        ret = add_content(error_404_form);
        if(!ret) return false;
        break;
    case BAD_REQUEST:
        add_statusline(400,error_400_title);
        add_header(strlen(error_400_form));
        ret = add_content(error_400_form);
        if(!ret) return false;
        break;
    case BAD_RANGE:
        add_statusline(416,error_416_title);
        add_header(strlen(error_416_form));
        add_header(strlen(error_416_form));
    case FORBIDDEN_REUQEST:
        add_statusline(403,error_403_title);
        add_header(strlen(error_403_form));
        ret = add_content(error_403_form);
        if(!ret) return false;
        break;
    case FILE_REQUEST:
        // 没有范围请求
        if(!range_request){
            add_statusline(200,ok_200_title);
            if(file_stat.st_size > 0){
                add_header(file_stat.st_size);
                
                m_iv[0].iov_base = write_buf;
                m_iv[0].iov_len = cur_wr_idx;
                m_iv[1].iov_base = mmap_addr;
                m_iv[1].iov_len = file_stat.st_size;
                m_iv_cnt = 2;
                bytes_to_send = cur_wr_idx + file_stat.st_size;
                return true;
            } 
            // 空文件
            else {
                const char *ok_string = "<html><body></body></html>";
                add_header(strlen(ok_string));
                bool ret = add_content(ok_string);
                if(!ret){
                    return false;
                }
            }
        } 
        // 范围请求
        else{
            process_range();
        }
        
        break;
    default:
        return false;
        break;
    }
    m_iv[0].iov_base = write_buf;
    m_iv[0].iov_len = cur_wr_idx;
    bytes_to_send = cur_wr_idx;
    m_iv_cnt = 1;
    return true;
}
// 处理范围请求
// 暂时处理单个范围  可以解析出来 但是写 边界值没确定
bool http::process_range(){

    add_statusline(206,ok_206_title);
    //暂时处理单个范围
    auto range = ranges[0];
    int beg = 0; int size = 0;
    // -b
    if(range[0] < 0){
        size = range[1];
        beg = bytes_to_send - size;
    } //  a- 
    else if (range[1] < 0){
        size = bytes_to_send - range[0];
        beg = range[0];
    } // a-b 
    else {
        size = range[1] - range[0] + 1;
        beg = range[0];
    }
    // 请求范围超出了
    if(beg + size > file_stat.st_size){
        add_statusline(416,error_416_title);
        add_header(strlen(error_416_form));
        int ret = add_content(error_416_form);
        return false;
    }
    add_header(size);
    add_content_range(beg,beg + size -1,file_stat.st_size);
    if(file_stat.st_size > 0){
        m_iv[0].iov_base = write_buf;
        m_iv[0].iov_len = cur_wr_idx;
        m_iv[1].iov_base = mmap_addr + beg;
        m_iv[1].iov_len = size;
        m_iv_cnt = 2;
        bytes_to_send = cur_wr_idx + size;
        return true;
    }
    // 空文件
    else {
        const char *ok_string = "<html><body></body></html>";
        add_header(strlen(ok_string));
        bool ret = add_content(ok_string);
        if(!ret){
            return false;
        }
    }
    
/*
    for(auto & range: ranges){
        int beg = 0; int size = 0;
        // -b
        if(range[0] < 0){
            size = range[1];
            beg = bytes_to_send - size;
        } //  a- 
        else if (range[1] < 0){
            size = bytes_to_send - range[0];
            beg = range[0];
        } // a-b 
        else {
            size = range[1] - range[0] + 1;
            beg = range[0];
        }
        // 请求范围超出了
        if(beg + size > file_stat.st_size){
            add_statusline(416,error_416_title);
            add_header(strlen(error_416_form));
            int ret = add_content(error_416_form);
            break;
        }
        add_header(size);
        add_content_range(beg,beg + size -1,file_stat.st_size);
        if(file_stat.st_size > 0){
            m_iv[0].iov_base = write_buf;
            m_iv[0].iov_len = cur_wr_idx;
            m_iv[1].iov_base = mmap_addr + beg;
            m_iv[1].iov_len = size;
            m_iv_cnt = 2;
            bytes_to_send = cur_wr_idx + size;
            return true;
        } 
*/ 
}
// 写函数
bool http::write_to_socket(){
    while( true) {
        int bytes = writev(sockfd,m_iv,m_iv_cnt);
        if(bytes < 0){
            if(errno == EAGAIN){
                // 缓存区满了  重置为 oneshoot的 OUT
                tools::getInstance()->epoll_mod(http::epollfd, sockfd,EPOLLOUT,true,epoll_trigger_model ==ET);
                return true;
            }
            // 否则出错 
            unmmap();
            LOG_ERROR("%s %d%s% d","write to socket error the sockfd is",sockfd," the errorno is",errno)
            return false;
        }
        // 单次写完 更新结构体
        bytes_have_send += bytes;
        bytes_to_send -= bytes;
        if(bytes_have_send >= m_iv[0].iov_len){
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = mmap_addr + (bytes_have_send - cur_wr_idx);
            m_iv[1].iov_len = bytes_to_send;
        } else {
            m_iv[0].iov_base = write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
        }
        // 发送完毕
        if(bytes_to_send <= 0){
            unmmap();
            tools::getInstance()->epoll_mod(http::epollfd, sockfd,EPOLLIN,true,epoll_trigger_model ==ET);
            if(KeepAlive){
                init(); 
                LOG_INFO("%s%d%s","finish write to socket sockfd ",sockfd, " is choosing keep alive")
                return true;
            }
            // return false 表明关闭连接
            LOG_INFO("%s%d%s","finish write to socket sockfd ",sockfd, " is choosing close connection")
            return false;
        }
    }
}


// 判断是否是proactor
bool http::isProactor(){
    return actor_model == proactor;
}
// 设置epollfd
void http::set_epoll_fd(int fd){
    http::m_lock.lock();
    http::epollfd = fd;
    http::m_lock.unlock();
}
// 设置actormodel
 void http::set_actor_model(int model){
    http::m_lock.lock();
    http::actor_model = model;
    http::m_lock.unlock();
 }

 //通知服务器关闭连接
 void http::inform_close(){
    m_lock.lock();
    int fd = this->fd();
    int pre_erro = errno;
    send(tools::close_pipefd[1],&fd,sizeof(fd),0);
    m_lock.unlock();
    errno = pre_erro;
 }