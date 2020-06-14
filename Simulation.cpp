#pragma once
//Header Files
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <fstream>
#include <math.h>
#include <queue>
#include <vector>
#include <string>
#include "readcsv.h"

#define TOTAL_STATIONS 3
#define DEFAULT_CAPACITY 300
#define WARMUP_PERIOD 3600
#define TOTAL_SIMULATION_TIME 7200
#define MAX_TRANSFER 8
#define MAX_POLICY_NUM 4	// the largest possible num of optimal policy from station i to station j

// declaration
struct Report;				// the struct to report to the RL model
struct WaitingPassengers;	// the struct to store the information of waiting passenegers in a queue
struct Train;				// the struct to store the information of a train

//Vector of Queues for passengers
typedef std::queue<WaitingPassengers> Q;
//typedef std::vector<Q> vecQ;
//typedef std::vector<int> transfer_list;

enum EventType {
	// only three types of events are considered in the simulation
	// ARRIVAL is the arrival of a train
	// SUSPEND is to suspend the program to wait for the RL model's new input
	// NEW_OD is to add new passengers (including the transfer passengers) to 
	//			the queues of the stations.
	// TRANSFER is to add future OD of transfer passengers
	ARRIVAL,
	SUSPEND,
	NEW_OD,
	TRANSFER
};

struct WaitingPassengers {
	//int arrivingTime;
	int numPassengers;
	int destination;
};

// the struct to depict the decision a passenger will make at a situation.
struct Policy { // itenerary
	int direction;
	int transferStation;
	int transferDirection;
};

// Event Class
class Event {
	// the base class for events
public:
	const EventType type;	// the type of the event
	double time;			// the happening time of the event, in the unit of sec
	int** OD = NULL;		// if is the global OD, it will be a matrix
							// if is a transfer OD, OD[0] will be an array containing
							// [int from, int to, int num]
	Train* train = NULL;

	Event(double t, EventType type = ARRIVAL) : time(t), type(type) { }
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
	int ID;					// station ID
	int lineID;				// line ID
	bool isTerminal[2];		// if the station is the terminal station
	bool isTransfer;		// if the station is a transfer station
	Q queue[2];				// passenger queues for both directions
	double avg_inStationTime[2];	//avg arriving time of passengers in the queue, used for delay calculation

	Station(int ID, int lineID, bool isTerminalInDir0, bool isTerminalInDir1, bool isTransfer = false) : \
		ID(ID), lineID(lineID), isTransfer(isTransfer) {
		isTerminal[0] = isTerminalInDir0;
		isTerminal[1] = isTerminalInDir1;
	}

