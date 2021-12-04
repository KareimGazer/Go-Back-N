/* added another reciever network buffer */

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

// #define MAX_PKT 1024
#define MAX_SEQ 7
/* we will assume that the time out value will be 10*/
#define TIME_OUT_VALUE 5

typedef unsigned int seq_nr;
// typedef struct {unsigned char data[MAX_PKT];} packet;
typedef unsigned char packet;
typedef enum {frame_arrival, cksum_err, timeout, network_layer_ready, end_of_transaction} 
event_type;

// useless in go-back-N
//typedef enum {data, ack, nak} frame_kind;
typedef struct { /* frames are transported in this layer */
  // frame_kind kind; /* what kind of frame is it? data or control only*/
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

list<packet> network_layer_sender_buffer;
list<packet> network_layer_receiver_buffer;
bool networkLayerReady = false;

bool in_transaction = false;
string outputMessage = "";
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
void showNetworkLayer(list<packet> buffer);
void from_network_layer(packet * p);
void to_network_layer(packet * p);

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

void wait_for_event(event_type *event); // last

void from_physical_layer(frame *r);
void to_physical_layer(frame *s);

bool between(seq_nr a, seq_nr b, seq_nr c);
void protocol5(void);
void inc(seq_nr & k);

/*          Timer Functions           */
void start_timer(seq_nr seq);
void stop_timer(seq_nr seq);
void TimerInterrupt1();
void TimerInterrupt();

int main() {
  string message;
  cout << "please, enter your message: " << endl;
  //cin >> message;
  getline(cin, message);
  setupNetworkLayer(message);
  showNetworkLayer(network_layer_sender_buffer);
  in_transaction = true;
  
  protocol5();

  showNetworkLayer(network_layer_receiver_buffer);

  cout << endl << "programe received: " << endl;

  cout << outputMessage << endl;

  physicalBuffer.close();
  return 0;
}

void enable_network_layer(void){
  networkLayerReady = true;
}
void disable_network_layer(void){
  networkLayerReady = false;
}

void setupNetworkLayer(string message){
  for(packet p : message) network_layer_sender_buffer.push_back(p);
}

void showNetworkLayer(list<packet> buffer){
  outputMessage = "";
  cout << endl <<"printing network layer: " << endl;
  cout << "PCKT_NUM\tPCKT" << endl;
  packet p;
  int i = 0;
  for(auto it= buffer.begin(); it != buffer.end(); it++){
    // from_network_layer(&p);
    p = *it;
    outputMessage += p;
    cout << i <<"\t"<< p <<endl;
    i++;
  }
}

void from_network_layer(packet * p){
  if(network_layer_sender_buffer.empty()) disable_network_layer();
  if(networkLayerReady){
    *p = network_layer_sender_buffer.front();
    cout << "getting '" << *p << "' from network layer" << endl;
    network_layer_sender_buffer.pop_front();
    if(network_layer_sender_buffer.empty()) disable_network_layer();
  }
  else *p = '\0';
}
void to_network_layer(packet * p){
  cout << "sending '" << *p << "' to network layer" << endl;
  network_layer_receiver_buffer.push_back(*p);
  // enable_network_layer();
}

string packet2Binary(packet p) {
  string result = "";
  for (int i = 7; i >= 0; --i) result += ((p & (1 << i))? '1' : '0');
  return result;
}

packet binary2Packet(string binary){
  return static_cast<packet>(bitset<8>(binary).to_ulong());
}

string frame2Binary(frame f){
  // casting so they can be sent
  packet seq = f.seq + '0'; 
  packet ack = f.ack + '0'; 
  packet info = f.info;
  return packet2Binary(seq) + packet2Binary(ack) + packet2Binary(info);
}

frame binary2frame(string binary){
  frame f;
  f.seq = binary2Packet(binary.substr(0, 8)) - '0';
  f.ack = binary2Packet(binary.substr(8, 8)) - '0';
  f.info = binary2Packet(binary.substr(16, 8));
  return f;
}

string repeat(string s, int n){
  string s1 = s;
  for(int i=1; i<n;i++) s += s1;
  return s;
}

string xor1(string a, string b){
  string result = "";
  int n = b.length();
  for(int i = 1; i < n; i++){
    if (a[i] == b[i]) result += "0";
    else result += "1";
  }
  return result;
}

string mod2div(string divident, string divisor){
  int pick = divisor.length();
  string tmp = divident.substr(0, pick);
  int n = divident.length(); 
  while (pick < n){
    if (tmp[0] == '1') tmp = xor1(divisor, tmp) + divident[pick];
    else tmp = xor1(std::string(pick, '0'), tmp) + divident[pick];
    pick += 1;
  }
  if (tmp[0] == '1') tmp = xor1(divisor, tmp);
  else tmp = xor1(std::string(pick, '0'), tmp);      
  return tmp;
}

string encodeData(string data, string key){
  int l_key = key.length();
  string appended_data = (data + std::string(l_key - 1, '0'));
  string remainder = mod2div(appended_data, key);
  string codeword = data + remainder;
  return codeword;
}

void printFrame(frame f){
  cout << "printing frame: " << endl;
  cout << "seqNum: " << f.seq << endl;
  cout << "ACKnum: " << f.ack << endl;
  cout << "Info: " << f.info << endl; 
}

void from_physical_layer(frame *r){
  string data;
  if(!physical_layer_buffer.empty()){
    data = physical_layer_buffer.front();
    // cout << data << endl;
    physical_layer_buffer.pop_front();
    *r = binary2frame(data);
  }
}

void to_physical_layer(frame *s){
  cout << "data payload: " << s->info << endl;
  string data = frame2Binary(*s);
  string encodedData = encodeData(data, key);
  std::ofstream outfile;
  outfile.open(peerBuffer, std::ios_base::app); // append instead of overwrite
  outfile << encodedData << "\n";
}

void lock() {while (locked.test_and_set(std::memory_order_acquire)) {}}
void unlock() {locked.clear(std::memory_order_release);}

void wait_for_event(event_type *event){
  string textLine = ""; // should be without \n
  while(1){
    if(networkLayerReady){
      *event = network_layer_ready;
      break;
    }
    else if(getline(physicalBuffer, textLine)){ // exceutes if not EOF
      string rem = mod2div(textLine, key);
      if(rem == "000") {
        *event = frame_arrival;
        physical_layer_buffer.push_back(textLine);
        // cout << textLine << endl;
      }
      else *event = cksum_err;
      break;
    }
    else if (is_timer_on) {
      cout << "timer state: "<< is_timer_on << endl;
      sleep(1);
      if (!time_out) continue;
      else {
        *event = timeout;
        break;
      }
    }
    else {
      *event = end_of_transaction;
      break;
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
        sleep(1);
    }
    return;
}

void start_timer(seq_nr seq) {
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

void stop_timer(seq_nr seq) {
    lock();
    char flag = 0;

    /* this loop will disable the timer */
    if (timer_task.s == seq) {
        timer_task.timer_enable = false;
        is_timer_on = false;
    }

    unlock();
}

/* Return true if a <= b < c circularly; false otherwise. */
bool between(seq_nr a, seq_nr b, seq_nr c) {
  if (((a <= b) && (b < c)) || ((c < a) && (a <= b)) || ((b < c) && (c < a)))
    return(true);
  else
    return(false);
}

/* Construct and send a data frame. */
void send_data(seq_nr frame_nr, seq_nr frame_expected, packet buffer[ ]) {
  cout << "sending data" << endl;
  frame s; /* scratch variable */
  s.info = buffer[frame_nr]; /* insert packet into frame */
  s.seq = frame_nr; /* insert sequence number into frame */
  s.ack = (frame_expected + MAX_SEQ) % (MAX_SEQ + 1);/* piggyback ack */
  to_physical_layer(&s); /* transmit the frame */
  start_timer(frame_nr); /* start the timer running */
}

void protocol5(void) {
  seq_nr next_frame_to_send; /* MAX SEQ > 1; used for outbound stream */
  seq_nr ack_expected; /* oldest frame as yet unacknowledged */
  seq_nr frame_expected; /* next frame expected on inbound stream */
  frame r; /* scratch variable */
  packet buffer[MAX_SEQ + 1]; /* buffers for the outbound stream */
  seq_nr nbuffered; /* number of output buffers currently in use */
  seq_nr i; /* used to index into the buffer array */
  event_type event;
  // now needed global = false
  enable_network_layer(); /* allow network layer ready events */
  ack_expected = 0; /* next ack expected inbound */
  next_frame_to_send = 0; /* next frame going out */
  frame_expected = 0; /* number of frame expected inbound */
  nbuffered = 0; /* initially no packets are buffered */
  while (in_transaction) {
    wait_for_event(&event);
    cout << endl << "event: " << event << endl;
    switch(event) {
      case network_layer_ready: /* the network layer has a packet to send */
        /* Accept, save, and transmit a new frame. */
        from_network_layer(&buffer[next_frame_to_send]); /* fetch new packet */
        nbuffered = nbuffered + 1; /* expand the sender’s window */
        send_data(next_frame_to_send, frame_expected, buffer); /* transmit the frame */
        inc(next_frame_to_send); /* advance sender’s upper window edge */
        break;

      case frame_arrival: /* a data or control frame has arrived */
        from_physical_layer(&r); /* get incoming frame from physical layer */
        // printFrame(r);
        if (r.seq == frame_expected) {
          /* Frames are accepted only in order. */
          to_network_layer(&r.info); /* pass packet to network layer */
          inc(frame_expected); /* advance lower edge of receiver’s window */
        }
        // cout << "ACK: " << r.ack << " ------------" << endl;
        /* Ack n implies n − 1, n − 2, etc. Check for this. */
        while (between(ack_expected, r.ack, next_frame_to_send)) {
          /* Handle piggybacked ack. */
          nbuffered = nbuffered - 1; /* one frame fewer buffered */
          stop_timer(ack_expected); /* frame arrived intact; stop timer */
          cout << "ACKed: " << ack_expected << " ------------" <<endl;
          cout << "is timer on? " << is_timer_on << endl;
          inc(ack_expected); /* contract sender’s window */
        }
        break;

      case cksum_err: 
        cout << "checksum error" << endl;
        break; /* just ignore bad frames */

      case timeout: /* trouble; retransmit all outstanding frames */
        next_frame_to_send = ack_expected; /* start retransmitting here */
        for (i = 1; i <= nbuffered; i++) {
          send_data(next_frame_to_send, frame_expected, buffer);
        inc(next_frame_to_send); /* prepare to send the next one */
      }
      break;

      case end_of_transaction:
       in_transaction = false;
       cout << "transaction ended" << endl;
       break;
    }
    //if (nbuffered < MAX_SEQ) enable_network_layer();
   // else disable_network_layer();
  }
}

void inc(seq_nr & k){
  if (k < MAX_SEQ) k = k + 1; else k = 0;
}
