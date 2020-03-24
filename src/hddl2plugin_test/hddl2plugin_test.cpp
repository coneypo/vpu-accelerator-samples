#include "hddl2plugin_helper.hpp"

int main()
{
    HDDL2pluginHelper_t helperHDDL2;

    helperHDDL2.setup();
    helperHDDL2.update();
    helperHDDL2.infer();
}