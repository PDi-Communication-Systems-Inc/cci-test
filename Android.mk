LOCAL_PATH:= $(call my-dir)

common_src_files := \
	rs232.c 

common_c_includes := \


common_shared_libraries := \


include $(CLEAR_VARS)

LOCAL_MODULE:= cci-test

LOCAL_SRC_FILES := \
	main.c \
	$(common_src_files)

LOCAL_C_INCLUDES := $(common_c_includes)

LOCAL_CFLAGS := 

LOCAL_SHARED_LIBRARIES := $(common_shared_libraries) \
    libcutils 

LOCAL_MODULE_TAGS := eng tests

include $(BUILD_EXECUTABLE)


