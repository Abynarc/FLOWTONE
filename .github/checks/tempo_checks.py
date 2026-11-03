"""Production-code tempo acceptance, including half/double BPM and changing input."""
from pathlib import Path
import argparse,json,os,subprocess,tempfile,time
import numpy as np
import soundfile as sf
BPMS=(72,96,128,174)
PATTERNS=('reference','bass-eighths','alternating-kicks','swing','syncopation','triplet-hats','human-jitter','no-bass')
RATE,DURATION=24000,28
def cases():
    for bpm in BPMS:
        for pattern in PATTERNS:
            yield bpm, pattern

def generate(out):
    for bpm, pattern in cases():
        rng = np.random.default_rng(9173)
        audio = np.zeros(RATE * DURATION)

        def hit(beat, kind, level):
            pos = beat * 60 / bpm
            if pattern == 'human-jitter':
                pos += rng.uniform(-.012, .012)
            start = round(pos * RATE)
            count = min(round(RATE * .18), len(audio) - start)
            if start < 0 or count <= 0:
                return
            t = np.arange(count) / RATE
            if kind == 'kick':
                sound = np.sin(2*np.pi*(48*t+2.5*(1-np.exp(-t*40)))) * np.exp(-t*35)
            elif kind == 'snare':
                sound = rng.normal(size=count) * np.exp(-t*55)
            else:
                sound = np.diff(rng.normal(size=count+1)) * np.exp(-t*130)
            audio[start:start+count] += level * sound

        for beat in range(int(DURATION*bpm/60)):
            if pattern == 'syncopation':
                if beat % 4 == 0:
                    for delta in (0, 1.5, 2.75, 3.5):
                        hit(beat+delta, 'kick', .4)
            elif pattern != 'no-bass':
                hit(beat, 'kick', .45 if pattern != 'alternating-kicks' or beat % 2 == 0 else .08)
            if pattern == 'bass-eighths':
                hit(beat+.5, 'kick', .32)
            if beat % 4 in (1, 3):
                hit(beat, 'snare', .24)
            hit(beat, 'hat', .04)
            hit(beat+(2/3 if pattern == 'swing' else .5), 'hat', .12)
            if pattern == 'triplet-hats':
                hit(beat+1/3, 'hat', .12)
                hit(beat+2/3, 'hat', .12)
        audio *= .8 / max(.8, np.max(np.abs(audio)))
        sf.write(out/f'{pattern}-{bpm}.wav', audio, RATE, subtype='FLOAT')


def error(actual, expected):
    if actual <= 0:
        return float('inf')
    return min(abs(actual/factor-expected)/expected for factor in (.5,1,2))


