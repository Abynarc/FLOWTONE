from pathlib import Path
import json,subprocess,time,os
import numpy as np
import soundfile as sf
r=Path(__file__).resolve().parent;out=r/'file-analysis';out.mkdir(exist_ok=True)
records={}
def run(path):
 p=subprocess.run([os.environ.get('FLOWTONE_VERIFY_DRIVER',str(r/'driver')),'file-analyse',str(path)],capture_output=True,text=True,check=True)
 a=json.loads(p.stdout);records[path.name]=a;return a
for path in sorted((r/'fixtures').glob('music-*.wav')):
 a=run(path);_,key,minor,bpm=path.stem.split('-')
 assert a['key']==int(key) and a['minor']==bool(int(minor)),(path.name,a)
 assert abs(a['bpm']-int(bpm))<.25,(path.name,a)
for name in ['noise.wav','silence.wav','sine-48000.wav']:
 a=run(r/'fixtures'/name);assert a['bpm']==0 and a['key']==-1,(name,a)
music,rate=sf.read(r/'fixtures/music-9-1-124.wav')
# Musical section starts after the initial 36 seconds; the whole file is read.
for name,audio in [('long-intro.wav',np.concatenate([np.zeros((rate*36,2)),music])),('long-outro.wav',np.concatenate([music,music,np.zeros((rate*72,2))]))]:
 path=out/name;sf.write(path,audio,rate,subtype='FLOAT');a=run(path)
 assert abs(a['bpm']-124)<.25 and a['key']==9 and a['minor'],(name,a)
 assert abs(a['seconds']-len(audio)/rate)<.01
# AIFF uses the same offline path as WAV; invalid and unavailable files are tested by lifecycle mode.
path=out/'music.aif';sf.write(path,music,rate,format='AIFF',subtype='PCM_24');a=run(path);assert abs(a['bpm']-124)<.25 and a['key']==9
p=subprocess.run([os.environ.get('FLOWTONE_VERIFY_DRIVER',str(r/'driver')),'file-lifecycle',str(out/'long-outro.wav'),str(r/'fixtures/tempo-80.wav')],text=True,capture_output=True,check=True)
records['lifecycle']=json.loads(p.stdout)
(out/'results.json').write_text(json.dumps(records,indent=2)+'\n')
print(json.dumps(records,indent=2))
print('File analysis, whole-file coverage, cache, cancellation, repeat, recovery and switching PASS')
