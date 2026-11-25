"""Small private D-Bus NM fixture; never connects to or alters the host system bus."""
import copy
import dbus
import dbus.service
from dbus.mainloop.glib import DBusGMainLoop
from gi.repository import GLib
import sys
NM='org.freedesktop.NetworkManager'
ROOT='/org/freedesktop/NetworkManager'
DEVICE=NM+'.Device'
WIRELESS=DEVICE+'.Wireless'
PROFILE=NM+'.Settings.Connection'
ACTIVE=NM+'.Connection.Active'
U=lambda v: dbus.UInt32(v)
S=lambda v: dbus.String(v)
O=lambda v: dbus.ObjectPath(v)
D=lambda v: dbus.Dictionary(v,signature='sv')
ARRAY=lambda v: dbus.Array([O(p) for p in v],signature='o')
class Obj(dbus.service.Object):
    def __init__(self,server,path):
        self.server=server
        self.path=path
        self.props={}
        super().__init__(server.bus,path)
    @dbus.service.method('org.freedesktop.DBus.Properties',in_signature='s',out_signature='a{sv}')
    def GetAll(self,interface):return D(self.props.get(interface,{}))
class Connection(Obj):
    def __init__(self,s,path,settings):
        super().__init__(s,path)
        self.settings=settings
        self.props[PROFILE]={'Unsaved':dbus.Boolean(False)}
    @dbus.service.method(PROFILE,in_signature='',out_signature='a{sa{sv}}')
    def GetSettings(self):return self.settings
    @dbus.service.method(PROFILE,in_signature='a{sa{sv}}',out_signature='')
    def UpdateUnsaved(self,settings):
        if 'ipv6' in self.settings:assert settings.get('ipv6')==self.settings['ipv6']
        self.settings=settings;self.props[PROFILE]['Unsaved']=dbus.Boolean(True)
    @dbus.service.method(PROFILE,in_signature='a{sa{sv}}',out_signature='')
    def Update(self,settings):self.settings=settings;self.Save()
    @dbus.service.method(PROFILE,in_signature='',out_signature='')
    def Save(self):self.props[PROFILE]['Unsaved']=dbus.Boolean(False)
    @dbus.service.method(PROFILE,in_signature='',out_signature='')
    def Delete(self):self.remove_from_connection();self.server.connections.pop(self.path,None)
    @dbus.service.method(PROFILE,in_signature='s',out_signature='a{sa{sv}}')
    def GetSecrets(self,setting):return {setting:D({'psk':S('test-only-pass')})}
class Settings(Obj):
    @dbus.service.method(NM+'.Settings',in_signature='',out_signature='ao')
    def ListConnections(self):return ARRAY(list(self.server.connections))
    @dbus.service.method(NM+'.Settings',in_signature='s',out_signature='o')
    def GetConnectionByUuid(self,uuid):
        for path,p in self.server.connections.items():
            if p.settings['connection']['uuid']==uuid:return O(path)
        raise dbus.exceptions.DBusException('org.freedesktop.NetworkManager.Settings.InvalidConnection')
    @dbus.service.method(NM+'.Settings',in_signature='a{sa{sv}}',out_signature='o')
    def AddConnectionUnsaved(self,settings):
        path=ROOT+'/Settings/'+str(len(self.server.connections)+10)
        self.server.connections[path]=Connection(self.server,path,settings)
        return O(path)
class Device(Obj):
    @dbus.service.method(WIRELESS,in_signature='a{sv}',out_signature='')
    def RequestScan(self,opts):self.props[WIRELESS]['LastScan']=dbus.Int64(int(self.props[WIRELESS]['LastScan'])+1)
