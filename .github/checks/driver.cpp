#include "../../Source/PluginProcessor.h"
#include "../../Source/PluginEditor.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <new>
#include <time.h>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
static uint64_t threadCpuNanoseconds()
{
#if JUCE_WINDOWS
 FILETIME created,exited,kernel,user;
 if(!GetThreadTimes(GetCurrentThread(),&created,&exited,&kernel,&user))return 0;
 const uint64_t k=(static_cast<uint64_t>(kernel.dwHighDateTime)<<32)|kernel.dwLowDateTime;
 const uint64_t u=(static_cast<uint64_t>(user.dwHighDateTime)<<32)|user.dwLowDateTime;
 return (k+u)*100;
#else
 return clock_gettime_nsec_np(CLOCK_THREAD_CPUTIME_ID);
#endif
}
thread_local bool watching=false;
thread_local size_t allocations=0;
void* operator new(std::size_t n){if(watching)++allocations;if(auto* p=std::malloc(n))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete[](void* p) noexcept{std::free(p);}
static void require(bool value,const char* error){if(!value)throw std::runtime_error(error);}
static void load(const juce::File& f,juce::AudioBuffer<float>& audio,double& rate)
{
    juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(f));require(reader!=nullptr,"reader");
    require(reader->lengthInSamples<100000000&&reader->numChannels<=2,"input size");rate=reader->sampleRate;audio.setSize(static_cast<int>(reader->numChannels),static_cast<int>(reader->lengthInSamples));require(reader->read(&audio,0,audio.getNumSamples(),0,true,true),"read");
}
static void save(const juce::File& f,const juce::AudioBuffer<float>& audio,double rate)
{
    f.getParentDirectory().createDirectory();f.deleteFile();std::unique_ptr<juce::OutputStream> stream=f.createOutputStream();juce::WavAudioFormat format;auto writer=format.createWriterFor(stream,juce::AudioFormatWriterOptions().withSampleRate(rate).withNumChannels(audio.getNumChannels()).withBitsPerSample(24));require(writer!=nullptr,"writer");require(writer->writeFromAudioSampleBuffer(audio,0,audio.getNumSamples()),"write");
}
static void shot(FLOWTONEAudioProcessor& processor,const juce::File& file)
{
    auto editor=std::unique_ptr<juce::AudioProcessorEditor>(processor.createEditor());editor->setSize(1600,1000);juce::Timer::callPendingTimersSynchronously();auto picture=editor->createComponentSnapshot(editor->getLocalBounds(),true,1.0f);file.deleteFile();auto stream=file.createOutputStream();require(stream!=nullptr,"shot stream");require(juce::PNGImageFormat().writeImageToStream(picture,*stream),"screenshot");
}
int main(int argc,char** argv)
{
    try{
        juce::ScopedJuceInitialiser_GUI gui;require(argc>=2,"mode required");const juce::String mode=argv[1];
        if(mode=="retained-stream")
        {
            require(argc>=3,"input file required");
            const auto file=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("FLOWTONE-stream-"+juce::Uuid().toString()+".wav");
            require(juce::File(argv[2]).copyFileTo(file),"temporary source copy");
            FLOWTONEAudioProcessor p;
           #if JUCE_WINDOWS
            auto editor=std::make_unique<FLOWTONEAudioProcessorEditor>(p);
            editor->filesDropped({file.getFullPathName()},0,0);editor.reset();
           #else
            auto stream=std::shared_ptr<juce::InputStream>(file.createInputStream().release());require(stream!=nullptr,"open stream");
            p.loadAnalysisFile(file,{},std::move(stream));
           #endif
            require(file.deleteFile(),"delete temporary source while handle open");
            auto wait=[&]{const auto deadline=juce::Time::getMillisecondCounterHiRes()+10000;while(p.fileAnalysis.snapshot().busy&&juce::Time::getMillisecondCounterHiRes()<deadline)juce::Thread::sleep(1);require(!p.fileAnalysis.snapshot().busy,"stream timeout");};
            wait();auto first=p.fileAnalysis.snapshot();require(first.available&&std::abs(first.result.bpm-124)<.25,"deleted source analysis");
            p.resetAnalysis();wait();auto second=p.fileAnalysis.snapshot();require(second.available&&std::abs(second.result.bpm-first.result.bpm)<.001,"deleted source repeat");
            std::cout<<"Opened stream remains readable after source deletion; analysis and repeat PASS\n";return 0;
        }
        if(mode=="formats")
        {
            juce::AudioFormatManager formats;formats.registerBasicFormats();std::cout<<formats.getWildcardForAllFormats()<<"\n";
           #if JUCE_MAC
            for(auto kind:{juce::CoreAudioFormat::StreamKind::kNone,juce::CoreAudioFormat::StreamKind::kM4a})
            {
                juce::CoreAudioFormat format(kind);auto stream=juce::File(argv[2]).createInputStream();
                std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(stream.release(),true));
                std::cout<<"kind="<<static_cast<int>(kind)<<" reader="<<(reader!=nullptr);
                if(reader)std::cout<<" rate="<<reader->sampleRate<<" channels="<<reader->numChannels<<" frames="<<reader->lengthInSamples;
                std::cout<<"\n";
            }
           #endif
            return 0;
        }
        if(mode=="file-analyse"||mode=="file-lifecycle")
        {
            struct StateListener final:juce::AudioProcessorListener
            {
                std::atomic<unsigned> notifications{0};
                void audioProcessorParameterChanged(juce::AudioProcessor*,int,float)override{}
                void audioProcessorChanged(juce::AudioProcessor* p,const ChangeDetails& details)override
                {if(details.nonParameterStateChanged){juce::MemoryBlock state;p->getStateInformation(state);++notifications;}}
            } listener;
            FLOWTONEAudioProcessor processor;
            processor.addListener(&listener);
            auto wait=[&]
            {
                const auto deadline=juce::Time::getMillisecondCounterHiRes()+60000;
                while(processor.fileAnalysis.snapshot().busy&&juce::Time::getMillisecondCounterHiRes()<deadline)juce::Thread::sleep(2);
                require(!processor.fileAnalysis.snapshot().busy,"file analysis timeout");
            };
            const auto start=juce::Time::getMillisecondCounterHiRes();
            processor.loadAnalysisFile(juce::File(argv[2]));
            if(mode=="file-lifecycle")
            {
                require(argc>=4,"second file required");
                for(int i=0;i<15;++i)processor.loadAnalysisFile(juce::File(argv[i%2?2:3]));
                processor.loadAnalysisFile(juce::File(argv[3]));
            }
            wait();auto file=processor.fileAnalysis.snapshot();
            const auto notifyDeadline=juce::Time::getMillisecondCounterHiRes()+2000;
            while(listener.notifications.load()<2&&juce::Time::getMillisecondCounterHiRes()<notifyDeadline)juce::Thread::sleep(1);
            require(listener.notifications.load()>=2,"host not notified of completed file state");
            require(file.available&&file.error.isEmpty(),file.error.toRawUTF8());require(processor.usesFileAnalysis(),"file source not selected");
            require(processor.analysis.snapshot().seconds==0,"file analysis unexpectedly requires live audio");
            processor.setValue("shift",.5f);juce::MemoryBlock state;processor.getStateInformation(state);
            FLOWTONEAudioProcessor restored;restored.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
            auto saved=restored.fileAnalysis.snapshot();require(restored.usesFileAnalysis()&&saved.available&&!saved.busy,"cached state unavailable");
            require(std::abs(saved.result.bpm-file.result.bpm)<1e-6&&saved.result.key==file.result.key&&saved.name==file.name,"cached result mismatch");
            for(size_t i=0;i<512;++i)require(std::abs(saved.waveform[i]-file.waveform[i])<1e-5,"cached waveform mismatch");
            // prepare/release at an unrelated host rate must not clear the file.
            restored.setRateAndBufferSizeDetails(44100,256);restored.prepareToPlay(44100,256);restored.releaseResources();
            require(restored.fileAnalysis.snapshot().available,"prepare cleared file result");
            restored.selectAnalysisSource(false);require(!restored.usesFileAnalysis(),"online source switch");
            restored.selectAnalysisSource(true);require(restored.analysisSnapshot().bpm==saved.result.bpm,"file result lost on switch");
            // Exercise the existing live path after switching back. The cached
            // file must not leak into the online estimate or the rendered audio.
            if(mode=="file-lifecycle")
            {
                juce::AudioBuffer<float> source;double rate;load(juce::File(argv[2]),source,rate);
                processor.selectAnalysisSource(false);processor.setRateAndBufferSizeDetails(rate,256);processor.prepareToPlay(rate,256);
                FLOWTONEAudioProcessor reference;reference.setRateAndBufferSizeDetails(rate,256);reference.prepareToPlay(rate,256);reference.setValue("shift",.5f);
                juce::MidiBuffer midi;float difference=0;
                for(int offset=0;offset<source.getNumSamples();offset+=256)
                {
                    const int n=juce::jmin(256,source.getNumSamples()-offset);juce::AudioBuffer<float> block(source.getNumChannels(),n),expected(source.getNumChannels(),n);
                    for(int c=0;c<source.getNumChannels();++c){block.copyFrom(c,0,source,c,offset,n);expected.copyFrom(c,0,source,c,offset,n);}
                    processor.processBlock(block,midi);reference.processBlock(expected,midi);
                    for(int c=0;c<block.getNumChannels();++c)for(int i=0;i<n;++i)difference=juce::jmax(difference,std::abs(block.getSample(c,i)-expected.getSample(c,i)));
                    while(processor.analysis.pendingSamples()>32000||reference.analysis.pendingSamples()>32000)juce::Thread::sleep(1);
                }
                while(processor.analysis.pendingSamples()>0)juce::Thread::sleep(1);juce::Thread::sleep(10);
                require(processor.analysisSnapshot().seconds>20,"online analysis not resumed");require(difference<1e-6f,"loaded file changed insert audio");
                processor.selectAnalysisSource(true);require(processor.analysisSnapshot().bpm==file.result.bpm,"online playback overwrote file result");
                processor.releaseResources();reference.releaseResources();
            }
            if(mode=="file-lifecycle")
            {
                require(file.name==juce::File(argv[3]).getFileName(),"stale job published");
                processor.resetAnalysis();wait();require(processor.fileAnalysis.snapshot().available,"repeat failed");
                processor.loadAnalysisFile(juce::File(argv[3]).getSiblingFile("missing-flowtone-file.wav"));wait();
                require(!processor.fileAnalysis.snapshot().available&&processor.fileAnalysis.snapshot().error.isNotEmpty(),"missing file not reported");
                processor.loadAnalysisFile(juce::File(argv[2]));wait();require(processor.fileAnalysis.snapshot().available,"recovery failed");
            }
            if(mode=="file-analyse"&&argc>3)shot(processor,juce::File(argv[3]));
            std::cout<<"{\"bpm\":"<<file.result.bpm<<",\"key\":"<<file.result.key<<",\"minor\":"<<(file.result.minor?"true":"false")<<",\"seconds\":"<<file.result.seconds<<",\"elapsedMs\":"<<juce::Time::getMillisecondCounterHiRes()-start<<",\"state\":\"PASS\",\"switch\":\"PASS\"}\n";
            return 0;
        }
        if(mode=="analyse")
        {
            juce::AudioBuffer<float> audio;double rate;load(juce::File(argv[2]),audio,rate);flowtone::TrackAnalysis analysis;analysis.prepare(rate);
            for(int offset=0;offset<audio.getNumSamples();offset+=512){const int n=juce::jmin(512,audio.getNumSamples()-offset);juce::AudioBuffer<float> part(audio.getArrayOfWritePointers(),audio.getNumChannels(),offset,n);analysis.push(part);while(analysis.pendingSamples()>32000)juce::Thread::sleep(1);}
            while(analysis.pendingSamples()>0)juce::Thread::sleep(1);juce::Thread::sleep(10);const auto a=analysis.snapshot();
            std::cout<<"{\"bpm\":"<<a.bpm<<",\"key\":"<<a.key<<",\"minor\":"<<(a.minor?"true":"false")<<",\"seconds\":"<<a.seconds<<",\"complete\":"<<(a.complete?"true":"false")<<",\"chroma\":[";for(int i=0;i<12;++i)std::cout<<(i?",":"")<<a.pitchClasses[static_cast<size_t>(i)];std::cout<<"],\"bass\":[";for(int i=0;i<12;++i)std::cout<<(i?",":"")<<a.bassPitchClasses[static_cast<size_t>(i)];std::cout<<"]}\n";analysis.stop();return 0;
        }
        if(mode=="empty-shot"){FLOWTONEAudioProcessor p;shot(p,juce::File(argv[2]));return 0;}
        if(mode=="state")
        {
            FLOWTONEAudioProcessor p;p.setValue("shift",2.5f);p.setValue("quality",0);p.setValue("gain",-3);p.selectProfile(1);p.setValue("shift",-4.5f);p.setValue("formants",0);juce::MemoryBlock state;p.getStateInformation(state);FLOWTONEAudioProcessor restored;restored.setStateInformation(state.getData(),static_cast<int>(state.getSize()));require(std::abs(restored.value("shift")+4.5f)<1e-4f&&restored.value("formants")==0&&restored.activeProfile()==1,"state current");restored.selectProfile(0);require(std::abs(restored.value("shift")-2.5f)<1e-4f&&restored.value("quality")==0&&std::abs(restored.value("gain")+3)<1e-4f,"state profile A");std::cout<<"State and A/B PASS\n";return 0;
        }
        require(mode=="render"&&argc>=7,"render arguments");juce::AudioBuffer<float> source;double rate;load(juce::File(argv[2]),source,rate);
        FLOWTONEAudioProcessor p;auto layout=p.getBusesLayout();layout.inputBuses.set(0,juce::AudioChannelSet::canonicalChannelSet(source.getNumChannels()));layout.outputBuses.set(0,layout.inputBuses[0]);require(p.setBusesLayout(layout),"layout");
        const int blockSize=std::getenv("FLOWTONE_TEST_BLOCK")?std::stoi(std::getenv("FLOWTONE_TEST_BLOCK")):256;
        p.setRateAndBufferSizeDetails(rate,blockSize);p.setNonRealtime(true);p.setValue("shift",std::stof(argv[4]));p.setValue("after",std::stof(argv[5]));p.setValue("formants",std::stof(argv[6]));if(argc>7)p.setValue("quality",std::stof(argv[7]));
        p.prepareToPlay(rate,blockSize);const int latency=p.getLatencySamples();juce::AudioBuffer<float> output(source.getNumChannels(),source.getNumSamples());output.clear();juce::MidiBuffer midi;
        double longest=0;size_t beforeAllocations=0;std::vector<double> wallTimes,cpuTimes;wallTimes.reserve(static_cast<size_t>((source.getNumSamples()+latency)/blockSize+2));cpuTimes.reserve(wallTimes.capacity());
        for(int offset=0;offset<source.getNumSamples()+latency;offset+=blockSize)
        {
            const int n=juce::jmin(blockSize,source.getNumSamples()+latency-offset);juce::AudioBuffer<float> block(source.getNumChannels(),n);block.clear();
            for(int c=0;c<source.getNumChannels();++c)if(offset<source.getNumSamples())block.copyFrom(c,0,source,c,offset,juce::jmin(n,source.getNumSamples()-offset));
            if(argc>8&&std::string(argv[8])=="dynamic"&&offset>source.getNumSamples()/2)p.setValue("shift",-0.5f);
            const double start=juce::Time::getMillisecondCounterHiRes();const auto cpuStart=threadCpuNanoseconds();watching=true;p.processBlock(block,midi);watching=false;const auto cpuEnd=threadCpuNanoseconds();const double wall=juce::Time::getMillisecondCounterHiRes()-start;longest=juce::jmax(longest,wall);wallTimes.push_back(wall);cpuTimes.push_back((cpuEnd-cpuStart)*1e-6);beforeAllocations=allocations;
            for(int i=0;i<n;++i){const int target=offset+i-latency;if(target>=0&&target<output.getNumSamples())for(int c=0;c<source.getNumChannels();++c)output.setSample(c,target,block.getSample(c,i));}
            while(p.analysis.pendingSamples()>32000)juce::Thread::sleep(1);
        }
        while(p.analysis.pendingSamples()>0)juce::Thread::sleep(1);juce::Thread::sleep(10);const auto a=p.analysis.snapshot();
        save(juce::File(argv[3]),output,rate);if(argc>9)shot(p,juce::File(argv[9]));
        std::sort(wallTimes.begin(),wallTimes.end());std::sort(cpuTimes.begin(),cpuTimes.end());std::cout<<"p99Ms="<<wallTimes[wallTimes.size()*99/100]<<" maxCpuMs="<<cpuTimes.back()<<" ";
        std::cout<<"latency="<<latency<<" maxBlockMs="<<longest<<" allocations="<<beforeAllocations<<" bpm="<<a.bpm<<" key="<<a.key<<" minor="<<a.minor<<" result="<<p.resultKey()<<"\n";
        require(p.getSampleRate()==rate,"host rate changed");p.releaseResources();return beforeAllocations==0?0:3;
    }catch(const std::exception& e){watching=false;std::cerr<<e.what()<<"\n";return 1;}
}
