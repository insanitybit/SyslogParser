// Apparmor Rule Parser
// Colin O'Brien - insanitybit
// g++ ./syslogparse.cpp -O0 --std=c++0x -pthread -Wall
// source available at: https://github.com/insanitybit/SyslogParser
/*
The goal of this program is to parse the system log for two things:
1) Apparmor denials
2) Iptables logs

The current tools for both of these actions are lacking in terms of both performance and usability.
*/


// testlibs
#include <chrono>

// seccomp
#include <sys/prctl.h>     /* prctl */
#include <seccomp.h>   /* libseccomp */

// necessary
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <sys/mman.h>
#include <string>
#include <stack>
#include <vector>
#include <array>
#include <map>
#include <thread>
#include <mutex>
#include <algorithm>
#include <functional>
#include <assert.h>
#include <err.h>

using namespace std;

/*
chunk() takes in the mmap'd file buffer, CPU count, and size of the buffer. It splits it into 
CPUcount chunks and returns a vector 'chunks'.
*/

vector <string> chunk(const char &buff, const uint32_t numCPU, const size_t length);

/*
Parser fn's will take in the vector from chunks. They parse this and pull out a vector
of string vectors, each one holding the relevant information for a rule
*/

void aaparse(const string str, vector<vector<string> >& parsedvals);
void ipparse(const string str, vector<vector<string> >& parsedvals);
/*
Generators take in a vector of string vectors generated by parse fn's. They look at each string
within a vector and generate the logical rule for that vector.
*/

void aagen(const vector<string>& pvals, vector<string>& rules);
void ipgen(const vector<string>& pvals, vector<string>& rules);

mutex mtx;

int main(int argc, char *argv[]){

	//Sandboxing

	//ToDo: chroot here or have parent process do it, probably parent


	// if(prctl(PR_SET_NO_NEW_PRIVS, 1) == -1)
	//  	err(0, "PR_SET_NO_NEW_PRIVS failed");
	// prctl(PR_SET_NO_NEW_PRIVS, 1); // currently fails, figure out why later

	if(prctl(PR_SET_DUMPABLE, 0) == -1)
		err(0, "PR_SET_DUMPABLE failed");

	scmp_filter_ctx ctx;
	ctx = seccomp_init(SCMP_ACT_KILL);

	//rules
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(rt_sigreturn), 0);
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(exit), 0);
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(exit_group), 0);
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(tgkill), 0);

	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(brk), 0);

	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(read), 0);
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(write), 0);
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fstat), 0);
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(open), 0);
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(close), 0);

	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mprotect), 0);
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mmap), 0);
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(munmap), 0);
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(madvise), 0);

	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(futex), 0);
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(access), 0);
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(execve), 0);
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(clone), 0);


	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getrlimit), 0);
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(rt_sigaction), 0);
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(rt_sigprocmask), 0);
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(set_robust_list), 0);
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(set_tid_address), 0);

	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(prctl), 0);
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(arch_prctl), 0);

	//for benchmarking
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(clock_gettime), 0);

	if(ctx == NULL)
	 	err(0, "ctx == NULL");

	if(seccomp_load(ctx) != 0)			//activate filter
		err(0, "seccomp_load failed"); 

  	cout << "seccomp: enabled" << endl;

	// for benchmarking
    std::chrono::time_point<std::chrono::steady_clock> start, end;
	std::chrono::duration<double> elapsed_seconds;

	// real shit
	int64_t fd = -1;
	uint32_t tmpCPU;	
	uint32_t MAX_CPU = 7600; //TODO: /proc/sys/kernel/threads-max - currently a worst case
	size_t i;
	struct stat buff;
	char * logbuff;
	vector<string> threadbuffs;
	
	void (*parse)(const string str, vector<vector<string> >& parsedvals);
	void (*gen)(const vector<string>& pvals, vector<string>& rules);

	if(argc <= 1){
		cout << "\nPass in either \"apparmor\" or \"iptables\" as a parameter. \n\n";
		exit(1);
	}

	const string dec = argv[1];

   	if(dec == "apparmor"){
   		parse = &aaparse;
		gen = &aagen;
	}else if(dec == "iptables"){
		parse = &ipparse;
		gen = &ipgen;
	}else{
		err(0, "Invalid argument");
	}

	// Activate later when we start doing chroot sandboxing?
	// if(getuid() != 0)
	// 	err(0, "getuid != 0");

