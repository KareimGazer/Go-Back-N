/*
you may need to add another reciever network buffer
*/

#include <iostream>
#include <list>
#include <string>
#include <algorithm>
#include <bitset>
#include <fstream>
/*make the counting function of the timer counts in the background so we have to add new thread */
#include <thread>
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif
using namespace std;
/**********************************************************************
*                           Definitions                               *
***********************************************************************/

/* we will assume that the time out value will be 10*/
#define TIME_OUT_VALUE 5
// #define MAX_PKT 1024
typedef unsigned int seq_nr;
// typedef struct {unsigned char data[MAX_PKT];} packet;
typedef unsigned char packet;
typedef enum { frame_arrival, cksum_err, timeout, network_layer_ready, end_of_transaction } event_type;
// useless in go-back-N
//typedef enum {data, ack, nak} frame_kind;
typedef struct {
    /* frames are transported in this layer */
    /* frame_kind kind; /* what kind of frame is it? data or control only*/
    seq_nr seq; /* sequence number */
    seq_nr ack; /* acknowledgement number */
    packet info; /* a single network layer packet */
} frame;

typedef struct {
    seq_nr s;
    uint32_t curr_timeout_value;
    bool time_out;
    bool timer_enable;

}Timer_task;
/**********************************************************************
*                           Global Variables                          *
***********************************************************************/

/* list of unsigned char is taken from the network layer*/
list<packet> network_layer_buffer;
bool networkLayerReady = false;
/* this variable will hold the timer value*/
bool is_timer_on = false;
bool time_out = false;
list<string> physical_layer_buffer;
// not in the book but in case of excessive use
// bool physicalLayerReady = false;
string myBuffer = "mybuffer.txt";
string peerBuffer = "peerbuffer.txt";
ifstream physicalBuffer(myBuffer);
string key = "1001";
/* for lock and unlock*/
atomic_flag locked;
/* if more then one frame has a timer at the same time*/
Timer_task timer_task;
/**********************************************************************
*                           Functions prototype                       *
***********************************************************************/
void lock();
void unlock();
void enable_network_layer(void);
void disable_network_layer(void);
void setupNetworkLayer(string message);
void showNetworkLayer();
void from_network_layer(packet* p);
void to_network_layer(packet* p);

// physical layer helper functions
string packet2Binary(packet p);
packet binary2Packet(string binary);
string frame2Binary(frame f);
frame binary2frame(string binary); // to be done
string repeat(string s, int n);
string xor1(string a, string b);
string mod2div(string divident, string divisor);
string encodeData(string data, string key);
void printFrame(frame f); // debugging

void wait_for_event(event_type* event); // last

void from_physical_layer(frame* r);
void to_physical_layer(frame* s);
/*          Timer Functions           */
void startTimer(seq_nr seq);
void stopTimer(seq_nr seq);
void TimerInterrupt1();
void TimerInterrupt();

int main() {
    /*Timer test*/
  /*  startTimer(1);
    thread th1(TimerInterrupt);
    Sleep(20);
    stopTimer(1);
    th1.join();*/


    /* string message;
     cout << "please, enter your message: " << endl;
     cin >> message;
     setupNetworkLayer(message);
     showNetworkLayer();*/
     /* take the packet from the networl layer*/
    frame f;
    f.seq = 1;
    f.ack = 2;
    f.info = 'A';
    frame newFrame;
    event_type event;

    string data = frame2Binary(f); // Some string.
    cout << "frame data: " << data << endl;

    string encoded = encodeData(data, key);
    cout << "encodede data: " << encoded << endl;
    /* Start Timer Function with sq_num*/
    startTimer(1);
    thread th1(TimerInterrupt);
    /* send the frame on the physical layer*/
    to_physical_layer(&f);
    Sleep(500);
    string rem = mod2div(encoded, key);
    cout << "rem: " << rem << endl;
    /* wait until the event received (time out or */
    wait_for_event(&event);
    stopTimer(1);
    th1.join();
    cout << "event: " << event << endl;
    if (event == end_of_transaction) {
        /*finish the program */
        physicalBuffer.close();
        return 0;
    }
    from_physical_layer(&newFrame);
    printFrame(newFrame);

    physicalBuffer.close();
    return 0;
}
/*********************************************************************
*                           Function Definitions                     *
**********************************************************************/
void enable_network_layer(void) {
    networkLayerReady = true;
}
void disable_network_layer(void) {
    networkLayerReady = false;
}

void setupNetworkLayer(string message) {
    for (packet p : message) to_network_layer(&p);
}

void showNetworkLayer() {
    cout << "printing network layer: " << endl;
    cout << "PCKT_NUM\tPCKT" << endl;
    int i = 0;
    packet p;
    while (!network_layer_buffer.empty()) {
        from_network_layer(&p);
        cout << i << "\t" << p << endl;
        i++;
    }
}

void from_network_layer(packet* p) {
    if (networkLayerReady) {
        *p = network_layer_buffer.front();
        network_layer_buffer.pop_front();
        /*if the network buffer is empty , disable the network layer*/
        if (network_layer_buffer.size() == 0) disable_network_layer();
    }
    else *p = '\0';
}
void to_network_layer(packet* p) {
    network_layer_buffer.push_back(*p);
    enable_network_layer();
}

