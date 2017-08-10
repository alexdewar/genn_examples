#pragma once

#include <sys/stat.h>
#include <iostream>
#include <fstream>
#include "opencv2/opencv.hpp"

#define PM_LOG
#define PM_LOG_DIR "pm_dump/"

using namespace cv;

struct PerfectMemoryResult
{
    double heading;
    uint snapshot;
    double minval;
};

class PerfectMemory
{
public:
    PerfectMemory(unsigned int outputWidth, unsigned int outputHeight);
    void train(Mat &snap);
    void test(Mat &snap, PerfectMemoryResult &res);

private:
    std::vector<Mat> snapshots;
    Mat m_diff;
    Mat m_tmp1;
    Mat m_tmp2;
#ifdef PM_LOG
    int testCount = -1;
#endif
};

void shiftColumns(Mat in, int numRight, Mat &out);