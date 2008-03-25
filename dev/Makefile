
vastutil        := VASTutil
vast            := VAST 
vastsim         := sim/VASTsim
vastsim_console := sim/VASTsim_console

# tells make that the following labels are make targets, not filenames
.PHONY: all clean $(vast) $(vastsim) $(vastutil) $(vastsim_console) noace

all: $(vast) $(vastsim) $(vastutil) $(vastsim_console) $(errout)

$(vast) $(vastsim) $(vastutil) $(vastsim_console):
	$(MAKE) --directory=$@ $(TARGET)

$(vastsim): $(vast)

$(vastsim_console): $(vastsim)
 
clean: 
	make TARGET=clean

noace:
	make TARGET=noace
