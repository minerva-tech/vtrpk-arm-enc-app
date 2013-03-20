CROOT = /home/a/x-tools/arm-unknown-linux-gnueabi/bin/
CPPC = $(CROOT)/arm-unknown-linux-gnueabi-g++
CC   = $(CROOT)/arm-unknown-linux-gnueabi-gcc
LINK = $(CROOT)/arm-unknown-linux-gnueabi-g++
#CPPC = /home/kov/dm365/buildroot/output/host/usr/bin/arm-unknown-linux-gnueabi-g++
#CC   = /home/kov/dm365/buildroot/output/host/usr/bin/arm-unknown-linux-gnueabi-gcc
#LINK = /home/kov/dm365/buildroot/output/host/usr/bin/arm-unknown-linux-gnueabi-g++
#CC = arm-linux-gnueabi-g++-4.6
#LINK = arm-linux-gnueabi-g++-4.6
#CC_X86 = g++
#LINK_X86 = g++

# Which Boost modules to use (all)
BOOST_MODULES = \
    date_time \
    thread \
    system \
    chrono

#  filesystem    \
#  graph         \
#  iostreams     \
#  math_c99      \
#  system        \
#  serialization \
#  regex

OBJ = main.o \
      enc.o \
      comm.o \
      read_params.o \
      error_msg.o \
      cap.o \
      proto.o

BOOST_MODULES_TYPE = 

BOOST_MODULES_LIBS = $(addsuffix $(BOOST_MODULES_TYPE),$(BOOST_MODULES))

BOOST_LDFLAGS = $(addprefix -lboost_,$(BOOST_MODULES_LIBS))

#ROOT = /home/a/ti-dvsdk_dm365-evm_4_02_00_06
ROOT = /home/a/Documents/dm365/ti_tools
H264ROOT = /home/a/Documents/dm365
#DVSDK_INCLUDE = /home/kov/dm365/ti_tools/psp/linux-2.6.32.17-psp03.01.01.39/include/
DVSDK_INCLUDE = /home/a/Documents/dm365/buildroot/output/build/linux-custom/include/
H264_ENC_INCLUDE = $(H264ROOT)/dm365_h264enc_02_30_00_04_production/packages/ti/sdo/codecs/h264enc/apps/client/test/inc/
#FRAMEWORK_INCLUDE = $(ROOT)/framework-components_2_26_00_01/packages/
#XDAIS_INCLUDE = $(ROOT)/xdais_6_26_01_03/packages
FRAMEWORK_INCLUDE = $(ROOT)/framework_components_2_25_03_07/packages/
XDAIS_INCLUDE = $(ROOT)/xdais_6_25_02_11/packages
H264_IFACE_INCLUDE = $(H264ROOT)/dm365_h264enc_02_30_00_04_production/packages/ti/sdo/codecs/h264enc/apps/inc/
#XDC_INCLUDE = $(ROOT)/xdctools_3_16_03_36/packages
#XDC_INSTALL_DIR = $(ROOT)/xdctools_3_16_03_36/
XDC_INCLUDE = $(ROOT)/xdctools_3_15_01_59/packages
XDC_INSTALL_DIR = $(ROOT)/xdctools_3_15_01_59/
MVTOOL_DIR = ""
#LINUXUTILS_INCLUDE = $(ROOT)/linuxutils_2_26_01_02/packages/
#CE_INCLUDE = $(ROOT)/codec-engine_2_26_02_11/packages/
LINUXUTILS_INCLUDE = $(ROOT)/linuxutils_2_25_05_11/packages/
CE_INCLUDE = $(ROOT)/codec_engine_2_25_05_16/packages/
#LINUXKERNEL_INCLUDE = /home/kov/dm365/buildroot/output/build/linux-custom/include/

XDCPLATFORM = ti.platforms.evmDM365
XDCTARGET = gnu.targets.arm.GCArmv5T

INCLUDES += -I$(DVSDK_INCLUDE)
INCLUDES += -I$(XDAIS_INCLUDE)
INCLUDES += -I$(LINUXUTILS_INCLUDE)
INCLUDES += -I$(FRAMEWORK_INCLUDE)
INCLUDES += -I$(CE_INCLUDE)
INCLUDES += -I$(CE_INLCUDE)/../cetools/packages
INCLUDES += -I$(XDC_INCLUDE)
INCLUDES += -I$(H264_ENC_INCLUDE)
INCLUDES += -I$(H264_IFACE_INCLUDE)

#INCLUDES += -I$(DVSDK_INCLUDE)
#INCLUDES += -I$(LINUXKERNEL_INCLUDE)

XDC_PATH = $(XDAIS_INCLUDE);$(LINUXUTILS_INCLUDE);$(FRAMEWORK_INCLUDE);$(FRAMEWORK_INCLUDE)/../examples;$(CE_INCLUDE);$(CE_INCLUDE)/../cetools/packages;

