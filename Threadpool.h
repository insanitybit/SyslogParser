#ifndef THREADPOOL_H
#define THREADPOOL_H

using namespace std;
// T = function
// R = container to store output
// S = data to be acted upon
template<class T, class R, class S>
class Threadpool
{
	public:
		Threadpool(size_t count);
		void set_function(T);
		void set_input(R&);
		void set_output(S&);
		void set_output(S&, size_t i);
		S get_output();
		void execute_no_atomic();
		void join();
		~Threadpool();

	private:
		void thread_exec();
		size_t thread_count;
		size_t active_count;
		vector<thread> threads;
		R input;
		S output;
		T fn;
		mutex mtx;
		bool t;
	
};

template<class T, class R, class S> Threadpool<T,R,S>::Threadpool(size_t count){
	threads.resize(count);
	thread_count = count;
	active_count = 0;
}

template<class T, class R, class S> void Threadpool<T,R,S>::set_function(T f){
	fn = f;
}

template<class T, class R, class S> void Threadpool<T,R,S>::set_input(R& i){
	input = i;
}
template<class T, class R, class S> void Threadpool<T,R,S>::set_output(S& o){
	output = o;

	//output.resize(input.size());
}

template<class T, class R, class S> void Threadpool<T,R,S>::set_output(S& o, size_t i){
	output = o;
}

template<class T, class R, class S> void Threadpool<T,R,S>::execute_no_atomic(){
	// wrap the function we passed in into a thread, have each thread use the code below

	for (int i = 0; i < thread_count; ++i)
	{
		threads[i] = thread(&Threadpool::thread_exec, this);
		
	}
}

template<class T, class R, class S> void Threadpool<T,R,S>::thread_exec(){
//	size_t output_index = 0;
	auto currentIndex = input.begin();

	while(currentIndex != input.end()) {
		mtx.lock();

	    if(currentIndex == input.end()) {
	        break;
	    }
	    currentIndex++;
	    //cout << output.size() << endl;
	    mtx.unlock();

		fn(cref(*currentIndex), ref(output));
	}
}

template<class T, class R, class S> void Threadpool<T,R,S>::join(){
	//for_each(threads.begin(), threads.end(), mem_fn(&std::thread::join));

	//cout << threads.size() << endl;
	for (int i = 0; i < thread_count; ++i)
	{
		threads[i].join();
	}

}

template<class T, class R, class S> S Threadpool<T,R,S>::get_output(){
	return output;
}

template<class T, class R, class S> Threadpool<T,R,S>::~Threadpool(){
	threads.clear();
}
#endif