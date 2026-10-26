#include "TrackAnalysis.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <mutex>
#include <numeric>
#include <thread>
#include <stdexcept>
#if JUCE_MAC
#include <pthread/qos.h>
#endif

namespace flowtone
{
namespace
{
constexpr int capacity = 131072, keySize = 4096, fluxSize = 1024, hop = 128;
constexpr double pi = 3.14159265358979323846;
double correlation(const std::array<double, 12>& a, const std::array<double, 12>& b)
{
    const double ma = std::accumulate(a.begin(), a.end(), 0.0) / 12;
    const double mb = std::accumulate(b.begin(), b.end(), 0.0) / 12;
    double cross = 0, aa = 0, bb = 0;
    for (int i = 0; i < 12; ++i)
    { const auto x = a[static_cast<size_t>(i)] - ma, y = b[static_cast<size_t>(i)] - mb; cross += x*y; aa += x*x; bb += y*y; }
    return aa*bb > 1e-16 ? cross / std::sqrt(aa*bb) : 0;
}
}
struct TrackAnalysis::Impl
{
    juce::AbstractFifo fifo { capacity };
    std::array<float, capacity> queued {};
    std::atomic<bool> exiting { false };
    std::atomic<unsigned> revision { 0 }, dropped { 0 };
    unsigned producerRevision = 0, workingRevision = 0;
    std::thread worker;
    mutable std::mutex mutex;
    AnalysisSnapshot result;
    double sourceRate = 48000, rate = 12000, phase = 0, sum = 0, lp1 = 0, lp2 = 0;
    int sumCount = 0;
    juce::dsp::FFT keyFft { 12 }, fluxFft { 10 };
    std::array<float, keySize> window {}, fluxWindow {}, samples {};
    std::array<float, keySize*2> keyWork {};
    std::array<float, fluxSize*2> fluxWork {};
    std::array<float, fluxSize/2+1> previous {};
    std::array<double, 12> chroma {}, bassChroma {};
    std::vector<double> onsets, bassOnsets;
    int cursor = 0, untilHop = hop;
    uint64_t total = 0;
    int tonalFrames = 0;
    double signalEnergy = 0;
    bool offline = false;

