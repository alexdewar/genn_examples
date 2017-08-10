#pragma once

#define PERFECT_MEMORY
#define PM_LOG
#define PM_LOG_DIR "pm_dump/"

#include <sys/stat.h>
#include <iostream>
#include <fstream>
#include "opencv2/core/cuda.hpp"

using namespace cv;
using namespace std;

struct PerfectMemoryResult {
    double heading;
    uint snapshot;
    double minval;
};

class PerfectMemory {
public:

    PerfectMemory(unsigned int outputWidth, unsigned int outputHeight) :
    m_diff(outputWidth, outputHeight, CV_32FC1),
    m_tmp1(outputWidth, outputHeight, CV_32FC1),
    m_tmp2(outputWidth, outputHeight, CV_16UC1) {
#ifdef PM_LOG
        struct stat sb;
        if (stat(PM_LOG_DIR, &sb) == 0 && S_ISDIR(sb.st_mode))
            return;

        const int dir_err = mkdir(PM_LOG_DIR, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        if (dir_err < 0) {
            cout << "Error creating directory: " << dir_err << endl;
            exit(1);
        }
#endif
    }

    void train(Mat &snap) {
        cout << "\tAdding snapshot " << snapshots.size() << endl;

        Mat snap2 = snap.clone();
        snapshots.push_back(snap2);

#ifdef PM_LOG
        imwrite(PM_LOG_DIR "snapshot" + to_string(snapshots.size()) + ".png", snap2);
#endif
    }

    void test(Mat &snap, PerfectMemoryResult &res) {
#ifdef PM_LOG
        string pref = to_string(testCount) + "_";
        imwrite(PM_LOG_DIR + pref + "current.png", snap);
        
        ofstream ridffile;
        ridffile.open(PM_LOG_DIR + pref + "ridf.csv", ios::out | ios::trunc);
#endif
        
        Mat snap2(snap.size(), snap.type());
        int minrot;
        uint minsnap;
        double minval = numeric_limits<double>::infinity();
        for (int i = 0; i < snap.cols; i++) {
            shiftColumns(snap, i, snap2);
            for (uint j = 0; j < snapshots.size(); j++) {
                absdiff(snapshots.at(j), snap2, m_diff);
                double sumabsdiff = sum(m_diff)[0];
                if (sumabsdiff < minval) {
                    minval = sumabsdiff;
                    minrot = i;
                    minsnap = j;
                }
                
#ifdef PM_LOG
                if (j > 0) {
                    ridffile << ", ";
                }
                ridffile << sumabsdiff;
#endif
            }
#ifdef PM_LOG
            ridffile << "\n";
#endif
        }
        
#ifdef PM_LOG
        ridffile.close();

        ofstream logfile;
        logfile.open(PM_LOG_DIR + pref + "log.txt", ios::out | ios::trunc);
        
        double rot = 360.0 * (double)minrot / (double)snap.cols;
        logfile << "test" << testCount << ": " << endl
                << "- rotation: " << rot << endl
                << "- snap: " << minsnap << " (n=" << snapshots.size() << ")" << endl
                << "- value: " << minval << endl << endl;
        logfile.close();
#endif
        double ratio = (double)minrot / (double)snap.cols;
        res.heading = 2 * M_PI * ratio;
        res.snapshot = minsnap;
        res.minval = minval;
        
        cout << "\tHeading: " << 360.0 * ratio << " deg" << endl
             << "\tBest-matching snapshot: " << minsnap << " (of " << snapshots.size() << ")" << endl
             << "\tMinimum value: " << minval << endl;
    }

private:
    vector<Mat> snapshots;
    Mat m_diff;
    Mat m_tmp1;
    Mat m_tmp2;
#ifdef PM_LOG
    int testCount = -1;
#endif
    
    void shiftColumns(Mat in, int numRight, Mat &out) {
        if(numRight == 0){ 
            in.copyTo(out);
            return;
        }

        int ncols = in.cols;
        int nrows = in.rows;

        out = Mat::zeros(in.size(), in.type());

        numRight = numRight%ncols;
        if(numRight < 0)
            numRight = ncols+numRight;

        in(Rect(ncols-numRight,0, numRight,nrows)).copyTo(out(Rect(0,0,numRight,nrows)));
        in(Rect(0,0, ncols-numRight,nrows)).copyTo(out(Rect(numRight,0,ncols-numRight,nrows)));
    }
};