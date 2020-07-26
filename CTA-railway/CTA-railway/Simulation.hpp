#pragma once
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <fstream>
#include <math.h>
#include <queue>
#include <vector>
#include <string>

#define TOTAL_STATIONS 252	// ��øĳ�class��Ա��������init�г�ʼ��
#define DEFAULT_CAPACITY 500
#define START_TIME 18000	// not add passengers into the system until 5:00
#define WARMUP_PERIOD 0
#define SIMULATION_END_TIME 64800
#define MAX_POLICY_NUM 1	// the largest possible num of optimal policy from station i to station j

// declaration
struct Report;				// the struct to report to the RL model
struct WaitingPassengers;	// the struct to store the information of waiting passenegers in a queue
struct Train;				// the struct to store the information of a train

//Vector of Queues for passengers
typedef std::queue<WaitingPassengers> Q;
//typedef std::vector<Q> vecQ;
//typedef std::vector<int> transfer_list;

enum EventType {
	// only three types of events are considered in the simulation at present
	ARRIVAL,	// the arrival of a train
	SUSPEND,	// to suspend the program to wait for the RL model's new input
	NEW_OD		// to add new OD pairs (including the transfer passengers)
};

struct WaitingPassengers {
	//int arrivingTime;
	int numPassengers;
	int destination;
};

//// the struct to depict the decision a passenger will make at a situation.
//struct Policy { // itenerary
//	int direction;
//	int transferStation;
//	int transferDirection;
//};

// Event
struct Event {
	// the base class for events
	EventType type;			// the type of the event
	double time;			// the happening time of the event, in the unit of sec
	int from, to, num;		// if is a transfer OD, use the compact format of [int from, int to, int num]
	Train* train;			// handle of the arriving train
	bool isTransfer;		// mark if the OD is from a transfer behavior
	
	// init function
	Event(double t, EventType type = ARRIVAL, bool isTransfer = false) : time(t), type(type), from(-1), to(-1),\
		num(0), train(NULL), isTransfer(isTransfer) { }
};

//Compare events for Priority Queue
struct EventCompare {
	bool operator() (const Event left, const Event right) const {
		return left.time > right.time;
	}
};

class Station {
	// Each station in the system has a unique ID. For the transfer stations, consider there are 
	// several independent stations in each line, which have different IDs.
public:
	// static
	int ID;					// station ID
	int lineID;				// line ID
	bool isTerminal[2];		// if the station is the terminal station
	bool isTransfer;		// if the station is a transfer station

	// variable
	Q queue[2];				// passenger queues for both directions
	int queueSize[2];		// record the number of passengers waiting in the queues of both directions
	double avg_inStationTime[2];	//avg arriving time of passengers in the queue, used for delay calculation
	double delay[2];		// delay contributed by the direction
	int numPass[2];			// count of passengers entered this direction's queue

	Station(int ID, int lineID, bool isTerminalInDir0, bool isTerminalInDir1, bool isTransfer = false) : \
		ID(ID), lineID(lineID), isTransfer(isTransfer) {
		isTerminal[0] = isTerminalInDir0;
		isTerminal[1] = isTerminalInDir1;
		queueSize[0] = 0;
		queueSize[1] = 0;
		avg_inStationTime[0] = 0.0;
		avg_inStationTime[1] = 0.0;
		delay[0] = 0.0;
		delay[1] = 0.0;
	}

	int getQueueNum(int direction) {
		return queueSize[direction];
	}
};

struct Train {
	// the information about the train
	int trainID;			// the unique ID of a train from a terminal to the other terminal
	int lineID;				// the ID of the line the train in on
	int arrivingStation;	// the ID of the station the train arrives in the event
	int direction;			// 0 or 1, consistent with the map info
	int capacity;			// the remaining space on the train
	double lastTime;		// the time that the train set out at last station,
							// if the train is being initialized, set 'lastTime' to be the set out time at the starting station
	int destination[TOTAL_STATIONS] = { 0 };// numbers of passengers heading for each station
	int passengerNum;		// total number of passengers on the train

	Train(int trainID, int lineID, int direction, int arrivingStation, double startTime, int capacity = DEFAULT_CAPACITY) : \
		trainID(trainID), lineID(lineID), direction(direction), passengerNum(0), \
		arrivingStation(arrivingStation), lastTime(startTime), capacity(capacity) {}
};

