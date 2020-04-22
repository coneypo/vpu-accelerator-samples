#!/bin/sh

#
# Check values from the OpenVINO installation.
#
if [ -z $INTEL_CVSDK_DIR ]; then
    echo "User should include the setupvars.sh in OpenVINO."
    echo "  - INTEL_CVSDK_DIR is not defined."
    return 1
fi


#
# Include the vas lib path into LD_LIBRARY_PATH
#
export PATH_VAS="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && cd .. && pwd )"
export LD_LIBRARY_PATH="$PATH_VAS/lib/intel64:$LD_LIBRARY_PATH"

echo [setupvars.bat] VAS environment initialized
