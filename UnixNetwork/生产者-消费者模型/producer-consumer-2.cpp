/**
 * 要求：
 * 1、多线程生产者、多线程消费者
 * 2、当队列超过>100时，生产者停止生产，队列<20，消费者停止消费
 */

#include <iostream>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <thread>

using namespace std;

queue<int> q;
mutex mtx;  //consumer和producer互斥访问queue 
mutex mtx_producer_counter;
mutex mtx_consume_counter;
condition_variable less20_cond,greater100_cond;

void produce(int x) {
    while (true)
    {
        //unique_lock<mutex> lck(mtx_producer_counter);
        unique_lock<mutex> lck(mtx);
        //>=100睡眠,<100时唤醒
        while (q.size()>=100)
        {
            cout<<this_thread::get_id()<<" producer stop here"<<endl;
            greater100_cond.wait(lck);
        }
        //unique_lock<mutex> lck2(mtx);
        //可以生产
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
        unique_lock<mutex> lck(mtx);
        //>20时唤醒
        while (q.size()<=20)
        {
            cout<<this_thread::get_id()<<" consume stop here"<<endl;
            less20_cond.wait(lck);
        }
        //可以消费
        q.pop();
        cout<<"-> consumer-"<<this_thread::get_id()<<" ==> "<<q.size()<<endl;

        //离开临界区
        //只有少于100才可以唤醒生产者让他生产
        if(q.size()<100) greater100_cond.notify_all();
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