/*
setcap-chroot, open file, drop to chroot, do work
*/
	
	// Check for CPU core count
	if ((tmpCPU = sysconf( _SC_NPROCESSORS_ONLN )) < 1 || tmpCPU > MAX_CPU)
		tmpCPU = 2; // If we get some weird response, assume that the system is at least a dual core. It's 2014.

	const uint32_t numCPU = 1; //tmpCPU;

	// Get a handle to the file, get its attributes, and then mmap it
	if ((fd = open("/var/log/syslog", O_RDONLY, 0)) == -1)
		err(0, "open on syslog failed :(");

	if(fstat(fd, &buff) == -1)
		err(0, "fstat failed");

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
		err(0, "MAP_FAILED");

	close(fd); 	// we don't need the file descriptor anymore, use buffer from now on

    start = std::chrono::steady_clock::now();

	const_cast<vector<string>& > (threadbuffs) = chunk(*logbuff, numCPU, buff.st_size);

    end = std::chrono::steady_clock::now();
	elapsed_seconds = end-start;
	cout << "chunk::  " << elapsed_seconds.count() * 1000 << "ms" << endl;
	
	munmap(logbuff, buff.st_size); 	// no more logbuff. We use threadbuffs now. :)

	vector<std::thread> threads;	
	vector<vector<string>> parsedvals;
	start = std::chrono::steady_clock::now();

	// create threads. Pass thread *it, the string from that iterative position. cout << ' ' << *it;
	for (vector<string>::iterator it = threadbuffs.begin() ; it != threadbuffs.end(); ++it)
		threads.push_back(thread(parse, std::cref(*it), std::ref(parsedvals)));
	// join threads
	for_each(threads.begin(), threads.end(), mem_fn(&std::thread::join));

	assert(parsedvals.size() > 0);	
	
	end = std::chrono::steady_clock::now();
	elapsed_seconds = end-start;
	cout << "parse::  " << elapsed_seconds.count() * 1000 << "ms" << endl;
	cout << "size::   " << parsedvals.size() << endl;
	start = std::chrono::steady_clock::now();

	// remove duplicates
	sort(parsedvals.begin(), parsedvals.end());
	vector<vector<string> >::iterator it;
	it = unique (parsedvals.begin(), parsedvals.end()); 
  	parsedvals.resize( distance(parsedvals.begin(),it) );
	
	end = std::chrono::steady_clock::now();
  	elapsed_seconds = end-start;
  	cout << "unique:: " << elapsed_seconds.count() * 1000 << "ms" << endl;
	cout << "size::   " << parsedvals.size() << endl;

	// Begin rule generation
	start = std::chrono::steady_clock::now();
  	vector<string> rules;
	threads.clear();

	for (vector<vector<string> >::iterator it = parsedvals.begin() ; it != parsedvals.end(); ++it){
		// create threads numCPU at a time
		for(i = 0; i < numCPU; i++){
			threads.push_back(thread(gen, std::cref(*it), std::ref(rules)));
			if(it != parsedvals.end() - 1)//Last one? Iterate, push, and break
				++it;
			else
				break;
		}
		for_each(threads.begin(), threads.end(), mem_fn(&std::thread::join));
		threads.clear();
	}
		// If we break we have to clean up
		for_each(threads.begin(), threads.end(), mem_fn(&std::thread::join));
		threads.clear();

	end = std::chrono::steady_clock::now();
	elapsed_seconds = end-start;
	cout << "gen::    " << elapsed_seconds.count() * 1000 << "ms" << endl;

//	cout rules

	for (vector<string>::iterator it = rules.begin() ; it != rules.end(); ++it)
		cout << *it << endl;

	cout << "\nDONE\n";
	return 0;	
}