	int getQueueNum(int direction) {
		return queue[direction].size();
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
	double totalTravelTime;	// including on- and off- train time
	double totalDelay;		// the off-train delay, namely the waiting time in the station queue
	int num_departed;		// number of passengers put into the system
	int num_arrived;		// number of passengers arrived at the destination

	int policy_num[TOTAL_STATIONS][TOTAL_STATIONS];
	// the matrix stores the num of the optimal paths from station i to station j

	int policy[TOTAL_STATIONS][TOTAL_STATIONS][MAX_POLICY_NUM];
	// the matrix stores the optimal policy--what is the next station to go if the passenger is traveling
	// from station i to station j
	// considering that there may be several optimal solutions between two stations, at most MAX_POLICY_NUM 
	// policies can be stored here

	int directions[TOTAL_STATIONS][TOTAL_STATIONS];
	// return the direction from station i to station j
	// if they are not on the same line, return -1

	double transferTime[TOTAL_STATIONS][TOTAL_STATIONS];
	// this matrix stores the transfer time between two transfer stations.

	int lineIDOfStation[TOTAL_STATIONS];
	// an array to store the ID of the line that the station belongs to

	std::vector<std::vector<int>> startTrainInfo;
	// a 2-d matrix to store the information of train starting from the starting station, for reset().

	std::vector<std::vector<double>> arrivalTime;
	// a 2-d matrix to store the arrival time at each station (except the starting station) of each trainID

	std::vector<std::vector<int>> arrivalStationID;
	// a 2-d matrix to store the arriving station's ID of each arrival recorded in 'arrivalTime' matrix above

	Station* stations;
	// an array to store all the stations

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


protected:
	//Priority Queue for the events
	std::priority_queue < Event, std::vector<Event, std::allocator<Event> >, EventCompare > EventQueue;
	int totalTrainNum;		// record the total number of trains, important
	int* time_iter;			// iterator to iterate the arrivalTime matrix
	int* stationID_iter;	// iterator to iterate the arrivalStationID matrix

	Report report();	// return the system information
	bool isOnSameLine(int station, int line);	// return true if the station is in that line 
												// or the station's transfer station is in the line
	Policy getPolicy(int from, int to, int lineID);	// return the optimal traveling policy
	double getNextArrivalTime(int trainID);		// query the arrival time for each train
	int getNextArrivalStationID(int trainID);	// query the arrival station for each train

};

// defination of the Report structure
struct Report {
	bool isFinished;
	double totalTravelTime;
	double totalDelay;
};

// an inner function to arrange all the information needed in the RL model
Report Simulation::report() {
	Report result;
	if (time < TOTAL_SIMULATION_TIME)
		result.isFinished = false;
	else
		result.isFinished = true;
	result.totalDelay = totalDelay;
	result.totalTravelTime = totalTravelTime;

	return result;
}

// Run Simulation
Report Simulation::run() {

	do {
		if (EventQueue.empty()) {
			std::cout << "Empty Queue!" << std::endl;
			return report();
		}
		else {
			Event nextevent = EventQueue.top();
			EventQueue.pop();
			time = nextevent.time;

			if (nextevent.type == ARRIVAL) {

				Train* train = nextevent.train;
				int trainID = train->trainID;
				int station = train->arrivingStation;
				int direction = train->direction;
				int& capacity = train->capacity;
				int* destination = train->destination;
				int& passengerNum = train->passengerNum;
				int lineID = train->lineID;

				// calculate travel time and passenger get off
				totalTravelTime += passengerNum * (time - train->lastTime);
				passengerNum -= destination[station];
				capacity += destination[station];
				num_arrived += destination[station];
				destination[station] = 0;

				// if it's a transfer station, do the transfer (add new OD to the stations)
				if (stations[station].isTransfer) {
					// iterate the destination table of the train
					// ################# can be improved here ##################
					// 1.table iteration is slow, other structures can be considered
					// 2.all the transfer OD can be put into the queue at once! 
					for (int dest_station = 0; dest_station < TOTAL_STATIONS; dest_station++) {
						if (destination[dest_station] > 0) {
							// first, find the passengers whose trip is finished ( not at this station,
							// but at its transfer station ), finish them!
							if (transferTime[station][dest_station] >= 0.0) {		// try to use transfer time to do it...
								int off_num = destination[dest_station];
								passengerNum -= destination[dest_station];
								capacity += destination[dest_station];
								num_arrived += destination[dest_station];
								destination[dest_station] = 0;
								// if need to count the time they go to their desired exit, do it here
								// ############### ADD NEW CODE HERE ###############
								totalTravelTime += transferTime[station][dest_station] * off_num;

								// skip the transfer check
								continue;
							}

							// a redundant transfer_direction is calculated here.
							// ####### can be improved here #######
							Policy transfer_policy = getPolicy(station, dest_station, lineID);

							if (transfer_policy.direction == -1) { // meaning the passenger should transfer

								// get off the train
								int num_transfer = destination[dest_station];
								passengerNum -= num_transfer;
								capacity += num_transfer;

								///// note that the future OD should be an event,
								///// we can only add OD for the current time.
								int transfer_stationID = transfer_policy.transferStation;
								if (transferTime[station][transfer_stationID] == 0) {
									// for transfer on the same platform
									addPassengers(transfer_stationID, dest_station, num_transfer);
								}
								else {
									// if transfer time need to be count, do it here
									totalTravelTime += transferTime[station][transfer_stationID] * num_transfer;
									// set up a new event for a future OD pair
									Event newEvent(time + transferTime[station][transfer_stationID], TRANSFER);
									int** ODpair = new int*;
									ODpair[0] = new int[3];
									ODpair[0][0] = transfer_stationID;
									ODpair[0][1] = dest_station;
									ODpair[0][2] = num_transfer;
									newEvent.OD = ODpair;
									EventQueue.push(newEvent);
								}

							}
						}
					}
				}
				// if not terminal, new passengers board and calculate delay and travel time
				if (!stations[station].isTerminal[direction]) {
					Q* passengerQueue = &stations[station].queue[direction];

					// calculate delay and total travel time
					double delta_time = (time - stations[station].avg_inStationTime[direction])\
						* (double)passengerQueue->size();
					totalDelay += delta_time;
					totalTravelTime += delta_time;
					stations[station].avg_inStationTime[direction] = time;

					// if there is space on the train, get the passengers onto the train
					while (!passengerQueue->empty()) {
						WaitingPassengers* passengers = &passengerQueue->front();

						if (passengers->numPassengers <= capacity) {
							// all this destination group get on the train
							capacity -= passengers->numPassengers;
							passengerNum += passengers->numPassengers;
							destination[passengers->destination] += passengers->numPassengers;
							passengerQueue->pop();
						}
						else {
							// part of this destination group get on the train
							passengers->numPassengers -= capacity;
							passengerNum += capacity;
							destination[passengers->destination] += capacity;
							capacity = 0;
						}
					}

					// set up a new arrival event
					// rebuild with a new function to get the next arrival time and next station!!!!!!!!!!
					nextevent.time = getNextArrivalTime(trainID);
					train->arrivingStation = getNextArrivalStationID(trainID);
					EventQueue.push(nextevent);

					// if the station is the first station, start a new train
					// if (stations[station].isTerminal[1 - direction]) {
					// 	Event newEvent(time + headway[station], ARRIVAL);
					// 	Train* newTrain = new Train(lineID, direction, station);
					// 	// if the trains' capacity is different in different lines, it can be set here.
					// 	newEvent.train = newTrain;
					// 	EventQueue.push(newEvent);
					// }				// now we use a table to init all the trains at once

				}
				// if is terminal, delete the train
				// (debug) report the passenger numbers on the train
				if (stations[station].isTerminal[direction]) {
					if (passengerNum > 0)
						throw "passenger not cleared at terminal!";
					delete train;
					delete& nextevent;
				}

			}
			else if (nextevent.type == SUSPEND) {
				// return immediate cost for the RL model to make decision
				delete& nextevent;

				return report();
			}
			else if (nextevent.type == NEW_OD) {
				// very slow!!!!!!!!!!!!!!!!! better using more compact way
				// add new ODs to the corresponding stations' queues
				int** OD = nextevent.OD;
				for (int from = 0; from < TOTAL_STATIONS; from++) {
					for (int to = 0; to < TOTAL_STATIONS; to++) {
						// we add all these OD pairs all at the same time
						if (OD[from][to] != 0) {
							addPassengers(from, to, OD[from][to]);
						}
					}
				}
				/////////also need to delete the OD!!!!!!!!!!!!!!
				delete& nextevent;
			}
			else if (nextevent.type == TRANSFER) {
				// add transfer OD pairs
				int from = nextevent.OD[0][0];
				int to = nextevent.OD[0][1];
				int num = nextevent.OD[0][2];
				addPassengers(from, to, num);
				/////////also need to delete the OD!!!!!!!!!!!!!!
				delete& nextevent;
			}

		}

	} while (time < TOTAL_SIMULATION_TIME);

	// when time is up
	return report();
}

// if it is the OD just put into the system, lineID should be -1
Policy Simulation::getPolicy(int from, int to, int lineID) {
	int num = policy_num[from][to];
	int nextStation;
	int dir;
	bool transfer = true;
	if (num == 1) {
		nextStation = policy[from][to][0];
	}
	else {
		for (int i = 0; i < num; i++) {
			nextStation = policy[from][to][i];
			// if there is an optimal solution on the same line, abandon the transfer
			if (lineIDOfStation[nextStation] == lineID) {
				transfer = false;
				break;
			}
		}
		if (transfer) {
			// randomly choose a station to transfer to
			nextStation = policy[from][to][rand() % num];
		}
	}
	dir = directions[from][nextStation];
	if (dir != -1) {
		Policy result = { dir, -1, -1 };
		return result;
	}
	else {
		dir = directions[nextStation][to];
		Policy result = { -1, nextStation, dir };
		return result;
	}
}

// the function to add new OD pairs to the queues of stations
void Simulation::addPassengers(int from, int to, int num) {
	// check if need to transfer to another line
	// if need to do so, calculate the transfer time !!!!!!!
	Policy policy = getPolicy(from, to, -1);
	int direction = policy.direction;
	if (direction == -1) {	// if need to transfer
		// calculate the transfer time for the new OD put into the system
		totalTravelTime += transferTime[from][policy.transferStation] * num;

		from = policy.transferStation;
		direction = policy.transferDirection;
	}

	Station* station = &stations[from];

	// push the passengers into the queue, update avg_inStationTime
	double queue_len = (double)station->queue[direction].size();
	double new_len = queue_len + (double)num;
	station->avg_inStationTime[direction] = (queue_len * station->avg_inStationTime[direction]\
		+ (double)num * time) / new_len;
	WaitingPassengers passengers;

	//passengers.arrivingTime = time;
	passengers.destination = to;
	passengers.numPassengers = num;
	station->queue[direction].push(passengers);

	// update the num of departed passengers
	num_departed += num;
}

// this is the function to load the data and initalize the Simulation 
void Simulation::init() {
	// first load the data
	// arrivalStationID
	str_mat str_ASID =			readcsv("simple_data/arrivalStationID.csv");
	str_mat str_AT =			readcsv("simple_data/arrivalTime.csv");
	str_mat str_directions =	readcsv("simple_data/directions.csv");
	str_mat str_policy =		readcsv("simple_data/policy.csv");
	str_mat str_policy_num =	readcsv("simple_data/policy_num.csv");
	str_mat str_STI =			readcsv("simple_data/startTrainInfo.csv");
	str_mat str_stations =		readcsv("simple_data/stations.csv");
	str_mat str_TT =			readcsv("simple_data/transferTime.csv");




	// init the iterators
	totalTrainNum = str_STI.size();	// use startTranInfo to get the train number
	time_iter = new int[totalTrainNum];
	stationID_iter = new int[totalTrainNum];

	// reset using the loaded data
	reset();
}

// reset/init the simulation state using loaded data.
void Simulation::reset() {
	time = 0.0;
	totalTravelTime = 0.0;
	totalDelay = 0.0;
	num_departed = 0;
	num_arrived = 0;

	// clear the events, maybe a bit slow
	while (!EventQueue.empty())
		EventQueue.pop();

	// reset the iterators
	for (int i = 0; i < totalTrainNum; i++) {
		time_iter[i] = 0;
		stationID_iter[i] = 0;
	}

	// renew the start out trains
	for (int i = 0; i < totalTrainNum; i++) {
		int trainID = startTrainInfo[i][0];
		int startingStationID = startTrainInfo[i][1];
		int lineID = startTrainInfo[i][2];
		int direction = startTrainInfo[i][3];
		int capacity = startTrainInfo[i][4];
		double startTime = startTrainInfo[i][5];

		Train* newTrain = new Train(trainID, lineID, direction, startingStationID, startTime, capacity);
		Event newEvent(startTime, ARRIVAL);
		newEvent.train = newTrain;
		EventQueue.push(newEvent);
	}

	// renew the stations (queues)
	for (int i = 0; i < TOTAL_STATIONS; i++) {
		// reset the queues, maybe a bit slow
		while (!stations[i].queue[0].empty())
			stations[i].queue[0].pop();
		while (!stations[i].queue[1].empty())
			stations[i].queue[1].pop();

		// reset avg_inStationTime
		stations[i].avg_inStationTime[0] = 0.0;
		stations[i].avg_inStationTime[1] = 0.0;
	}
}