#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
//#include <unistd.h>
#include <windows.h>
#include <condition_variable>

/* 
 * 多消费者,多生产者(但任意时刻只能有1个人去操作queue)
 * 注意:    !!!!!!
 * 这里是只用一个大锁来进行控制,这样的话是出现这种情况
 * 就是我生产者生产完了 , 并且通知消费者了. 然后进行unlock,此时的锁的竞争情况为全体的生产者和全体的消费者
 * 一起竞争该锁.由于生产者while循环之后就是获得该锁 , 而消费者是cond.wait()返回时获得该锁,很大程度上做测试
 * 仍然是生产者继续获得该锁
 */
using namespace std;
mutex mtx;
condition_variable produce_cond, consume_cond;
queue<int> q;
const int MaxSize = 50;

//消费者
void consumer() {
  while (true) {
    //锁住,进入临界区
    unique_lock<mutex> lck(mtx);
    while (q.size() == 0) {
      //此时首先释放锁,再等待.等待成功后继续占有锁
      consume_cond.wait(lck);  //防止虚假唤醒
    }
    //另一种写法
    // consume.wait(lck, [] {return q.size() != 0; });     // wait(block)
    // consumer until q.size() != 0 is true
    Sleep(200);
    cout << "consumer " << this_thread::get_id() << ": ";
    q.pop();
    // do something消费
    cout << q.size() << "\n";

    //队列不在是满的,通知生产者
    //注意这里notify_all,仍是只有1个生产者能生产(因为所有生产者都去争用mtx锁)
    produce_cond.notify_all();  // nodity(wake up) producer when q.size() !=
                                // maxSize is true
    //离开临界区
    lck.unlock();
  }
}

void producer(int id) {
  // this_thread::sleep_for(chrono::milliseconds(900));
  // producer is a little faster than consumer
  while (true) {
    //进入临界区
    unique_lock<mutex> lck(mtx);
    //unique_lock<mutex> lck(produce_mtx);
    while (q.size() == MaxSize) {
      produce_cond.wait(lck);
    }
    // produce.wait(lck, [] {return q.size() != maxSize; });
    // wait(block) producer until q.size() != maxSize is true

    Sleep(200);
    cout << "-> producer " << this_thread::get_id() << ": ";
    q.push(id);
    cout << q.size() << '\n';
    //已经有商品被产生了
    consume_cond.notify_all();
    lck.unlock();
  }
}

int main(){
    thread consumers[2],producers[2];
    for (int i = 0; i < 2; i++)
    {
        consumers[i] = thread(consumer);
        //thread(Function, Args... args);
        //producers[i] = thread(producer,i+1);
    }
    for (int i = 0; i < 2; i++)
    {
        //consumers[i] = thread(consumer);
        //thread(Function, Args... args);
        producers[i] = thread(producer,i+1);
    }

    //2个生产者2个消费者
    for (int i = 0; i < 2; ++i)
    {
        producers[i].join();
        consumers[i].join();
    }
    return 0;
}