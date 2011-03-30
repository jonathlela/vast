

Overview
========
VAST is an open source software library (under LGPL license) that allows 
you to build scalable virtual world applications based on P2P techniques. 
It mainly supports Spatial Publish Subscribe (SPS) and is based on 
Voronoi-based Overlay Network (VON). Additionally, Voronoi Self-organizing 
Overlay (VSO) helps to perform load balancing, and VoroCast helps with 
message forwarding to large areas.

VAST is released under the GNU LESSER GENERAL PUBLIC LICENSE,
see "LICENSE.txt" for more information.

This release has been compiled and tested with the following compilers:

    - Visual C++ 2010 Express
    - g++ (Ubuntu 4.4.3-4ubuntu5) 4.4.3

For instructions on how to build VAST, please refer to the URL:
http://vastlib.wikispaces.com/Installation

Additional information can be found at the VAST homepage:
http://vast.sourceforge.net

Inquiries can be sent to VAST mailing list:
vast-tech@lists.sourceforge.net


Functions
=========

VAST currently supports the following main SPS functions:
(see "/common/VAST.h" for actual C++ header)

join (gateway)             - joining the VAST P2P overlay created by a given 'gateway'
leave ()                   - leave the P2P overlay
subscribe (area, layer)	   - subscribe 'area' to receive messages at a specified 'layer'
publish (area, layer, msg) - publish a message 'msg' to an 'area' at the specified 'layer'
move (subID, area)         - move an existing subscription area to a new 'area'
send (msg)                 - send a message 'msg' to a target host (specified within 'msg')
list (area)                - list currently known subscribers within 'area'
receive ()                 - receive messages from other nodes via 'send ()' or 'publish ()'

NOTE: VASTATE was introduced in 0.4.2 but is not functional in 0.4.4 as there has been heavy
      redesign of how VAST internally works in 0.4.4. So right now it may be broken. The
      following is kept for reference.

For state management, the VASTATE library builds upon VAST and provides the following services:

Agent (user) functions:

login (URL, authentication) - login the system with an authentication token
logout ()                   - logout the system
send (msg)                  - send a message to the gateway arbitrator (i.e., the 1st server)
setAOI ()                   - set the area of interest (AOI) for myself
join (position)             - join at a location to send events / receive updates
leave ()                    - leave the location
move (position)             - move to a new location while changing AOI
createEvent ()              - obtain an event message
act (event)                 - publish an application-specific event


Arbitrator (server) functions:

join (position)             - join at a specific location
leave ()                    - leave the system
admit (agent, status)       - allow an agent to enter the region I manage
send (msg)                  - publish a message to a specific agent target
createObject ()             - create shared object
destroyObject ()            - destroy shared object
updateObject ()             - modify the states of a shared object
moveObject ()               - move the object to a new location
notifyLoading ()            - application notifies VASTATE of loading to perform load balancing


see details in the following headers:

"/common/VASTATE.h",
"/common/Agent.h",
"/common/Arbitrator.h"
"/common/AgentLogic.h",
"/common/ArbitratorLogic.h"


Dependencies
============

Included with this release is the compiled Win32 DLL and partial source code of
ACE (used for network communications). Please see
"\Dependencies\licenses\ACE_wrappers" for licensing information.

Binaries of "zlib" is also included for compression purposes (used by VON's
forwarding model). Please see "\Dependencies\licenses\zlib" for licensing
information.



Directory structures
====================
/bin            default output directory for compiled binaries
/build          temporary file holder during compiling
/common         shared include files and common library
/Demo           various demo programs:
                "demo_chatva"       (Win32)             a movable client using ACE
                "demo_console"      (Win32/Linux)       console (text-only) version
                "VASTsim_gui"       (Win32)             VASTsim GUI interface
                "VASTsim_console"   (Win32)             VASTsim console interface

/Dependencies   external libraries used by VAST

/lib            default directory for compiled libraries
/VAST           source and include files for VAST       (P2P overlay for spatial publish/subscribe)
/VASTATE        source and include files for VASTATE    (P2P state management)
/VASTATEsim     simulator for VASTATE
/VASTnet        network layer for VAST (both real / simulated)
/VASTsim        simulator for VAST

* note that some "Dev" version of the libraries may also exist which are still
  under experimental testing



Contributors
============

(listed alphabetically by last names)

Shao-Chen Chang (cscxcs@gmail.com)              Bandwidth-limited simulator (net_emu_bl)
Tsu-Han Chen    (bkyo0829@yahoo.com.tw)         VON forwarding (FO) model
Chien-Hao Chien (chienhao1@gmail.com)           VASTsim
Shun-Yun Hu     (syhu@yahoo.com)                framework, VONpeer, Relay
Guan-Ming Liao  (gm.liao@msa.hinet.net)         Voronoi-related classes
Tzu-Hao Lin     (singy000@gmail.com)            Vivaldi physical coordinate

