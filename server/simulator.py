"""Simulação facultativa de telemetria; requer requests e servidor local."""
import json, random, time, urllib.request, os
url=os.getenv('SERVER_URL','http://127.0.0.1:8000')+'/api/device/telemetry'
key=os.getenv('DEVICE_KEY','dev-device-change-me')
for i in range(120):
    row={'device_id':'estufa-01','temperature':round(26+3*__import__('math').sin(i/10)+random.uniform(-.2,.2),1),'humidity':round(65+random.uniform(-4,4),1),'soil':round(47+random.uniform(-8,8),1),'light':round(650+random.uniform(-100,100)), 'tank_ok':True,'fan':i%23<8,'pump':i%30<3,'mode':'auto','alert':'NORMAL'}
    req=urllib.request.Request(url, json.dumps(row).encode(),{'Content-Type':'application/json','X-Device-Key':key}, method='POST')
    with urllib.request.urlopen(req,timeout=4) as response: print(response.status,row['temperature'])
    time.sleep(5)
