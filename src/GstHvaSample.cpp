#include <GstHvaSample.hpp>
#include <iostream>

int main(){

    gst_init(0, NULL);

    GstPipeContainer cont;

    cont.init();

    cont.start();

    while(cont.read()){

    }
    std::cout<<"Finished"<<std::endl;

    // /* Wait until error or EOS */
    // GstBus* bus = gst_element_get_bus(cont.pipeline);
    // GstMessage* msg =
    //     gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE,
    //     (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

    // /* Free resources */
    // if (msg != NULL)
    //     gst_message_unref (msg);
    // gst_object_unref (bus);

    return 0;

}