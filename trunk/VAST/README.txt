

Overview
========
VAST is a P2P network library that supports virtual environment
applications such as Massively Multiplayer Online Games (MMOG) or
large-scale simulations. It is scalable to millions of nodes,
bandwidth-efficient, and simple to use.

VAST is released under the GNU LESSER GENERAL PUBLIC LICENSE,
see "LICENSE.txt" for more information.

This release has been compiled and tested with the following compilers:

    - Visual C++ 2008 Express

Please see "INSTALL.TXT" for instructions on how to build VAST.

Additional information can be found at the VAST homepage:
http://vast.sourceforge.net

VAST is based on the research about Voronoi-based Overlay Network (VON)
and Spatial Publish Subscribe (SPS). Basically, VAST can be seen as an 
implementation of SPS using VON.

VON: http://vast.sourceforge.net/docs/pub/2006-hu-VON.pdf
SPS: http://vast.sourceforge.net/docs/pub/2009-MMVE-SPS.pdf

Inquiries may be sent to the VAST mailing list:
vast-tech@lists.sourceforge.net



Functions
=========

VAST currently supports the following main SPS functions:
(see "/common/VAST.h" for actual C++ header)

join (position)            - joining the VAST P2P overlay at a virutal coordinate
leave ()                   - leave the P2P overlay
subscribe (area, layer)	   - subscribe 'area' to receive messages at a specified 'layer'
publish (area, layer, msg) - publish a message 'msg' to an 'area' at the specified 'layer'
move (subID, area)         - move an existing subscription area to a new 'area'
send (msg)                 - send a message 'msg' to a target host (specified within 'msg')
list (area)                - list currently known subscribers within 'area'
receive ()                 - receive messages from other nodes via 'send ()' or 'publish ()'

As of release 0.4.0, the following features are not yet supported

- area publication (so currently published message can only be at a point)
- list () currently returns all known subscribers, not just those inside 'area'



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
/dataset        latency / bandwidth distribution dataset used by simulations
/Demo           various demo programs:
                "demo_chatva"       (Win32)             a movable client using ACE
                "demo_console"      (Win32)             console (text-only) version
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

