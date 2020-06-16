//
// concurrent_queue
//
#include <queue>

class SDL2_Scoped_Lock
{
private:
	SDL_mutex* m_mutex;
public:
	SDL2_Scoped_Lock(SDL_mutex* pMutex)
		: m_mutex(pMutex)
	{
		SDL_LockMutex(pMutex);
	}

	~SDL2_Scoped_Lock()
	{
		unlock();
	}

	void unlock()
	{
		if (m_mutex)
		{
			SDL_UnlockMutex(m_mutex);
			m_mutex = nullptr;
		}
	}
};

template<typename Data>
class concurrent_queue
{
private:
    std::queue<Data> the_queue;
    SDL_mutex* the_mutex;
    SDL_cond* the_condition_variable;
public:
	concurrent_queue()
	{
		the_mutex = SDL_CreateMutex();
		the_condition_variable = SDL_CreateCond();
	}
	~concurrent_queue()
	{
		SDL_DestroyMutex(the_mutex);
		SDL_DestroyCond(the_condition_variable);
	}

    void push(Data const& data)
    {
        SDL2_Scoped_Lock lock(the_mutex);
        the_queue.push(data);
        lock.unlock();
		SDL_CondSignal(the_condition_variable);
    }

    bool empty() const
    {
        SDL2_Scoped_Lock lock(the_mutex);
        return the_queue.empty();
    }

    bool try_pop(Data& popped_value)
    {
        SDL2_Scoped_Lock lock(the_mutex);
        if(the_queue.empty())
        {
            return false;
        }
        
        popped_value=the_queue.front();
        the_queue.pop();
        return true;
    }

    void wait_and_pop(Data& popped_value)
    {
        //SDL2_Scoped_Lock lock(the_mutex);
		SDL_LockMutex(the_mutex);
        while(the_queue.empty())
        {
			SDL_CondWait(the_condition_variable, the_mutex);
        }
        
        popped_value=the_queue.front();
        the_queue.pop();
    }
};


