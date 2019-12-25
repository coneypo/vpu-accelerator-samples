#include "hddlpipeline.h"
#include <QApplication>


int main(int argc, char *argv[])
{

    QApplication a(argc, argv);
    QString pipeline;
    for(int i =1 ; i<argc; i++){
        pipeline += " ";
        pipeline += argv[i];
    }
    pipeline += argv[argc];

    HddlPipeline w(pipeline, "mysink");
    w.show();
    w.run();
    return a.exec();
}
