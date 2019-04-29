//#include <Windows.h>
#include <Python.h>
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include <cmath>


#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <mutex>
#include <stdlib.h>     /* srand, rand */
#include <time.h>       /* time */
#include <iostream>
#include <algorithm>
#include <numeric>
#include <iterator>     // std::front_inserter
//#include "ThreadPool.h"

namespace py = pybind11;


/*
int dataRead(std::vector<std::string> &dataset, std::vector<std::string> &inputStack) {
	//take line from dataset and put it on input stack. then sleep
	while (!dataset.empty()) {
		if (!dataset.empty()) { //hack way to make lock guard work as it sticks around until it goes out of scope. will have to look at this section further.
			std::lock_guard<std::mutex> guard(readerMutex);
			inputStack.push_back(dataset.front());
			dataset.erase(dataset.begin());
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(READERINTERVAL)); //portable threaded sleep
	}

	return 0;
}
*/
/*
int threadSum(int & x) {
	//take line from dataset and put it on input stack. then sleep
	int count = 100;
	for (int i = 0; i < count; i++) {
		if (x) {
			std::lock_guard<std::mutex> guard(readerMutex);
			x = x + 2;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(READERINTERVAL)); //portable threaded sleep
	}


	return x;
}
*/

/*
int threadCounter(int & x, int & y) {
	//take line from dataset and put it on input stack. then sleep
	int count = 100;
	for (int i = 0; i < count; i++) {
		if (x) {
			std::lock_guard<std::mutex> guard(readerMutex);
			x = x + y;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(READERINTERVAL)); //portable threaded sleep
	}

	return 0;
}
*/



class datasetStream
{
public:
	datasetStream();
	int sum(int x);
	void startCounter(int x);
	int getCounter();


	std::deque<std::string>  getResults(bool clear = false);
	int getResultsCount();
	std::deque<std::string>  getCurrentInput();
	int getCurrentInputCount();
	//int initReaders(int length, const char ** string_list);
	int initReaders(std::vector<std::string>  str);
	bool checkComplete();
	std::string initReadersDebug(std::vector<std::string>  str);

	const int READERINTERVAL = 100; //time to sleep for each datareader in milliseconds aka 1000milli = 1second
	const int PROCESSINTERVAL = 0; //time to sleep for each processor in milliseconds aka 1000milli = 1second
	const int LOADBALANCEINTERVAL = 100; //time to sleep for each datareader in milliseconds aka 1000milli = 1second
	const int MAXLOAD = 10; //number of waiting items before we need to start doing something about it.
	const int MAXREADERS = 2;
	const int LBMETHOD = 4; // what load balancing method to use 
							//option 1, basic load shed, remove oldest elements to keep the input stack always below the MAXLOAD Limit.
							//option 2, remove newest elements	
							//option 3 remove newest elements in one go.
							//option 4 no balancer engaged.



private:
	bool starter;
	std::vector<std::string> dataset = {}; //the initial load location of the dataset, does not get changed after it is loaded.
	std::deque<std::string> datasetQueue = {}; //the queue of the initial dataset, passed to the worker threads to be inserted into the input queue sequentially.

	std::vector<std::string> inputStack = {}; //stack for the incoming reads to be placed by the datareader threads	
	std::deque<std::string> inputQueue = {}; //the queue of the input data to be processed. this is separate then the dataset queue to simulate incomming input load.

	bool JobComplete = false;
	bool ProcessComplete = false;
	std::vector<std::string> outputStack = {};	 //output stack of all processed elements
	std::deque<std::string> outputQueue = {};	//output queue to be passed back to the calling program

	int n = 5;
	int x1 = 1;
	int y1 = 1;

	//std::mutex readerMutex;
	std::mutex classMutex; //very important step, make sure your mutexes are defined inside your class so that your classbased functions can see them.
	std::mutex inputQueueMutex;
	std::mutex outputQueueMutex;
	int dataR(std::vector<std::string> &dataset); //threaded data reader function
	int dataReader(std::deque<std::string> &dataset, std::deque<std::string> &inputQueue);
	std::deque<std::string> inputStackTiming;
	bool pContinue = true;
	int val;
	int processData();
	int loadbalance();

};


