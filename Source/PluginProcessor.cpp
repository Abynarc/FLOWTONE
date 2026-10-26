#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
namespace { juce::String ru(const char* text){return juce::String::fromUTF8(text);} }
juce::AudioProcessorValueTreeState::ParameterLayout FLOWTONEAudioProcessor::layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("shift",1),ru(u8"Сдвиг тона"),juce::NormalisableRange<float>(-12,12,0.5f),0));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("formants",1),ru(u8"Сохранять тембр"),true));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("quality",1),ru(u8"Высокое качество"),true));
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("gain",1),ru(u8"Громкость выхода"),juce::NormalisableRange<float>(-12,12,0.1f),0));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("after",1),ru(u8"После обработки"),true));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("bypass",1),ru(u8"Обход"),false));
    p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID("tempoOctave",1),ru(u8"Множитель BPM"),-2,2,0));
    return p;
}
FLOWTONEAudioProcessor::FLOWTONEAudioProcessor()
    : AudioProcessor(BusesProperties().withInput(ru(u8"Вход"),juce::AudioChannelSet::stereo(),true).withOutput(ru(u8"Выход"),juce::AudioChannelSet::stereo(),true)),
      parameters(*this,nullptr,"FLOWTONE",layout()),
      fileAnalysis([this]{updateHostDisplay(ChangeDetails().withNonParameterStateChanged(true));}) {
    for(auto& v:history)v.store(0);
    rawShift=parameters.getRawParameterValue("shift");rawFormants=parameters.getRawParameterValue("formants");rawQuality=parameters.getRawParameterValue("quality");rawAfter=parameters.getRawParameterValue("after");rawBypass=parameters.getRawParameterValue("bypass");rawGain=parameters.getRawParameterValue("gain");
}
FLOWTONEAudioProcessor::~FLOWTONEAudioProcessor(){fileAnalysis.stop();analysis.stop();}
bool FLOWTONEAudioProcessor::isBusesLayoutSupported(const BusesLayout& b) const
{ return (b.getMainInputChannelSet()==juce::AudioChannelSet::mono() || b.getMainInputChannelSet()==juce::AudioChannelSet::stereo()) && b.getMainInputChannelSet()==b.getMainOutputChannelSet(); }
void FLOWTONEAudioProcessor::prepareToPlay(double rate,int)
{
    engine.prepare(rate,getTotalNumInputChannels());setLatencySamples(engine.latency());analysis.prepare(rate);
    waveInterval=juce::jmax(1,juce::roundToInt(rate*12/512));waveCount=0;wavePeak=0;historyWrite.store(0);
    for(auto& v:history)v.store(0);resetRequested.store(false);
}
void FLOWTONEAudioProcessor::releaseResources(){analysis.stop();inputPeak.store(0);}
void FLOWTONEAudioProcessor::reset(){resetRequested.store(true);}
void FLOWTONEAudioProcessor::processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer& midi){midi.clear();process(b,false);}
void FLOWTONEAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& b,juce::MidiBuffer& midi){midi.clear();process(b,true);}
void FLOWTONEAudioProcessor::process(juce::AudioBuffer<float>& buffer,bool forcedBypass)
{
    juce::ScopedNoDenormals noDenormals;
    if(resetRequested.exchange(false))engine.reset();
    const int channels=juce::jmin(getTotalNumInputChannels(),buffer.getNumChannels());
    for(int c=channels;c<buffer.getNumChannels();++c)buffer.clear(c,0,buffer.getNumSamples());
    if(channels<1)return;
    if(!fileSource.load())analysis.push(buffer);
    float maximum=0;
    for(int n=0;n<buffer.getNumSamples();++n)
    {
        float sample=0;
        for(int c=0;c<channels;++c){const float s=buffer.getSample(c,n);if(std::isfinite(s))sample=juce::jmax(sample,std::abs(s));}
        maximum=juce::jmax(maximum,sample);wavePeak=juce::jmax(wavePeak,sample);
        if(++waveCount>=waveInterval){const auto pos=historyWrite.load();history[pos%512].store(wavePeak);historyWrite.store(pos+1);waveCount=0;wavePeak=0;}
    }
    inputPeak.store(maximum);
    engine.process(buffer,rawShift->load(),rawFormants->load()>0.5f,rawQuality->load()>0.5f,rawAfter->load()>0.5f,forcedBypass||rawBypass->load()>0.5f,rawGain->load());
}
void FLOWTONEAudioProcessor::setValue(const char* id,float actual)
{
    if(auto* p=parameters.getParameter(id)){p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(actual));p->endChangeGesture();}
}
void FLOWTONEAudioProcessor::selectAnalysisSource(bool file)
{
    const auto previous=fileSource.exchange(file);
    if(previous&&!file)analysis.requestReset();
    if(previous!=file)updateHostDisplay(ChangeDetails().withNonParameterStateChanged(true));
}
void FLOWTONEAudioProcessor::loadAnalysisFile(const juce::File& file,std::shared_ptr<void> lease,std::shared_ptr<juce::InputStream> stream)
{fileAnalysis.load(file,std::move(lease),std::move(stream));fileSource.store(true);setValue("tempoOctave",0);updateHostDisplay(ChangeDetails().withNonParameterStateChanged(true));}
flowtone::AnalysisSnapshot FLOWTONEAudioProcessor::analysisSnapshot()const
{return fileSource.load()?fileAnalysis.snapshot().result:analysis.snapshot();}
void FLOWTONEAudioProcessor::resetAnalysis()
{if(fileSource.load())fileAnalysis.repeat();else analysis.requestReset();}
std::array<float,512> FLOWTONEAudioProcessor::waveform() const
{
    std::array<float,512> out {};const auto write=historyWrite.load();
    const auto count=juce::jmin(512u,write);
    for(unsigned i=0;i<count;++i)out[512-count+i]=history[(write-count+i)%512].load();
    return out;
}
void FLOWTONEAudioProcessor::selectProfile(int profile)
{
    const std::lock_guard<std::recursive_mutex> lock(stateMutex);
    static constexpr const char* ids[]={"shift","formants","quality","gain"};
    if(profile<0||profile>1||profile==currentProfile)return;
    for(size_t i=0;i<4;++i)profiles[static_cast<size_t>(currentProfile)][i]=value(ids[i]);
    currentProfile=profile;
    for(size_t i=0;i<4;++i)setValue(ids[i],profiles[static_cast<size_t>(currentProfile)][i]);
}
juce::String FLOWTONEAudioProcessor::resultKey() const
{
    const auto result=analysisSnapshot();if(result.key<0)return ru(u8"Недостаточно данных");
    const float shift=value("after")>0.5f && value("bypass")<0.5f ? value("shift") : 0;
    const int whole=static_cast<int>(shift),cents=juce::roundToInt((shift-whole)*100);
    auto name=flowtone::TrackAnalysis::keyName((result.key+whole+120)%12,result.minor);
    if(cents!=0)name+=(cents>0?" +":" ")+juce::String(cents)+ru(u8" центов");
    return name;
}
void FLOWTONEAudioProcessor::getStateInformation(juce::MemoryBlock& block)
{
    const std::lock_guard<std::recursive_mutex> lock(stateMutex);
    auto state=parameters.copyState();state.setProperty("version",2,nullptr);state.setProperty("profile",currentProfile.load(),nullptr);state.setProperty("fileSource",fileSource.load(),nullptr);
    // APVTS may contain custom children restored from an older project. Replace,
    // rather than append, the cached file analysis on every state save.
    for(auto old=state.getChildWithName("FILE_ANALYSIS");old.isValid();old=state.getChildWithName("FILE_ANALYSIS"))state.removeChild(old,nullptr);
    const auto file=fileAnalysis.snapshot();
    if(!file.name.isEmpty())
    {
        juce::ValueTree cache("FILE_ANALYSIS");
        cache.setProperty("name",file.name,nullptr);cache.setProperty("path",file.path,nullptr);
        cache.setProperty("available",file.available,nullptr);cache.setProperty("duration",file.duration,nullptr);
        cache.setProperty("rate",file.sampleRate,nullptr);cache.setProperty("channels",file.channels,nullptr);
        cache.setProperty("bpm",file.result.bpm,nullptr);cache.setProperty("key",file.result.key,nullptr);cache.setProperty("minor",file.result.minor,nullptr);
        juce::StringArray peaks;for(const auto peak:file.waveform)peaks.add(juce::String(peak,6));
        cache.setProperty("waveform",peaks.joinIntoString(","),nullptr);state.appendChild(cache,nullptr);
    }
    static constexpr const char* ids[]={"shift","formants","quality","gain"};
    for(size_t i=0;i<4;++i)profiles[static_cast<size_t>(currentProfile)][i]=value(ids[i]);
    for(int p=0;p<2;++p)for(int i=0;i<4;++i)state.setProperty("profile"+juce::String(p)+"value"+juce::String(i),profiles[static_cast<size_t>(p)][static_cast<size_t>(i)],nullptr);
    if(auto xml=state.createXml())copyXmlToBinary(*xml,block);
}
void FLOWTONEAudioProcessor::setStateInformation(const void* data,int bytes)
{
    const std::lock_guard<std::recursive_mutex> lock(stateMutex);
    auto xml=getXmlFromBinary(data,bytes);if(!xml || !xml->hasTagName("FLOWTONE"))return;
    auto state=juce::ValueTree::fromXml(*xml);const int version=static_cast<int>(state.getProperty("version",1));if(!state.isValid()||version<1||version>2)return;
    for(auto child:state){if(child.hasType("FILE_ANALYSIS"))continue;const auto id=child.getProperty("id").toString();if(parameters.getParameter(id)==nullptr||!std::isfinite(static_cast<float>(child.getProperty("value"))))return;}
    flowtone::FileAnalysisSnapshot cached;
    const auto cache=state.getChildWithName("FILE_ANALYSIS");
    if(cache.isValid())
    {
        cached.name=cache.getProperty("name").toString().substring(0,1024);cached.path=cache.getProperty("path").toString();
        cached.duration=static_cast<double>(cache.getProperty("duration"));cached.sampleRate=static_cast<double>(cache.getProperty("rate"));cached.channels=static_cast<int>(cache.getProperty("channels"));
        cached.result.bpm=static_cast<double>(cache.getProperty("bpm"));cached.result.key=static_cast<int>(cache.getProperty("key",-1));cached.result.minor=static_cast<bool>(cache.getProperty("minor"));
        if(!std::isfinite(cached.duration)||cached.duration<0||cached.duration>14400||!std::isfinite(cached.sampleRate)||cached.sampleRate<0||cached.sampleRate>384000||!std::isfinite(cached.result.bpm)||cached.result.bpm<0||cached.result.bpm>300||cached.result.key<-1||cached.result.key>11||cached.channels<0||cached.channels>2)return;
        const auto peaks=juce::StringArray::fromTokens(cache.getProperty("waveform").toString(),",","");
        if(peaks.size()!=512)return;
        for(size_t i=0;i<512;++i){const auto peak=peaks[static_cast<int>(i)].getFloatValue();if(!std::isfinite(peak)||peak<0)return;cached.waveform[i]=peak;}
        cached.available=static_cast<bool>(cache.getProperty("available"));cached.result.complete=cached.available;cached.result.seconds=cached.duration;
    }
    currentProfile=juce::jlimit(0,1,static_cast<int>(state.getProperty("profile",0)));
    for(int p=0;p<2;++p)for(int i=0;i<4;++i){const auto v=static_cast<float>(state.getProperty("profile"+juce::String(p)+"value"+juce::String(i),i==1||i==2?1:0));profiles[static_cast<size_t>(p)][static_cast<size_t>(i)]=std::isfinite(v)?v:0;}
    for(auto old=state.getChildWithName("FILE_ANALYSIS");old.isValid();old=state.getChildWithName("FILE_ANALYSIS"))state.removeChild(old,nullptr);
    parameters.replaceState(state);fileAnalysis.restore(std::move(cached));fileSource.store(static_cast<bool>(state.getProperty("fileSource",false)));resetRequested.store(true);analysis.requestReset();
}
juce::AudioProcessorEditor* FLOWTONEAudioProcessor::createEditor(){return new FLOWTONEAudioProcessorEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new FLOWTONEAudioProcessor();}