CONFIGURO = XDCPATH="$(XDC_PATH)" $(XDC_INSTALL_DIR)/xs xdc.tools.configuro
#$(CONFIGURO) -c $(MVTOOL_DIR) -o $(CONFIGPKG) -t $(XDCTARGET) -p $(XDCPLATFORM) $(CONFIGPKG).cfg

XDC_COMPILER_FILE = $(CONFIGPKG)/compiler.opt
XDC_LINKER_FILE = $(CONFIGPKG)/linker.cmd
CONFIGPKG = h264enc_ti_dm365_linux

ALG_LIB1 = $(H264ROOT)/dm365_h264enc_02_30_00_04_production/packages/ti/sdo/codecs/h264enc/lib/h264venc_ti_arm926.a
ALG_LIB2 = $(H264ROOT)/dm365_h264enc_02_30_00_04_production/packages/ti/sdo/codecs/h264enc/lib/h264v_ti_dma_dm365.a

BOOSTROOT = /home/a/Documents/boost_1_53_0

#CPPFLAGS += -I /home/kov/dm365/boost/boost_1_48_0/ $(BOOST_CPPFLAGS) -O2 -march=armv5t -D"_ARM926" -U"ENABLE_ARM926_TCM"
#CPPFLAGS += -I/home/a/Documents/boost_1_48_0/ $(INCLUDES) $(BOOST_CPPFLAGS) -O2 -march=armv5t -D"_ARM926" -U"ENABLE_ARM926_TCM" 
DEFINES  = -U"LOW_LATENCY_FEATURE" -D"_ARM926" -D"ON_LINUX" -D"H264ENC_EDMA" -D"_DEBUG" -D"DM365_IPC_INTC_ENABLE" -U"ENABLE_CACHE" -D"SEI_USERDATA_INSERTION" -U"BASE_PARAMS" -D"DEVICE_ID_CHECK" -U"PASS_SAD_MV_APPL" -U"ENABLE_ARM926_TCM" -D"ENABLE_ROI" -D"SIMPLE_TWO_PASS" -U"PROFILE_ONLINUX" -D"MEGA_PIX"
CPPFLAGS += -I$(BOOSTROOT) $(INCLUDES) $(BOOST_CPPFLAGS) -O2 -std=c++0x $(shell cat $(XDC_COMPILER_FILE)) -Wno-write-strings $(DEFINES)
#CPPFLAGS_X86 += -I /home/kov/dm365/boost/boost_1_48_0/ $(BOOST_CPPFLAGS) -O2
CPPFLAGS_X86 += $(BOOST_CPPFLAGS) -O2
#CPPFLAGS += -I /home/a/Documents/boost_1_48_0 $(BOOST_CPPFLAGS) -O2 -march=armv5t -D"_ARM926" -U"ENABLE_ARM926_TCM"
#CPPFLAGS += $(BOOST_CPPFLAGS) -O2 -std=c++0x -mcpu=arm9tdmi
#LDFLAGS += $(BOOST_LDFLAGS) -lpthread -static

#LDFLAGS += -L /home/kov/dm365/boost/boost_1_48_0/stage/lib_armv5t $(BOOST_LDFLAGS) $(ALG_LIB1) $(ALG_LIB2) $(XDC_LINKER_FILE) -lpthread -static
LDFLAGS += -L $(BOOSTROOT)/stage/lib_armv5t $(BOOST_LDFLAGS) $(ALG_LIB1) $(ALG_LIB2) $(XDC_LINKER_FILE) -lpthread -static

#LDFLAGS_X86 += -L /home/kov/dm365/boost/boost_1_48_0/stage/lib $(BOOST_LDFLAGS) -lpthread -static
LDFLAGS_X86 += $(BOOST_LDFLAGS) -lpthread -static
#LDFLAGS += -L /home/a/Documents/boost_1_48_0/stage/lib $(BOOST_LDFLAGS) -lpthread -static

console: $(CONFIGPKG) $(OBJ)
	$(LINK) -o console $(OBJ) h264venc.o alg_create.o alg_control.o alg_malloc.o cmem.o $(LDFLAGS)

$(CONFIGPKG):
	$(CONFIGURO) -c $(MVTOOL_DIR) -o $(CONFIGPKG) -t $(XDCTARGET) -p $(XDCPLATFORM) -b ./config.bld ./$(CONFIGPKG).cfg

%.o : %.cpp
	$(CC) $(CPPFLAGS) -c h264venc.c
	$(CC) $(CPPFLAGS) -c alg_create.c
	$(CC) $(CPPFLAGS) -c alg_control.c
	$(CC) $(CPPFLAGS) -c alg_malloc.c
	$(CC) $(CPPFLAGS) -c cmem.c
#	$(CC) $(CPPFLAGS) -c testapp_arm926intc.c
#	$(CC) $(CPPFLAGS) -c hdvicp_framework.c
	$(CPPC) $(CPPFLAGS) -c $<

clean:
	rm *.o console
