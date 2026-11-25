#include "envelope.hpp"
#include "openember/config/yaml.hpp"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
namespace openember::sensor::link {
std::string Token() {
  std::array<unsigned char, 32> bytes{};
  if (RAND_bytes(bytes.data(), bytes.size()) != 1)
    throw std::runtime_error("IMU token generation failed");
  std::string out;
  for (auto b : bytes) {
    out += "0123456789abcdef"[b >> 4];
    out += "0123456789abcdef"[b & 15];
  }
  return out;
}

Credential LoadCredential(const std::string& path) {
    const int fd=open(path.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    if(fd<0)throw std::runtime_error("cannot open IMU credential");
    struct stat st{};
    const bool safe=fstat(fd,&st)==0 && S_ISREG(st.st_mode) && st.st_uid==geteuid() &&
        !(st.st_mode&077) && st.st_size>0 && st.st_size<=8192;
    std::string bytes(safe?st.st_size:0,'\0'); size_t offset=0;
    while(safe && offset<bytes.size()) {
        const auto n=read(fd,bytes.data()+offset,bytes.size()-offset);
        if(n<0 && errno==EINTR)continue;
        if(n<=0)break;
        offset+=n;
    }
    close(fd);
    if(!safe || offset!=bytes.size())throw std::runtime_error("IMU credential requires an owned mode-0600 regular file");
    const auto root=openember::config::ParseYaml(bytes,path);
    root.CheckKeys({"schema_version","robot_id","source_id","secret","role"});
    root.Require(root.Get<int>("schema_version")==1,"unsupported credential schema");
    Credential c{root.Get<std::string>("robot_id"),root.Get<std::string>("source_id"),root.Get<std::string>("secret"),root.Get<std::string>("role")};
    root.Require(envelope::Id(c.robot_id)&&envelope::Id(c.source_id)&&c.secret.size()==64 &&
        c.secret.find_first_not_of("0123456789abcdefABCDEF")==std::string::npos,"invalid IMU credential");
    root.Require(c.role=="observe"||c.role=="use"||c.role=="maintenance","invalid IMU role");
    return c;
}
void CreateCredential(const std::string& path,const std::string& robot,const std::string& source,const std::string& role) {
    if(!envelope::Id(robot)||!envelope::Id(source)||(role!="observe"&&role!="use"&&role!="maintenance"))
        throw std::invalid_argument("invalid IMU identity or role");
    const std::string text="schema_version: 1\nrobot_id: "+robot+"\nsource_id: "+source+"\nsecret: "+Token()+"\nrole: "+role+"\n";
    const int fd=open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
    if(fd<0)throw std::runtime_error("cannot exclusively create IMU credential");
    size_t offset=0;
    while(offset<text.size()) {auto n=write(fd,text.data()+offset,text.size()-offset);if(n<0&&errno==EINTR)continue;if(n<=0)break;offset+=n;}
    const int result=close(fd);
    if(offset!=text.size()||result){unlink(path.c_str());throw std::runtime_error("IMU credential write failed");}
}
void RequireLoopback(const std::string& value) {
    const std::string prefix="tcp/127.0.0.1:";
    const auto port=value.substr(std::min(value.size(),prefix.size()));
    if(value.rfind(prefix,0)!=0||port.empty()||port.size()>5||port.find_first_not_of("0123456789")!=std::string::npos||std::stoul(port)<1||std::stoul(port)>65535)
        throw std::invalid_argument("IMU Link requires a loopback locator");
}
} // namespace openember::sensor::link
