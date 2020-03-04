
find_library (HDDLUNITE_LIB NAMES HddlUnite HINTS "$ENV{INSTALL_DIR}/lib")
# find_library (HDDLUNITE_LIB NAMES HddlUnite HINTS "/home/kmb1/xiaoxi/work/workshop/hddlunite/lib")

# find_path(HDDLUNITE_INCLUDE_DIR NAMES WorkloadContext.h HINTS "/home/kmb1/xiaoxi/work/workshop/hddlunite/include")
find_path(HDDLUNITE_INCLUDE_DIR NAMES WorkloadContext.h)
# include_directories (${HDDLUNITE_INCLUDE_DIR})
message ("HddlUnite header directory: ${HDDLUNITE_INCLUDE_DIR}")
message ("HddlUnite lib: ${HDDLUNITE_LIB}")