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
chunk() takes in the mmap'd file buffer, CPU count, and size of the buffer. It splits it into 
CPUcount chunks and returns a vector 'chunks'.
*/

vector <string> chunk(const char &buff, const uint8_t numCPU, const size_t length);

/*
aaparse() takes in a string from the vector chunk returns. It parses this and removes a vector
of string vectors, each one holding the relevant information for an apparmor rule
*/

void aaparse(const string str, vector<vector <string> >& parsedvals);

/*
aagen() will take in a vector of string vectors generated by aaparse. It will look at each string
within a vector and generate the logical rule for that vector.
*/

void aagen(const vector<string>& pvals, vector<string>& rules);


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
	//tm.start();
	
	// real shit
	int64_t fd = -1;
	struct stat buff;
	char * logbuff;
	vector<string> threadbuffs;
	uint8_t tmpCPU;	
	size_t i = 0;

	// Check for root, we need it -- or do we? Might be able to do this in another process.
	// if(getuid() != 0)
	// 	err(1, "getuid != 0");
	
	// Check for CPU core count
	if ((tmpCPU = sysconf( _SC_NPROCESSORS_ONLN )) < 1 || tmpCPU > 32)
		tmpCPU = 2; // If we get some weird response, assume that the system is at least a dual core. It's 2014.

	const uint8_t numCPU = tmpCPU;

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

	close(fd); 	// we don't need the file descriptor anymore, use buffer from now on

	const_cast<const char *> (logbuff);	//I should do this better.
	// split buffer
	tm.start();

	const_cast<vector<string>& > (threadbuffs) = chunk(*logbuff, numCPU, buff.st_size);

	tm.stop();
	cout << "\nchunk:: " << tm.duration() << endl;

	munmap(logbuff, buff.st_size); 	// no more logbuff. We use threadbuffs now. :)

	vector<std::thread> threads;
	vector<vector<string>> parsedvals;

	tm.start();
	// create threads. Pass thread *it, the string from that iterative position. cout << ' ' << *it;
	for (vector<string>::iterator it = threadbuffs.begin() ; it != threadbuffs.end(); ++it)
		threads.push_back(thread(aaparse, std::cref(*it), std::ref(parsedvals)));	
	
	// join threads
	for_each(threads.begin(), threads.end(), mem_fn(&std::thread::join));

	tm.stop();
	cout << "aaparse:: " << tm.duration() << endl;

	assert(parsedvals.size() > 0);

	vector<vector<string> >::iterator it;
	it = unique (parsedvals.begin(), parsedvals.end()); 
  	parsedvals.resize( std::distance(parsedvals.begin(),it) );


  	// Begin rule generation

	tm.start();
  	vector<string> rules;
	i = 0;
	threads.clear();

	for (vector<vector<string> >::iterator it = parsedvals.begin() ; it != parsedvals.end(); ++it){
		// create threads numCPU at a time
		for(i = 0; i < numCPU; i++){
			threads.push_back(thread(aagen, std::cref(*it), std::ref(rules)));
			if(it != parsedvals.end() - 1)//Last one? Iterate, push, and break
				++it;
			else
				break;
		}
		for_each(threads.begin(), threads.end(), mem_fn(&std::thread::join));
		threads.clear();
	}
		//If we break we have to clean up
		for_each(threads.begin(), threads.end(), mem_fn(&std::thread::join));
		threads.clear();

	tm.stop();
	cout << "aagen:: " << tm.duration() << endl;
	cout << "\nDONE\n";
	// tm.stop();
	// cout << "\nduration:: " << tm.duration() << endl;
	return 0;	
}

void aagen(const vector<string>& pvals, vector<string>& rules){
// Information Type A:[operation] [profile] [name] [denied_mask]
// Information Type B:[operation] [profile] [capname]

// Steps involved:
// Take in vector of strings containing either Information Type A or B
// Transform that information into a rule
// Reduce rules
// Expand rules
// Validate rules REGEX
// Rate 'danger' of rule
// Throw out rules
//	cout << "in" << endl;

	// if(!regex_match(pvals[1], regex for valid unix path )){
	// 	exit();
	// }

	// if(pvals[0] != "capable"){ // just like aaparse there is going ot hav eto be
								  // a fork based on this
		//validate path via regex for pvals[2]
	//	}

	// reduce
	// Is the last part of the path (p[2]) trailed by numbers?
	// .so.1251.12 should just be .so.*
	// Try to determine things like version numbers and glob them

	// expand
	// if you see read permission requested for a file ending in .so
	// give it automatic map permission
	// Maybe fstat the owner of the file is. If the process owns it, add owner?

	// throw out rules
	// certain rules may be too dangerous to allow? Nonsensical?

	//concat the pvals into one rule string



	mtx.lock();
	// cout << pvals[0] << "   " << pvals[1] << "   " << pvals[2] << "   " << pvals[3] << endl;
	mtx.unlock();
}

