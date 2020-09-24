####
#### Sample Makefile for building applications with the RIOT OS
####
#### The example file system layout is:
#### ./application Makefile
#### ../../RIOT
####

# Set the name of your application:
APPLICATION = sim7020

# If no BOARD is found in the environment, use this default:
BOARD ?= avr-rss2

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../RIOT-OS/

# Uncomment this to enable scheduler statistics for ps:
#CFLAGS += -DSCHEDSTATISTICS

# I2C bus configuration
CFLAGS += -DI2C_NUMOF=\(1U\) -DI2C_BUS_SPEED=I2C_SPEED_NORMAL  

# Print incoming AT bytes
CFLAGS += -DAT_PRINT_INCOMING=1

# Max no of incoming bytes
CFLAGS += -DAT_RADIO_MAX_RECV_LEN=768

# Size of buffer for incoming AT data (hex coded, so twice the
# size of incoming buffer plus EOL marks and NULL char). Also
# needs to be a power of 2 for thread-safe ring buffer.
# So, 2*AT_RADIO_MAX_RECV_LEN+3 rounded up to nearest power of 2.
CFLAGS += -DAT_BUF_SIZE=2048

# If you want to use native with valgrind, you should recompile native
# with the target all-valgrind instead of all:
# make -B clean all-valgrind

# Uncomment this to enable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP = 1

# Change this to 0 to show compiler invocation lines by default:
QUIET ?= 1

# Modules to include:

USEMODULE += sim7020_sock_udp

USEMODULE += ipv6_addr
USEMODULE += ipv4_addr
USEMODULE += shell
#USEMODULE += posix_headers
USEMODULE += at
USEMODULE += at_urc
USEMODULE += xtimer
USEMODULE += core_mbox

# For testing:
USEMODULE += xtimer
USEMODULE += emcute

# If your application is very simple and doesn't use modules that use
# messaging, it can be disabled to save some memory:

#DISABLE_MODULE += core_msg

INCLUDES += -I$(CURDIR)/include -DSOCK_HAS_IPV6

# Specify custom dependencies for your application here ...
# APPDEPS = app_data.h config.h
APPDEPS = application.h

include $(RIOTBASE)/Makefile.include

# ... and define them here (after including Makefile.include,
# otherwise you modify the standard target):
#proj_data.h: script.py data.tar.gz
#	./script.py

application.h: Makefile
	echo '#ifndef APPLICATION' >$@
	echo '#define APPLICATION "'$(APPLICATION)'"' >>$@
	echo '#endif' >>$@