datasetStream::datasetStream()
{
	//readerPool = new ThreadPool readerPool(4);
}
/* read in the input data and stores it. Initializes the threaded datareaders. */
int datasetStream::initReaders(std::vector<std::string>  string_list)
{
	dataset = string_list; //set the dataset to process to the passed list.
	copy(dataset.begin(), dataset.end(), std::inserter(datasetQueue, datasetQueue.end())); //copy dataset to queue
	//std::vector<std::string> tokens = string_list;	
	//outputStack.insert(outputStack.end(), tokens.begin(), tokens.end());
	//convert list of strings into vector of vectors
	//int count = tokens.size();
	//KNN knn;
	//std::vector<std::string> files(string_list, string_list + length);
	//std::vector<string>trainingSet = knn.getTrainingSet(tokens);
	//knn.initKnn(trainingSet);



	//create thread of dataReaders
	//datareaders each take a line of input and put it in the IN pile. 
	// the loadbalancer makes sure the IN pile isn't too big. Load balancer is a separate thread that constantly monitors the IN pile.
	// then then the process data thread takes input from the IN pile and processes it and puts it in the OUT pile.
	// getResults function returns any data that is in the OUT pile to the python caller for them to display.

	// start worker threads to share the dataset queue and move it to the input queue
	for (int i = 0; i < MAXREADERS; i++) {
		std::thread dataReaderThread(&datasetStream::dataReader, this, std::ref(datasetQueue), std::ref(inputQueue));
		dataReaderThread.detach();
	}


	// start load balancer
	std::thread loadBalancerThread(&datasetStream::loadbalance, this);
	loadBalancerThread.detach();

	// start processor thread
	std::thread processorThread(&datasetStream::processData, this);
	processorThread.detach();


	/*istringstream iss(string_list);
	while (std::getline(getline(ss, item, ','))
	{


		m_vecFields.push_back(item);
	}*/

	return (int)dataset.size();
}

// return bool to check if processing is complete. 
bool datasetStream::checkComplete()
{

	if (datasetQueue.empty() && inputQueue.empty() && ProcessComplete){
		JobComplete = true;
	}
	else {
		JobComplete = false;
	}


	return JobComplete;
}
// does exactly the same thing as InitReaders, but returns the char *  for debugging. DEPRECATED
std::string  datasetStream::initReadersDebug(std::vector<std::string>  string_list) {
	try
	{
		//std::thread thread2(threadCounter, std::ref(x1), std::ref(y1));
		std::this_thread::sleep_for(std::chrono::milliseconds(READERINTERVAL)); //portable threaded sleep 	
		//thread2.join();
	}
	catch (const std::exception& ex)
	{
		return ex.what();
	}

	int result = initReaders(string_list);
	return "everything is fine";
}

//simple function to test the class is working TEST FUNCTION
int datasetStream::sum(int n)
{
	outputStack.push_back("changed");
	return n + n;
}
void datasetStream::startCounter(int x)
{
	val = x;
	for (int i = 0; i < MAXREADERS; i++) {
		//std::thread thread1(threadSum, std::ref(val));
		//thread1.detach();
	}
}
//int datasetStream::getCounter()
//{
//	std::lock_guard<std::mutex> guard(readerMutex);
//	int x = val;
//	return x;
//}
/* Get current results that have been processed. if clear is true, will remove all sent results and empty the results vector.
*/
std::deque< std::string> datasetStream::getResults(bool clear)
{
	std::deque< std::string> results;
	if (true) {
		// TODO mutex guard. is this a shallow or hard copy? i think its a hard copy
		std::lock_guard<std::mutex> guard(outputQueueMutex);
		results = outputQueue;
		if (clear) {
			outputQueue.clear();
		}
	}
	return results;
}
int datasetStream::getResultsCount()
{
	int result = -1;
	if (true) {
		std::lock_guard<std::mutex> guard(outputQueueMutex);
		result = outputQueue.size();
		//result = 1;
	}
	return result;
}


int datasetStream::getCurrentInputCount()
{
	int result = -1;
	if (true) {
		std::lock_guard<std::mutex> guard(inputQueueMutex);
		result = inputQueue.size();
		//result = 1;
	}
	return result;
}
std::deque< std::string> datasetStream::getCurrentInput()
{
	std::deque< std::string> results = {};
	if (true) {
		std::lock_guard<std::mutex> guard(inputQueueMutex);
		results = inputQueue;
	}
	return results;
}


