//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#ifndef HDDLUNITE_SUBPROCESS_H
#define HDDLUNITE_SUBPROCESS_H

#include <memory>

namespace HddlUnite {
class SubProcess {
public:
    explicit SubProcess(std::vector<std::string> argv = {});
    ~SubProcess();

    bool execute(bool hide = true); // Only impact Windows. True - New console; False - Share console

    bool poll(); // True - executing; False - Stop Execute
    bool enableDaemon(bool enabled); // True - Monitor enable success; False - Failed
    void terminate();

    void addExitCode(int exitCode);
    void addExtraOption(std::string option);
    void setExecuteCommand(std::vector<std::string> argv);

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};
}

#endif //HDDLUNITE_SUBPROCESS_H
