# images
include $(PRJ_DIR)/generated/images/images.mk

include $(PRJ_DIR)/generated/lotties/lotties.mk

# fonts
include $(PRJ_DIR)/generated/fonts/fonts.mk

GEN_CSRCS += $(notdir $(wildcard $(PRJ_DIR)/generated/*.c))

DEPPATH += --dep-path $(PRJ_DIR)/generated
VPATH += :$(PRJ_DIR)/generated

CFLAGS += "-I$(PRJ_DIR)/generated"
