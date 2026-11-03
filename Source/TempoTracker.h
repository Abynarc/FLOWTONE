#pragma once
#include <array>
#include <vector>

namespace flowtone
{
// Worker-thread only. Features remain independent through candidate selection.
struct TempoEstimate
{
    double bpm = 0, confidence = 0, alternative = 0;
    bool ambiguous = false;
    std::vector<double> beats;
};

class TempoTracker
{
public:
    using Features = std::array<double, 5>;
    void reset(double framesPerSecond);
    void push(const Features&);
    TempoEstimate estimate(bool offline) const;
    static double octaveDistance(double a, double b);
private:
    double frameRate = 93.75;
    std::vector<Features> frames;
    unsigned long long frameCount = 0;
};
}
