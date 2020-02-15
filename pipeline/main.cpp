#include "hddlchannel.h"
#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);
    QString pipeline;
    for (int i = 1; i < argc - 1; i++) {
        pipeline += " ";
        pipeline += argv[i];
    }
    int channelId = std::stoi(argv[argc - 1]);

    HddlChannel w(channelId);
    w.initConnection();
    w.setupPipeline(pipeline, "mysink");
    w.show();
    w.run();
    return a.exec();
}