void ipgen(const vector<string>& pvals, vector<string>& rules){
	// [direction][device][mac][srcip][dstip][protocol][srcport][destport]
	// iptables -A OUTPUT -p tcp -m multiport --dports 80,8080,8000,443 -j ACCEPT

	const string iptables 			= "iptables -A ";
	const string input 				= "INPUT ";
	const string output				= "OUTPUT ";
	const string in_device			= "-i ";
	const string out_device			= "-o ";
	const string source 			= " -s ";
	const string destination 		= " -d ";
	const string protocol			= " -p ";
	const string source_port		= " --sport ";
	const string destination_port	= " --dport ";

	string rule = "";
	mtx.lock();
	if(pvals[0] == "INPUT"){							
				rule  = iptables + input +
						in_device + pvals[1] +
						source + pvals[4] +
						destination + pvals[5] +
						protocol + pvals[6] +
						destination_port + pvals[7];
	}else if(pvals[0] == "OUTPUT"){
				rule  = iptables + output
					+	out_device + pvals[1]
					+	source + pvals[4]
					+	destination + pvals[5]
				 	+	protocol + pvals[6]
				 	+	source_port + pvals[7];
	}else{
		err(0, "pvals[0] is not right.");
	}
	mtx.unlock();
	mtx.lock();
	rules.push_back(rule);
	mtx.unlock();
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

//int8_t danger = -1;
// make this thing a map
// const array<int, 10> caps = 	{				//arbitrary right now for testing
// 							dac_override 	= 5,
// 							setgid 			= 5,
// 							setuid			= 5,
// 							sys_rawio		= 2,
// 							ipc_owner		= 3,
// 							chown			= 3,
// 							fsetid			= 1,
// 							sys_admin		= 10,
// 							sys_chroot		= 1,
// 							sys_ptrace		= 5,
// 							};


	string operation 	= pvals[0];
	string profile		= pvals[1];

	// for(int i = 0; i < pvals.size(); ++i){
	// 	mtx.lock();
	// 	cout << pvals[i] << endl;
	// 	mtx.unlock();
	// }

	if(operation == "capable"){
		// for (int i = 0; i < caps.size(); ++i)
		// {
		// 	if(caps[i] == pvals[2])
		// 		danger += caps[i];
		// }
		string capname = pvals[2];
		string rule = "profile:: " + profile + " rule:: capability " + capname + ",\n";
		//validate capname first
		mtx.lock();
		rules.push_back(rule);
		mtx.unlock();
		return;
	}

	string name = pvals[2];
	string denied_mask = pvals[3];

	size_t cpos;
	if((cpos = profile.find("///")) == string::npos){ //no child found
		string rule = "profile:: " + profile + " child:: NULL rule:: " + name + " " + denied_mask + ",\n";
		mtx.lock();
		rules.push_back(rule);
		mtx.unlock();
		return;
	}

	//get the child path
	string child = profile.substr((cpos + 2), profile.length());
	profile = profile.substr(0, cpos);
	string rule = "profile:: " + profile + " child:: " + child + " rule:: " + name + " " + denied_mask + ",\n";
	mtx.lock();
	rules.push_back(rule);
	mtx.unlock();
	return;
}

