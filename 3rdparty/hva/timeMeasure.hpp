#ifndef TEST_TIME_MEASURE
#define TEST_TIME_MEASURE

#include <chrono>
#include <iostream>
#include <map>

class FpsMeasure{
using ms = std::chrono::milliseconds;
public:
    static FpsMeasure& getInstance(){
        static FpsMeasure inst;
        return inst;
    };

    FpsMeasure(const FpsMeasure&) = delete;

    FpsMeasure& operator=(const FpsMeasure& ) = delete;

    void setStart(){
        start = std::chrono::system_clock::now();
    };

    void setFinish(std::size_t idx){
        finish[idx] = std::chrono::system_clock::now();
    };

    void report(){
        float fps = 0.0, sum = 0.0;
        std::cout<<"-------------------FPS Report---------------------"<<std::endl;
        for(const auto& pair: finish){
            std::cout<<pair.first<<": "<<std::chrono::duration_cast<ms>(pair.second - start).count()<<" ms";
            fps = 2585.0/(std::chrono::duration_cast<ms>(pair.second - start).count()/1000.0);
            std::cout<<" with fps: "<<fps<<"\n"<<std::endl;
            sum += fps;
        }
        std::cout<<"Total fps: "<<sum<<std::endl;
    };

private:
    FpsMeasure() { };

    std::chrono::time_point<std::chrono::high_resolution_clock> start;
    std::map<std::size_t, std::chrono::time_point<std::chrono::high_resolution_clock>> finish;
};

#endif //#ifndef TEST_TIME_MEASURE