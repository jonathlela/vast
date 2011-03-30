
There are various demo programs of VAST under win32 in the /bin directory.

Simulation Demo
---------------

The easiest way to gain a visualization of how VAST works is to run the
VASTsim_gui.exe demo and press 'Enter'. After executing, use the following keys 
for commands:

Enter - continue the simulation
Space - pause the simulation
e     - toggle edge
f     - follow mode

You may also click on differnet nodes to see their local views. 

Basically, the big circle is the area of interest (AOI) of the currently focused node.
And after running for a while, you may start to see edges appear. These edges
define the regions managed by each "Matcher". The Matchers are super-peers elevated
from regular peers, and begin from node 1, to node 2, 3, 4... 

You will see different Matchers actually have different views on the spatial partitioning,
this is because each Matcher only knows its own region and its neighbors, who may host
user peers (called "Clients") of interest to itself. This interest is defined by 
what the Clients this Matcher manages can see. In other words, the AOI of the Matcher,
is the collective AOI of its Clients. 


Real Network Demo
-----------------

In another demo, to see VAST with real networking. run the following batch files:

run-gateway-gui.bat		    (to start a gateway node in GUI mode)
run-4-matchers-gui.bat		(to start 4 matcher nodes in GUI mode)
run-10-clients-console.bat	(to start 10 clients in console mode)

You can start clients for as many as you'd like (supportable by your machine). 
When you switch back to the gateway view, you can see how clients may move according
to gateway's AOI (which is set to be bigger than the usual client AOI). 

Again, after a while you'll be able to see edges appearing, which indicate the
partitioning of the managed regions of the matchers. 