class Server(Obj):
    def __init__(self):
        DBusGMainLoop(set_as_default=True)
        self.bus=dbus.SystemBus()
        self.name=dbus.service.BusName(NM,self.bus)
        self.connections={};self.devices={};self.objects=[];self.active={};self.checkpoint=None;self.fail_recovery=False
        super().__init__(self,ROOT)
        self.settings=Settings(self,ROOT+'/Settings')
        for j,(name,typ) in enumerate([('lo',1),('radio9',2)]):
            uuid=('11111111-1111-4111-8111-111111111111','22222222-2222-4222-8222-222222222222')[j]
            path=ROOT+'/Settings/'+str(j+1)
            settings={'connection':D({'id':S('Fixture '+name),'uuid':S(uuid),'type':S('802-3-ethernet' if typ==1 else '802-11-wireless'),'autoconnect':dbus.Boolean(True)}),'ipv4':D({'method':S('auto')}),'ipv6':D({'method':S('auto')})}
            if typ==2:settings['802-11-wireless']=D({'ssid':dbus.ByteArray(b'Fixture WiFi'),'mode':S('infrastructure')})
            self.connections[path]=Connection(self,path,settings)
            dev=Device(self,ROOT+'/Devices/'+str(j+1));self.devices[name]=dev
            dev.props[DEVICE]={'Interface':S(name),'DeviceType':U(typ),'Managed':dbus.Boolean(True),'State':U(100),'AvailableConnections':ARRAY([path])}
            if typ==2:
                aps=[]
                for k in (1,2):
                    ap=Obj(self,ROOT+'/AccessPoint/'+str(k));ap.props[NM+'.AccessPoint']={'Ssid':dbus.ByteArray(b'Fixture WiFi'),'Flags':U(1),'WpaFlags':U(0),'RsnFlags':U(0x100),'Strength':dbus.Byte(75),'Frequency':U(2412)};self.objects.append(ap);aps.append(ap.path)
                dev.props[WIRELESS]={'Mode':U(2),'WirelessCapabilities':U(0x40),'ActiveAccessPoint':O('/'),'AccessPoints':ARRAY(aps),'LastScan':dbus.Int64(1)}
            self.ActivateConnection(path,dev.path,'/')
        self.props[NM]={'Version':S('fixture'),'Devices':ARRAY([d.path for d in self.devices.values()])}
    @dbus.service.method(NM,in_signature='s',out_signature='o')
    def GetDeviceByIpIface(self,name):return O(self.devices[str(name)].path)
    @dbus.service.method(NM,in_signature='ooo',out_signature='o')
    def ActivateConnection(self,profile,device,specific):
        p=self.connections[str(profile)]
        if any(a.get('address')=='192.0.2.99' for a in p.settings.get('ipv4',{}).get('address-data',[])):
            self.fail_recovery=True
            raise dbus.exceptions.DBusException('Injected activation failure',name=NM+'.Device.Failed')
        dev=next(d for d in self.devices.values() if d.path==device)
        path=ROOT+'/ActiveConnection/'+device.rsplit('/',1)[-1]
        active=next((a for a in self.objects if a.path==path),None)
        if active is None:active=Obj(self,path);self.objects.append(active)
        c=p.settings['connection'];active.props[ACTIVE]={'Uuid':c['uuid'],'Id':c['id'],'Connection':O(profile)}
        dev.props[DEVICE]['ActiveConnection']=O(path);dev.props[DEVICE]['State']=U(100)
        if WIRELESS in dev.props:
            dev.props[WIRELESS]['Mode']=U(3 if p.settings.get('802-11-wireless',{}).get('mode')=='ap' else 2)
            dev.props[DEVICE]['AvailableConnections']=ARRAY([key for key,value in self.connections.items() if value.settings['connection']['type']=='802-11-wireless'])
        return O(path)
    @dbus.service.method(NM,in_signature='o',out_signature='')
    def DeactivateConnection(self,path):
        for dev in self.devices.values():
            if dev.props[DEVICE]['ActiveConnection']==path:dev.props[DEVICE]['ActiveConnection']=O('/');dev.props[DEVICE]['State']=U(30)
    @dbus.service.method(NM,in_signature='aouu',out_signature='o')
    def CheckpointCreate(self,devices,timeout,flags):
        assert int(timeout)==90
        self.checkpoint=copy.deepcopy({n:d.props for n,d in self.devices.items()})
        return O(ROOT+'/Checkpoint/1')
    @dbus.service.method(NM,in_signature='o',out_signature='')
    def CheckpointDestroy(self,path):self.checkpoint=None
    @dbus.service.method(NM,in_signature='o',out_signature='a{su}')
    def CheckpointRollback(self,path):
        assert self.checkpoint
        if self.fail_recovery:return {d.path:U(1) for d in self.devices.values()}
        for n,p in self.checkpoint.items():self.devices[n].props=p
        return {d.path:U(0) for d in self.devices.values()}
if __name__=='__main__':
    server=Server();print('FAKE_NM_READY',flush=True);GLib.MainLoop().run()
