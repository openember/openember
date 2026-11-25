"""Isolated D-Bus integration test, no system NetworkManager / root required."""
import os,pathlib,subprocess,sys,tempfile,time
with tempfile.TemporaryDirectory(prefix='ember-nm-test-') as tmp:
    out=subprocess.check_output(['dbus-daemon','--session','--fork','--print-address=1','--print-pid=1'],text=True).splitlines()
    env=dict(os.environ,DBUS_SYSTEM_BUS_ADDRESS=out[0]);pid=int(out[1]);fake=None
    try:
        with open(pathlib.Path(tmp)/'fake.log','w+') as log:
            fake=subprocess.Popen(['/usr/bin/python3',str(pathlib.Path(__file__).with_name('fake_nm.py'))],env=env,stdout=log,stderr=subprocess.STDOUT)
            end=time.monotonic()+5
            while True:
                log.flush();log.seek(0)
                if 'FAKE_NM_READY' in log.read().splitlines():break
                if fake.poll() is not None or time.monotonic()>end:log.seek(0);raise RuntimeError(log.read())
                time.sleep(.05)
            subprocess.run([sys.argv[1]],env=env,check=True,timeout=20)
    finally:
        if fake is not None:fake.terminate();fake.wait(timeout=3)
        os.kill(pid,15)
