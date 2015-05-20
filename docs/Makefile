LOCAL_PATH:= $(call my-dir)

CONFIGS:= \
	base_config.txt \
	all_files.txt

DOXYGEN_CONFIG_FILE:= $(OUT_INTERMEDIATES)/$(LOCAL_PATH)/finalconfig.txt

$(DOXYGEN_CONFIG_FILE): $(addprefix $(BUILD_SYSTEM)/doxygen/,$(CONFIGS))
	mkdir -p $(dir $(@))
	cat $^ >$@

.PHONY: docs
docs:: $(DOXYGEN_CONFIG_FILE) interfaces
	mkdir -p $(OUT_DOCS)
	cp -rf $(LOCAL_PATH)/raw/* $(OUT_DOCS)
	( \
		export PROJECT_NAME=OpenBinder; \
		export ROOT=$(PWD)/$(TOP); \
		export OUTPUT_DIRECTORY=$(PWD)/$(OUT_DOCS); \
		cd $(BUILD_SYSTEM)/doxygen; \
		$(DOXYGEN) $(PWD)/$< \
	)

docs:: LOCAL_PATH:= $(LOCAL_PATH)

ALL_INTERMEDIATES+= $(DOXYGEN_CONFIG_FILE)

.PHONY: cleandocs
cleandocs::
	@rm -f $(DOXYGEN_CONFIG_FILE)
	@rm -rf $(OUT_DOCS)

cleandocs:: DOXYGEN_CONFIG_FILE:= $(DOXYGEN_CONFIG_FILE)

clean:: cleandocs
