
Overview
========
VAST is a P2P network library that supports virtual environment 
applications such as Massively Multiplayer Online Games (MMOG) or 
large-scale simulations. It is scalable to millions of nodes, 
bandwidth-efficient, and simple to use.

VAST is released under the GNU LESSER GENERAL PUBLIC LICENSE, 
see "LICENSE.txt" for more information.

Included with this release is the compiled Win32 DLL and partial
source code of ACE (used for network communications). Please see 
"ACE_wrappers/COPYING" for licensing information. 

Binaries of "zlib" is also included for compression purposes
(used by VON's forwarding model). Please see "zlib/README.txt" 
for licensing information.

This release has been compiled and tested with the following compilers:

    - Visual C++ 2005 Express
    - Visual C++ 2003
    - GNU C++
   
Please see "INSTALL.TXT" for instructions on how to build VAST.

Additional information can be found at the VAST homepage: 
http://vast.sourceforge.net

Inquiries may be sent to the VAST mailing list:
vast-tech@lists.sourceforge.net



Directory structures
====================
/include        public/shared include files
/VAST           source and include files for VAST       (P2P overlay for neighbor discovery)
/VASTutil       source code for debug info/logger/movement model library

/sim            source and include files for running simulations:
                "VASTsim"           (Win32 & Linux)     simulator for VAST
                "VASTsim_gui"       (Win32)             VASTsim GUI interface
                "VASTsim_console"   (Win32 & Linux)     VASTsim console interface
                
/demo           various demo programs:
                "demo_chatva"       (Win32)             a movable client using ACE                
                "demo_gateway"      (Win32 & Linux)     

/test           various functionality test programs
/bin            default output directory for compiled binaries

/ACE_wrappers   selected source and precompiled binaries of ACE (for network layer support)
/zlib           include files and precompiled binaries of zlib  (for compression purposes)

            
                        
Filename explanations
=====================
below are the lists of the header files and their respective functions:

/include (for VAST)

config.h                library-wide configs/definitions/macros (loaded by all other files)
msghandler.h            interface to allow network messages be processed by various "hooks"
network.h               generic network interface (used by the main 'vast' class)
typedef.h               definitions for all data structures used by VAST
vast.h                  the main interface of the VAST library (used by a single node)
vastsim.h               main interface for the VAST simulator (VASTsim)
vastverse.h             factory class for creating/destorying a VAST node
voronoi.h               main interface for Voronoi-related functions (used by the main 'vast' class)

/vast

net_ace.h               physical (real) network layer using ACE (subclass of 'network')
net_ace_acceptor.h      class for accepting incoming connections
net_ace_handler.h       class for handling a TCP stream connection
net_emu.h               emulated (virtual) network layer
net_emubridge.h         a shared storage of address mappings for the emulated network layer
net_emu_bl.h            bandwidth-limited version of net_emu
net_emubridge_bl.h      bandwidth-limited version of net_emubridge
net_msg.h               message class used by all network implementations
vast_dc.h               implemention of the 'vast' interface (Direct Connection, or DC model)
vast_fo.h               implemention of the 'vast' interface (Forwarding, or FO model)


Contributors
============

(listed alphabetically by last names)

Shao-Chen Chang (cscxcs@gmail.com)              Bandwidth-limited simulator (net_emu_bl)
Tsu-Han Chen    (bkyo0829@yahoo.com.tw)         VON forwarding (FO) model 
Shun-Yun Hu     (syhu@yahoo.com)                VON direct connection (DC) model, framework
Guan-Ming Liao  (gm.liao@msa.hinet.net)         Voronoi-related classes

