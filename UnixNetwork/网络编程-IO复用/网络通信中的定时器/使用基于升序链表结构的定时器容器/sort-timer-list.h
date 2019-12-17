/**
 * 基于升序链表的定时器
 * 即一个程序中可能需要设置多个定时器事件,为了不相互影响,且有效管理
 * 使用基于升序链表的结构去管理它们
 * 时间复杂度为: 添加定时器O(n),删除定时器O(1),执行定时任务O(1)
 */
#ifndef LST_TIMER
#define LST_TIMER
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#define BUFFER_SIZE 64
class util_timer;

//用户数据结构
struct client_data {
  sockaddr_in address;    //客户端socket地址
  int sockfd;             // socket
  char buf[BUFFER_SIZE];  //读缓冲
  util_timer *timer;      //定时器
};

//定时器类
class util_timer {
 public:
  util_timer() : prev(nullptr), next(nullptr) {}

 public:
  time_t expire;                   //定时器的超时时间,用绝对时间
  void (*cb_func)(client_data *);  //任务回调函数
  client_data *user_data;          //用户数据
  util_timer *prev;                //上一个定时器
  util_timer *next;                //下一个定时器
};

//定时器链表类: 升序,双链表,带有头,尾指针
class sort_timer_list {
 public:
  sort_timer_list() : head(nullptr), tail(nullptr) {}
  ~sort_timer_list() {
    util_timer *temp = head;
    while (temp) {
      head = temp->next;
      delete temp;
      temp = head;
    }
  }

  //添加定时器到链表中
  void add_timer(util_timer *timer) {
    if (!timer) return;
    if (!head) {
      head = tail = timer;
      return;
    }
    //当前定时器超时时间若最小
    if (timer->expire < head->expire) {
      timer->next = head;
      head->prev = timer;
      head = timer;
      return;
    }
    //否则添加到合适位置(调用重载的addtimer)
    add_timer(timer, head);
  }

  //若某个定时任务发送变换,调整它的定时器在链表中的位置
  //该函数只考虑定时任务超时时间延长的情况
  void adjust_timer(util_timer *timer) {
    if (!timer) return;

    util_timer *temp = timer->next;
    //若该节点是尾节点或不小于它的next超时时间,则不变
    if (!temp || timer->expire < temp->expire) {
      return;
    }
    //如果该定时器是头节点,则去掉重新插入
    if (timer == head) {
      head = head->next;
      head->prev = NULL;
      timer->next = NULL;
      add_timer(timer);
    } else {
      //否则,将这个节点去掉插入到位于原本位置之后的链表中
      timer->prev->next = timer->next;
      timer->next->prev = timer->prev;
      add_timer(timer, timer->next);
    }
  }

  void del_timer(util_timer *timer) {
    if (!timer) return;
    if (timer == head && timer == tail) {
      delete timer;
      head = tail = timer = nullptr;
      return;
    }
    if (timer == head) {
      head = head->next;
      head->prev = nullptr;
      delete timer;
      timer = nullptr;
      return;
    }
    if (timer == tail) {
      tail = tail->prev;
      tail->next = nullptr;
      delete timer;
      timer = nullptr;
      return;
    }
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
    timer = nullptr;
  }

  // SIGALRM信号每次触发,都要在信号处理函数中(统一信号源在主循环中)执行此函数
  //将已到达超时时间的定时任务执行,并从链表中删除
  void tick() {
    if (!head) return;
    std::cout << "timer tick" << std::endl;
    //获得系统当前时间
    time_t cur = time(NULL);
    util_timer *temp = head;
    while (temp) {
      //如果没到超时时间,就break,因为链表是升序排列的
      if (temp->expire > cur) break;
      //执行定时任务
      temp->cb_func(temp->user_data);
      head = temp->next;
      if (head) head->prev = nullptr;
      delete temp;
      temp = head;
    }
  }

 private:
  //工具函数,将timer添加到lst_head之后的链表部分中
  void add_timer(util_timer *timer, util_timer *lst_head) {
    if (!timer) return;
    util_timer *pre = lst_head;
    util_timer *temp = pre->next;
    while (temp) {
      if (temp->expire < timer->expire) {
        pre = temp;
        temp = temp->next;
      } else {
        pre->next = timer;
        temp->prev = timer;
        timer->prev = pre;
        timer->next = temp;
        break;
      }
    }

    if (!temp) {
      pre->next = timer;
      timer->prev = pre;
      timer->next = nullptr;
      tail = timer;
    }
  }

 private:
  util_timer *head;
  util_timer *tail;
};

#endif