void aaparse(const string str, vector<vector<string> >& parsedvals){

// Parse out individual pieces of information that are relevant:
// Information Type A:[operation] [profile] [name] [denied_mask]
// Information Type B:[operation] [profile] [capname]

	const string apparmor 		= "apparmor=\"";
	const string operation		= "operation=\"";
	const string profile 		= "profile=\"";		// the profile path for the program
	const string denied_mask	= "denied_mask=\"";	// how the program tried ot access the file
	const string name 			= "name=\"";		// the file the program tried to access	
	const string capname		= "capname=\"";
	const string status 		= "STATUS";

	const string atts[3] 	= {profile, name, denied_mask};
	const string catts[2]	= {profile, capname};
	vector<string> attributes;

	size_t i;
	size_t aapos;		// position of apparmor statement beginning
	size_t laapos = 0;	// position of the previous apparmor statement beginning
	size_t pos1;		// beginning of string we want to snip
	size_t pos2;		// end of string we want to snip
	string pstr;

	aapos = str.find(apparmor, laapos);
	if(aapos == string::npos)
		return;

	while(true){

		//find apparmor violation
		aapos = str.find(apparmor, laapos);
		if(aapos == string::npos)
			return;

		aapos += apparmor.length();
		laapos = aapos;

		//handle apparmor="STATUS"
		//if we end up needing "ALLOWED" or "DENIED" just do it here
		pos1 = aapos;
		pos2 = str.find("\"", pos1);
		pos2 -= pos1;
		pstr = str.substr(pos1, pos2);

		if(pstr == status)
			continue;
		
		//is this a capability or not?

		pos1 = str.find(operation, aapos);
		pos1 += operation.length();
		pos2 = str.find("\"", pos1);
		pos2 -= pos1;
		pstr = str.substr(pos1, pos2);
		attributes.push_back(pstr);

		//if it is a capability
		if(pstr == "capable"){
			for(i = 0; i < 2; i++){
				pos1 = str.find(catts[i], aapos);
				if(pos1 == string::npos)
				break;

				pos1 += catts[i].length();
				pos2 = str.find("\"", pos1);
				pos2 = pos2 - pos1;
				pstr = str.substr(pos1, pos2);
				attributes.push_back(pstr);
			}
		}

		//if it is not a capability
		//extract the 3 attributes we want from it. profile, denied mask, name
		else{
			for(i = 0; i < 3; i++){

				pos1 = str.find(atts[i], aapos);

				if(pos1 == string::npos)
				break;

				pos1 += atts[i].length();
				pos2 = str.find("\"", pos1);
				pos2 = pos2 - pos1;
				pstr = str.substr(pos1, pos2);				
				attributes.push_back(pstr);
			}
		mtx.lock();
		parsedvals.push_back(attributes);
		mtx.unlock();
		attributes.clear();
		}
	}
}

vector<string> chunk(const char &buff, const uint8_t numCPU, const size_t length){

	size_t i;
	uint8_t j;
	size_t lp = 1;
	const string ch (&buff);
	vector<string> chunks;
	string rbuff;

	for(j = 1; j <= numCPU; j++) {
		i = (static_cast<float>(length) * (static_cast<float>(j) / static_cast<float>(numCPU)));

 		i = ch.find('\n', i);

 		if(i == string::npos){				//If we search for \n but don't find it
 			i = (length - 1);
			rbuff.back() = '\0';				// necessary?
			rbuff = ch.substr(lp, (i - lp));
			chunks.push_back(rbuff);
			return chunks;
 		}
 		rbuff.back() = '\0';				// necessary?
		rbuff = ch.substr(lp, (i - lp));
		lp = i + 1; // lp is equal to the character AFTER the new line
		chunks.push_back(rbuff);
 	}
return chunks;
}