    Impl()
    {
        for (int n = 0; n < keySize; ++n) window[static_cast<size_t>(n)] = static_cast<float>(0.5 - 0.5*std::cos(2*pi*n/(keySize-1)));
        for (int n = 0; n < fluxSize; ++n) fluxWindow[static_cast<size_t>(n)] = static_cast<float>(0.5 - 0.5*std::cos(2*pi*n/(fluxSize-1)));
        onsets.reserve(6000);bassOnsets.reserve(6000);
    }
    void clearAnalysis()
    {
        samples.fill(0); previous.fill(0); chroma.fill(0); bassChroma.fill(0); onsets.clear();bassOnsets.clear();
        cursor = 0; untilHop = hop; total = 0; tonalFrames = 0; signalEnergy = 0;
        const std::lock_guard<std::mutex> lock(mutex); result = {};
    }
    void analyseFrame()
    {
        for (int n = 0; n < fluxSize; ++n)
            fluxWork[static_cast<size_t>(n)] = samples[static_cast<size_t>((cursor + keySize - fluxSize + n) % keySize)] * fluxWindow[static_cast<size_t>(n)];
        fluxFft.performFrequencyOnlyForwardTransform(fluxWork.data());
        double novelty = 0, bassNovelty = 0;
        for (int b = 3; b < fluxSize/2; ++b)
        {
            const auto value = std::log1p(20.0f*fluxWork[static_cast<size_t>(b)]);
            const auto difference = juce::jmax(0.0f, value - previous[static_cast<size_t>(b)]);
            novelty += difference;
            if(b<=20)bassNovelty+=difference;
            previous[static_cast<size_t>(b)] = value;
        }
        if (onsets.size() >= 6000) {onsets.erase(onsets.begin(), onsets.begin() + 1000);bassOnsets.erase(bassOnsets.begin(),bassOnsets.begin()+1000);}
        onsets.push_back(novelty);bassOnsets.push_back(bassNovelty);
        if (total < keySize || (onsets.size() % 4) != 0) return;
        for (int n = 0; n < keySize; ++n)
            keyWork[static_cast<size_t>(n)] = samples[static_cast<size_t>((cursor + n) % keySize)] * window[static_cast<size_t>(n)];
        keyFft.performFrequencyOnlyForwardTransform(keyWork.data());
        const auto first = juce::jmax(3, static_cast<int>(55.0*keySize/rate));
        const auto last = juce::jmin(keySize/2-5, static_cast<int>(3500.0*keySize/rate));
        float maximum = 0;
        for (int b = first; b < last; ++b) maximum = juce::jmax(maximum, keyWork[static_cast<size_t>(b)]);
        if (maximum < 0.02f) return;
        std::array<double, 12> frame {}, bass {};
        int peaks = 0;
        for (int b = first; b < last; ++b)
        {
            const double a = keyWork[static_cast<size_t>(b-1)], v = keyWork[static_cast<size_t>(b)], c = keyWork[static_cast<size_t>(b+1)];
            if (v < maximum*0.035 || v <= a || v <= c) continue;
            const double floor = (keyWork[static_cast<size_t>(b-3)] + keyWork[static_cast<size_t>(b+3)]) * 0.5;
            if (v < floor*2.0) continue;
            const double la = std::log(juce::jmax(1e-12, a)), lv = std::log(v), lc = std::log(juce::jmax(1e-12, c));
            const double denominator = la - 2*lv + lc;
            const double delta = std::abs(denominator) > 1e-12 ? juce::jlimit(-0.5, 0.5, 0.5*(la-lc)/denominator) : 0;
            const double frequency = (b + delta)*rate/keySize;
            const double midi = 69 + 12*std::log2(frequency/440);
            const int nearest = static_cast<int>(std::round(midi));
            const double distance = midi - nearest;
            if (std::abs(distance) > 0.38) continue;
            const double weight = (v / maximum) * std::exp(-distance*distance/0.055) / std::pow(frequency/110, 0.2);
            const auto pc = static_cast<size_t>((nearest % 12 + 12) % 12);
            frame[pc] += weight;
            if (frequency < 330) bass[pc] += weight;
            ++peaks;
        }
        if (peaks < 2) return;
        const double weight = std::accumulate(frame.begin(), frame.end(), 0.0);
        if (weight < 1e-8) return;
        for (size_t pc = 0; pc < 12; ++pc) { chroma[pc] += frame[pc]/weight; bassChroma[pc] += bass[pc]/weight; }
        ++tonalFrames;
    }
    double tempo() const
    {
        if (onsets.size() < static_cast<size_t>(rate/hop*(offline ? 4 : 8))) return 0;
        const int n = juce::jmin(static_cast<int>(onsets.size()), static_cast<int>(rate/hop*24));
        const int offset = static_cast<int>(onsets.size()) - n;
        std::vector<double> values(static_cast<size_t>(n));
        std::vector<double> bassValues(static_cast<size_t>(n));
        double energy = 0, bassEnergy = 0;
        for (int i = 0; i < n; ++i)
        {
            double mean = 0, bassMean = 0; int count = 0;
            for (int j = juce::jmax(0, i-8); j <= juce::jmin(n-1, i+8); ++j) { mean += onsets[static_cast<size_t>(offset+j)];bassMean+=bassOnsets[static_cast<size_t>(offset+j)]; ++count; }
            const double value = juce::jmax(0.0, onsets[static_cast<size_t>(offset+i)] - mean/count);
            const double bass = juce::jmax(0.0, bassOnsets[static_cast<size_t>(offset+i)]-bassMean/count);
            values[static_cast<size_t>(i)] = value; energy += value*value;
            bassValues[static_cast<size_t>(i)]=bass;bassEnergy+=bass*bass;
        }
        if (energy < 1e-6) return 0;
        if(bassEnergy>1e-8)for(int i=0;i<n;++i)values[static_cast<size_t>(i)]=0.35*values[static_cast<size_t>(i)]/std::sqrt(energy/n)+0.65*bassValues[static_cast<size_t>(i)]/std::sqrt(bassEnergy/n);
        const int first = static_cast<int>(60*rate/(hop*240)), last = static_cast<int>(60*rate/(hop*45));
        std::vector<double> scores(static_cast<size_t>(last*3+3));
        for (int lag = first; lag <= last*3 && lag < n/2; ++lag)
        {
            double product = 0, e1 = 0, e2 = 0, sumA = 0, sumB = 0;
            for (int i = lag; i < n; ++i)
            { const auto a = values[static_cast<size_t>(i)], b = values[static_cast<size_t>(i-lag)]; product += a*b; e1 += a*a; e2 += b*b;sumA+=a;sumB+=b; }
            const double count=n-lag;product-=sumA*sumB/count;e1-=sumA*sumA/count;e2-=sumB*sumB/count;
            scores[static_cast<size_t>(lag)] = e1*e2 > 1e-12 ? product/std::sqrt(e1*e2) : 0;
        }
        int best = -1; double bestScore = 0;
        for (int lag = first+1; lag < last; ++lag)
        {
            const double value = scores[static_cast<size_t>(lag)];
            if (value < 0.2 || value < scores[static_cast<size_t>(lag-1)] || value < scores[static_cast<size_t>(lag+1)]) continue;
            const double bpm = 60*rate/(hop*lag);
            const double prior = 0.92 + 0.08*std::exp(-0.5*std::pow(std::log2(bpm/120.0)/0.7, 2));
            const double combined = (value + 0.45*scores[static_cast<size_t>(lag*2)] + 0.2*scores[static_cast<size_t>(lag*3)]) * prior;
            if (combined > bestScore) { bestScore = combined; best = lag; }
        }
        if (best < 0 || bestScore < 0.38) return 0;
        // Prefer the shorter pulse when its correlation is still strong. This
        // prevents alternating snare accents from forcing every track to half time.
        while(best/2>first)
        {
            int candidate=best/2;
            for(int j=juce::jmax(first+1,best/2-1);j<=juce::jmin(last-1,best/2+1);++j)
                if(scores[static_cast<size_t>(j)]>scores[static_cast<size_t>(candidate)])candidate=j;
            if(scores[static_cast<size_t>(candidate)]<0.70*scores[static_cast<size_t>(best)]||scores[static_cast<size_t>(candidate)]<0.3)break;
            best=candidate;
        }
        const double a = scores[static_cast<size_t>(best-1)], b = scores[static_cast<size_t>(best)], c = scores[static_cast<size_t>(best+1)];
        const double den = a-2*b+c;
        const double lag = best + (std::abs(den) > 1e-9 ? juce::jlimit(-0.5,0.5,0.5*(a-c)/den) : 0.0);
        const double coarse=60*rate/(hop*lag);
        // Refine over the full rhythmic window instead of rounding to a hop.
        double refined=coarse,bestPower=-1;
        for(int step=-60;step<=60;++step)
        {
            const double candidate=coarse+step*0.025;
            if(candidate<45||candidate>240)continue;
            const double angle=2*pi*candidate*hop/(60*rate),cr=std::cos(angle),ci=std::sin(angle);
            double real=0,imag=0,phaseR=1,phaseI=0;
            for(int i=0;i<n;++i)
            {
                const double sample=values[static_cast<size_t>(i)]*(0.5-0.5*std::cos(2*pi*i/(n-1)));
                real+=sample*phaseR;imag+=sample*phaseI;
                const double next=phaseR*cr-phaseI*ci;phaseI=phaseI*cr+phaseR*ci;phaseR=next;
            }
            const double power=real*real+imag*imag;
            if(power>bestPower){bestPower=power;refined=candidate;}
        }
        return refined;
    }
    void publish()
    {
        if(workingRevision!=revision.load())return;
        AnalysisSnapshot next;
        next.seconds = total/rate;
        next.bpm = tempo();next.pitchClasses=chroma;next.bassPitchClasses=bassChroma;
        if (next.seconds >= (offline ? 4 : 12) && tonalFrames >= 30)
        {
            static constexpr double major[] = {6.35,2.23,3.48,2.33,4.38,4.09,2.52,5.19,2.39,3.66,2.29,2.88};
            static constexpr double minor[] = {6.33,2.68,3.52,5.38,2.60,3.53,2.54,4.75,3.98,2.69,3.34,3.17};
            static constexpr double major2[]={5,2,3.5,2,4.5,4,2,4.5,2,3.5,1.5,4};
            static constexpr double minor2[]={5,2,3.5,4.5,2,4,2,4.5,3.5,2,1.5,4};
            std::array<double, 24> scores {};
            int active = 0;
            const auto max = *std::max_element(chroma.begin(),chroma.end());
            for (auto value : chroma) if (value > max*0.13) ++active;
            for (int mode = 0; mode < 2; ++mode)
                for (int tonic = 0; tonic < 12; ++tonic)
                {
                    std::array<double,12> profile {};
                    for (int pc = 0; pc < 12; ++pc) profile[static_cast<size_t>(pc)] = (mode ? minor : major)[(pc-tonic+12)%12];
                    const double first=correlation(chroma,profile);
                    for(int pc=0;pc<12;++pc)profile[static_cast<size_t>(pc)]=(mode?minor2:major2)[(pc-tonic+12)%12];
                    scores[static_cast<size_t>(mode*12+tonic)] = 0.55*first+0.45*correlation(chroma,profile);
                }
            const int best = static_cast<int>(std::max_element(scores.begin(),scores.end())-scores.begin());
            auto sorted = scores; std::sort(sorted.begin(),sorted.end(),std::greater<double>());
            if (active >= 4 && sorted[0] >= 0.70 && sorted[0]-sorted[1] >= 0.05)
            { next.key = best%12; next.minor = best>=12; }
        }
        next.complete = next.seconds >= 18 && next.bpm > 0 && next.key >= 0;
        const std::lock_guard<std::mutex> lock(mutex);if(workingRevision==revision.load())result = next;
    }
    void run()
    {
#if JUCE_MAC
        pthread_set_qos_class_self_np(QOS_CLASS_UTILITY,0);
#endif
        unsigned seenRevision = revision.load(), seenDropped = dropped.load();
        workingRevision=seenRevision;clearAnalysis();
        while (!exiting.load())
        {
            if (seenRevision != revision.load() || seenDropped != dropped.load())
            {
                seenRevision = revision.load(); seenDropped = dropped.load();workingRevision=seenRevision;
                int x=0,nx=0,y=0,ny=0;fifo.prepareToRead(fifo.getNumReady(),x,nx,y,ny);fifo.finishedRead(nx+ny);
                clearAnalysis();
            }
            int a=0, na=0, b=0, nb=0; fifo.prepareToRead(2048,a,na,b,nb);
            auto consume = [&](int start, int count)
            {
                for (int i=0;i<count;++i)
                {
                    const float sample = queued[static_cast<size_t>(start+i)];
                    samples[static_cast<size_t>(cursor)] = sample; cursor=(cursor+1)%keySize; ++total;
                    signalEnergy += sample*sample;
                    if (--untilHop == 0) { analyseFrame(); untilHop=hop; }
                    if (total % static_cast<uint64_t>(rate) == 0) publish();
                }
            };
            consume(a,na); consume(b,nb); fifo.finishedRead(na+nb);
            if(na+nb>0 && fifo.getNumReady()==0)publish();
            if (na+nb==0) std::this_thread::sleep_for(std::chrono::milliseconds(3));
        }
    }
};
TrackAnalysis::TrackAnalysis() : impl(std::make_unique<Impl>()) {}
AnalysisSnapshot TrackAnalysis::analyseFile(juce::AudioFormatReader& reader, std::array<float,512>& waveform,
    const std::function<bool(double)>& progress)
{
    auto storage=std::make_unique<Impl>();
    auto& p=*storage;
    p.offline=true;p.sourceRate=reader.sampleRate;p.rate=juce::jmin(12000.0,p.sourceRate);
    const double alpha=1-std::exp(-2*pi*juce::jmin(3000.0,p.sourceRate*0.3)/p.sourceRate);
    juce::AudioBuffer<float> block(static_cast<int>(reader.numChannels),4096);
    std::vector<double> tempos;
    uint64_t nextVote=static_cast<uint64_t>(p.rate*12);
    uint64_t lastVote=0;
    waveform.fill(0);
    for(juce::int64 offset=0;offset<reader.lengthInSamples;offset+=block.getNumSamples())
    {
        if(!progress(static_cast<double>(offset)/static_cast<double>(reader.lengthInSamples)))return {};
        const int count=static_cast<int>(juce::jmin<juce::int64>(block.getNumSamples(),reader.lengthInSamples-offset));
        if(!reader.read(&block,0,count,offset,true,true))throw std::runtime_error("read");
        int channel=0;
        if(block.getNumChannels()>1&&block.getMagnitude(1,0,count)>block.getMagnitude(0,0,count)*1.4f)channel=1;
        for(int i=0;i<count;++i)
        {
            double peak=0;
            for(int c=0;c<block.getNumChannels();++c){const auto s=block.getSample(c,i);if(std::isfinite(s))peak=juce::jmax(peak,std::abs(static_cast<double>(s)));}
            const auto bin=static_cast<size_t>(juce::jmin<juce::int64>(511,(offset+i)*512/reader.lengthInSamples));
            waveform[bin]=juce::jmax(waveform[bin],static_cast<float>(peak));
            const auto value=block.getSample(channel,i);const double sample=std::isfinite(value)?value:0;
            p.lp1+=alpha*(sample-p.lp1);p.lp2+=alpha*(p.lp1-p.lp2);p.sum+=p.lp2;++p.sumCount;p.phase+=p.rate;
            if(p.phase<p.sourceRate)continue;
            p.phase-=p.sourceRate;
            p.samples[static_cast<size_t>(p.cursor)]=static_cast<float>(p.sum/p.sumCount);p.cursor=(p.cursor+1)%keySize;++p.total;
            p.sum=0;p.sumCount=0;
            if(--p.untilHop==0){p.analyseFrame();p.untilHop=hop;}
            if(p.total>=nextVote)
            {
                const auto bpm=p.tempo();if(bpm>0)tempos.push_back(bpm);
                lastVote=p.total;
                nextVote+=static_cast<uint64_t>(p.rate*12);
                if(!progress(static_cast<double>(offset+i)/static_cast<double>(reader.lengthInSamples)))return {};
            }
        }
    }
    p.publish();auto result=p.result;
    if(result.bpm>0&&(tempos.empty()||p.total-lastVote>=static_cast<uint64_t>(p.rate*4)))tempos.push_back(result.bpm);
    // Retain evidence from the whole file: later quiet sections cannot discard
    // tempo estimates from the beginning. Compare absolute pulse levels first.
    if(!tempos.empty())
    {
        std::vector<double> strongest;
        for(const auto candidate:tempos)
        {
            std::vector<double> group;
            for(const auto bpm:tempos)if(std::abs(std::log2(bpm/candidate))<0.04)group.push_back(bpm);
            if(group.size()>strongest.size())strongest=std::move(group);
        }
        std::sort(strongest.begin(),strongest.end());
        result.bpm=strongest[strongest.size()/2];
    }
    result.seconds=static_cast<double>(reader.lengthInSamples)/reader.sampleRate;
    result.complete=true;
    return result;
}
TrackAnalysis::~TrackAnalysis() { stop(); }
void TrackAnalysis::prepare(double sourceRate)
{
    stop(); auto& p=*impl;
    p.sourceRate=sourceRate; p.rate=juce::jmin(12000.0,sourceRate); p.phase=0; p.sum=0; p.sumCount=0; p.lp1=p.lp2=0;
    p.fifo.reset(); p.exiting.store(false); p.producerRevision=p.revision.load();
    p.worker=std::thread([&p]{p.run();});
}
void TrackAnalysis::stop()
{ impl->exiting.store(true); if(impl->worker.joinable()) impl->worker.join(); }
void TrackAnalysis::requestReset() { impl->revision.fetch_add(1);const std::lock_guard<std::mutex> lock(impl->mutex);impl->result={}; }
int TrackAnalysis::pendingSamples() const { return impl->fifo.getNumReady(); }
void TrackAnalysis::push(const juce::AudioBuffer<float>& buffer)
{
    auto& p=*impl;
    if(p.exiting.load() || buffer.getNumChannels()==0) return;
    if(p.producerRevision!=p.revision.load()) {p.producerRevision=p.revision.load();p.phase=p.sum=0;p.sumCount=0;p.lp1=p.lp2=0;}
    int channel=0;
    if(buffer.getNumChannels()>1 && buffer.getMagnitude(1,0,buffer.getNumSamples()) > buffer.getMagnitude(0,0,buffer.getNumSamples())*1.4f) channel=1;
    const auto* input=buffer.getReadPointer(channel);
    const double alpha=1-std::exp(-2*pi*juce::jmin(3000.0,p.sourceRate*0.3)/p.sourceRate);
    for(int i=0;i<buffer.getNumSamples();++i)
    {
        const double sample=std::isfinite(input[i]) ? input[i] : 0;
        p.lp1+=alpha*(sample-p.lp1);p.lp2+=alpha*(p.lp1-p.lp2);p.sum+=p.lp2;++p.sumCount;p.phase+=p.rate;
        if(p.phase>=p.sourceRate)
        {
            p.phase-=p.sourceRate; int a=0,na=0,b=0,nb=0;p.fifo.prepareToWrite(1,a,na,b,nb);
            if(na>0){p.queued[static_cast<size_t>(a)]=static_cast<float>(p.sum/p.sumCount);p.fifo.finishedWrite(1);}else p.dropped.fetch_add(1);
            p.sum=0;p.sumCount=0;
        }
    }
}
AnalysisSnapshot TrackAnalysis::snapshot() const
{ const std::lock_guard<std::mutex> lock(impl->mutex); return impl->result; }
juce::String TrackAnalysis::keyName(int key,bool minor)
{
    if(key<0) return {};
    static constexpr const char* names[]={"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
    return juce::String(names[(key%12+12)%12])+(minor?" minor":" major");
}
}
