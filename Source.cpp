#include <iostream>
#include <vector>
#include <queue>
#include <algorithm>
#include <fstream>
#include <string>

#pragma once
#define MAX_PKT 1024 /* determines packet size in bytes */
typedef enum { False, True } boolean; /* boolean type */
typedef unsigned int seq_nr; /* sequence or ack numbers */
typedef struct { unsigned char data; } packet; /* packet definition */
typedef enum { data, ack, nak } frame_kind; /* frame kind definition */
typedef enum { frame_arrival, cksum_err, timeout, network_layer_ready, END_OF_PROT } event_type;
typedef struct
{ /* frames are transported in this layer */
	frame_kind kind; /* what kind of frame is it? */
	seq_nr seq; /* sequence number */
	seq_nr ack; /* acknowledgement number */
	packet info; /* the network layer packet */
} frame;
/* Wait for an event to happen; return its type in event. */
void wait_for_event(event_type *event);
/* Fetch a packet from the network layer for transmission on the channel. */
void from_network_layer(packet *p);
/* Deliver information from an inbound frame to the network layer. */
void to_network_layer(packet *p);
/* Go get an inbound frame from the physical layer and copy it to r. */
void from_physical_layer(frame *r);
/* Pass the frame to the physical layer for transmission. */
void to_physical_layer(frame *s);
/* Start the clock running and enable the timeout event. */
void start_timer(seq_nr k);
/* Stop the clock and disable the timeout event. */
void stop_timer(seq_nr k);
/* Start an auxiliary timer and enable the ack timeout event. */
void start_ack_timer(void);
/* Stop the auxiliary timer and disable the ack timeout event. */
void stop_ack_timer(void);
/* Allow the network layer to cause a network layer ready event. */
void enable_network_layer(void);
/* Forbid the network layer from causing a network layer ready event. */
void disable_network_layer(void);
/* Macro inc is expanded in-line: increment k circularly. */
#define inc(k) if (k < MAX_SEQ) k = k + 1; else k = 0
using namespace std;
#define MAX_SEQ 7
bool flagError = 0;
bool flagErrorAck = 0;
bool flagEnd = 0;
bool first_recieved = 0;
bool flag_ack = 0;
seq_nr ack_expected; /* oldest frame as yet unacknowledged */
unsigned long counter_at_sender = 0;
unsigned long counter_at_reciever = 0;
unsigned long counter_at_Acknowledment = 0;
unsigned long time_counter = 0;
unsigned long frame_drop_number = 0;
unsigned long no_o_frame = 0;
unsigned long processing_time = 0;
unsigned long transmission_time = 0;
unsigned long counter_for_ack = 0;
vector<unsigned char> packets;
queue<int> sent_time_arr;
queue<int> sent_time_ack;
queue<frame> network_layer;
queue<frame> physical_layer;
queue<unsigned long> physical_layer_last_Ack_send;
queue<unsigned long> frame_drop_id;
event_type event;
void start_timer(seq_nr k, seq_nr ack)
{
	if (k - ack == MAX_SEQ)
		event = timeout;
}
void wait_for_event(event_type* e)
{
	while (event != *e);
}
void enable_network_layer(void)
{
	//event = network_layer_ready;
}
void disable_network_layer(void)
{
	event = timeout;
}
bool between(seq_nr a, seq_nr b, seq_nr c)
{
	/* Return true if a <= b < c circularly; false otherwise. */
	if (((a <= b) && (b < c)) || ((c < a) && (a <= b)) || ((b < c) && (c < a)))
		return (true);
	else
		return (false);
}
void from_network_layer(packet* p, seq_nr i)
{
	sent_time_ack.push(time_counter);
	sent_time_arr.push(time_counter);
	cout << "\nFrame " << counter_at_sender << " is sent at Time : " << time_counter;
	if (counter_at_sender > no_o_frame) {
		cout << "\t\t \"Wait for Ack\"";
	}
	counter_at_sender++;
	p->data = packets[i];
}
void send_data(seq_nr frame_nr, seq_nr frame_expected, packet buffer[])
{
	/* Construct and send a data frame. */
	frame s; /* scratch variable */
	s.info = buffer[frame_nr]; /* insert packet into frame */
	s.seq = frame_nr; /* insert sequence number into frame */
	s.ack = (frame_expected + MAX_SEQ) % (MAX_SEQ + 1); /* piggyback ack */
	to_physical_layer(&s); /* transmit the frame*/
	start_timer(frame_nr, frame_expected); /*start the timer running */
}
void to_physical_layer(frame* s)
{
	physical_layer.push(*s);
	event = frame_arrival;
}
void from_physical_layer(frame* r)
{
	*r = physical_layer.front();
	if (!frame_drop_id.empty()) {
		if (counter_at_reciever == frame_drop_id.front()) {
			flagError = 1;
			physical_layer_last_Ack_send.push(frame_drop_id.front());
			frame_drop_id.pop();
			return;
		}
	}
	if (flagError != 1) {
		cout << "\n ----------Frame " << counter_at_reciever << " is recieved at Time " << time_counter;
		network_layer.push(*r);
		counter_at_reciever++;
		physical_layer.pop();
	}
}
void to_network_layer(packet* p, seq_nr j)
{
	p->data = packets[j];
	if (!physical_layer_last_Ack_send.empty()) {
		if (j == physical_layer_last_Ack_send.front()) {
			flagErrorAck = 1;
			physical_layer_last_Ack_send.pop();
			return;
		}
	}
	if (flagErrorAck != 1) {
		cout << "\n ----------RECIEVED Ack of frame " << counter_at_Acknowledment << " is recieved at time " << time_counter;
		counter_at_Acknowledment++;
		inc(ack_expected); /* contract sender’s window */
		event = network_layer_ready;
	}
}
void protocol5()
{
	seq_nr next_frame_to_send; /* MAX_SEQ > 1; used for outbound stream */
	seq_nr ack_expected; /* oldest frame as yet unacknowledged */
	seq_nr frame_expected; /* next frame_expected on inbound stream */
	frame r; /* scratch variable */
	packet buffer[MAX_SEQ + 1]; /* buffers for the outbound stream */
	seq_nr nbuffered; /* number of output buffers currently in use */
	seq_nr i; /* used to index into the buffer array */
	enable_network_layer(); /* allow network_layer_ready events */
	ack_expected = 0; /* next ack_expected inbound */
	next_frame_to_send = 0; /* next frame going out */
	frame_expected = 0; /* number of frame_expected inbound */
	nbuffered = 0; /* initially no packets are buffered */
	event = network_layer_ready;
	while (true) {
		time_counter++;
		if (flagEnd == 1) {
			break;
		}
		if (counter_at_Acknowledment > no_o_frame) {
			event = END_OF_PROT;
		}
		wait_for_event(&event); /* four possibilities: see event_type above */
		switch (event) {
		case END_OF_PROT:
			flagEnd = 1;
			break;
		case network_layer_ready: /* the network layer has a packet to send */
			/* Accept, save, and transmit a new frame. */
			if (time_counter % processing_time == 0) {
				if (next_frame_to_send >= no_o_frame) {
					event = END_OF_PROT;
					break;
				}
				from_network_layer(&buffer[next_frame_to_send], next_frame_to_send);
				/* fetch new packet */
				send_data(next_frame_to_send, frame_expected, buffer);
				/* transmit the frame */
				inc(next_frame_to_send);
				/* advance sender’s upper window edge */
			}
			break;
		case frame_arrival: /* a data or control frame has arrived */
			if ((sent_time_arr.front() + transmission_time) == time_counter) {
				sent_time_arr.pop();
				first_recieved = 1;
			}
			if (first_recieved == 1) {
				from_physical_layer(&r); /* get incoming frame from_physical_layer */
				if (!network_layer.empty()) {
					if (network_layer.front().seq == frame_expected) {
						if ((sent_time_ack.front() + (transmission_time * 2)) == time_counter) {
							if (!physical_layer.empty()) {
								to_network_layer(&physical_layer.front().info,
									frame_expected); /* pass packet to_network_layer */
								network_layer.pop();
								inc(frame_expected);
								/* advance lower edge of receiver’s window */
								sent_time_ack.pop();
							}
						}
					}
				}
			}
			time_counter--;
			event = network_layer_ready;
			break;
		case cksum_err:
			break; /* just ignore bad frames */
		case timeout: /* trouble; retransmit all outstanding frames */
			cout << "\n\n********** TimeOut for frame " << counter_at_Acknowledment << " **********" << endl;
			next_frame_to_send = frame_expected; /* start retransmitting here */
			counter_at_sender = counter_at_Acknowledment;
			counter_at_reciever = counter_at_Acknowledment;
			nbuffered = 0;
			time_counter--;
			flagError = 0;
			flagErrorAck = 0;
			flagEnd = 0;
			first_recieved = 0;
			flag_ack = 0;
			while (!network_layer.empty()) {
				network_layer.pop();
			}
			while (!physical_layer.empty()) {
				physical_layer.pop();
			}
			while (!physical_layer_last_Ack_send.empty()) {
				physical_layer_last_Ack_send.pop();
			}
			while (!sent_time_arr.empty()) {
				sent_time_arr.pop();
			}
			while (!sent_time_ack.empty()) {
				sent_time_ack.pop();
			}
			event = network_layer_ready;
		}
		if ((counter_at_sender - counter_at_Acknowledment) < MAX_SEQ)
			enable_network_layer();
		else
			disable_network_layer();
	}
}
int main()
{
	processing_time = 1;
	cout << "Processing Time For Frame : 1";
	cout << "\nNumber of frames to sent : ";
	cin >> no_o_frame;
	cout << "\nTransmission Time of Frame : 2 or 3 :";
	cin >> transmission_time;
	while ((2 * transmission_time + 1) > MAX_SEQ) {
		cout << "\nthis is too long and will cause time out at first frame .... \nenter smaller transmission time : ";
		cin >> transmission_time;
	}
	cout << "\nNmber of frames to be dropped : ";
	cin >> frame_drop_number;
	for (int i = 0; i < no_o_frame; i++) {
		packets.push_back(i);
	}
	if (frame_drop_number > 0) {
		unsigned long frame_id;
		unsigned j = 1;
		cout << "\nFirst frame id to drop : ";
		cin >> frame_id;
		frame_drop_id.push(frame_id);
		for (unsigned long i = 1; i < frame_drop_number; i++) {
			cout << "\nNext frame id to drop : ";
			cin >> frame_id;
			if (frame_id > MAX_SEQ * (j)) {
				frame_drop_id.push(frame_id);
				j++;
			}
		}
	}
	cout << endl;
	cout << "******************* THE RESULT ************************\n";
	protocol5();
	system("pause");
	return 0;
}