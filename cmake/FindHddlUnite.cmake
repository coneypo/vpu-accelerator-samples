
find_library (HDDLUNITE_LIB NAMES HddlUnite HINTS "$ENV{INSTALL_DIR}/lib")

find_path(HDDLUNITE_INCLUDE_DIR NAMES WorkloadContext.h)
# include_directories (${HDDLUNITE_INCLUDE_DIR})
message ("HddlUnite header directory: ${HDDLUNITE_INCLUDE_DIR}")
message ("HddlUnite lib: ${HDDLUNITE_LIB}")