
/* Copyright declaration */

/*
 *  VSM Implementation: vast-state (vastate)
 *  gateway.h - gateway interface class - gateway for system enterance
 *
 *  ver. 20080521  Csc
 */

/*
 *  class structure:
 *  gateway    <--- gateway_impl
 *  msghandler <-|
 *
 *  msghandler from network model of vast
 *
 */

#ifndef _VASTATE_IGATEWAY_H
#define _VASTATE_IGATEWAY_H

#include "vastate_typedef.h"

namespace VAST {

    class gateway
    {
    public:
        // startup gateway
        virtual bool start (const Addr & listen_addr) = 0;

        // stop gateway
        virtual bool stop () = 0;

        inline
        id_t get_id ()
        {
            return _id;
        }

    protected:
        // to enforce constrution gateway from vastate
        gateway (id_t my_id)
            : _id (my_id)   {}

        virtual ~gateway ()
        {
        }

    private:
        id_t    _id;
    };

}

#endif /* _VASTATE_IGATEWAY_H */
