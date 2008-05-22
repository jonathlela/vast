
/* Copyright declaration */

/*
 *  VSM Implementation: vast-state (vastate)
 *  gateway_impl.h - gateway class - gateway for system enterance
 *
 *  ver. 20080520  Csc
 */

/*
 *  class structure:
 *  gateway    <--- gateway_impl
 *  msghandler <-|
 *
 *  msghandler from network model of vast
 *
 */

#include "precompile.h"
#include "gateway_impl.h"

namespace VAST {

    gateway_impl::gateway_impl 
        (id_t my_id, 
         VAST::vastverse & v, VAST::network *net, const system_parameter_t & sp)
        : gateway (my_id), _vastworld (v), _net (net), _sp (sp)
        , _overlay (NULL)
    {
    }

    gateway_impl::~gateway_impl ()
    {
    }

    // startup gateway
    bool gateway_impl::start (const Addr & listen_addr)
    {
        if (_overlay != NULL)
            return true;

        if ((_overlay = _vastworld.create_node (_net, 20)) == NULL)
            return false;

        VAST::Addr la = listen_addr;
        _overlay->join (get_id (), _sp.aoi, GATEWAY_DEFAULT_POSITION, la);

        return true;
    }

    // stop gateway
    bool gateway_impl::stop ()
    {
        if (_overlay != NULL)
        {
            _vastworld.destroy_node (_overlay);
            _overlay = NULL;
        }

        return true;
    }

    // returns whether the message has been handled successfully
    bool gateway_impl::
        handlemsg (id_t from_id, msgtype_t msgtype, timestamp_t recvtime, char *msg, int size)
    {
        return false;
    }

    // do things after messages are all handled
    void gateway_impl::
        post_processmsg ()
    {
    }

} /* namespace VAST */

