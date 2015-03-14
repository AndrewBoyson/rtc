project    := rtc
executable := /usr/bin/$(project)
sources    := $(wildcard *.c */*.c) #Look in this directory and all subdirectories for any c source files
objects    := $(patsubst %.c,%.o,$(sources))
depends    := $(patsubst %.c,%.d,$(sources))

.DEFAULT_GOAL : $(executable)
.PHONY : clean gz help

#The default goal makes the executable if it is older than any of the objects
$(executable) : $(objects)
	@echo Linking $(executable)
	@cc -Wall -lpthread -lrt -o $(executable) $(objects)

#include generated dependency files eg: listen.o: listen.c log.h socket.h client.h connection.h
#These rules rely on a pattern rule for compiling source c to object o (see below)
#The include files are remade first
-include $(depends)

#Pattern rule which overrides the implicit rule for compiling a source to an object file
#$@ is the target and $< is the prerequisite
%.o : %.c
	@echo Compiling $@
	@cc -Wall -Wno-unused-function -std=gnu99 -D_GNU_SOURCE -c -o $@ $<

#Pattern rule to update a dependency (see include above) if it is older than its corresponding source.
%.d : %.c
	@echo Making dependency $@
	@cc -MM -MF $@ $<




#Non default goal for cleaning up.
clean :
	@rm -vf $(objects) $(depends) *.gz
	
#non default goal for making a gz backup
gz :
	@tar -czvf $(project).gz --exclude='*.o' --exclude='*.d' --exclude='*.gz' *
	
#non default goal to list help
help :
	@echo "sudo make  : compile and link"
	@echo "make clean : remove o's and d's"
	@echo "make gz    : make $(project).gz"
	@echo "make help  : print this list"

