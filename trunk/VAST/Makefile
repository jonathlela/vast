
VASTnet := VASTnet
VAST    := VAST 
VASTsim := VASTsim
common  := common
VASTATE := VASTATE
VASTATEsim := VASTATEsim
demo_console := Demo/demo_console

# tells make that the following labels are make targets, not filenames
.PHONY: all clean $(VASTnet) $(VAST) $(VASTsim) $(common) $(VASTATE) $(VASTATEsim) $(demo_console)

all: $(common) $(VASTnet) $(VAST) $(VASTsim) $(VASTATE) $(VASTATEsim) $(demo_console)

$(VAST) $(VASTsim) $(common) $(VASTnet) $(VASTATE) $(VASTATEsim) $(demo_console):
	$(MAKE) --directory=$@ $(TARGET)

$(VASTnet) : $(common)
$(VAST) : $(VASTnet) 
$(VASTsim): $(VAST)
$(VASTATE) : $(VAST)
$(VASTATEsim) : $(VASTATE)
$(demo_console) : $(VASTATEsim)

clean: 
	make TARGET=clean

noace:
	make TARGET=noace 
