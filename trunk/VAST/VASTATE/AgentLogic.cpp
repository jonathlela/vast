/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2009 Shun-Yun Hu (syhu@yahoo.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "AgentLogic.h"
#include "Agent.h"

namespace Vast
{
    // store access to Agent class for the callback to perform Agent-specific tasks
    void 
    AgentLogic::registerInterface (void *agent)
    {
        _agent = agent;
        _self  = ((Agent *)_agent)->getSelf ();
    }

    void 
    AgentLogic::unregisterInterface ()
    {
        _agent = NULL;
    }

    Node *
    AgentLogic::getSelf ()
    {
        return _self;
    }
}

