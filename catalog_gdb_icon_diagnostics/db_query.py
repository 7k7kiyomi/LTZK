import os, subprocess, sys, winreg
cfg={}
key=winreg.OpenKey(winreg.HKEY_CURRENT_USER,r'Software\GarMap\MapProductManager\db')
for name in ['host','port','database','schema','user','password']:
    try: cfg[name]=str(winreg.QueryValueEx(key,name)[0])
    except OSError: pass
env=os.environ.copy()
env['PGPASSWORD']=cfg.get('password','')
env['PGCONNECT_TIMEOUT']='5'
env['PGCLIENTENCODING']='UTF8'
env['PGOPTIONS']='-c default_transaction_read_only='+('off' if '--write' in sys.argv else 'on')+' -c search_path='+cfg.get('schema','public')
args=[r'E:\osgeo-4w-qgis-dev\3rdParty\bin\psql.exe','-X','-w','-A','-t','-v','ON_ERROR_STOP=1','-h',cfg.get('host','localhost'),'-p',cfg.get('port','5432'),'-U',cfg.get('user','YOUR_PASSWORD'),'-d',cfg.get('database','ltzk_platform'),'-c',sys.stdin.read()]
r=subprocess.run(args,env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=20)
for out in [r.stdout,r.stderr]: print(out.decode('utf-8','replace').encode('ascii','backslashreplace').decode('ascii'))
sys.exit(r.returncode)
