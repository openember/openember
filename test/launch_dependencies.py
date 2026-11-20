"""Native, isolated readiness/dependency supervision tests; no devices."""
import json, os, pathlib, signal, socket, subprocess, sys, tempfile, time
if len(sys.argv)>1 and sys.argv[1]=='worker':
    _,_,name,logfile,delay,notify=sys.argv
    def log(event):
        with open(logfile,'a') as stream:
            stream.write(json.dumps([time.monotonic(),name,event,os.getpid()])+'\n')
    def stop(*_):
        log('stop'); time.sleep(.15); log('exit'); sys.exit(0)
    signal.signal(signal.SIGTERM,stop)
    log('start'); time.sleep(float(delay))
    if notify=='yes':
        channel=socket.socket(fileno=int(os.environ.pop('OPENEMBER_READY_FD')))
        log('ready'); channel.send(b'R'); channel.close()
    while True: time.sleep(.02)
launcher=str(pathlib.Path(sys.argv[1]).resolve())
def wait(predicate,seconds=8):
    until=time.monotonic()+seconds
    while time.monotonic()<until:
        if predicate(): return
        time.sleep(.025)
    raise AssertionError('condition timed out')
with tempfile.TemporaryDirectory(prefix='ember-dependencies-') as folder:
    root=pathlib.Path(folder)
    def events():
        p=root/'events'; return [json.loads(x) for x in p.read_text().splitlines()] if p.exists() else []
    def spec(name,deps=(),notify='yes',delay='.1'):
        return dict(name=name,command=sys.executable,args=[__file__,'worker',name,str(root/'events'),delay,notify],
                    depends_on=list(deps),readiness='notify',startup_timeout_ms=1200,shutdown_timeout_ms=1000,
                    restart='on-failure',restart_limit=1)
    def start(nodes):
        with socket.socket() as sock:
            sock.bind(('127.0.0.1',0)); port=sock.getsockname()[1]
        (root/'launch.json').write_text(json.dumps(dict(runtime=dict(robot_id='launch-test',link=dict(listen=f'tcp/127.0.0.1:{port}')),nodes=nodes)))
        output=(root/'output').open('w')
        process=subprocess.Popen([launcher,'--launch',str(root/'launch.json'),'--pid-file',str(root/'pid')],stdout=output,stderr=subprocess.STDOUT,start_new_session=True)
        return process,output
    def finish(process,output):
        if process.poll() is None: process.terminate()
        try: process.wait(timeout=8)
        finally:
            try: os.killpg(process.pid,signal.SIGKILL)
            except ProcessLookupError: pass
            output.close()
    process,output=start([spec('consumer',['provider']),spec('provider',delay='.5')])
    try:
        wait(lambda:any(e[1:3]==['consumer','ready'] for e in events()))
        entries=events()
        assert next(e[0] for e in entries if e[1:3]==['consumer','start'])>next(e[0] for e in entries if e[1:3]==['provider','ready'])
        provider=next(e[3] for e in entries if e[1:3]==['provider','start'])
        os.kill(provider,signal.SIGKILL)
        wait(lambda:sum(e[1:3]==['consumer','ready'] for e in events())==2)
        assert sum(e[1:3]==['consumer','stop'] for e in events())>=1
        process.terminate(); assert process.wait(timeout=8)==0
        entries=events()
        assert max(e[0] for e in entries if e[1:3]==['consumer','exit'])<max(e[0] for e in entries if e[1:3]==['provider','stop'])
    finally: finish(process,output)
    (root/'events').unlink()
    process,output=start([spec('blocked',['silent']),spec('silent',notify='no')])
    try:
        wait(lambda:sum(e[1:3]==['silent','start'] for e in events())==2)
        time.sleep(1.8)
        assert not any(e[1]=='blocked' for e in events())
    finally: finish(process,output)
    for nodes in ([spec('a',['missing'])],[spec('a',['b']),spec('b',['a'])],[spec('a'),spec('a')]):
        process,output=start(nodes)
        try: assert process.wait(timeout=5)!=0
        finally: finish(process,output)
    print('LAUNCH_DEPENDENCIES passed ready=1 restart=1 reverse_shutdown=1 timeout=1 invalid_graph=1')
