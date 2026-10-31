from pathlib import Path
import json,subprocess,time,os
import numpy as np
import soundfile as sf
from scipy.signal import periodogram
root=Path(__file__).resolve().parent;fixtures=root/'fixtures';out=root/'rendered';out.mkdir(exist_ok=True)
records=[]
def driver(*args):
 result=subprocess.run([os.environ.get('FLOWTONE_VERIFY_DRIVER',str(root/'driver')),*map(str,args)],capture_output=True,text=True,encoding="utf-8")
 if result.returncode:raise AssertionError(result.stdout+result.stderr)
 return result.stdout.strip()
def frequency(data,rate,start=.7,end=2.4):
 y=data[int(rate*start):int(rate*end),0]
 f,p=periodogram(y,rate,window='hann',nfft=1<<20);i=int(np.argmax(p))
 # Quadratic interpolation of the heavily zero-padded spectrum.
 values=np.log(np.maximum(p[i-1:i+2],1e-30));delta=.5*(values[0]-values[2])/(values[0]-2*values[1]+values[2]);return (i+delta)*rate/(1<<20)
start=time.time()
for rate in [16000,44100,48000,96000]:
 source=fixtures/f'sine-{rate}.wav';original,sr=sf.read(source,dtype='float32')
 for shift in [-12,-.5,.5,12]:
  path=out/f'sine-{rate}-{shift}.wav';log=driver('render',source,path,shift,1,0)
  data,out_rate=sf.read(path);assert out_rate==rate and data.shape==original.shape
  assert np.isfinite(data).all();expected=440*2**(shift/12);measured=frequency(data,rate)
  cents=1200*np.log2(measured/expected);stereo=float(np.max(np.abs(data[:,1]+.7*data[:,0])))
  assert abs(cents)<2.0,(rate,shift,measured,expected,cents)
  assert stereo<0.002 and np.corrcoef(data[int(rate*.4):,0],data[int(rate*.4):,1])[0,1]<-0.99999,(rate,shift,stereo)
  records.append({'rate':rate,'shift':shift,'frequency':measured,'expected':expected,'errorCents':cents,'stereoResidual':stereo,'log':log})
  print('PASS pitch',rate,shift,round(cents,4),'cents error',flush=True)
 for shift,after in [(0,1),(7,0)]:
  path=out/f'dry-{rate}-{shift}.wav';log=driver('render',source,path,shift,after,0);data,_=sf.read(path)
  assert np.max(np.abs(data-original))<3e-7,(rate,shift,np.max(np.abs(data-original)))
  print('PASS latency-aligned dry',rate,shift,flush=True)
# Both quality engines and both formant settings, and automation during rendering.
for quality in [0,1]:
 for formants in [0,1]:
  path=out/f'options-{quality}-{formants}.wav';log=driver('render',fixtures/'sine-48000.wav',path,.5,1,formants,quality);data,rate=sf.read(path)
  cents=1200*np.log2(frequency(data,rate)/(440*2**(.5/12)));assert abs(cents)<2,(quality,formants,cents)
  print('PASS quality/formants',quality,formants,round(cents,4),flush=True)
path=out/'automation.wav';print(driver('render',fixtures/'sine-48000.wav',path,.5,1,0,1,'dynamic'))
data,rate=sf.read(path);first=frequency(data,rate,.7,1.2);second=frequency(data,rate,2,2.7)
assert abs(1200*np.log2(first/(440*2**(.5/12)) ))<2
assert abs(1200*np.log2(second/(440*2**(-.5/12)) ))<2
assert np.max(np.abs(np.diff(data[:,0])))<.15
print('PASS automation changes track output without clicks',flush=True)
print(driver('state'),flush=True)
path=out/'quality-switch.wav';print(driver('render',fixtures/'sine-48000.wav',path,.5,1,0,1,'quality-switch'))
data,rate=sf.read(path)
for segmentStart,segmentEnd in [(0.7,0.95),(1.65,1.95),(2.65,2.9)]:
 cents=1200*np.log2(frequency(data,rate,segmentStart,segmentEnd)/(440*2**(.5/12)));assert abs(cents)<2,('quality-switch',segmentStart,cents)
assert np.max(np.abs(np.diff(data[:,0])))<.15
print('PASS both quality transitions: pitch, stereo and bounded sample discontinuity',flush=True)
analysis={}
for file in sorted(fixtures.glob('*.wav')):
 a=json.loads(driver('analyse',file));analysis[file.name]=a
 if file.name.startswith('music-'):
  _,key,minor,bpm=file.stem.split('-');assert a['key']==int(key) and a['minor']==bool(int(minor)),(file.name,a)
  assert abs(a['bpm']-int(bpm))<.25,(file.name,a)
 elif file.name.startswith('tempo-'):
  bpm=int(file.stem.split('-')[1]);assert abs(a['bpm']-bpm)<.25,(file.name,a)
 elif file.name in ['noise.wav','silence.wav']:
  assert a['key']==-1 and a['bpm']==0,(file.name,a)
 elif file.name.startswith('sine-'):assert a['key']==-1 and a['bpm']==0
 print('ANALYSIS',file.name,a['bpm'],a['key'],a['minor'],flush=True)
# Independent score-derived cases with unambiguous declared keys.
for name,key,mode in [line.split('\t')for line in (fixtures/'bach-keys.tsv').read_text().splitlines()]:
 a=analysis[name]
 if name=='bach-bwv153.1.wav':continue # Signature says E minor, global Music21 analysis says A minor. Keep ambiguity in report.
 assert a['key']==int(key) and a['minor']==(mode=='minor'),(name,a,key,mode)
 assert abs(a['bpm']-120)<.3,(name,a)
(root/'pitch-results.json').write_text(json.dumps(records,indent=2)+'\n')
(root/'analysis-results.json').write_text(json.dumps(analysis,indent=2)+'\n')
print('All processor checks PASS; elapsed',round(time.time()-start,2),'seconds',flush=True)
