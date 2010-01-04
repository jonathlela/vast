
VASTnet := VASTnet
VAST    := VAST 
VASTsim := VASTsim
common  := common
VASTATE := VASTATE
VASTATEsim := VASTATEsim

# tells make that the following labels are make targets, not filenames
.PHONY: all clean $(VASTnet) $(VAST) $(VASTsim) $(common) $(VASTATE) $(VASTATEsim)

all: $(common) $(VASTnet) $(VAST) $(VASTsim) $(VASTATE) $(VASTATEsim)

$(VAST) $(VASTsim) $(common) $(VASTnet) $(VASTATE) $(VASTATEsim):
	$(MAKE) --directory=$@ $(TARGET)

$(VASTnet) : $(common)
$(VAST) : $(VASTnet) 
$(VASTsim): $(VAST)
$(VASTATE) : $(VAST)
$(VASTATEsim) : $(VASTATE)

clean: 
	make TARGET=clean

noace:
	make TARGET=noace 
