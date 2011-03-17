
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
        _up_cond = new ACE_Condition<ACE_Thread_Mutex>(mutex);
        
        printf ("VASTThread::open (), before activate thread count: %lu\n", this->thr_count ()); 
        
        // activate the ACE network layer as a thread
        // NOTE: _active should be set true by now, so that the loop in svc () may proceed correctly
        _active = true;
        this->activate (); 

        printf ("after calling activate ()\n");
            
        // wait until server is up and running (e.g. svc() is executing)
        mutex.acquire ();
        _up_cond->wait ();
        mutex.release ();
        
        delete _up_cond;
        _up_cond = NULL;
                    
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
            _down_cond = new ACE_Condition<ACE_Thread_Mutex>(mutex);
                   
            printf ("VASTThread::close () thread count: %lu (before closing)\n", this->thr_count ()); 

            // allow the reactor to leave its event handling loop
            _active = false;

            // wait until the svc() thread terminates
            mutex.acquire ();
            _down_cond->wait ();
            mutex.release ();
            
            delete _down_cond;
            _down_cond = NULL;

            _world = NULL;

            printf ("VASTThread::close (), thread count: %lu (after closing)\n", this->thr_count ()); 
        }

        return 0;
    }
    
    // service method
    int 
    VASTThread::svc (void)
    {
        // NEW_THREAD
        printf ("VASTThread::svc () called\n");

        // wait a bit (doesn't seem necessary)
        // NOTE the 2nd parameter is specified in microseconds (us) not milliseconds
        //ACE_Time_Value duration (0, 500000);
        //ACE_OS::sleep (duration); 

        // continue execution of original thread in open ()
        _up_cond->signal ();

        // how much time in microseconds for each frame
        size_t  time_budget = MICROSECOND_PERSEC/_ticks_persec;
        size_t  sleep_time = 0;         // time to sleep (in microseconds)
        //int     time_left = 0;          // how much time left for ticking VAST, in microseconds
    
        size_t  tick_per_sec = 0;       // tick count per second
        size_t  tick_count = 0;         // # of ticks so far (# of times the main loop has run)
 
        printf ("\nVASTThread svc (): time_budget: %lu us ticks_persec: %d\n\n", time_budget, _ticks_persec);

        // entering main loop
        while (_active)
        {   
            tick_count++;
            tick_per_sec++;
            
            // whether per-sec tasks were performed
            bool per_sec = false;
               
            // execute tick while obtaining time left, sleep out the remaining time
            // ESSENTIAL: execute tick () per cycle, the parameters are all optional
            sleep_time = ((VASTVerse *)_world)->tick (time_budget, &per_sec);
                        
            if (per_sec)
            {
                // print something to show liveness
                //ACE_Time_Value curr_time = ACE_OS::gettimeofday ();
                //printf ("%ld s, tick %lu, tick_persec %lu, sleep: %lu us\n", (long)curr_time.sec (), tick_count, tick_per_sec, sleep_time);
                tick_per_sec = 0;
            }

            if (sleep_time > 0)
            {
                //printf ("sleep %d\n", sleep_time);
                // NOTE the 2nd parameter is specified in microseconds (us) not milliseconds
                ACE_Time_Value duration (0, sleep_time);            
                ACE_OS::sleep (duration); 
            }            
        }

        printf ("VASTThread::svc () leave ticking loop\n");

        // continue execution of original thread in close ()
        // to ensure that svc () will exit
        if (_down_cond != NULL)
            _down_cond->signal ();

        return 0;
    }

} // namespace Vast
