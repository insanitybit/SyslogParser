#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <iostream>
#include <functional>
#include <algorithm>
#include <thread>
#include <mutex>
#include <vector>
#include <condition_variable>
#include <atomic> 
//#include <assert.h>

// T = function
// R = container to store output
// S = data to be acted upon
template<class T, class R, class S>
class Threadpool
{
	public:
		Threadpool(const T&);
		Threadpool(const size_t count);
		Threadpool(const size_t count, const T&);

		void set_function(const T&);
		void execute_no_atomic(R&, S&);
		void execute_atomic(R&, S&);
		void join();
		void sleep_all();
		void wake_all();
		size_t get_active_count();
		size_t get_items_processed();
		size_t get_thread_count();

	private:
		void thread_exec(R&, S&, std::atomic<size_t>& it);
		void thread_exec_i(R&, S&, std::atomic<size_t>& it);
		size_t thread_count;
		size_t active_count;
		std::vector<std::thread> threads;
		T fn;
		std::mutex mtx;
		std::condition_variable condition;
		std::atomic<size_t> it;
		bool spin;
};

template<class T, class R, class S> 
Threadpool<T,R,S>::Threadpool(const size_t thread_count){
	threads.resize(thread_count);
	this->thread_count = thread_count;
	active_count = 0;
	it = 0;
	spin = false;
}

template<class T, class R, class S> 
Threadpool<T,R,S>::Threadpool(const T& fn){
	thread_count = std::thread::hardware_concurrency();
	this->fn = fn;
	threads.resize(thread_count);
	active_count = 0;
	it = 0;
	spin = false;
}

template<class T, class R, class S> 
Threadpool<T,R,S>::Threadpool(const size_t thread_count, const T& fn){
	threads.resize(thread_count);
	this->fn = fn;
	this->thread_count = thread_count;
	active_count = 0;
	it = 0;
	spin = false;
}

template<class T, class R, class S> 
void Threadpool<T,R,S>::set_function(const T& fn){
	this->fn = fn;
}

template<class T, class R, class S> 
void Threadpool<T,R,S>::execute_no_atomic(R& input, S& output){
	active_count = thread_count;
	for (size_t i = 0; i < thread_count; ++i)
	{
		threads[i] = std::thread(&Threadpool::thread_exec, this, std::ref(input), std::ref(output), std::ref(it));
	}
}

template<class T, class R, class S> 
void Threadpool<T,R,S>::thread_exec(R& input, S& output, std::atomic<size_t>& it){
	std::unique_lock<std::mutex> lck(mtx);
	while(it < input.size()) {
		while(spin){
			active_count--;
			condition.wait(lck);
			active_count++;
		}
		size_t currentIndex = it++;
		if(currentIndex >= input.size()) {
			it--;
			break;
		}
		fn(std::ref(input[currentIndex]), std::ref(output));
	}
	active_count--;
}

template<class T, class R, class S> 
void Threadpool<T,R,S>::execute_atomic(R& input, S& output){
	active_count = thread_count;
	for (size_t i = 0; i < thread_count; ++i)
	{
		threads[i] = std::thread(&Threadpool::thread_exec_i, this, std::ref(input), std::ref(output), std::ref(it));
	}
}

template<class T, class R, class S> 
void Threadpool<T,R,S>::thread_exec_i(R& input, S& output, std::atomic<size_t>& it){
	std::unique_lock<std::mutex> lck(mtx);
	while(it < input.size()) {
		while(spin){
			active_count--;
			condition.wait(lck);
			active_count++;
		}
		size_t currentIndex = it++;
		if(currentIndex >= input.size()) {
			it--;
			break;
		}
		fn(std::ref(input[currentIndex]), std::ref(output), currentIndex);
	}
	active_count--;
}

template<class T, class R, class S> 
void Threadpool<T,R,S>::join(){
	for (size_t i = 0; i < thread_count; ++i)
	{
		threads[i].join();
	}
}

// sleep and wake not implemented
template<class T, class R, class S> 
void Threadpool<T,R,S>::sleep_all(){
	spin = true;
}

// sleep and wake not implemented
template<class T, class R, class S> 
void Threadpool<T,R,S>::wake_all(){
	spin = false;
	condition.notify_all();
}

template<class T, class R, class S> 
size_t Threadpool<T,R,S>::get_active_count(){
	return active_count;
}

template<class T, class R, class S> 
size_t Threadpool<T,R,S>::get_items_processed(){
	return it;
}

template<class T, class R, class S> 
size_t Threadpool<T,R,S>::get_thread_count(){
	return thread_count;
}

#endif