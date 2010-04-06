
VASTnet := VASTnet
VAST    := VAST 
VASTsim := VASTsim
common  := common
#VASTATE := VASTATE
#VASTATEsim := VASTATEsim
demo_console := Demo/demo_console

# tells make that the following labels are make targets, not filenames
.PHONY: all clean $(VASTnet) $(VAST) $(VASTsim) $(common) $(demo_console)

all: $(common) $(VASTnet) $(VAST) $(VASTsim) $(demo_console)


$(VAST) $(VASTsim) $(common) $(VASTnet) $(demo_console):
	$(MAKE) --directory=$@ $(TARGET)

$(VASTnet) : $(common)
$(VAST) : $(VASTnet) 
$(VASTsim): $(VAST)
$(demo_console) : $(VASTsim)

clean: 
	make TARGET=clean

noace:
	make TARGET=noace 
