
vastutil        := VASTutil
vast            := VAST 
vastsim         := sim/VASTsim
vastsim_console := sim/VASTsim_console
vastate         := VASTATE
vastatesim      := sim/VASTATEsim
vastatesim_console := sim/VASTATEsim_console

# tells make that the following labels are make targets, not filenames
.PHONY: all clean noace $(vast) $(vastsim) $(vastutil) $(vastsim_console) $(vastate) $(vastatesim) $(vastatesim_console)

all: $(vast) $(vastsim) $(vastutil) $(vastsim_console) $(vastate) $(vastatesim) $(vastatesim_console)

$(vast) $(vastsim) $(vastutil) $(vastsim_console) $(vastate) $(vastatesim) $(vastatesim_console):
	$(MAKE) --directory=$@ $(TARGET)

$(vastsim): $(vast)

$(vastsim_console): $(vastsim)

$(vastate): $(vast) $(vastutil)

$(vastatesim): $(vastate)

$(vastatesim_console): $(vastatesim)
 
clean: 
	make TARGET=clean

noace:
	make TARGET=noace