void ipparse(const string str, vector<vector<string> >& parsedvals){

/*
IPTABLESINPUT: IN=lo OUT= MAC=00:**:** SRC=127.0.0.1 DST=127.0.0.1 
LEN=240 TOS=0x00 PREC=0x00 TTL=64 ID=10566 DF PROTO=UDP SPT=42102 DPT=123 LEN=220 

[direction][device][mac][srcip][dstip][protocol][srcport][destport]
*/

	const string iptables 			= "IPTABLES";
	const string input 				= "INPUT:";
	const string output				= "OUTPUT:";
	const string in_device			= "IN=";
	const string out_device			= "OUT=";
	const string macaddr			= "MAC=";
	const string source 			= "SRC=";
	const string destination 		= "DST=";
	const string protocol			= "PROTO=";
	const string source_port		= "SPT=";
	const string destination_port	= "DPT=";

	const array <string, 8> atts = {
									in_device,
									out_device,
									macaddr,
									source,
									destination,
									protocol,
									source_port,
									destination_port
									};
	size_t i;
	size_t aapos;		// position of apparmor statement beginning
	size_t laapos = 0;	// position of the previous apparmor statement beginning
	size_t pos1;		// beginning of string we want to snip
	size_t pos2;		// end of string we want to snip
	string pstr;

	vector<string> attributes;

	aapos = str.find(iptables, laapos);
	if(aapos == string::npos)
		return;

	while(true){
		// find apparmor violation
		aapos = str.find(iptables, laapos); //point to beginning of IPTABLES
		if(aapos == string::npos)
			return;

		aapos += iptables.length();			//point to end of IPTABLEs
		laapos = aapos;
		
		// is this input or output
		pos1 = str.find(iptables, aapos); 	//point to beginning of INPUT: or OUTPUT:
		if(pos1 == string::npos)
			break;
		pos1 += iptables.length();			//point to end of INPUT or OUTPUT:
		pos2 = str.find(":", pos1);
		if(pos2 == string::npos)
			break;
		pos2 -= pos1;
		pstr = str.substr(pos1, pos2);
		attributes.push_back(pstr);
		if(pstr != "INPUT" && pstr != "OUTPUT")
			break;


		// This could be faster, can split into two loops instead
		for(i = 0; i < atts.size(); i++){
			pos1 = str.find(atts.at(i), aapos);
			if(pos1 == string::npos)
				return;
			
			if(attributes[0] == "INPUT" && i == 6){
				attributes.push_back("SPT");
				continue;
			}
			else if(attributes[0] == "OUTPUT" && i == 7){
				attributes.push_back("DPT");
				continue;
			}
			if(i == 2){
				attributes.push_back("MAC"); 	// MAC is annoying and unimportant for security.
				continue;						// Plus, removing makes unique() far more effective
			}
			pos1 += atts.at(i).length();
			pos2 = str.find(" ", pos1);
			if(pos2 == string::npos)
				return;
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

	const array<string, 3> atts 	= {profile, name, denied_mask};
	const array<string, 2> catts 	= {profile, capname};
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
		// find apparmor violation
		aapos = str.find(apparmor, laapos);
		if(aapos == string::npos)
			return;

		aapos += apparmor.length();
		laapos = aapos;

		// handle apparmor="STATUS"
		// if we end up needing "ALLOWED" or "DENIED" just do it here
		pos1 = aapos;
		pos2 = str.find("\"", pos1);
		if(pos2 == string::npos)
			return; // I should maybe turn these into continues
		pos2 -= pos1;
		pstr = str.substr(pos1, pos2);
		if(pstr == status)
			continue;
		// is this a capability or not?
		pos1 = str.find(operation, aapos);
		if(pos1 == string::npos)
			return;
		pos1 += operation.length();
		pos2 = str.find("\"", pos1);
		if(pos2 == string::npos)
			return;
		pos2 -= pos1;
		pstr = str.substr(pos1, pos2);
		attributes.push_back(pstr);

		// if it is a capability
		if(pstr == "capable"){
			for(i = 0; i < catts.size(); i++){
				pos1 = str.find(catts.at(i), aapos);
				if(pos1 == string::npos)
				return;

				pos1 += catts.at(i).length();
				pos2 = str.find("\"", pos1);
				if(pos2 == string::npos)
					return;
				pos2 = pos2 - pos1;
				pstr = str.substr(pos1, pos2);
				attributes.push_back(pstr);
			}
		}
		// if it is not a capability
		// extract the 3 attributes we want from it. profile, denied mask, name
		else{
			for(i = 0; i < atts.size(); i++){
				pos1 = str.find(atts.at(i), aapos);
				if(pos1 == string::npos)
					return;

				pos1 += atts.at(i).length();
				pos2 = str.find("\"", pos1);
				if(pos2 == string::npos)
					return;
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

vector<string> chunk(const char &buff, const uint32_t numCPU, const size_t length){

	size_t i;
	uint8_t j;
	size_t lp = 1;
	const string ch (&buff);
	vector<string> chunks;
	string rbuff;

	for(j = 1; j <= numCPU; j++) {
		i = (static_cast<float>(length) * (static_cast<float>(j) / static_cast<float>(numCPU)));
 		i = ch.find('\n', i);
 		if(i == string::npos){			// If we search for \n but don't find it
 			i = (length - 1);
			rbuff = ch.substr(lp, (i - lp));
			chunks.push_back(rbuff);
			return chunks;
 		}
		rbuff = ch.substr(lp, (i - lp));
		lp = i + 1; 					// lp is equal to the character AFTER the new line
		chunks.push_back(rbuff);
 	}
return chunks;
}