def arrangement(bpm,rate,duration=26,seed=20261102):
    rng=np.random.default_rng(seed)
    audio=np.zeros(round(rate*duration))
    beat=60/bpm
    for number in range(int(duration/beat)+1):
        start=number*beat
        if number>0:start+=rng.uniform(-.008,.008)
        def add(offset,kind,level):
            first=round((start+offset*beat)*rate)
            count=min(round(rate*.22),len(audio)-first)
            if first<0 or count<=0:return
            t=np.arange(count)/rate
            if kind=='kick':
                sound=np.sin(2*np.pi*(45*t+50/35*(1-np.exp(-35*t))))*np.exp(-t*32)
            elif kind=='snare':
                sound=(rng.normal(size=count)+.2*np.sin(2*np.pi*180*t))*np.exp(-t*45)
            else:
                sound=np.diff(rng.normal(size=count+1))*np.exp(-t*100)
            audio[first:first+count]+=level*sound
        if number%4==0 or rng.random()>.15:add(0,'kick',.35)
        if number%2:add(0,'snare',.12)
        add(0,'hat',.025);add(.5,'hat',.07)
    t=np.arange(len(audio))/rate
    for note in (48,52,55):
        frequency=440*2**((note-69)/12)
        envelope=.02*(.6+.4*np.cos(2*np.pi*t/(beat*4)))
        audio+=envelope*np.sin(2*np.pi*frequency*t)
    return audio*(.8/max(.8,np.max(np.abs(audio))))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--driver',default=os.environ.get('FLOWTONE_VERIFY_DRIVER'))
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--work-dir',type=Path)
    args=parser.parse_args()
    if not args.driver:parser.error('--driver or FLOWTONE_VERIFY_DRIVER required')
    driver=Path(args.driver).resolve()
    work=args.work_dir or Path(tempfile.mkdtemp(prefix='flowtone-tempo-'))
    work.mkdir(parents=True,exist_ok=True);args.output.mkdir(parents=True,exist_ok=True)
    report={'metric':'Relative error <= 1%, only factors 0.5/1/2 accepted; thirds are errors.',
            'cases':[], 'timeline':{},'complete':False}
    def save():
        (args.output/'acceptance.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(mode,path):
        started=time.perf_counter()
        process=subprocess.run([str(driver),mode,str(path)],capture_output=True,text=True,timeout=90)
        if process.returncode:raise AssertionError((mode,path.name,process.stdout,process.stderr))
        value=json.loads(process.stdout)
        if isinstance(value,dict):value['wallSeconds']=time.perf_counter()-started
        return value
    def test(path,expected,group):
        row={'name':path.name,'expected':expected,'group':group}
        for mode in ('analyse','file-analyse'):
            result=run(mode,path);row[mode]=result
            assert np.isfinite(result['confidence']) and 0<=result['confidence']<=1,(path.name,result)
            if expected:
                assert error(result['bpm'],expected)<=.01,(mode,path.name,expected,result)
                assert result['beats']>=5,(mode,path.name,result)
            else:
                assert result['bpm']==0 and not result['stable'],(mode,path.name,result)
        report['cases'].append(row);save();print('PASS',group,path.name,[(m,row[m]['bpm'])for m in ('analyse','file-analyse')],flush=True)
    generate(work)
    for bpm,pattern in cases():test(work/f'{pattern}-{bpm}.wav',bpm,'stress32')
    bpms=(45,53.7,67.3,83.5,109.7,137.2,158.6,193.4,221.3,240)
    for index,bpm in enumerate(bpms):
        rate=(16000,44100,48000,96000)[index%4]
        path=work/f'fractional-{bpm}-{rate}.wav'
        sf.write(path,arrangement(bpm,rate,seed=20261102+index),rate,subtype='FLOAT')
        test(path,bpm,'additional-arrangements')
    rate=24000;t=np.arange(rate*24)/rate
    negatives={'silence':np.zeros(len(t)),
               'stationary-tone':.2*np.sin(2*np.pi*440*t),
               'stationary-chord':sum(.08*np.sin(2*np.pi*f*t)for f in (261.6256,329.6276,391.9954)),
               'noise':np.random.default_rng(91).normal(0,.03,len(t))}
    for name,audio in negatives.items():
        path=work/(name+'.wav');sf.write(path,audio,rate,subtype='FLOAT');test(path,0,'non-rhythmic')
    changes=work/'tempo-change.wav'
    sf.write(changes,np.concatenate([arrangement(96,rate,32),arrangement(132,rate,32,93)]),rate,subtype='FLOAT')
    timeline=run('tempo-timeline',changes);report['timeline']['change']=timeline;save()
    first=next((row for row in timeline if row['stable']),None)
    assert first and first['seconds']<=12 and error(first['bpm'],96)<.01,first
    assert all(row['stable'] and error(row['bpm'],132)<.01 for row in timeline[-5:]),timeline[-8:]
    file_result=run('file-analyse',changes);report['timeline']['change-file']=file_result;save()
    assert file_result['variable'] and not file_result['stable'],file_result
    ending=work/'long-silence.wav'
    sf.write(ending,np.concatenate([arrangement(124,rate,24),np.zeros(rate*24)]),rate,subtype='FLOAT')
    timeline=run('tempo-timeline',ending);report['timeline']['silence']=timeline;save()
    assert timeline[-1]['bpm']==0 and not timeline[-1]['stable'] and timeline[-1]['beats']==0,timeline[-3:]
    file_result=run('file-analyse',ending);report['timeline']['silence-file']=file_result;save()
    assert error(file_result['bpm'],124)<.01,file_result
    report['stateContract']=run('tempo-state-contract',work/'reference-128.wav');save()
    report['complete']=True;save()
    print('PASS: 32 stress rhythms, 10 additional arrangements, 4 non-rhythmic signals; both analysis paths; live tempo changes, reset, silence clearing, whole-file retention and cached evidence.')


if __name__=='__main__':main()
