#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)


CFLAGS += $(subst ",,$(CONFIG_OPTIMIZATION)) -D$(subst ",,$(CONFIG_RUN_TYPE))=1
CFLAGS += -I$(IDF_PATH)/examples/common_components/protocol_examples_common/include

COMPONENT_SRCS := web_server.c