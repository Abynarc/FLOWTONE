from pathlib import Path
import numpy as np
import soundfile as sf
root=Path(__file__).resolve().parent/'fixtures';root.mkdir(exist_ok=True)
rate=48000
rng=np.random.default_rng(724)
def music(tonic,minor,bpm,duration=24):
 t=np.arange(int(rate*duration))/rate; y=np.zeros(len(t));beat=60/bpm
 progression=([(0,0),(0,0),(5,0),(7,0),(0,0),(9,1),(2,1),(7,0)] if not minor else [(0,1),(0,1),(5,1),(7,0),(0,1),(8,0),(3,0),(7,0)])
 # Musical chord definitions are independent of the detector's scoring profiles.
 for index,start in enumerate(np.arange(0,duration,beat*4)):
  step,is_minor=progression[index%len(progression)];base=48+tonic+step
  notes=[base-12,base,base+(3 if is_minor else 4),base+7,base+12]
  end=min(duration,start+beat*4);a=int(start*rate);b=int(end*rate);local=t[a:b]-start
  env=np.minimum(local/0.015,1)*np.minimum((end-start-local)/0.05,1)*np.exp(-local/3)
  for number in notes:
   f=440*2**((number-69)/12);y[a:b]+=0.07*env*(np.sin(2*np.pi*f*local)+0.16*np.sin(4*np.pi*f*local)+0.05*np.sin(6*np.pi*f*local))
 for n,start in enumerate(np.arange(0,duration,beat)):
  a=int(start*rate);b=min(len(t),a+int(rate*0.2));local=t[a:b]-start
  y[a:b]+=0.32*np.sin(2*np.pi*(48*local+3*(1-np.exp(-local*35))))*np.exp(-local*30)
  if n%2:
   y[a:b]+=0.06*rng.normal(size=b-a)*np.exp(-local*45)
 y*=0.8/max(1,np.max(np.abs(y)));return np.stack([y,y*0.72],axis=1)
for tonic in [0,2,5,7,9,11]:
 for minor in [False,True]:
  sf.write(root/f'music-{tonic}-{int(minor)}-124.wav',music(tonic,minor,124),rate,subtype='FLOAT')
for bpm in [60,80,100,150,180]:sf.write(root/f'tempo-{bpm}.wav',music(9,True,bpm),rate,subtype='FLOAT')
for rate_test in [16000,44100,48000,96000]:
 t=np.arange(int(rate_test*3))/rate_test;y=0.25*np.sin(2*np.pi*440*t);sf.write(root/f'sine-{rate_test}.wav',np.stack([y,-y*0.7],axis=1),rate_test,subtype='FLOAT')
sf.write(root/'silence.wav',np.zeros((48000*20,2)),48000,subtype='FLOAT')
sf.write(root/'noise.wav',rng.normal(0,0.03,(48000*20,2)),48000,subtype='FLOAT')
(root/'bach-keys.tsv').write_text('') # Score-derived cases remain in the private macOS suite.
print('Generated synthetic CI fixtures:',len(list(root.glob('*.wav'))))
