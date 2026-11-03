#include "TrackAnalysis.h"
#include "TempoTracker.h"
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
    TempoTracker tempoTracker;
    double previousRms = 0;
    TempoEstimate stableTempo;
    struct Observation { double seconds; TempoEstimate tempo; };
    std::vector<Observation> tempoHistory;
    double lastTempoSupport = 0;
    int cursor = 0, untilHop = hop;
    uint64_t total = 0, lastPublished = 0;
    int tonalFrames = 0;
    double signalEnergy = 0;
    bool offline = false;

    Impl()
    {
        for (int n = 0; n < keySize; ++n) window[static_cast<size_t>(n)] = static_cast<float>(0.5 - 0.5*std::cos(2*pi*n/(keySize-1)));
        for (int n = 0; n < fluxSize; ++n) fluxWindow[static_cast<size_t>(n)] = static_cast<float>(0.5 - 0.5*std::cos(2*pi*n/(fluxSize-1)));
        tempoTracker.reset(rate/hop);tempoHistory.reserve(10);
    }
    void clearAnalysis()
    {
        samples.fill(0); previous.fill(0); chroma.fill(0); bassChroma.fill(0); tempoTracker.reset(rate/hop);previousRms=0;stableTempo={};tempoHistory.clear();lastTempoSupport=0;
        cursor = 0; untilHop = hop; total = 0; lastPublished = 0; tonalFrames = 0; signalEnergy = 0;
        const std::lock_guard<std::mutex> lock(mutex); result = {};
    }
    void analyseFrame()
    {
        for (int n = 0; n < fluxSize; ++n)
            fluxWork[static_cast<size_t>(n)] = samples[static_cast<size_t>((cursor + keySize - fluxSize + n) % keySize)] * fluxWindow[static_cast<size_t>(n)];
        fluxFft.performFrequencyOnlyForwardTransform(fluxWork.data());
        TempoTracker::Features novelty {};
        double rms = 0;
        for (int b = 3; b < fluxSize/2; ++b)
        {
            const auto value = std::log1p(20.0f*fluxWork[static_cast<size_t>(b)]);
            const auto difference = juce::jmax(0.0f, value - previous[static_cast<size_t>(b)]);
            novelty[0] += difference;
            novelty[b<=20?1u:b<128?2u:3u] += difference;
            rms += fluxWork[static_cast<size_t>(b)]*fluxWork[static_cast<size_t>(b)];
            previous[static_cast<size_t>(b)] = value;
        }
        const auto logRms=std::log1p(rms);
        novelty[4]=std::max(0.0,logRms-previousRms);previousRms=logRms;
        tempoTracker.push(novelty);
        if (total < keySize || ((total/hop) % 4) != 0) return;
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
    TempoEstimate tempo() const { return tempoTracker.estimate(offline); }
    TempoEstimate liveTempo()
    {
        auto current=tempo();const double seconds=static_cast<double>(total)/rate;
        tempoHistory.push_back({seconds,current});
        tempoHistory.erase(std::remove_if(tempoHistory.begin(),tempoHistory.end(),[&](const auto& v){return seconds-v.seconds>5;}),tempoHistory.end());
        if(current.bpm<=0)
        {
            if(seconds-lastTempoSupport>2.5)stableTempo={};
            auto held=stableTempo;held.confidence*=0.65;held.ambiguous=true;return held;
        }
        int support=0;
        for(const auto& vote:tempoHistory)
            if(vote.tempo.bpm>0&&TempoTracker::octaveDistance(vote.tempo.bpm,current.bpm)<0.025)++support;
        if(support<2&&!offline)
        {
            auto pending=current;pending.confidence*=0.65;pending.ambiguous=true;return pending;
        }
        if(stableTempo.bpm>0&&TempoTracker::octaveDistance(stableTempo.bpm,current.bpm)<0.025)
        {
            const int octave=static_cast<int>(std::round(std::log2(stableTempo.bpm/current.bpm)));
            if(octave==1)
            {
                std::vector<double> beats;
                for(size_t i=0;i<current.beats.size();++i)
                {if(i>0)beats.push_back((current.beats[i-1]+current.beats[i])*0.5);beats.push_back(current.beats[i]);}
                current.beats=std::move(beats);current.bpm*=2;
            }
            else if(octave==-1)
            {
                std::vector<double> beats;
                for(size_t i=0;i<current.beats.size();i+=2)beats.push_back(current.beats[i]);
                current.beats=std::move(beats);current.bpm*=0.5;
            }
        }
        stableTempo=current;lastTempoSupport=seconds;
        return current;
    }
    void publish()
    {
        if(workingRevision!=revision.load())return;
        AnalysisSnapshot next;
        next.seconds = static_cast<double>(total)/rate; next.droppedSamples = dropped.load();
        lastPublished = total;
        const auto estimate=offline?tempo():liveTempo();
        next.bpm=estimate.bpm;next.tempoConfidence=estimate.confidence;
        next.alternativeBpm=estimate.alternative;next.tempoAmbiguous=estimate.ambiguous;
        next.tempoStable=estimate.bpm>0&&estimate.confidence>=0.65;
        next.beatTimes=estimate.beats;
        if(!offline&&estimate.bpm>0)
        {
            for(const auto& candidate:tempoHistory)
            {
                if(candidate.tempo.confidence<0.65||TempoTracker::octaveDistance(candidate.tempo.bpm,estimate.bpm)<0.06)continue;
                int support=0;
                for(const auto& vote:tempoHistory)
                    if(vote.tempo.confidence>=0.65&&TempoTracker::octaveDistance(vote.tempo.bpm,candidate.tempo.bpm)<0.025)++support;
                if(support>=2){next.tempoVariable=true;next.tempoAmbiguous=true;next.tempoStable=false;break;}
            }
        }
        next.pitchClasses=chroma;next.bassPitchClasses=bassChroma;
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
        next.complete = next.seconds >= 18 && next.tempoStable && next.key >= 0;
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
            if(na+nb>0 && fifo.getNumReady()==0 && total-lastPublished>=static_cast<uint64_t>(rate*0.5))publish();
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
    p.offline=true;p.sourceRate=reader.sampleRate;p.rate=juce::jmin(12000.0,p.sourceRate);p.tempoTracker.reset(p.rate/hop);
    const double alpha=1-std::exp(-2*pi*juce::jmin(3000.0,p.sourceRate*0.3)/p.sourceRate);
    juce::AudioBuffer<float> block(static_cast<int>(reader.numChannels),4096);
    std::vector<TempoEstimate> tempos;
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
                auto vote=p.tempo();if(vote.bpm>0)tempos.push_back(std::move(vote));
                lastVote=p.total;
                nextVote+=static_cast<uint64_t>(p.rate*12);
                if(!progress(static_cast<double>(offset+i)/static_cast<double>(reader.lengthInSamples)))return {};
            }
        }
    }
    p.publish();auto result=p.result;
    if(result.bpm>0&&(tempos.empty()||p.total-lastVote>=static_cast<uint64_t>(p.rate*4)))tempos.push_back(p.tempo());
    // Merge confidence-weighted evidence from the entire file. Equivalent
    // half/double pulse levels agree; unrelated tempi remain separate clusters.
    if(!tempos.empty())
    {
        const TempoEstimate* winner=nullptr;double bestWeight=0,totalWeight=0;
        for(const auto& vote:tempos)totalWeight+=vote.confidence;
        for(const auto& candidate:tempos)
        {
            double weight=0;
            for(const auto& vote:tempos)if(TempoTracker::octaveDistance(vote.bpm,candidate.bpm)<0.025)weight+=vote.confidence;
            if(weight>bestWeight+1e-9||(std::abs(weight-bestWeight)<1e-9&&winner!=nullptr&&candidate.beats.size()>winner->beats.size()))
            {bestWeight=weight;winner=&candidate;}
        }
        std::vector<std::pair<double,double>> group;
        for(const auto& vote:tempos)
            if(TempoTracker::octaveDistance(vote.bpm,winner->bpm)<0.025)
            {
                const double aligned=vote.bpm*std::pow(2.0,std::round(std::log2(winner->bpm/vote.bpm)));
                group.emplace_back(aligned,vote.confidence);
            }
        std::sort(group.begin(),group.end());double accumulated=0;
        for(const auto& vote:group){accumulated+=vote.second;if(accumulated>=bestWeight*0.5){result.bpm=vote.first;break;}}
        result.tempoConfidence=winner->confidence*bestWeight/totalWeight;
        result.tempoVariable=tempos.size()>=3&&bestWeight<totalWeight*0.75;
        result.tempoAmbiguous=winner->ambiguous||result.tempoVariable;
        result.tempoStable=result.tempoConfidence>=0.65&&!result.tempoVariable;
        result.alternativeBpm=winner->alternative;result.beatTimes=winner->beats;
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
    p.fifo.reset(); p.dropped.store(0); p.exiting.store(false); p.producerRevision=p.revision.load();
    p.worker=std::thread([&p]{p.run();});
}
void TrackAnalysis::stop()
{ impl->exiting.store(true); if(impl->worker.joinable()) impl->worker.join(); }
void TrackAnalysis::requestReset() { impl->revision.fetch_add(1);impl->dropped.store(0);const std::lock_guard<std::mutex> lock(impl->mutex);impl->result={}; }
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
    std::array<float,512> batch {};
    int count=0;
    auto enqueue=[&]
    {
        if(count==0)return;
        int a=0,na=0,b=0,nb=0;p.fifo.prepareToWrite(count,a,na,b,nb);
        std::copy_n(batch.data(),na,p.queued.data()+a);
        std::copy_n(batch.data()+na,nb,p.queued.data()+b);
        p.fifo.finishedWrite(na+nb);
        if(na+nb<count)p.dropped.fetch_add(static_cast<unsigned>(count-na-nb));
        count=0;
    };
    for(int i=0;i<buffer.getNumSamples();++i)
    {
        const double sample=std::isfinite(input[i]) ? input[i] : 0;
        p.lp1+=alpha*(sample-p.lp1);p.lp2+=alpha*(p.lp1-p.lp2);p.sum+=p.lp2;++p.sumCount;p.phase+=p.rate;
        if(p.phase>=p.sourceRate)
        {
            p.phase-=p.sourceRate;batch[static_cast<size_t>(count++)]=static_cast<float>(p.sum/p.sumCount);
            p.sum=0;p.sumCount=0;
            if(count==static_cast<int>(batch.size()))enqueue();
        }
    }
    enqueue();
}
AnalysisSnapshot TrackAnalysis::snapshot() const
{ const std::lock_guard<std::mutex> lock(impl->mutex); auto result=impl->result;result.droppedSamples=impl->dropped.load();return result; }
juce::String TrackAnalysis::keyName(int key,bool minor)
{
    if(key<0) return {};
    static constexpr const char* names[]={"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
    return juce::String(names[(key%12+12)%12])+(minor?" minor":" major");
}
}
