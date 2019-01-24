#ifndef _SUBPROCESS_H_
#define _SUBPROCESS_H_

#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

class SubProcess {
public:
    SubProcess(std::vector<std::string> argv);
    ~SubProcess();

    /*
     * Start the executable as child process
     * Return: true - started; false - error.
     */
    bool execute();

    /*
     * Check if the executable still running
     * Return: true - executing; false - non-exist.
     */
    bool poll();

    /*
     * Enable wait thread, when process exits callback will be called
     * Return: true - successful; false - failed.
     */
    bool enableWaitNotify(std::function<void()> callback);

    /*
     * Stop executable by sending SIGTERM
     */
    void terminate();

private:
    void waitRoutine(std::function<void()> callback);

private:
    pid_t m_processId;
    std::mutex m_waitMutex;
    std::condition_variable m_waitCond;
    bool m_waitEnabled;
    std::vector<std::string> m_argv;
};

#endif // _SUBPROCESS_H_
