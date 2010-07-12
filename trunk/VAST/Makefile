
VASTnet := VASTnet
VAST    := VAST 
VASTsim := VASTsim
common  := common
#VASTATE := VASTATE
#VASTATEsim := VASTATEsim
#demo_console := Demo/demo_console
test_console := Demo/test_console

# tells make that the following labels are make targets, not filenames
.PHONY: all clean $(VASTnet) $(VAST) $(VASTsim) $(common) $(test_console)

all: $(common) $(VASTnet) $(VAST) $(VASTsim) $(test_console)


$(VAST) $(VASTsim) $(common) $(VASTnet) $(test_console):
	$(MAKE) --directory=$@ $(TARGET)

$(VASTnet) : $(common)
$(VAST) : $(VASTnet) 
$(VASTsim): $(VAST)
$(test_console) : $(VASTsim)

clean: 
	make TARGET=clean

noace:
	make TARGET=noace 
