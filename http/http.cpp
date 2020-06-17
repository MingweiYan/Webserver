#include<string.h>
#include <sys/stat.h>
#include <unistd.h>
#include"../http/http.h"

// http的工作函数
void http_work_fun(http* http_conn){
    if(!http_conn->isProactor()){
        if(http_conn->read_once()){

        }
    }
}
// 从socket读取信息到读缓存  根据ET和LT选择不同的读取方式
bool http::read_once(){
    if(cur_rd_idx>read_buf_size){
        return false;
    }
    int bytes_recv = 0;
    if(epoll_trigger_model == ET){
        while(true){
            bytes_recv = recv(sockfd,read_buf+cur_rd_idx,read_buf_size-cur_rd_idx,0);
            if(bytes_recv ==-1){
                if(errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
            } else if (bytes_recv == 0){
                return false;
            }
            cur_rd_idx += bytes_recv;
        }
        return true;
    } else if(epoll_trigger_model == LT) {
        bytes_recv = recv(sockfd,read_buf+cur_rd_idx,read_buf_size-cur_rd_idx,0);
        if(bytes_recv<0){
            return false;
        }
        cur_rd_idx += bytes_recv;
        return true;
    }
}
// 从Mysql中获取用户登录信息
void http::getLoginformation(){
    //先从连接池中取一个连接
    mysqlconnection mysqlcon;
    MYSQL *mysql = mysqlcon->get_mysql();
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
        users[temp1] = temp2;
    }
}
// 获取新读入数据指针
char* http::get_line(){
    return read_buf + cur_parse_idx;
}
// 解析一行
LINE_STATE http::parse_line(){
    for(; cur_parse_idx<cur_rd_idx; ++cur_parse_idx){
        char temp = read_buf[cur_parse_idx];
        if(temp == '\n' && cur_parse_idx ==0){
            return LINE_BAD;
        }
        if(temp == '\n' && read_buf[cur_parse_idx-1]=='\r'){
            readbuf[cur_parse_idx-1] = '\0';
            read_buf[cur_parse_idx++] = '\0';
            return LINE_OK;
        }
    }
    return LINE_OPEN;
}
// 解析请求行
REQUEST_STATE  http::parse_requestline(char* line){
    char * idx = NULL;
    // 第一个\t出现的位置 
    idx = strpbrk(line, " \t");
    if(pos == NULL){
        return BAD_REQUEST;
    }
    *idx = '\0';
    ++idx;
    // 得到请求方法
    char * method = line;
    // 忽略大小写比较  
    if(strcasecmp(method, "GET") == 0){
        HTTP_METHOD = GET;
    } else if(strcasecmp(method, "POST") == 0){
        HTTP_METHOD = POST;
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
    version = strspn(version," \t");
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

    if (strlen(request_url) == 1)
        strcat(request_url, "judge.html");
    master_state = CHECK_HEADER; 

    return NO_REQUEST;
}
// 解析头部信息
REQUEST_STATE http::parse_header(char* line){
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
    else if (strncasecmp(line, "Host:", 5) == 0){
        line += 5;
        line += strspn(line, " \t");
    }
    else{
        LOG_INFO("oop!unknow header: %s", line);
    }
    return NO_REQUEST;
}
// 处理POST请求内容
REQUEST_STATE http::parse_content(char* line){
    if(cur_rd_idx >= (cur_parse_idx+content_len) ){
        line[content_len] = '\0';
        post_line = line;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}
// 读取报文并得到请求
void http::process_read(){
    while( (master_state == CHECK_CONTENT && line_state == LINE_OK) || line_state = parse_line() ){
        char* line = get_line();
        LOG_INFO("%s", line);
        switch (master_state)
        {
        case CHECK_REQUESTLINE:
            REQUEST_STATE ret = parse_requestline(line);
            if(ret==BAD_REQUEST){
                return BAD_REQUEST;
            }
            break;
        case CHECK_HEADER:
            REQUEST_STATE ret = parse_header(line);
            if(ret == BAD_REQUEST){
                return BAD_REQUEST;
            } 
            else if(ret == GET_REQUEST){
                return do_request();
            }
            break;
        case CHECK_CONTENT:
            REQUEST_STATE ret = parse_content(line);
            if(ret == GET_REQUEST){
                return do_request();
            }
            // 这里是不是应该在括号里
            line_state = LINE_OPEN;
            break;
        default:
            return INTERNAL_ERROR;
        }
    }
}


// 根据得到的请求内容处理 登录注册 发送文件等
REQUEST_STATE http::do_request(){
    strcpy(native_request_url,workdir);
    int len = strlen(doc_root);
    printf("m_url:%s\n", request_url);
    const char* p = strrchr(request_url,'/');
    //注册页面
    if(*(p+1) == '0'){
        char* tail_url = (char*) malloc(sizeof(char)*200);
        strcpy(tail_url,"/register.html");
        strncpy(native_request_url,tail_url,strlen(tail_url));
        free(tail_url);
    }
    // 登录页面
    else if(*(p+1) == '1'){
        char* tail_url = (char*) malloc(sizeof(char)*200);
        strcpy(tail_url,= "/log.html";
        strncpy(native_request_url,tail_url,strlen(tail_url));
        free(tail_url);
    }
    // 登录
    else if(*(p+1) == '2'){
        char* tail_url  = (char*) malloc(sizeof(char)*200);
        strcpy(tail_url,"/");
        // 这里为什么要加2
        strcat(tail_url,request_url+2);
        //这里修改了
        strncpy(native_request_url,tail_url,strlen(tail_url));
        free(tail_url);

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
        if(users.count(name) && users[name] == password){
            strcpy(request_url,"/welcome.html");
        } else {
            strcpy(request_url,"/logError.html");
        } 
    } 
    // 注册
    else if(*(p+1) == '3'){
        char* tail_url  = (char*) malloc(sizeof(char)*200);
        strcpy(tail_url,"/");
        // 这里为什么要加2
        strcat(tail_url,request_url+2);
        //这里修改了
        strncpy(native_request_url,tail_url,strlen(tail_url));
        free(tail_url);

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

        std::string  sql_insert = " INSERT INTO user(username, passwd) VALUES(' ";
        sql_insert += name;
        sql_insert += " ',' ";
        sql_insert += password;
        sql_insert += " ') ";

        if(!users.count(name)){
            m_lock.lock();
            int res = mysql_query(mysqlconn,sql_insert);
            users.insert({name,password});
            m_lock.unlock();
            if(res==0){
                strcpy(request_url,"/log.html");
            } else {
                strcpy(request_url,"/welcome.html");
            }
        } else {
            strcpy(request_url,"/registerError.html");
        } 
    }
    // 请求图片
    else if(*(p+1) == '5'){
        char* tail_url = (char*) malloc(sizeof(char)*200);
        strcpy(tail_url,= "/picture.html";
        strncpy(native_request_url,tail_url,strlen(tail_url));
        free(tail_url);
    }
    // 请求视频
    else if(*(p+1) == '6'){
        char* tail_url = (char*) malloc(sizeof(char)*200);
        strcpy(tail_url,= "/video.html";
        strncpy(native_request_url,tail_url,strlen(tail_url));
        free(tail_url);
    }
    // 请求视频
    else if(*(p+1) == '7'){
        char* tail_url = (char*) malloc(sizeof(char)*200);
        strcpy(tail_url,= "/fans.html";
        strncpy(native_request_url,tail_url,strlen(tail_url));
        free(tail_url);
    }
    // 其他情况
    else{
        strncpy(native_request_url + len, request_url, strlen(request_url));
    }
    int ret = stat(native_request_url,&file_stat);
    if(ret<0){
        return NO_RESOURCE;
    } 
    // 其他用户可读
    if(!(file_stat.st_mode && S_IROTH)){
        return FORBIDDEN_REUQEST;
    }
    if(S_ISDIR(file_stat.st_mode)){
        return BAD_REQUEST;
    }

    int fd = open(native_request_url,O_RDONLY);
    mmap_addr = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

// 取消mmap映射
void http::unmmap(){
    if(mmap_addr){
        munmap(mmap_addr,file_stat.st_size);
        mmap_addr = 0; 
    }
}


//按给定格式写一行内容到写缓存
bool http::add_line(const char* format, ...){
    if(cur_wr_idx>= write_buf_size){
        return false;
    }
    va_list va;
    va_start(va,format);
    int len = vsnprintf(write_buf+cur_wr_idx,write_buf_size-cur_wr_idx-1,format,va);
    if(len >= (write_buf_size-cur_wr_idx-1) ){
        va_end(va);
        return false;
    }
    cur_wr_idx += len;
    va_end(va);
    LOG_INFO("request:%s", write_buf);
    return true;
}
//写状态行
bool http::add_statusline(int status,const char* detail){
    return add_line("%s %d %s\r\n", "HTTP/1.1", status, detail);
}
// 写首部字段
bool http::add_header(int content_len){
    return add_content_len(content_len) && add_keepalive() && add_blank_line();
}
//写空白行
bool http::add_blank_line(){
    return add_line("%s", "\r\n");
}
// 写长度
bool http::add_content_len(int size){
    return add_line("Content-Length:%d\r\n", size);
}
// 添加数据类型
bool http::add_content_type(){
    return add_line("Content-Type:%s\r\n", "text/html");
}
// 添加是否长连接
bool http::add_keepalive(){
    return add_line("Connection:%s\r\n", (KeepAlive == true) ? "keep-alive" : "close");
}
// 写内容
bool http::add_content(const char* content){
     return add_line("%s", content);
}
// 判断是否是proactor
bool http::isProactor(){
    return actor_model == proactor;
}