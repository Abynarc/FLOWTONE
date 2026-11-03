#include "TempoTracker.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace flowtone
{
namespace
{
constexpr double pi = 3.14159265358979323846;
constexpr double minimumBpm = 45, maximumBpm = 240;
struct Feature
{
    std::vector<double> values, weighted;
    double weightSum = 0, quality = 0;
};
struct Candidate { double bpm = 0, score = 0; };

double gridCoverage(const Feature& feature,double period)
{
    const auto& values=feature.values;
    const int count=static_cast<int>(values.size());
    const auto peak=*std::max_element(values.begin(),values.end());
    const auto radius=std::max(1,static_cast<int>(period*0.04));
    double best=0;
    for(int phase=0;phase<static_cast<int>(std::ceil(period));++phase)
    {
        double supported=0;int beats=0;
        for(double position=phase;position<count-radius;position+=period)
        {
            const auto centre=static_cast<int>(std::round(position));
            double strength=0;
            for(int i=std::max(0,centre-radius);i<=std::min(count-1,centre+radius);++i)
                strength=std::max(strength,values[static_cast<size_t>(i)]);
            supported+=std::min(1.0,strength/(peak*0.3+1e-15));++beats;
        }
        if(beats>=5)best=std::max(best,supported/beats);
    }
    return best;
}

double interpolate(const std::vector<double>& values, double index)
{
    const auto left = static_cast<size_t>(std::max(0.0, index));
    if (left + 1 >= values.size()) return 0;
    return values[left] + (index-static_cast<double>(left))*(values[left+1]-values[left]);
}

// A pulse is supported by several harmonics. Unlike a single Fourier bin,
// this also works when alternating accents cancel the fundamental.
double pulsePower(const Feature& feature, double bpm, double frameRate)
{
    double power = 0;
    for (int harmonic = 1; harmonic <= 4; ++harmonic)
    {
        const double angle = 2*pi*bpm*harmonic/(60*frameRate);
        const double cr = std::cos(angle), ci = std::sin(angle);
        double real = 0, imag = 0, pr = 1, piValue = 0;
        for (const auto value : feature.weighted)
        {
            real += value*pr; imag += value*piValue;
            const auto next = pr*cr-piValue*ci;
            piValue = piValue*cr+pr*ci; pr = next;
        }
        power += (real*real+imag*imag)/harmonic;
    }
    return power/(feature.weightSum*feature.weightSum*(1+0.5+1.0/3+0.25)+1e-20);
}

std::vector<Candidate> candidates(const Feature& feature, double frameRate)
{
    const auto& values = feature.values;
    const int count = static_cast<int>(values.size());
    const int first = static_cast<int>(60*frameRate/maximumBpm);
    const int last = std::min(count/2-2, static_cast<int>(std::ceil(60*frameRate/minimumBpm)));
    std::vector<double> acf(static_cast<size_t>(last+2));
    for (int lag = std::max(1,first-1); lag <= last+1; ++lag)
    {
        double product = 0, aa = 0, bb = 0, aSum = 0, bSum = 0;
        for (int i = lag; i < count; ++i)
        {
            const auto a = values[static_cast<size_t>(i)], b = values[static_cast<size_t>(i-lag)];
            product += a*b; aa += a*a; bb += b*b; aSum += a; bSum += b;
        }
        const double n = count-lag;
        product -= aSum*bSum/n; aa -= aSum*aSum/n; bb -= bSum*bSum/n;
        acf[static_cast<size_t>(lag)] = aa*bb > 1e-15 ? product/std::sqrt(aa*bb) : 0;
    }
    std::vector<Candidate> peaks;
    for (int lag = first; lag <= last; ++lag)
    {
        const auto a = acf[static_cast<size_t>(lag-1)], b = acf[static_cast<size_t>(lag)], c = acf[static_cast<size_t>(lag+1)];
        if (b < 0.16 || b < a || b < c) continue;
        const auto denominator = a-2*b+c;
        const auto delta = std::abs(denominator) > 1e-12 ? std::clamp(0.5*(a-c)/denominator, -0.5, 0.5) : 0;
        const auto period = lag+delta;
        // Compare harmonics at fractional delays, never at 2*roundedPeriod.
        const double support = b+0.25*interpolate(acf, period*2);
        peaks.push_back({60*frameRate/period, support});
    }
    std::sort(peaks.begin(), peaks.end(), [](const auto& a, const auto& b){return a.score>b.score;});
    if (peaks.size()>6) peaks.resize(6);
    return peaks;
}

std::vector<double> trackBeats(const std::vector<double>& values, double period)
{
    const int size = static_cast<int>(values.size());
    std::vector<double> score(static_cast<size_t>(size));
    std::vector<int> previous(static_cast<size_t>(size), -1);
    const auto first = std::max(1, static_cast<int>(period*0.75));
    const auto last = static_cast<int>(std::ceil(period*1.25));
    for (int i = 0; i < size; ++i)
    {
        double best = 0; int predecessor = -1;
        for (int distance = first; distance <= last && distance <= i; ++distance)
        {
            const auto penalty = 40*std::pow(std::log(distance/period), 2);
            const auto candidate = 0.90*score[static_cast<size_t>(i-distance)]-penalty;
            if (candidate>best) {best=candidate;predecessor=i-distance;}
        }
        score[static_cast<size_t>(i)] = values[static_cast<size_t>(i)]+best;
        previous[static_cast<size_t>(i)] = predecessor;
    }
    const int begin = std::max(0, size-static_cast<int>(period*1.5));
    int end = static_cast<int>(std::max_element(score.begin()+begin,score.end())-score.begin());
    std::vector<double> beats;
    while (end>=0)
    {
        const int left = std::max(0,end-1), right = std::min(size-1,end+1);
        const auto a=values[static_cast<size_t>(left)], b=values[static_cast<size_t>(end)], c=values[static_cast<size_t>(right)];
        const auto denominator = a-2*b+c;
        const double delta=std::abs(denominator)>1e-9?std::clamp(0.5*(a-c)/denominator,-0.5,0.5):0;
        beats.push_back(std::clamp(end+delta,0.0,static_cast<double>(size-1)));
        end=previous[static_cast<size_t>(end)];
    }
    std::reverse(beats.begin(),beats.end());
    return beats;
}
}

void TempoTracker::reset(double framesPerSecond)
{
    frameRate=framesPerSecond;frames.clear();frameCount=0;
    frames.reserve(static_cast<size_t>(std::ceil(frameRate*26)));
}
void TempoTracker::push(const Features& input)
{
    if (frames.size()>=static_cast<size_t>(frameRate*26))
        frames.erase(frames.begin(),frames.begin()+static_cast<int>(frameRate*2));
    frames.push_back(input);++frameCount;
}
double TempoTracker::octaveDistance(double a,double b)
{
    if (a<=0||b<=0) return 1e6;
    const auto octaves=std::log2(a/b);
    return std::min({std::abs(octaves),std::abs(octaves-1),std::abs(octaves+1)});
}

TempoEstimate TempoTracker::estimate(bool offline) const
{
    TempoEstimate result;
    if (frames.size()<static_cast<size_t>(frameRate*(offline?4:8))) return result;
    const int count=std::min(static_cast<int>(frames.size()),static_cast<int>(frameRate*(offline?24:16)));
    const int offset=static_cast<int>(frames.size())-count;
    std::array<Feature,5> features;
    std::vector<Candidate> pool;
    for (size_t channel=0;channel<features.size();++channel)
    {
        auto& feature=features[channel];feature.values.resize(static_cast<size_t>(count));
        double energy=0;
        for(int i=0;i<count;++i)
        {
            double mean=0;int n=0;
            for(int j=std::max(0,i-8);j<=std::min(count-1,i+8);++j){mean+=frames[static_cast<size_t>(offset+j)][channel];++n;}
            const double value=std::max(0.0,frames[static_cast<size_t>(offset+i)][channel]-mean/n);
            feature.values[static_cast<size_t>(i)]=value;energy+=value*value;
        }
        if(energy<1e-7)continue;
        const auto rms=std::sqrt(energy/count);
        feature.weighted.resize(static_cast<size_t>(count));
        for(int i=0;i<count;++i)
        {
            feature.values[static_cast<size_t>(i)]=std::min(6.0,feature.values[static_cast<size_t>(i)]/rms);
            const auto weighted=feature.values[static_cast<size_t>(i)]*(0.5-0.5*std::cos(2*pi*i/(count-1)));
            feature.weighted[static_cast<size_t>(i)]=weighted;feature.weightSum+=weighted;
        }
        for(const auto& candidate:candidates(feature,frameRate))
        {
            // Each independently detected period is refined before selection.
            Candidate refined;
            for(int step=-16;step<=16;++step)
            {
                const auto bpm=candidate.bpm+step*0.1;
                if(bpm<minimumBpm||bpm>maximumBpm)continue;
                const auto score=pulsePower(feature,bpm,frameRate);
                if(score>refined.score)refined={bpm,score};
            }
            feature.quality=std::max(feature.quality,refined.score);
            if(refined.score>0.08)pool.push_back(refined);
        }
    }
    if(pool.empty())return result;
    std::sort(pool.begin(),pool.end(),[](const auto& a,const auto& b){return a.score>b.score;});
    std::vector<Candidate> unique;
    for(const auto& candidate:pool)
    {
        if(std::none_of(unique.begin(),unique.end(),[&](const auto& other){return std::abs(std::log2(candidate.bpm/other.bpm))<0.015;}))
            unique.push_back(candidate);
        if(unique.size()>=12)break;
    }
    double totalQuality=0;
    for(const auto& feature:features)if(feature.quality>=0.08)totalQuality+=feature.quality;
    Candidate winner;double runner=0;int winnerSupport=0;
    std::vector<Candidate> ranked;
    for(auto candidate:unique)
    {
        double score=0;int support=0;
        for(const auto& feature:features)
        {
            if(feature.quality<0.08)continue;
            auto power=pulsePower(feature,candidate.bpm,frameRate);
            // Half/double tempo belongs to the same valid musical pulse family.
            if(candidate.bpm/2>=minimumBpm)power=std::max(power,0.95*pulsePower(feature,candidate.bpm/2,frameRate));
            if(candidate.bpm*2<=maximumBpm)power=std::max(power,0.95*pulsePower(feature,candidate.bpm*2,frameRate));
            const auto coverage=gridCoverage(feature,60*frameRate/candidate.bpm);
            score+=feature.quality*(0.85*coverage+0.15*std::min(1.0,power/feature.quality));
            if(coverage>=0.65)++support;
        }
        const double prior=0.94+0.06*std::exp(-0.5*std::pow(std::log2(candidate.bpm/110.0)/0.7,2));
        candidate.score=score/(totalQuality+1e-20)*prior;
        ranked.push_back(candidate);
        if(candidate.score>winner.score){winner=candidate;winnerSupport=support;}
    }
    for(const auto& candidate:ranked)
        if(octaveDistance(candidate.bpm,winner.bpm)>0.06&&candidate.score>runner)
        {runner=candidate.score;result.alternative=candidate.bpm;}
    std::vector<double> combined(static_cast<size_t>(count));
    double weight=0;
    for(const auto& feature:features)
    {
        if(feature.quality<0.08)continue;
        const auto power=pulsePower(feature,winner.bpm,frameRate);
        if(power<feature.quality*0.3)continue;
        for(int i=0;i<count;++i)combined[static_cast<size_t>(i)]+=feature.values[static_cast<size_t>(i)]*power;
        weight+=power;
    }
    if(weight<=0)return result;
    for(auto& value:combined)value/=weight;
    auto beats=trackBeats(combined,60*frameRate/winner.bpm);
    if(beats.size()<5)return result;
    double supported=0;
    for(const auto beat:beats)if(interpolate(combined,beat)>0.35)++supported;
    const auto coverage=supported/static_cast<double>(beats.size());
    const auto margin=std::clamp((winner.score-runner)/0.25,0.0,1.0);
    result.confidence=std::clamp(0.45*winner.score+0.30*coverage+0.25*margin,0.0,1.0);
    if(winnerSupport<2||coverage<0.45||winner.score<0.5||result.confidence<0.55)return {};
    // Fit all beat locations rather than rounding individual inter-beat gaps.
    const auto beatCount=static_cast<double>(beats.size());
    const double xMean=(beatCount-1)*0.5,yMean=std::accumulate(beats.begin(),beats.end(),0.0)/beatCount;
    double xx=0,xy=0;
    for(size_t i=0;i<beats.size();++i){const auto x=static_cast<double>(i)-xMean;xx+=x*x;xy+=x*(beats[i]-yMean);}
    const auto fitted=xy>0?60*frameRate*xx/xy:winner.bpm;
    result.bpm=std::abs(std::log2(fitted/winner.bpm))<0.025?fitted:winner.bpm;
    result.ambiguous=runner>winner.score*0.88;
    const auto startTime=(static_cast<double>(frameCount)-count)/frameRate;
    for(auto& beat:beats)beat=startTime+beat/frameRate;
    result.beats=std::move(beats);
    return result;
}
}
