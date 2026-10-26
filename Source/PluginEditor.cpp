#include "PluginEditor.h"
#include <cmath>
namespace
{
const juce::Colour green(0xff5aff91), white(0xffeef4f4), muted(0xffa6b2b7), edge(0xff29413d);
juce::String ru(const char* s){return juce::String::fromUTF8(s);}
juce::String decimal(double value,int places=1){return juce::String(value,places).replace(".",",");}
juce::String clockTime(double seconds){const int value=juce::jmax(0,static_cast<int>(seconds));return juce::String(value/60)+":"+juce::String(value%60).paddedLeft('0',2);}
void font(juce::Graphics& g,float size,bool bold=false){g.setFont(juce::Font(juce::FontOptions("Arial",size,bold?juce::Font::bold:juce::Font::plain)));}
void text(juce::Graphics& g,const juce::String& value,float size,juce::Rectangle<float> bounds,juce::Colour colour=white,bool bold=false,juce::Justification align=juce::Justification::centredLeft)
{font(g,size,bold);g.setColour(colour);g.drawFittedText(value,bounds.toNearestInt(),align,1,0.8f);}
void panel(juce::Graphics& g,juce::Rectangle<float> r,float radius=12)
{
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff101b1b),r.getX(),r.getY(),juce::Colour(0xff060d0f),r.getRight(),r.getBottom(),false));g.fillRoundedRectangle(r,radius);
    g.setColour(edge);g.drawRoundedRectangle(r,radius,1);
}
void note(juce::Graphics& g,float x,float y)
{
    g.setColour(green);g.fillEllipse(x,y+22,16,11);g.fillEllipse(x+22,y+15,16,11);
    g.fillRect(x+12,y,4.0f,28.0f);g.fillRect(x+34,y-6,4.0f,28.0f);
    juce::Path p;p.startNewSubPath(x+12,y);p.lineTo(x+38,y-6);p.lineTo(x+38,y+2);p.lineTo(x+12,y+8);p.closeSubPath();g.fillPath(p);
}
}
void ToneLookAndFeel::drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour&,bool over,bool pressed)
{
    auto r=b.getLocalBounds().toFloat().reduced(1);const bool active=b.getToggleState();
    if(active){g.setGradientFill(juce::ColourGradient(green,r.getX(),r.getY(),juce::Colour(0xff38d970),r.getRight(),r.getBottom(),false));g.fillRoundedRectangle(r,8);}
    else{panel(g,r,8);if(over||pressed){g.setColour(green.withAlpha(pressed?0.16f:0.07f));g.fillRoundedRectangle(r,8);}}
    g.setColour(active||over?green:edge);g.drawRoundedRectangle(r,8,1);
}
void ToneLookAndFeel::drawButtonText(juce::Graphics& g,juce::TextButton& b,bool,bool)
{text(g,b.getButtonText(),juce::jlimit(12.0f,20.0f,b.getHeight()*0.36f),b.getLocalBounds().toFloat().reduced(5),b.getToggleState()?juce::Colour(0xff03110c):white,b.getToggleState(),juce::Justification::centred);}
void ToneLookAndFeel::drawToggleButton(juce::Graphics& g,juce::ToggleButton& b,bool,bool)
{
    const auto r=b.getLocalBounds().toFloat();text(g,b.getButtonText(),r.getHeight()*0.45f,r.withTrimmedRight(85),white);
    auto toggle=juce::Rectangle<float>(r.getRight()-76,4,74,r.getHeight()-8);const auto on=b.getToggleState();
    g.setColour(juce::Colour(0xff071311));g.fillRoundedRectangle(toggle,toggle.getHeight()/2);g.setColour(on?green:edge);g.drawRoundedRectangle(toggle,toggle.getHeight()/2,1.4f);
    const float size=toggle.getHeight()-10;g.setColour(on?green:muted);g.fillEllipse(on?toggle.getRight()-size-5:toggle.getX()+5,toggle.getY()+5,size,size);
}
void ToneLookAndFeel::drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float value,float start,float end,juce::Slider&)
{
    auto r=juce::Rectangle<float>(static_cast<float>(x),static_cast<float>(y),static_cast<float>(w),static_cast<float>(h)).reduced(18);
    const float radius=juce::jmin(r.getWidth(),r.getHeight())*0.5f;const auto c=r.getCentre();const float angle=start+value*(end-start);
    juce::Path base,lit;base.addCentredArc(c.x,c.y,radius,radius,0,start,end,true);lit.addCentredArc(c.x,c.y,radius,radius,0,start,angle,true);
    g.setColour(juce::Colour(0xff273438));g.strokePath(base,juce::PathStrokeType(9));g.setColour(green.withAlpha(0.13f));g.strokePath(lit,juce::PathStrokeType(24));g.setColour(green);g.strokePath(lit,juce::PathStrokeType(8));
    for(int i=0;i<=48;++i){const float a=start+(end-start)*i/48;const auto p=c+juce::Point<float>(std::sin(a),-std::cos(a))*(radius+13);const auto q=c+juce::Point<float>(std::sin(a),-std::cos(a))*(radius+(i%4==0?22:17));g.setColour(muted.withAlpha(i%4==0?0.8f:0.4f));g.drawLine({p,q},1);}
    auto knob=juce::Rectangle<float>(c.x-radius*0.83f,c.y-radius*0.83f,radius*1.66f,radius*1.66f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff667471),knob.getX(),knob.getY(),juce::Colour(0xff050a0d),knob.getRight(),knob.getBottom(),false));g.fillEllipse(knob);
    for(int i=0;i<120;++i){const float a=juce::MathConstants<float>::twoPi*i/120;const float b=a+juce::MathConstants<float>::twoPi/120+0.002f;const float shine=0.07f+0.58f*std::pow(std::abs(std::cos(a-0.5f)),12.0f);g.setColour(juce::Colour(0xff060c0e).interpolatedWith(juce::Colour(0xffb4c0b9),shine));juce::Path p;p.startNewSubPath(c);p.lineTo(c.x+std::cos(a)*radius*0.79f,c.y+std::sin(a)*radius*0.79f);p.lineTo(c.x+std::cos(b)*radius*0.79f,c.y+std::sin(b)*radius*0.79f);p.closeSubPath();g.fillPath(p);}
    g.setColour(juce::Colour(0xff52635f));g.drawEllipse(knob,1.5f);g.setColour(juce::Colour(0xff05090a));g.drawEllipse(knob.reduced(5),3);
    const auto dot=c+juce::Point<float>(std::sin(angle),-std::cos(angle))*radius*0.76f;g.setColour(green.withAlpha(0.15f));g.fillEllipse(dot.x-11,dot.y-11,22,22);g.setColour(green);g.fillEllipse(dot.x-6,dot.y-6,12,12);
}
FLOWTONEAudioProcessorEditor::FLOWTONEAudioProcessorEditor(FLOWTONEAudioProcessor& p):AudioProcessorEditor(p),processor(p)
{
    setLookAndFeel(&look);
    for(auto* b:{&down,&up,&resetShift,&half,&doubleTempo,&repeat,&before,&after,&bypass,&profileA,&profileB,&settings,&previous,&next,&liveSource,&fileSource,&openFile})addAndMakeVisible(b);
    liveSource.setButtonText(ru(u8"Онлайн"));fileSource.setButtonText(ru(u8"Файл"));openFile.setButtonText(ru(u8"Выбрать файл"));
    liveSource.onClick=[this]{++dropTicket;receivingFile=false;processor.selectAnalysisSource(false);processor.setValue("tempoOctave",0);};
    fileSource.onClick=[this]{processor.selectAnalysisSource(true);};openFile.onClick=[this]{chooseFile();};
    liveSource.setTooltip(ru(u8"Определять BPM и тональность по входу дорожки при воспроизведении проекта."));
    fileSource.setTooltip(ru(u8"Анализ всего загруженного файла без воспроизведения. Перетащите аудиофайл или регион на осциллограмму."));
    addAndMakeVisible(nativeDrop);nativeDrop.canDrop=[] {return true;};
    nativeDrop.hover=[this](bool hover){dropHover=hover;repaint();};
    nativeDrop.beginDrop=[this]() -> flowtone::NativeRegionDrop::Completion
    {
        const auto ticket=++dropTicket;receivingFile=true;processor.selectAnalysisSource(true);repaint();
        const juce::Component::SafePointer<FLOWTONEAudioProcessorEditor> safe(this);
        return [safe,ticket](flowtone::RegionDrop drop)
        {
            if(!safe||safe->dropTicket!=ticket)return;
            safe->receivingFile=false;
            if(drop.error.isNotEmpty())juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,ru(u8"Не удалось принять регион"),drop.error,ru(u8"Понятно"),safe.getComponent());
            else safe->processor.loadAnalysisFile(drop.file,std::move(drop.lease),std::move(drop.openedStream));
            safe->repaint();
        };
    };
    for(auto* c:std::array<juce::Component*,5>{&shift,&gain,&formants,&quality,&presets})addAndMakeVisible(c);
    down.setButtonText(ru(u8"−"));up.setButtonText("+");resetShift.setButtonText(ru(u8"Сброс"));half.setButtonText(ru(u8"½"));doubleTempo.setButtonText(ru(u8"×2"));repeat.setButtonText(ru(u8"Повторить анализ"));before.setButtonText(ru(u8"ДО"));after.setButtonText(ru(u8"ПОСЛЕ"));bypass.setButtonText(ru(u8"ОБХОД"));profileA.setButtonText("A");profileB.setButtonText("B");settings.setButtonText(ru(u8"⚙"));previous.setButtonText(ru(u8"‹"));next.setButtonText(ru(u8"›"));
    shift.setSliderStyle(juce::Slider::RotaryVerticalDrag);shift.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);shift.setRotaryParameters(juce::MathConstants<float>::pi*1.25f,juce::MathConstants<float>::pi*2.75f,true);shift.setDoubleClickReturnValue(true,0);
    gain.setSliderStyle(juce::Slider::LinearHorizontal);gain.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);gain.setDoubleClickReturnValue(true,0);gain.setColour(juce::Slider::trackColourId,green);gain.setColour(juce::Slider::thumbColourId,green);gain.setColour(juce::Slider::backgroundColourId,edge);
    formants.setButtonText(ru(u8"Сохранять тембр"));bypass.setClickingTogglesState(true);
    quality.addItem(ru(u8"Стандартное"),1);quality.addItem(ru(u8"Высокое"),2);
    for(auto* c:{&quality,&presets}){c->setColour(juce::ComboBox::backgroundColourId,juce::Colour(0xff0b1416));c->setColour(juce::ComboBox::outlineColourId,edge);c->setColour(juce::ComboBox::textColourId,white);}
    presets.addItem(ru(u8"По умолчанию"),1);presets.addItem(ru(u8"На полтона выше"),2);presets.addItem(ru(u8"На полтона ниже"),3);presets.addItem(ru(u8"На четверть тона выше"),4);presets.addItem(ru(u8"На четверть тона ниже"),5);presets.setSelectedId(1,juce::dontSendNotification);
    presets.onChange=[this]{applyPreset(presets.getSelectedId());};previous.onClick=[this]{presets.setSelectedId((presets.getSelectedId()+3)%5+1);};next.onClick=[this]{presets.setSelectedId(presets.getSelectedId()%5+1);};
    quality.onChange=[this]{processor.setValue("quality",quality.getSelectedId()==2?1:0);};
    down.onClick=[this]{processor.setValue("shift",processor.value("shift")-0.5f);};up.onClick=[this]{processor.setValue("shift",processor.value("shift")+0.5f);};resetShift.onClick=[this]{processor.setValue("shift",0);};
    half.onClick=[this]{processor.setValue("tempoOctave",processor.value("tempoOctave")-1);};doubleTempo.onClick=[this]{processor.setValue("tempoOctave",processor.value("tempoOctave")+1);};
    repeat.onClick=[this]{processor.resetAnalysis();processor.setValue("tempoOctave",0);};before.onClick=[this]{processor.setValue("after",0);};after.onClick=[this]{processor.setValue("after",1);};
    profileA.onClick=[this]{processor.selectProfile(0);};profileB.onClick=[this]{processor.selectProfile(1);};settings.onClick=[this]{showHelp();};
    shiftAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters,"shift",shift);gainAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters,"gain",gain);formantAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters,"formants",formants);bypassAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters,"bypass",bypass);
    shift.setTooltip(ru(u8"Сдвиг высоты звука дорожки. Шаг: четверть тона = 50 центов = 0,5 полутона. Двойной клик — сброс. Темп и длительность сохраняются."));
    half.setTooltip(ru(u8"Уменьшить определённый BPM вдвое. Скорость дорожки не меняется."));doubleTempo.setTooltip(ru(u8"Увеличить определённый BPM вдвое. Скорость дорожки не меняется."));
    formants.setTooltip(ru(u8"Компенсировать сдвиг формант, чтобы сохранить характер голоса. Сравните оба положения на полном миксе."));quality.setTooltip(ru(u8"Высокое качество использует более длинное спектральное окно. Задержка компенсируется хостом и остаётся постоянной при переключении."));
    repeat.setTooltip(ru(u8"Файл: заново прочитать весь файл. Онлайн: сбросить анализ и слушать дорожку при воспроизведении."));
    before.setTooltip(ru(u8"Исходный звук дорожки с компенсацией задержки."));after.setTooltip(ru(u8"Звук дорожки с выбранным сдвигом тона. Обработка попадает в экспорт проекта."));
    setResizable(true,true);setResizeLimits(1000,625,1600,1000);getConstrainer()->setFixedAspectRatio(1.6);
    int width=1280;if(auto* d=juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())width=juce::jlimit(1000,1280,d->userArea.getWidth()-100);
    setSize(width,width*5/8);startTimerHz(24);timerCallback();
}
FLOWTONEAudioProcessorEditor::~FLOWTONEAudioProcessorEditor(){stopTimer();setLookAndFeel(nullptr);}
juce::Rectangle<int> FLOWTONEAudioProcessorEditor::area(int x,int y,int w,int h)const
{const float s=getWidth()/1600.0f;return{juce::roundToInt(x*s),juce::roundToInt(y*s),juce::roundToInt(w*s),juce::roundToInt(h*s)};}
void FLOWTONEAudioProcessorEditor::resized()
{
    presets.setBounds(area(582,46,250,40));previous.setBounds(area(538,46,40,40));next.setBounds(area(836,46,40,40));profileA.setBounds(area(910,46,52,40));profileB.setBounds(area(962,46,52,40));bypass.setBounds(area(1110,46,104,40));settings.setBounds(area(1523,44,45,44));
    liveSource.setBounds(area(388,112,148,32));fileSource.setBounds(area(542,112,118,32));openFile.setBounds(area(674,112,190,32));nativeDrop.setBounds(area(44,148,1512,98));
    shift.setBounds(area(104,578,300,270));down.setBounds(area(794,575,80,57));up.setBounds(area(887,575,80,57));resetShift.setBounds(area(794,646,174,50));
    half.setBounds(area(602,418,78,50));doubleTempo.setBounds(area(690,418,78,50));formants.setBounds(area(1053,580,498,47));quality.setBounds(area(1224,683,331,49));gain.setBounds(area(1053,823,498,42));repeat.setBounds(area(45,914,312,50));before.setBounds(area(1240,917,102,46));after.setBounds(area(1342,917,108,46));
}
void FLOWTONEAudioProcessorEditor::paint(juce::Graphics& graphics)
{
    juce::Graphics::ScopedSaveState saved(graphics);graphics.addTransform(juce::AffineTransform::scale(getWidth()/1600.0f));auto& g=graphics;
    g.fillAll(juce::Colour(0xff050c0e));g.setGradientFill(juce::ColourGradient(juce::Colour(0xff0d1d1b),0,0,juce::Colour(0xff03080a),1600,1000,false));g.fillAll();g.setColour(edge);g.drawRoundedRectangle({8,20,1584,964},12,1);
    text(g,"FLOW",54,{38,31,181,48},white,true);text(g,"TONE",54,{219,31,185,48},green,true);text(g,ru(u8"ТЕМП  ·  ТОНАЛЬНОСТЬ  ·  СДВИГ ТОНА"),12,{40,78,420,20},muted);
    text(g,"F L O W M I N N E R S",18,{1290,42,250,27},white);text(g,ru(u8"МУЗЫКАЛЬНЫЕ ИНСТРУМЕНТЫ"),9,{1320,73,210,16},muted);
    panel(g,{22,104,1556,190});panel(g,{22,304,772,183});panel(g,{805,304,773,183});panel(g,{22,497,996,391});panel(g,{1028,497,550,391});panel(g,{22,900,1556,73});
    const bool file=processor.usesFileAnalysis();const auto loaded=processor.fileAnalysis.snapshot();const auto a=file?loaded.result:processor.analysis.snapshot();const bool signal=processor.inputLevel()>0.0001f;
    text(g,file?ru(u8"АНАЛИЗ ФАЙЛА"):ru(u8"СИГНАЛ С ДОРОЖКИ"),19,{44,114,280,30},white,true);g.setColour((file?loaded.available:signal)?green:edge);g.fillEllipse(336,123,13,13);
    const auto status=file?(receivingFile?ru(u8"Получаю аудиофайл…"):loaded.busy?ru(u8"Анализ всего файла · ")+juce::String(juce::roundToInt(loaded.progress*100))+"%":loaded.error.isNotEmpty()?loaded.error:loaded.available?ru(u8"Анализ файла завершён"):ru(u8"Перетащите регион или выберите файл")):
        a.complete?ru(u8"Анализ завершён"):a.seconds>=12?ru(u8"Слушаю · ")+decimal(a.seconds)+ru(u8" с"):signal||a.seconds>0?ru(u8"Анализирую · ")+decimal(a.seconds)+ru(u8" с"):ru(u8"Включите воспроизведение проекта");
    text(g,status,14,{880,116,675,28},(file?loaded.available:a.complete)?green:muted,false,juce::Justification::centredRight);
    const auto waveform=file?loaded.waveform:processor.waveform();const float left=46,right=1554,middle=198,amplitude=45;
    g.setColour(juce::Colour(0xff172629));for(int i=0;i<=48;++i){const float x=left+(right-left)*i/48;g.drawLine(x,146,x,244,1);}g.setColour(juce::Colour(0xff2e403e));g.drawLine(left,middle,right,middle,1);
    for(size_t i=0;i<waveform.size();++i){const float x=left+(right-left)*static_cast<float>(i)/511;const float h=amplitude*std::sqrt(juce::jlimit(0.0f,1.0f,waveform[i]));g.setColour(i>480?green:juce::Colour(0xffa8b3b9));g.drawLine(x,middle-h,x,middle+h,2.0f);}
    for(int i=0;i<=6;++i)text(g,clockTime((file?loaded.duration:12)*i/6),11,{left+(right-left)*i/6-10,249,65,15},muted);
    text(g,file?(loaded.name.isEmpty()?ru(u8"Весь файл · без воспроизведения"):loaded.name+" · "+clockTime(loaded.duration)+ru(u8" · весь файл")):ru(u8"Последние 12 секунд · онлайн"),13,{45,270,1000,20},muted);
    text(g,ru(u8"Перетащите аудиофайл или регион сюда"),13,{1080,270,475,20},muted,false,juce::Justification::centredRight);
    if(dropHover){g.setColour(green.withAlpha(0.10f));g.fillRoundedRectangle({38,148,1524,96},8);g.setColour(green);g.drawRoundedRectangle({38,148,1524,96},8,2);text(g,ru(u8"Отпустите файл — анализ начнётся автоматически"),22,{260,175,1080,45},green,true,juce::Justification::centred);}
    if(file&&loaded.busy){g.setColour(green);g.fillRect(46.0f,241.0f,1508*loaded.progress,3.0f);}
    juce::Path pulse;pulse.startNewSubPath(48,341);pulse.lineTo(56,341);pulse.lineTo(62,330);pulse.lineTo(68,352);pulse.lineTo(75,323);pulse.lineTo(82,341);pulse.lineTo(90,341);g.setColour(green);g.strokePath(pulse,juce::PathStrokeType(3));
    text(g,ru(u8"ТЕМП"),23,{100,322,200,31},white,true);const double bpm=a.bpm*std::pow(2.0,processor.value("tempoOctave"));
    text(g,bpm>0?decimal(bpm):ru(u8"—"),94,{87,350,300,90},white,true);text(g,"BPM",30,{388,382,95,43},muted);
    text(g,bpm>0?(file?ru(u8"Определён по файлу целиком"):ru(u8"Определён по входу дорожки")):ru(u8"Нужен ритмичный фрагмент"),16,{91,447,480,25},muted);
    note(g,837,326);text(g,ru(u8"ТОНАЛЬНОСТЬ"),23,{891,322,460,32},white,true);
    text(g,a.key>=0?flowtone::TrackAnalysis::keyName(a.key,a.minor):ru(u8"—"),68,{891,351,640,80},white,true);
    g.setColour(a.key>=0?green:edge);g.fillEllipse(896,430,14,14);
    text(g,a.key>=0?ru(u8"Анализ завершён"):file?(loaded.busy?ru(u8"Анализирую файл…"):ru(u8"Недостаточно данных")):a.seconds<12?ru(u8"Слушаю музыкальный фрагмент…"):ru(u8"Недостаточно данных"),17,{924,422,600,31},a.key>=0?green:muted);
    text(g,ru(u8"↕"),32,{48,510,35,38},green,true);text(g,ru(u8"ИЗМЕНЕНИЕ ТОНА"),23,{98,515,480,35},white,true);
    text(g,"0",20,{236,555,30,25},white,false,juce::Justification::centred);text(g,ru(u8"−12"),20,{92,810,70,30},white);text(g,"+12",20,{362,810,70,30},white);text(g,ru(u8"ПОЛУТОНА"),13,{166,847,185,27},muted,false,juce::Justification::centred);
    const auto v=processor.value("shift");text(g,(v>0?"+":"")+decimal(v),96,{480,557,300,104},green,true);text(g,ru(u8"полутона"),35,{570,669,205,41},white,true);
    text(g,ru(u8"Шаг: ¼ тона · 50 центов"),18,{495,719,460,30},muted);g.setColour(edge);g.drawLine(485,757,990,757,1);text(g,ru(u8"ПОСЛЕ СДВИГА"),13,{495,767,420,26},white,true);
    panel(g,{485,796,505,80},9);text(g,processor.resultKey(),29,{509,802,463,43},a.key>=0?green:muted,true);
    text(g,bpm>0?ru(u8"Темп остаётся ")+decimal(bpm)+" BPM":ru(u8"Темп и длительность сохраняются"),15,{509,846,455,25},muted);
    text(g,ru(u8"⚙"),34,{1053,512,42,40},green);text(g,ru(u8"ОБРАБОТКА"),23,{1112,516,400,35},white,true);text(g,ru(u8"Бережно к голосу"),15,{1053,623,400,26},muted);text(g,ru(u8"Качество"),20,{1053,688,170,32});
    g.setColour(edge);g.drawLine(1053,767,1553,767,1);text(g,ru(u8"Громкость выхода"),19,{1053,791,330,35});text(g,decimal(processor.value("gain"))+ru(u8" дБ"),19,{1410,791,143,35},white,false,juce::Justification::centredRight);
    text(g,ru(u8"Сдвиг тона применяется к звуку дорожки"),12,{570,927,580,25},muted,false,juce::Justification::centred);
}
void FLOWTONEAudioProcessorEditor::timerCallback()
{
    const float currentShift=processor.value("shift");int preset=0;
    if(std::abs(processor.value("gain"))<0.001f&&processor.value("formants")>0.5f&&processor.value("quality")>0.5f)
    {const float values[]={0,1,-1,0.5f,-0.5f};for(int i=0;i<5;++i)if(std::abs(currentShift-values[i])<0.001f)preset=i+1;}
    if(preset>0)presets.setSelectedId(preset,juce::dontSendNotification);else{presets.setSelectedId(0,juce::dontSendNotification);presets.setText(ru(u8"Свои настройки"),juce::dontSendNotification);}
    const auto active=processor.value("after")>0.5f;before.setToggleState(!active,juce::dontSendNotification);after.setToggleState(active,juce::dontSendNotification);
    profileA.setToggleState(processor.activeProfile()==0,juce::dontSendNotification);profileB.setToggleState(processor.activeProfile()==1,juce::dontSendNotification);quality.setSelectedId(processor.value("quality")>0.5f?2:1,juce::dontSendNotification);
    liveSource.setToggleState(!processor.usesFileAnalysis(),juce::dontSendNotification);fileSource.setToggleState(processor.usesFileAnalysis(),juce::dontSendNotification);
    const auto loaded=processor.fileAnalysis.snapshot();repeat.setEnabled(!processor.usesFileAnalysis()||(!loaded.busy&&!receivingFile&&!loaded.name.isEmpty()));
    down.setEnabled(processor.value("shift")>-12);up.setEnabled(processor.value("shift")<12);half.setEnabled(processor.value("tempoOctave")>-2);doubleTempo.setEnabled(processor.value("tempoOctave")<2);repaint();
}
bool FLOWTONEAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files)
{return files.size()==1;}
void FLOWTONEAudioProcessorEditor::filesDropped(const juce::StringArray& files,int,int)
{
   #if JUCE_WINDOWS
    nativeDrop.filesDropped(files,0,0);
   #else
    if(files.size()==1){++dropTicket;receivingFile=false;processor.loadAnalysisFile(juce::File(files[0]));}
   #endif
}
void FLOWTONEAudioProcessorEditor::chooseFile()
{
    chooser=std::make_unique<juce::FileChooser>(ru(u8"Выберите аудиофайл для анализа"),juce::File(),"*.wav;*.aif;*.aiff;*.mp3;*.flac;*.m4a;*.ogg");
    const juce::Component::SafePointer<FLOWTONEAudioProcessorEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe](const juce::FileChooser& selected)
    {if(safe&&selected.getResult().existsAsFile()){++safe->dropTicket;safe->receivingFile=false;safe->processor.loadAnalysisFile(selected.getResult());}});
}
void FLOWTONEAudioProcessorEditor::applyPreset(int id)
{
    static constexpr float shifts[]={0,1,-1,0.5f,-0.5f};if(id<1||id>5)return;processor.setValue("shift",shifts[id-1]);processor.setValue("formants",1);processor.setValue("quality",1);processor.setValue("gain",0);processor.setValue("after",1);
}
void FLOWTONEAudioProcessorEditor::showHelp()
{
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,"FLOWTONE " JucePlugin_VersionString,
      ru(u8"Два способа анализа. «Файл»: перетащите один аудиофайл или регион на осциллограмму, либо нажмите «Выбрать файл». Вся запись анализируется в фоне без воспроизведения; результат сохраняется в проекте. «Онлайн»: включите воспроизведение — анализируется вход дорожки. Обычно нужен музыкальный фрагмент от 12–20 секунд. При неоднозначном материале показывается «Недостаточно данных»; абсолютная точность не гарантируется.\n\n"
         u8"Загруженный файл используется только для анализа. Сдвиг всегда обрабатывает звук дорожки проекта. Для анализа региона учитывается именно переданный секвенсором аудиофайл; он может отличаться от дорожки с другими эффектами и монтажом.\n\n"
         u8"Ручка меняет высоту звука самой дорожки, включая экспорт проекта. Шаг ¼ тона — это 50 центов, или 0,5 полутона. Темп и длительность не меняются. Диапазон: октава вниз и вверх.\n\n"
         u8"«До / После» сравнивает исходный и обработанный звук. «Обход» отключает обработку. Системный плеер, микрофон и запись не нужны.\n\n"
         u8"Кнопки ½ и ×2 уточняют BPM, а не меняют скорость. «Повторить анализ» очищает прежнее определение. A/B хранит два набора настроек. Названия тональностей — на английском. При сдвиге на четверть тона к названию добавляются центы."),ru(u8"Понятно"),this);
}
