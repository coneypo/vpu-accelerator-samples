#include "hddlpipeline.h"
#include <QApplication>


int main(int argc, char *argv[])
{

    QApplication a(argc, argv);
    QString pipeline;
    for(int i =1 ; i<argc-1; i++){
        pipeline += " ";
        pipeline += argv[i];
    }
    int launchIndex = std::stoi(argv[argc-1]);

    HddlPipeline w(pipeline, "mysink", launchIndex);
    w.show();
    w.run();
    return a.exec();
}
