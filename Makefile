#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := crumbmark

include $(IDF_PATH)/make/project.mk
REQUIRED_COMPONENTS = esp_http_server esp_event esp_wifi nvs_flash web_server

# EXTRA_COMPONENT_DIRS = $(IDF_PATH)/examples/common_components/protocol_examples_common