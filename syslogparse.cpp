// Apparmor Rule Parser
// Colin O'Brien - insanitybit
// g++ ./syslogparse.cpp -O0 --std=c++0x -pthread -Wall
// source available at: https://github.com/insanitybit/SyslogParser
/*
The goal of this program is to parse the system log for two things:
1) Apparmor denials
2) Iptables logs

The current tools for both of these actions are lacking in terms of both performance and usability.

By making use of native and threaded code I should be able to speed things up considerably.
Ideally this should perform one round of tasks in less than 100ms. This is ambitious but realistic.
*/
/*
Benchmarking code taken from:
http://stackoverflow.com/questions/3797708/millisecond-accurate-benchmarking-in-c

I'll be moving to a new method later but for now this gives me a good estimate and allows me to
identify critical performance issues.
*/


// testlibs
#include <sys/time.h>

// necessary
#include <string>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <err.h>
#include <sys/mman.h>
#include <stack>
#include <vector>
#include <thread>
#include <algorithm>
#include <streambuf>
#include <functional>
#include <mutex>
#include <assert.h>

using namespace std;

#define MAX_CPU 32 // Can go higher or lower later as an argument


/*
chunk() takes in the mmap'd file buffer, CPU count, and size of the buffer. It splits it into CPUcount chunks
and returns a vector 'chunks'.
*/

vector <string> chunk(const char &buff, const uint8_t numCPU, const size_t length);


/*
aaparse() takes in a string from the vector chunk returns. It parses this string and returns
its own vector of strings (TDB: vector of vector of strings? Probably).
*/

void aaparse(const string str, vector<string>& parsedvals);



class Timer
{
    timeval timer[2];

  public:

    timeval start()
    {
        gettimeofday(&this->timer[0], NULL);
        return this->timer[0];
    }

    timeval stop()
    {
        gettimeofday(&this->timer[1], NULL);
        return this->timer[1];
    }

    int duration() const
    {
        int secs(this->timer[1].tv_sec - this->timer[0].tv_sec);
        int usecs(this->timer[1].tv_usec - this->timer[0].tv_usec);

        if(usecs < 0)
        {
            --secs;
            usecs += 1000000;
        }

        return static_cast<int>(secs * 1000 + usecs / 1000.0 + 0.5);
    }
};


mutex mtx;

int main(){
	// for benchmarking

	Timer tm;
	tm.start();
	
	// real shit
	int64_t fd = -1;
	struct stat buff;
	char * logbuff;
	vector<string> threadbuffs;
	uint8_t tmpCPU;
	
	size_t i = 0;

	// Check for root, we need it
	// if(getuid() != 0)
	// 	err(1, "getuid != 0");
	
	// Check for CPU core count
	if ((tmpCPU = sysconf( _SC_NPROCESSORS_ONLN )) < 1 || tmpCPU > 32)
		tmpCPU = 2; // If we get some weird response, assume that the system is at least a dual core. It's 2014.

	const uint8_t numCPU = tmpCPU; // if your CPU count changes partway through... bad

	// Get a handle to the file, get its attributes, and then mmap it
	if ((fd = open("/var/log/syslog", O_RDONLY, 0)) == -1)
		err(1, "open on syslog failed :(");

	if(fstat(fd, &buff) == -1)
		err(1, "fstat failed");

	if(( logbuff = 
		static_cast<char*>(mmap(
			NULL,				// OS chooses address
			buff.st_size,		// Size of file
			PROT_READ,			// Read only
			MAP_PRIVATE,		// copy on write
			fd,
			0
			))
		) == MAP_FAILED
	)
		err(1, "MAP_FAILED");
				// and fill in a vector of parsed values	
	close(fd); 	// we don't need the file descriptor anymore, use buffer from now on

	const_cast<const char *> (logbuff);	//I should do this better.
	// split buffer
	
	const_cast<vector<string>& > (threadbuffs) = chunk(*logbuff, numCPU, buff.st_size);
	munmap(logbuff, buff.st_size); 	// no more logbuff. We use threadbuffs now. :)
	
	vector<std::thread> threads;
	vector<string> parsedvals;


	// create threads. Pass thread *it, the string from that iterative position. cout << ' ' << *it;
	for (vector<string>::iterator it = threadbuffs.begin() ; it != threadbuffs.end(); ++it)
		threads.push_back(thread(aaparse, *it, std::ref(parsedvals)));	
	

	// join threads
	for_each(threads.begin(), threads.end(), mem_fn(&std::thread::join));

	for (vector<string>::iterator its = parsedvals.begin() ; its != parsedvals.end(); ++its)
	//	cout << *its << "\n\n";

	assert(parsedvals.size() == numCPU);

	tm.stop();
	cout << "\nduration:: " << tm.duration() << endl;
	return 0;	
}

// parse is mostly just test code right now to verify a few things and benchmark
void aaparse(const string str, vector<string>& parsedvals){

//	size_t i = 0;

// Parse out individual pieces of information that are relevant:
//	operation - may not even be necessary?
//	profile
//	name
// denied_mask="w"
// Generate a rule:[permission] [path] [denied_mask] for [profile]

//This part is clearly broken. It's either broken here or it's broken in chunk.
// See issue #5 https://github.com/insanitybit/SyslogParser/issues/5

		size_t pos1 = str.find("apparmor=\"");
		if(pos1 > str.length())
			err(1, "aaparse overflow");

		size_t pos2 = str.find("\""); // test
		string pstr (str.substr(pos1, 200));

		mtx.lock();
		parsedvals.push_back(pstr);
		cout << pstr << "\n\n"; // If you want to print out a string for debug, do here
		mtx.unlock();

}


vector<string> chunk(const char &buff, const uint8_t numCPU, const size_t length){

	size_t i;
	uint8_t j;
	size_t lp = 1;
	const string ch (&buff);
	vector<string> chunks;
	// cout << " length = " << length << endl;

	for(j = 1; j <= numCPU; j++) {
		i = (static_cast<float>(length) * (static_cast<float>(j) / static_cast<float>(numCPU)));

		while(i < length && ch[i] != '\n') 	// check length before \n, save one operation.
 			i++;

		const char * tmbuff = new char[(i - lp) + 1];	// create char array of appropriate size

		string rbuff (tmbuff);				//I want to avoid this copy later. Seems dumb.
		delete[] tmbuff;					// release char array
		rbuff.back() = '\0';				// necessary?

			if(&rbuff == NULL)
				err(1, "rbuff is null");

		rbuff = ch.substr(lp, (i - lp));
		lp = i + 1; // lp is equal to the character AFTER the new line
		chunks.push_back(rbuff);
 	}

return chunks;
}