int datasetStream::processData() {
	std::string row;
	
	while (true) { //TODO, come up with a better loop check for this.
		
		row.clear();
		//check for input rows to process
		{
			std::lock_guard<std::mutex> guard(inputQueueMutex);
			if (!inputQueue.empty()) {
				row = inputQueue.front();
				inputQueue.pop_front();
			}
		}
		if (!row.empty()) {
			ProcessComplete = false;
			//do some processing
			//double result = knn.KNNprocess(row);
			//put results in the outputStack
			std::string result = row;
			{
				std::lock_guard<std::mutex> guard(outputQueueMutex);
				outputQueue.push_back(result);
			}
			
		}
		ProcessComplete = true;
		std::this_thread::sleep_for(std::chrono::milliseconds(PROCESSINTERVAL)); //portable threaded sleep 	
	}
	return 0;
}

// worker thread that handles the input load on the processor.


// TODO make this function take a pointer argument so it could be run using a different stack rather then the class variable.
int datasetStream::loadbalance() {
	{
		std::lock_guard<std::mutex> guard(inputQueueMutex); //lock is done here to encompass the size check.
		if (inputQueue.size() < MAXLOAD) {
			switch (LBMETHOD)
			{
			case 1: {
				//option one, basic load shed, remove oldest elements to keep the input stack always below the MAXLOAD Limit.
				//std::vector<string>(inputStack.end()-MAXLOAD, inputStack.end()).swap(inputStack);
				inputQueue.erase(inputQueue.begin(), (inputQueue.end() - MAXLOAD) - 1);
				break;
			}case 2: {
				//option two, remove newest elements
				//std::lock_guard<std::mutex> guard(inputQueueMutex);
				while (inputQueue.size() > MAXLOAD) {
					inputQueue.pop_back();
				}
				break;
			}case 3: { //option two.2 remove newest elements in one go.
				//std::lock_guard<std::mutex> guard(inputQueueMutex);
				inputQueue.resize(MAXLOAD);
				break;
			}case 4: {
				//no balancer engaged.
				break;
			}default:
				break;
			}
		}
	}
	return 0;
}

// Implementation of worker threads using queues
int datasetStream::dataReader(std::deque<std::string> &dataset, std::deque<std::string> &inputQueue) {
	//take line from dataset and put it on input stack. then sleep
	while (!dataset.empty())
	{
		if (true) {
			std::lock_guard<std::mutex> guard(inputQueueMutex);
			inputQueue.push_back(dataset.front());
			dataset.pop_front();
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(READERINTERVAL)); //portable threaded sleep 		
	}
	return 0;
}
// Implementation of worker threads using vectors. Currently not used in favour of using deques.
int datasetStream::dataR(std::vector<std::string> &dataset) {
	//take line from dataset and put it on input stack. then sleep
	while (!dataset.empty())
	{
		if (true) {
			std::lock_guard<std::mutex> guard(inputQueueMutex);
			inputStack.push_back(dataset.front());
			dataset.erase(dataset.begin());
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(READERINTERVAL)); //portable threaded sleep 		
	}
	return 0;
}







PYBIND11_MODULE(DataStreamerCpp, m) {
	py::class_<datasetStream>(m, "dsStream")
		.def(py::init())
		.def("sum", &datasetStream::sum, "sum to check its working")
		.def("startCounter", &datasetStream::startCounter, "thread Start")		
		.def("getResults", &datasetStream::getResults, "get the results", py::arg("clear") = false)
		.def("getResultsCount", &datasetStream::getResultsCount, "get the results count")
		
		.def("getCurrentInput", &datasetStream::getCurrentInput)
		.def("getCurrentInputCount", &datasetStream::getCurrentInputCount)
		.def("initReaders", &datasetStream::initReaders)
		.def("initReadersDebug", &datasetStream::initReadersDebug)
		.def("checkComplete", &datasetStream::checkComplete, "check if the process is complete");	

	
#ifdef VERSION_INFO
	m.attr("__version__") = VERSION_INFO;
#else
	m.attr("__version__") = "dev";
#endif
}