
#include "VASTThread.h"
#include "VASTVerse.h"      // for accessing VASTVerse object

namespace Vast
{   

    //
    //  Standard ACE_Task methods (must implement)
    //

    // service initialization method
    int 
    VASTThread::open (void *p)
    {
        // store pointer to the VASTVerse we'll be running
        _world = p;

        // we need to use a 'ACE_Condition' to wait for the thread to properly comes up
        // otherwise there might be conflict in data access by two threads
        
        ACE_Thread_Mutex mutex;
        _cond = new ACE_Condition<ACE_Thread_Mutex>(mutex);
        
        printf ("VASTThread::open (), before activate thread count: %lu\n", this->thr_count ()); 
        
        // activate the ACE network layer as a thread
        // NOTE: _active should be set true by now, so that the loop in svc () may proceed correctly
        _active = true;
        this->activate (); 
            
        // wait until server is up and running (e.g. svc() is executing)
        mutex.acquire ();
        _cond->wait ();
        mutex.release ();
        
        delete _cond;
                    
        printf ("VASTThread::open (), after activate thread count: %lu\n", this->thr_count ()); 

        return 0;
    }
    
    // service termination method;
    int 
    VASTThread::close (u_long i)
    {
        if (_active == true)
        {
            // notify event processing loop to stop
            //_active = false;
            //_world  = NULL;

            ACE_Thread_Mutex mutex;
            _cond = new ACE_Condition<ACE_Thread_Mutex>(mutex);
                   
            printf ("VASTThread::close () thread count: %lu (before closing)\n", this->thr_count ()); 
            
            // allow the reactor to leave its event handling loop
            _active = false;
            _world = NULL;            
                            
            // wait until the svc() thread terminates
            mutex.acquire ();
            _cond->wait ();
            mutex.release ();
            
            delete _cond;

            printf ("VASTThread::close (), thread count: %lu (after closing)\n", this->thr_count ()); 
        }

        return 0;
    }
    
    // service method
    int 
    VASTThread::svc (void)
    {
        // continue execution of original thread in open ()
        _cond->signal ();

        // how much time in microseconds for each frame
        size_t  time_budget = 1000000/_ticks_persec;
        size_t  sleep_time = 0;         // time to sleep (in microseconds)
        int     time_left = 0;          // how much time left for ticking VAST, in microseconds
    
        size_t  tick_per_sec = 0;       // tick count per second
        size_t  tick_count = 0;         // # of ticks so far (# of times the main loop has run)

        // entering main loop
        while (_active)
        {   
            tick_count++;
            tick_per_sec++;
    
            // make sure this cycle doesn't exceed the time budget
            TimeMonitor::instance ()->setBudget (time_budget);
        
            // whether per-sec tasks were performed
            bool per_sec = false;
               
            // execute tick while obtaining time left, sleep out the remaining time
            time_left = TimeMonitor::instance ()->available ();
    
            // ESSENTIAL: execute tick () per cycle, the parameters are all optional
            sleep_time = ((VASTVerse *)_world)->tick (time_left, &per_sec);
            
            if (per_sec)
            {
                ACE_Time_Value curr_time = ACE_OS::gettimeofday ();
                printf ("%ld s, tick %lu, tick_persec %lu, sleep: %lu us, time_left: %d\n", (long)curr_time.sec (), tick_count, tick_per_sec, sleep_time, time_left);
                tick_per_sec = 0;
            }

            if (sleep_time > 0)
            {
                // NOTE the 2nd parameter is specified in microseconds (us) not milliseconds
                ACE_Time_Value duration (0, sleep_time);            
                ACE_OS::sleep (duration); 
            }
        }

        // continue execution of original thread in close ()
        // to ensure that svc () will exit
        if (_cond != NULL)
            _cond->signal ();

        return 0;
    }

    /*
    void 
    VASTThread::checkJoin ()
    {    
        // obtain a created VAST node, if any
        switch (_state)
        {
        case ABSENT:
            if ((_vastnode = ((VASTVerse *)_world)->getVASTNode ()) != NULL)
            {   
                _sub_id = _vastnode->getSubscriptionID (); 
                _state = JOINED;
            }
            break;
    
        default:
            break;
        }
    }
    */


} // namespace Vast