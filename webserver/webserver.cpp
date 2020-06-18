

#include"../http/http.h"


// 线程池的工作函数  
void http_work_func(http* conn){
    // reactor
    if(!conn->isProactor()){
        int ret = conn->read_once();
        if(ret){
            conn->process();
        }
        ret = conn->write_to_socket();
    }
    // proactor
    else{
        conn->process();
    }
}