//Simulation Class
class Simulation {
public:
	double time;			// the system time in the unit of sec
	double _last_time;		// record the last time so to monitor the system
	double totalTravelTime;	// including on- and off- train time
	double totalDelay;		// the off-train delay, namely the waiting time in the station queue
	int num_departed;		// number of passengers put into the system
	int num_arrived;		// number of passengers arrived at the destination

	int** policy_num; // [TOTAL_STATIONS] [TOTAL_STATIONS] = { 0 };
	// the matrix stores the num of the optimal paths from station i to station j

	int*** policy; // [TOTAL_STATIONS] [TOTAL_STATIONS] [MAX_POLICY_NUM] = { -1 };
	// the matrix stores the optimal policy--what is the next station to go if the passenger is traveling
	// from station i to station j
	// considering that there may be several optimal solutions between two stations, at most MAX_POLICY_NUM 
	// policies can be stored here

	int*** policy_offpeak; //	[TOTAL_STATIONS] [TOTAL_STATIONS] [MAX_POLICY_NUM] = { -1 };
	// the policy matrix for off-peak hours when the P line is not fully running

	int** directions; //  [TOTAL_STATIONS] [TOTAL_STATIONS] = { -1 };
	// return the direction from station i to station j
	// only when i and j are adjacent stations

	double** transferTime; // [TOTAL_STATIONS] [TOTAL_STATIONS] = { -1 };
	// this matrix stores the transfer time between two transfer stations.

	//int lineIDOfStation[TOTAL_STATIONS] = { -1 };
	// an array to store the ID of the line that the station belongs to

	std::vector<std::vector<int>> startTrainInfo;
	// a 2-d matrix to store the information of train starting from the starting station, for reset().

	std::vector<std::vector<double>> arrivalTime;
	// a 2-d matrix to store the arrival time at each station (except the starting station) of each trainID

	std::vector<std::vector<int>> arrivalStationID;
	// a 2-d matrix to store the arriving station's ID of each arrival recorded in 'arrivalTime' matrix above

	std::vector<Station> stations;
	// an array to store all the stations

	std::vector<std::vector<int>> fixedOD;
	// a 2-d matrix to store the fixed OD data;

	Simulation() : time(0), totalTravelTime(0), totalDelay(0), num_departed(0), num_arrived(0), EventQueue() {
		srand((unsigned int)(std::time(NULL)));
	}

	// to start work from here
	void init();	// load the initial state from the data files.
	Report run();	// return a pointer of several doubles,
					// including time, totalTravelTime and totalDelay.
	void reset();	// reset to the initial state using the loaded data.
	void addPassengers(int from, int to, int num);	// add passengers right now
	void addEvent(Event newevent) {
		EventQueue.push(newevent);
	}
	double getStationDelay(int stationID, int direction);
	int getStationPass(int stationID, int direction);
	int getStationWaitingPassengers(int stationID, int direction);
	double getTime();
	

protected:
	//Priority Queue for the events
	std::priority_queue < Event, std::vector<Event, std::allocator<Event> >, EventCompare > EventQueue;
	int totalTrainNum;		// record the total number of trains, important
	int* time_iter;			// iterator to iterate the arrivalTime matrix
	int* stationID_iter;	// iterator to iterate the arrivalStationID matrix

	Report report();	// return the system information
	//Policy getPolicy(int from, int to, int lineID);	// return the optimal traveling policy
	int getNextStation(int from, int to, int lineID);	// return the next station to go

	//**************************************************************************************
	// getNextArrivalTime must be called together with getNextArrivalStationID
	double getNextArrivalTime(int trainID);		// query the arrival time for each train
	int getNextArrivalStationID(int trainID);	// query the arrival station for each train
	//**************************************************************************************
	bool trainEnd(int trainID);	// check if the train has arrived at its end before the terminal station
	
	// find the station where the passenger really take the train, return the transfer time.
	int getRealStation(int from, int to, double& _transfer_time);	

};

// defination of the Report structure
struct Report {
	bool isFinished;
	double totalTravelTime;
	double totalDelay;
	int numDeparted;
	int numArrived;
	void show();
};
