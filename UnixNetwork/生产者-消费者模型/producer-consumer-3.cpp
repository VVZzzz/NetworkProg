/**
 * 要求：
 * 1、多线程生产者、多线程消费者
 * 2、当队列超过>100时，生产者停止生产，队列<20，消费者停止消费
 */
/**
 * 这个和2不同,生产者生产完了,先通知消费者,首先让消费者选出一个线程.然后这个线程现在阻塞在要操作queue的锁那里
 * 此时生产者通知完,在放开queue锁,这个时候消费者线程会率先得到这个queue锁,因为全体生产者还要先竞争
 * produce_mutex_cond锁
 * 
 */
/**
 * TODO: 其实还有两个地方可以考虑:
 * 1. 能不能多个生产者同时操作queue(生产者和消费者互斥,生产者和生产者不互斥),假如说
 * 这个不是个queue,而是一个数组,多个生产者直接操作数组不同的位置是不是也可以.
 * 2. 能不能生产者生产的同时,消费者同时消费(同样是只要不在同一个区域)
 * 答: 其实将mtx去除之后即可,这样可以允许一个生产者和一个消费者同时进行push和pop,其实这里
 * 换成数组int[]更好理解 , 生产者操作int[i],消费者操作int[j].同时操作是可以的 , 这样就不用
 * 先等int[i],再等int[j]了
 */

#include <iostream>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <thread>
#include <windows.h>

using namespace std;

queue<int> q;
mutex mtx;  //consumer和producer互斥访问queue 
mutex mtx_producer_counter;  //只有1个消费者消费
mutex mtx_consume_counter;  //只有1个生产线程生产
condition_variable less20_cond,greater100_cond;

void produce(int x) {
    while (true)
    {
        unique_lock<mutex> lck(mtx_producer_counter);
        //unique_lock<mutex> lck(mtx);
        //>=100睡眠,<100时唤醒
        while (q.size()>=100)
        {
            greater100_cond.wait(lck);
        }
        //unique_lock<mutex> lck2(mtx);
        //可以生产
        //Sleep(200);
        q.push(x);
        cout<<"producer-"<<this_thread::get_id()<<" ==> "<<q.size()<<endl;
        //只有已经超过20个产品之后才可以唤醒消费者
        if(q.size()>20) less20_cond.notify_all();
        //lck2.unlock();
        lck.unlock();
    }
}

void consume() {
    while (true)
    {
        unique_lock<mutex> lck(mtx_consume_counter);
        //>20时唤醒
        while (q.size()<=20)
        {
            less20_cond.wait(lck);
        }
        //unique_lock<mutex> lck2(mtx);
        //可以消费
        //Sleep(200);
        q.pop();
        cout<<"-> consumer-"<<this_thread::get_id()<<" ==> "<<q.size()<<endl;
        //离开临界区
        //只有少于100才可以唤醒生产者让他生产
        if(q.size()<100) greater100_cond.notify_all();
        //lck2.unlock();
        lck.unlock();
    }
}

int main(){
    thread consumers[3],producers[3];
    for (int i = 0; i < 3; i++)
    {
        consumers[i] = thread(consume);
        //thread(Function, Args... args);
        producers[i] = thread(produce,i+1);
    }

    //3个生产者3个消费者
    for (int i = 0; i < 3; ++i)
    {
        producers[i].join();
        consumers[i].join();
    }
    return 0;
}