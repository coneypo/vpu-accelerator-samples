#include "hddl2plugin_helper.hpp"

int main()
{
    // HDDL2pluginHelper_t helperHDDL2{"/home/kmb/cong/graph/resnet-50-dpu/resnet-50-dpu.blob", 1080, 1080};
    HDDL2pluginHelper_t helperHDDL2{
        "/home/kmb/cong/graph/opt/yolotiny/yolotiny.blob",
        0,
        &HDDL2pluginHelper_t::postprocYolotinyv2_u8,
        1080,
        1080};

    helperHDDL2.setup();
    helperHDDL2.update();
    helperHDDL2.infer();

    helperHDDL2.postproc();

}