string packet2Binary(packet p) {
    string result = "";
    for (int i = 7; i >= 0; --i) result += ((p & (1 << i)) ? '1' : '0');
    return result;
}

packet binary2Packet(string binary) {
    return static_cast<packet>(bitset<8>(binary).to_ulong());
}

string frame2Binary(frame f) {
    // casting so they can be sent
    packet seq = f.seq + '0';
    packet ack = f.ack + '0';
    packet info = f.info;
    return packet2Binary(seq) + packet2Binary(ack) + packet2Binary(info);
}

frame binary2frame(string binary) {
    frame f;
    f.seq = binary2Packet(binary.substr(0, 8)) - '0';
    f.ack = binary2Packet(binary.substr(8, 8)) - '0';
    f.info = binary2Packet(binary.substr(16, 8));
    return f;
}

string repeat(string s, int n) {
    string s1 = s;
    for (int i = 1; i < n; i++) s += s1;
    return s;
}

string xor1(string a, string b) {
    string result = "";
    int n = b.length();
    for (int i = 1; i < n; i++) {
        if (a[i] == b[i]) result += "0";
        else result += "1";
    }
    return result;
}
/* this function is used to calculate CRC*/
string mod2div(string divident, string divisor) {
    int pick = divisor.length();
    string tmp = divident.substr(0, pick);
    int n = divident.length();
    while (pick < n) {
        if (tmp[0] == '1') tmp = xor1(divisor, tmp) + divident[pick];
        else tmp = xor1(std::string(pick, '0'), tmp) + divident[pick];
        pick += 1;
    }
    if (tmp[0] == '1') tmp = xor1(divisor, tmp);
    else tmp = xor1(std::string(pick, '0'), tmp);
    return tmp;
}

string encodeData(string data, string key) {
    int l_key = key.length();
    string appended_data = (data + std::string(l_key - 1, '0'));
    string remainder = mod2div(appended_data, key);
    string codeword = data + remainder;
    return codeword;
}

void printFrame(frame f) {
    cout << "printing frame: " << endl;
    cout << "seqNum: " << f.seq << endl;
    cout << "ACKnum: " << f.ack << endl;
    cout << "Info: " << f.info << endl;
}

void from_physical_layer(frame* r) {
    // get string from buffer
    // make the frame
    // shrink buffer
    // if buffer.size == 0
    // disable
    string data;
    if (!physical_layer_buffer.empty()) {
        data = physical_layer_buffer.front();
        // cout << data << endl;
        physical_layer_buffer.pop_front();
        *r = binary2frame(data);
    }
}
void to_physical_layer(frame* s) {
    string data = frame2Binary(*s);
    string encodedData = encodeData(data, key);
    std::ofstream outfile;
    outfile.open(peerBuffer, std::ios_base::app); // append instead of overwrite
    outfile << encodedData << "\n";
}
void lock() {
    while (locked.test_and_set(std::memory_order_acquire)) {
    }
}
void unlock() {
    locked.clear(std::memory_order_release);
}
// timeout to be added after timers
void wait_for_event(event_type* event) {

    string textLine = ""; // should be without \n

    while (1) {
        if (networkLayerReady) {
            *event = network_layer_ready;
            break;
        }
        else if (getline(physicalBuffer, textLine)) {

            string rem = mod2div(textLine, key);
            if (rem == "000") {
                *event = frame_arrival;
                physical_layer_buffer.push_back(textLine);
                // cout << textLine << endl;
            }
            else *event = cksum_err;
            break;
        }
        else if (is_timer_on) {
            Sleep(1);
            if (!time_out)
                continue;
            else {
                *event = timeout;
                break;
            }
        }
        else {
            /*that means there is no timer*/
            *event = end_of_transaction;
        }

    }

}
void TimerInterrupt1() {
    /*this is the critical section*/
    lock();
    if (timer_task.timer_enable == true) {
        if (timer_task.curr_timeout_value == 0) {
            timer_task.time_out = true;
            time_out = true;
            cout << " Timer is expired " << endl;
        }
        else {
            timer_task.curr_timeout_value--;
        }
    }

    unlock();
}
/*generate new thread for this function as it must run in the background to count the time */
void TimerInterrupt() {
    /*Generate Timer Interrupt every 1msec */
    static uint32_t counter = 0;
    /* this flag to break the infinite loop*/
    char flag = 0;
    while (1) {
        cout << counter++ << endl;
        TimerInterrupt1();
        if (timer_task.timer_enable == false || timer_task.time_out) {
            break;
        }
        /*sleep one ms*/
        Sleep(1);
    }
    return;
}
void startTimer(seq_nr seq) {
    is_timer_on = true;
    lock();
    Timer_task t;
    t.timer_enable = true;
    t.s = seq;
    t.curr_timeout_value = TIME_OUT_VALUE;
    t.time_out = false;
    timer_task = t;
    unlock();
}
void stopTimer(seq_nr seq) {
    lock();
    char flag = 0;

    /* this loop will disable the timer */
    if (timer_task.s == seq) {
        timer_task.timer_enable = false;
        is_timer_on = false;
    }

    unlock();
}