// Standard C++ includes
#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>
#include <mutex>
#include <random>
#include <set>
#include <sstream>
#include <thread>
#include <vector>

// Standard C includes
#include <cassert>
#include <csignal>
#include <cstdlib>

// OpenCV includes
#include <opencv2/highgui/highgui.hpp>

// Common example includes
#include "../common/dvs_128.h"
#include "../common/motor.h"
#include "../common/spike_csv_recorder.h"
#include "../common/timer.h"

// Optical flow includes
#include "parameters.h"

// Auto-generated simulation code
#include "optical_flow_CODE/definitions.h"

//----------------------------------------------------------------------------
// Anonymous namespace
//----------------------------------------------------------------------------
namespace
{
typedef void (*allocateFn)(unsigned int);

volatile std::sig_atomic_t g_SignalStatus;

void signalHandler(int status)
{
    g_SignalStatus = status;
}


unsigned int getNeuronIndex(unsigned int resolution, unsigned int x, unsigned int y)
{
    return x + (y * resolution);
}

void print_sparse_matrix(unsigned int pre_resolution, const SparseProjection &projection)
{
    const unsigned int pre_size = pre_resolution * pre_resolution;
    for(unsigned int i = 0; i < pre_size; i++)
    {
        std::cout << i << ":";

        for(unsigned int j = projection.indInG[i]; j < projection.indInG[i + 1]; j++)
        {
            std::cout << projection.ind[j] << ",";
        }

        std::cout << std::endl;
    }
}

void build_centre_to_macro_connection(SparseProjection &projection, allocateFn allocate)
{
    // Allocate centre_size * centre_size connections
    allocate(Parameters::centreSize * Parameters::centreSize);

    // Calculate start and end of border on each row
    const unsigned int near_border = (Parameters::inputSize - Parameters::centreSize) / 2;
    const unsigned int far_border = near_border + Parameters::centreSize;

    // Loop through rows of pixels in centre
    unsigned int s = 0;
    unsigned int i = 0;
    for(unsigned int yi = 0; yi < Parameters::inputSize; yi++)
    {
        for(unsigned int xi = 0; xi < Parameters::inputSize; xi++)
        {
            projection.indInG[i++] = s;

            // If we're in the centre
            if(xi >= near_border && xi < far_border && yi >= near_border && yi < far_border) {
                const unsigned int yj = (yi - near_border) / Parameters::kernelSize;
                const unsigned int xj = (xi - near_border) / Parameters::kernelSize;
                projection.ind[s++] = getNeuronIndex(Parameters::macroPixelSize, xj, yj);
            }
        }
    }

    // Add ending entry to data structure
    projection.indInG[i] = s;

    // Check
    assert(s == (Parameters::centreSize * Parameters::centreSize));
    assert(i == (Parameters::inputSize * Parameters::inputSize));
}

void buildDetectors(SparseProjection &excitatoryProjection, SparseProjection &inhibitoryProjection,
                    allocateFn allocateExcitatory, allocateFn allocateInhibitory)
{
    allocateExcitatory(Parameters::detectorSize * Parameters::detectorSize * Parameters::DetectorMax);
    allocateInhibitory(Parameters::detectorSize * Parameters::detectorSize * Parameters::DetectorMax);

    // Loop through macro cells
    unsigned int sExcitatory = 0;
    unsigned int iExcitatory = 0;
    unsigned int sInhibitory = 0;
    unsigned int iInhibitory = 0;
    for(unsigned int yi = 0; yi < Parameters::macroPixelSize; yi++)
    {
        for(unsigned int xi = 0; xi < Parameters::macroPixelSize; xi++)
        {
            // Mark start of 'synaptic row'
            excitatoryProjection.indInG[iExcitatory++] = sExcitatory;
            inhibitoryProjection.indInG[iInhibitory++] = sInhibitory;

            // If we're not in border region
            if(xi >= 1 && xi < (Parameters::macroPixelSize - 1)
                && yi >= 1 && yi < (Parameters::macroPixelSize - 1))
            {
                const unsigned int xj = (xi - 1) * Parameters::DetectorMax;
                const unsigned int yj = yi - 1;

                // Add excitatory synapses to all detectors
                excitatoryProjection.ind[sExcitatory++] = getNeuronIndex(Parameters::detectorSize * Parameters::DetectorMax,
                                                                         xj + Parameters::DetectorLeft, yj);
                excitatoryProjection.ind[sExcitatory++] = getNeuronIndex(Parameters::detectorSize * Parameters::DetectorMax,
                                                                         xj + Parameters::DetectorRight, yj);
                excitatoryProjection.ind[sExcitatory++] = getNeuronIndex(Parameters::detectorSize * Parameters::DetectorMax,
                                                                         xj + Parameters::DetectorUp, yj);
                excitatoryProjection.ind[sExcitatory++] = getNeuronIndex(Parameters::detectorSize * Parameters::DetectorMax,
                                                                         xj + Parameters::DetectorDown, yj);
            }


            // Create inhibitory connection to 'left' detector associated with macropixel one to right
            if(xi < (Parameters::macroPixelSize - 2)
                && yi >= 1 && yi < (Parameters::macroPixelSize - 1))
            {
                const unsigned int xj = (xi - 1 + 1) * Parameters::DetectorMax;
                const unsigned int yj = yi - 1;
                inhibitoryProjection.ind[sInhibitory++] = getNeuronIndex(Parameters::detectorSize * Parameters::DetectorMax,
                                                                         xj + Parameters::DetectorLeft, yj);
            }

            // Create inhibitory connection to 'right' detector associated with macropixel one to right
            if(xi >= 2
                && yi >= 1 && yi < (Parameters::macroPixelSize - 1))
            {
                const unsigned int xj = (xi - 1 - 1) * Parameters::DetectorMax;
                const unsigned int yj = yi - 1;
                inhibitoryProjection.ind[sInhibitory++] = getNeuronIndex(Parameters::detectorSize * Parameters::DetectorMax,
                                                                         xj + Parameters::DetectorRight, yj);
            }

            // Create inhibitory connection to 'up' detector associated with macropixel one below
            if(xi >= 1 && xi < (Parameters::macroPixelSize - 1)
                && yi < (Parameters::macroPixelSize - 2))
            {
                const unsigned int xj = (xi - 1) * Parameters::DetectorMax;
                const unsigned int yj = yi - 1 + 1;
                inhibitoryProjection.ind[sInhibitory++] = getNeuronIndex(Parameters::detectorSize * Parameters::DetectorMax,
                                                                         xj + Parameters::DetectorUp, yj);
            }

            // Create inhibitory connection to 'down' detector associated with macropixel one above
            if(xi >= 1 && xi < (Parameters::macroPixelSize - 1)
                && yi >= 2)
            {
                const unsigned int xj = (xi - 1) * Parameters::DetectorMax;
                const unsigned int yj = yi - 1 - 1;
                inhibitoryProjection.ind[sInhibitory++] = getNeuronIndex(Parameters::detectorSize * Parameters::DetectorMax,
                                                                         xj + Parameters::DetectorDown, yj);
            }

        }
    }

    // Add ending entry to data structure
    excitatoryProjection.indInG[iExcitatory] = sExcitatory;
    inhibitoryProjection.indInG[iInhibitory] = sInhibitory;

    // Check
    assert(sExcitatory == (Parameters::detectorSize * Parameters::detectorSize * Parameters::DetectorMax));
    assert(iExcitatory == (Parameters::macroPixelSize * Parameters::macroPixelSize));
    assert(sInhibitory == (Parameters::detectorSize * Parameters::detectorSize * Parameters::DetectorMax));
    assert(iInhibitory == (Parameters::macroPixelSize * Parameters::macroPixelSize));
}

template<typename Generator, typename ShuffleEngine>
void readCalibrateInput(Generator &gen, ShuffleEngine &engine)
{
    // Create array containing coordinates of all pixels within a macrocell
    std::vector<std::pair<unsigned int, unsigned int>> macroCellIndices;
    macroCellIndices.reserve(Parameters::kernelSize * Parameters::kernelSize);
    for(unsigned int x = 0; x < Parameters::kernelSize; x++)
    {
        for(unsigned int y = 0; y < Parameters::kernelSize; y++)
        {
            macroCellIndices.push_back(std::make_pair(x, y));
        }
    }

    spikeCount_DVS = 0;

    const unsigned int nearBorder = (Parameters::inputSize - Parameters::centreSize) / 2;

    // Loop through macropixels
    for(unsigned int yi = 0; yi < Parameters::macroPixelSize; yi++)
    {
        // Create binomial distribution of probability of neuron in kernel firing based on y coordinate
        std::binomial_distribution<> d(Parameters::kernelSize * Parameters::kernelSize,
                                       0.1 * ((double)yi / (double)Parameters::macroPixelSize));
        for(unsigned int xi = 0; xi < Parameters::macroPixelSize; xi++)
        {
            // Shuffle the order of the macrocell indices
            std::shuffle(macroCellIndices.begin(), macroCellIndices.end(), engine);

            // Draw number of active pixels from binomial
            const unsigned int numActiveNeurons = d(gen);
            for(unsigned int i = 0; i < numActiveNeurons; i++)
            {
                unsigned int a = getNeuronIndex(Parameters::inputSize,
                                                (xi * Parameters::kernelSize) + nearBorder + macroCellIndices[i].first,
                                                (yi * Parameters::kernelSize) + nearBorder + macroCellIndices[i].second);
                assert(a < (Parameters::inputSize * Parameters::inputSize));
                spike_DVS[spikeCount_DVS++] = a;

            }
        }
    }
    // 9 spikes per ms
    // 8 spikes per ms
    // 7 spikes per ms
    // 6 spikes per ms
    // 5 spikes
}

unsigned int read_p_input(std::ifstream &stream, std::vector<unsigned int> &indices)
{
    // Read lines into string
    std::string line;
    std::getline(stream, line);

    if(line.empty()) {
        return std::numeric_limits<unsigned int>::max();
    }

    // Create string stream from line
    std::stringstream lineStream(line);

    // Read time from start of line
    std::string nextTimeString;
    std::getline(lineStream, nextTimeString, ';');
    unsigned int nextTime = (unsigned int)std::stoul(nextTimeString);

    // Clear existing times
    indices.clear();

    while(lineStream.good()) {
        // Read input spike index
        std::string inputIndexString;
        std::getline(lineStream, inputIndexString, ',');
        indices.push_back(std::atoi(inputIndexString.c_str()));
    }

    return nextTime;
}

void displayThreadHandler(std::mutex &outputMutex, const float (&output)[Parameters::detectorSize][Parameters::detectorSize][2])
{
    // Create output image
#ifndef NO_X
    const unsigned int outputImageSize = Parameters::detectorSize * Parameters::outputScale;
    cv::Mat outputImage(outputImageSize, outputImageSize, CV_8UC3);
#endif 

    Motor motor("192.168.1.1", 2000);

    int turningTime = 20;
    motor.tank(1.0, 1.0);
    while(g_SignalStatus == 0)
    {
#ifndef NO_X
        // Clear background
        outputImage.setTo(cv::Scalar::all(0));
#endif
        float leftFlow = 0;
        float rightFlow = 0;

        {
            std::lock_guard<std::mutex> lock(outputMutex);

            // Loop through output coordinates
            for(unsigned int x = 0; x < Parameters::detectorSize; x++)
            {
                for(unsigned int y = 0; y < Parameters::detectorSize; y++)
                {
                    const cv::Point start(x * Parameters::outputScale, y * Parameters::outputScale);
                    const cv::Point end = start + cv::Point(Parameters::outputVectorScale * output[x][y][0],
                                                            Parameters::outputVectorScale * output[x][y][1]);

#ifndef NO_X
                    cv::line(outputImage, start, end,
                             CV_RGB(0xFF, 0xFF, 0xFF));
#endif

                    if(x > (Parameters::detectorSize / 2)) {
                        leftFlow += output[x][y][0];
                    }
                    else {
                        rightFlow += output[x][y][0];
                    }
                }
            }

        }

        char flow[255];
        const int stabiliseTime = 12;
        const int turnTime = 7;
        const float flowThreshold = 30.0; // 100.0 weirdly worked in lab
        if(turningTime > 0)
        {
            if(turningTime < stabiliseTime) {
                motor.tank(1.0, 1.0);
                sprintf(flow, "STABILISING (Right:%f)", rightFlow);
#ifndef NO_X
                cv::putText(outputImage, flow, cv::Point(0, outputImageSize - 5),
                            cv::FONT_HERSHEY_COMPLEX_SMALL, 1.0, CV_RGB(0, 0, 0xFF));
#else
                std::cout << flow << std::endl;
#endif
            }
            else {
                sprintf(flow, "MOVING (Right:%f)", rightFlow);
#ifndef NO_X
                cv::putText(outputImage, flow, cv::Point(0, outputImageSize - 5),
                            cv::FONT_HERSHEY_COMPLEX_SMALL, 1.0, CV_RGB(0, 0xFF, 0xFF));
#else
                std::cout << flow << std::endl;
#endif
            }
            turningTime--;
        }
        else
        {
             // Heading away from wall
            // **YUCK** or lost wall
            if(rightFlow > 40.0f || rightFlow < 9.0f) {
                motor.tank(-0.5, 0.5);
                turningTime = stabiliseTime + turnTime;

                std::cout << "Turn right" << std::endl;

            }
            // Approaching wall
            else if(rightFlow < 20.0f) {
                motor.tank(0.5, -0.5);
                turningTime = stabiliseTime + turnTime;

                std::cout << "Turn left" << std::endl;
            }

            else {
                motor.tank(1.0, 1.0);

                sprintf(flow, "CENTRE:%f", rightFlow);
                std::cout << flow << std::endl;
            }
        }

#ifdef NO_X
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
#else
        cv::imshow("Frame", outputImage);
        cv::waitKey(33);
#endif
    }

    motor.tank(0.0, 0.0);
}

void runLive()
{
     // Create DVS 128 device
    DVS128 dvs(DVS128::Polarity::On);

    const cv::Point detectorPoint[Parameters::DetectorMax] = {
        cv::Point(),//DetectorLeft
        cv::Point(),//DetectorRight
        cv::Point(),//DetectorUp
        cv::Point(),//DetectorDown
    };
    double dvsGet = 0.0;
    double step = 0.0;
    double render = 0.0;

    std::mutex outputMutex;
    float output[Parameters::detectorSize][Parameters::detectorSize][2] = {0};

    //std::thread displayThread(displayThreadHandler, std::ref(outputMutex), std::ref(output));

    // Catch interrupt (ctrl-c) signals
    std::signal(SIGINT, signalHandler);

    // Start revieving DVS events
    dvs.start();

    // Convert timestep to a duration
    const auto dtDuration = std::chrono::duration<double, std::milli>{DT};

    // Duration counters
    std::chrono::duration<double, std::milli> sleepTime{0};
    std::chrono::duration<double, std::milli> overrunTime{0};
    unsigned int i = 0;
    for(i = 0; g_SignalStatus == 0; i++)
    {
         auto tickStart = std::chrono::high_resolution_clock::now();

        {
            TimerAccumulate<std::milli> timer(dvsGet);
            dvs.readEvents(spikeCount_DVS, spike_DVS);
        }

        {
            TimerAccumulate<std::milli> timer(step);

            // Simulate
#ifndef CPU_ONLY
            stepTimeGPU();
            pullOutputCurrentSpikesFromDevice();
#else
            stepTimeCPU();
#endif
        }

        {
            TimerAccumulate<std::milli> timer(render);

            {
                std::lock_guard<std::mutex> lock(outputMutex);

                for(unsigned int s = 0; s < spikeCount_Output; s++)
                {
                    const unsigned int spike = spike_Output[s];

                    const auto spikeCoord = std::div((int)spike, (int)Parameters::detectorSize * Parameters::DetectorMax);
                    const int spikeY = spikeCoord.quot;

                    const auto xCoord = std::div(spikeCoord.rem, (int)Parameters::DetectorMax);
                    const int spikeX =  xCoord.quot;

                    switch(xCoord.rem)
                    {
                        case Parameters::DetectorLeft:
                            output[spikeX][spikeY][0] -= 1.0f;
                            break;

                        case Parameters::DetectorRight:
                            output[spikeX][spikeY][0] += 1.0f;
                            break;

                        case Parameters::DetectorUp:
                            output[spikeX][spikeY][1] -= 1.0f;
                            break;

                        case Parameters::DetectorDown:
                            output[spikeX][spikeY][1] += 1.0f;
                            break;

                    }
                }

                // Decay output
                const float persistence = 0.995f;
                for(unsigned int x = 0; x < Parameters::detectorSize; x++)
                {
                    for(unsigned int y = 0; y < Parameters::detectorSize; y++)
                    {
                        output[x][y][0] *= persistence;
                        output[x][y][1] *= persistence;
                    }
                }
            }
        }

        // Get time of tick start
        auto tickEnd = std::chrono::high_resolution_clock::now();

        // If there we're ahead of real-time pause
        auto tickDuration = tickEnd - tickStart;
        if(tickDuration < dtDuration) {
            auto tickSleep = dtDuration - tickDuration;
            sleepTime += tickSleep;
            std::this_thread::sleep_for(tickSleep);
        }
        else {
            overrunTime += (tickDuration - dtDuration);
        }
    }

    //displayThread.join();
    dvs.stop();
    std::cout << "Ran for " << i << " " << DT << "ms timesteps, overan for " << overrunTime.count() << "ms, slept for " << sleepTime.count() << "ms" << std::endl;
    std::cout << "DVS:" << dvsGet << "ms, Step:" << step << "ms, Render:" << render << std::endl;
}

void runFromFile(const char *filename)
{
    std::ifstream spikeInput(filename);
    assert(spikeInput.good());

    // Read first line of input
    std::vector<unsigned int> inputIndices;
    unsigned int nextInputTime = read_p_input(spikeInput, inputIndices);

    SpikeCSVRecorder dvsPixelSpikeRecorder("dvs_pixel_spikes.csv", glbSpkCntDVS, glbSpkDVS);
    SpikeCSVRecorder macroPixelSpikeRecorder("macro_pixel_spikes.csv", glbSpkCntMacroPixel, glbSpkMacroPixel);
    SpikeCSVRecorder outputSpikeRecorder("output_spikes.csv", glbSpkCntOutput, glbSpkOutput);

    double record = 0.0;
    double dvsGet = 0.0;
    double step = 0.0;

    unsigned int i = 0;
    for(i = 0; nextInputTime < std::numeric_limits<unsigned int>::max(); i++)
    {
         // If we should supply input this timestep
        if(nextInputTime == i) {
            // Copy into spike source
            spikeCount_DVS = inputIndices.size();
            std::copy(inputIndices.cbegin(), inputIndices.cend(), &spike_DVS[0]);

#ifndef CPU_ONLY
            // Copy to GPU
            pushDVSCurrentSpikesToDevice();
#endif

            // Read NEXT input
            nextInputTime = read_p_input(spikeInput, inputIndices);
        }

        dvsPixelSpikeRecorder.record(t);

        // Simulate
#ifndef CPU_ONLY
        stepTimeGPU();

        pullMacroPixelCurrentSpikesFromDevice();
        pullOutputCurrentSpikesFromDevice();
#else
        stepTimeCPU();
#endif
        macroPixelSpikeRecorder.record(t);
        outputSpikeRecorder.record(t);
    }

    std::cout << "Ran for " << i << " " << DT << "ms timesteps" << std::endl;
}
}

int main(int argc, char *argv[])
{
    allocateMem();
    initialize();

    build_centre_to_macro_connection(CDVS_MacroPixel, &allocateDVS_MacroPixel);
    buildDetectors(CMacroPixel_Output_Excitatory, CMacroPixel_Output_Inhibitory,
                   &allocateMacroPixel_Output_Excitatory, &allocateMacroPixel_Output_Inhibitory);
    //print_sparse_matrix(Parameters::inputSize, CDVS_MacroPixel);
    initoptical_flow();

#ifdef LIVE
    runLive();
#else
    assert(argc > 1);
    runFromFile(argv[0]);
#endif
    return 0;
}
