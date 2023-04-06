/*
 * Copyright (c) 2017-2022, DreamGrow Team
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-08-15     luhuadong    the first version
 */

#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/route.h>
#include <net/if_arp.h>  /* ARPHRD_ETHER */
#include <resolv.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include "netdev.h"

#define TAG           "netdev"
#define NONE_IP       "0.0.0.0"
#define NETDEV_MAX    2

static netdev_attr_t netdev_attr[NETDEV_MAX] = {
    { NONE_IP, NONE_IP, NONE_IP, NONE_IP },
    { NONE_IP, NONE_IP, NONE_IP, NONE_IP }
};

/**
 * This function will get the MAC address of a specified net device.
 *
 * @param ifname the device name of netdev
 * @param mac    the MAC address buffer
 *
 * @return AG_EOK while success, 
 *         -AG_ERROR while failure.
 */
int get_local_mac(const char *ifname, char *mac)
{
    struct ifreq ifr;
    int sd;
    
    bzero(&ifr, sizeof(struct ifreq));
    if( (sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        LOG_E("get %s mac address socket creat error", ifname);
        return -AG_ERROR;
    }
    
    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name) - 1);

    if(ioctl(sd, SIOCGIFHWADDR, &ifr) < 0) {
        LOG_E("get %s mac address error", ifname);
        close(sd);
        return -AG_ERROR;
    }
 
    snprintf(mac, MAC_SIZE, "%02x:%02x:%02x:%02x:%02x:%02x",
        (unsigned char)ifr.ifr_hwaddr.sa_data[0], 
        (unsigned char)ifr.ifr_hwaddr.sa_data[1],
        (unsigned char)ifr.ifr_hwaddr.sa_data[2], 
        (unsigned char)ifr.ifr_hwaddr.sa_data[3],
        (unsigned char)ifr.ifr_hwaddr.sa_data[4],
        (unsigned char)ifr.ifr_hwaddr.sa_data[5]);
 
    close(sd);
    return AG_EOK;
}

/**
 * This function will set the MAC address to a specified net device.
 *
 * @param ifname the device name of netdev
 * @param mac    the MAC address you want to set
 *
 * @return AG_EOK while success, 
 *         -AG_ERROR while failure.
 */
int set_local_mac(const char *ifname, const char *mac)
{
    int sd, ret;
    struct ifreq ifr;
    int values[6];
    
    if((0 != getuid()) && (0 != geteuid())) {
        return -AG_ERROR;
    }

    if((sd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        return -AG_ERROR;
    }

    strcpy(ifr.ifr_name, ifname);
    ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;

    ret = sscanf(mac, "%x:%x:%x:%x:%x:%x%*c", &values[0], &values[1], &values[2], &values[3], &values[4], &values[5]);
    if(6 > ret) {
        LOG_E("invalid mac address");
        return -AG_ERROR;
    }

    /* convert and save to ifr_hwaddr */
    for(int i=0; i<6; i++) {
        ifr.ifr_hwaddr.sa_data[0] = (uint8_t)values[0];
        ifr.ifr_hwaddr.sa_data[1] = (uint8_t)values[1];
        ifr.ifr_hwaddr.sa_data[2] = (uint8_t)values[2];
        ifr.ifr_hwaddr.sa_data[3] = (uint8_t)values[3];
        ifr.ifr_hwaddr.sa_data[4] = (uint8_t)values[4];
        ifr.ifr_hwaddr.sa_data[5] = (uint8_t)values[5];
    }
    
    if(ioctl(sd, SIOCSIFHWADDR, &ifr) < 0) {
        LOG_E("get %s mac address error", ifname);
        close(sd);
        return -AG_ERROR;
    }
    
    close(sd);
    return AG_EOK;
}

/**
 * This function will get the IP address of a specified net device.
 *
 * @param ifname the device name of netdev
 * @param mac    the IP address buffer
 *
 * @return AG_EOK while success, 
 *         -AG_ERROR while failure.
 */
int get_local_ip(const char *ifname, char *ip)
{
    int sd;
    struct sockaddr_in sin;
    struct ifreq ifr;
 
    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (-1 == sd) {
        LOG_E("socket error: %s", strerror(errno));
        return -AG_ERROR;		
    }

    //bzero(ifr.ifr_name, sizeof(ifr.ifr_name));
    //strncpy(ifr.ifr_name, ifName,sizeof(ifr.ifr_name)-1);
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    ifr.ifr_name[IFNAMSIZ - 1] = 0;
    
    // if error: No such device
    if (ioctl(sd, SIOCGIFADDR, &ifr) < 0) {
        LOG_E("ioctl error: %s", strerror(errno));
        close(sd);
        return -AG_ERROR;
    }

    memcpy(&sin, &ifr.ifr_addr, sizeof(sin));
    snprintf(ip, IP_SIZE, "%s", inet_ntoa(sin.sin_addr));

    close(sd);
    return AG_EOK;
}

/**
 * This function will set the IP address to a specified net device.
 *
 * @param ifname the device name of netdev
 * @param mac    the IP address you want to set
 *
 * @return AG_EOK while success, 
 *         -AG_ERROR while failure.
 */
int set_local_ip(const char *ifname, const char *ip)
{
    int sd;
    struct ifreq ifr;
    struct sockaddr_in *sin;

    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (-1 == sd) {
        LOG_E("socket error: %s", strerror(errno));
        return -AG_ERROR;		
    }

    sin = (struct sockaddr_in *)&ifr.ifr_addr;
    sin->sin_family = AF_INET;
    sin->sin_port = 0;
    sin->sin_addr.s_addr = inet_addr(ip);

    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    ifr.ifr_name[IFNAMSIZ - 1] = 0;

    if (ioctl(sd, SIOCSIFADDR, &ifr) < 0) {
        LOG_E("ioctl error: %s", strerror(errno));
        close(sd);
        return -AG_ERROR;
    }

    close(sd);
    return AG_EOK;
}

/**
 * This function will get the netmask address of a specified net device.
 *
 * @param ifname the device name of netdev
 * @param mac    the netmask address buffer
 *
 * @return AG_EOK while success, 
 *         -AG_ERROR while failure.
 */
int get_local_netmask(const char *ifname, char *netmask_addr)
{
    int sd;  
    struct ifreq ifr;  
          
    sd = socket(AF_INET, SOCK_STREAM, 0);  
    if (sd == -1) {  
        LOG_E("socket error: %s", strerror(errno));
        return -AG_ERROR;
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name)-1);

    if ((ioctl(sd, SIOCGIFNETMASK, &ifr)) < 0 ) {
        LOG_E("netmask ioctl error");
        close(sd);
        return -AG_ERROR;
    }
    
    strncpy(netmask_addr, inet_ntoa(((struct sockaddr_in*)&ifr.ifr_netmask)->sin_addr), IP_SIZE);
    close(sd);
    return AG_EOK;
}

/**
 * This function will set the netmask address to a specified net device.
 *
 * @param ifname the device name of netdev
 * @param mac    the netmask address you want to set
 *
 * @return AG_EOK while success, 
 *         -AG_ERROR while failure.
 */
int set_local_netmask(const char *ifname, const char *netmask_addr)
{
    int sd;
    struct ifreq ifr;
    struct sockaddr_in *sin_net_mask;

    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1) {
        LOG_E("socket error: %s", strerror(errno));
        return -AG_ERROR;
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name)-1);

    sin_net_mask = (struct sockaddr_in *)&ifr.ifr_addr;
    sin_net_mask->sin_family = AF_INET;
    inet_pton(AF_INET, netmask_addr, &sin_net_mask->sin_addr);

    if (ioctl(sd, SIOCSIFNETMASK, &ifr) < 0) {
        LOG_E("sock_netmask ioctl error");
        close(sd);
        return -AG_ERROR;
    }

    close(sd);
    return AG_EOK;
}

/**
 * This function will get the gateway address of a specified net device.
 *
 * @param ifname the device name of netdev
 * @param mac    the gateway address buffer
 *
 * @return AG_EOK while success, 
 *         -AG_ERROR while failure.
 */
int get_local_gateway(const char* ifname, char *gateway) 
{
    assert(gateway);

    char cmd [1000] = {0x0};
    char line[256]  = {0x0};

    sprintf(cmd,"route -n | grep %s  | grep 'UG[ \t]' | awk '{print $2}'", ifname);
    FILE* fp = popen(cmd, "r");
    if (fp == NULL) {
        return -AG_ERROR;
    }

    if (NULL == fgets(line, sizeof(line), fp)) {
        return -AG_ERROR;
    }
    pclose(fp);

    if (line[strlen(line)-1] == '\n') {
        line[strlen(line)-1] = '\0';
    }
    
    strcpy(gateway, line);
    return AG_EOK;
}

/**
 * This function will set the gateway address to a specified net device.
 *
 * @param ifname the device name of netdev
 * @param mac    the gateway address you want to set
 *
 * @return AG_EOK while success, 
 *         -AG_ERROR while failure.
 */
int set_local_gateway(const char *ifname, const char *gateway)
{
    int sd;
    struct rtentry route;
    struct ifreq ifr;
    struct sockaddr_in *sin;

    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1) {
        LOG_E("socket error: %s", strerror(errno));
        return -AG_ERROR;
    }
    
    memset(&route,  0, sizeof(struct rtentry));
    memset(&ifr, 0, sizeof(ifr));
    strcpy(ifr.ifr_name, ifname);

    sin = (struct sockaddr_in*)&ifr.ifr_addr;
    memset(sin, 0, sizeof(struct sockaddr_in));
    sin->sin_family = AF_INET;
    sin->sin_port   = 0;
    
    if(inet_aton(gateway, &sin->sin_addr) < 0) {
        LOG_E("inet_aton failed");
        close(sd);
        return -AG_ERROR;
    }
    
    memcpy(&route.rt_gateway, sin, sizeof(struct sockaddr_in));
    //init_sockaddr_in((struct sockaddr_in *) &route.rt_gateway,gateway);
    ((struct sockaddr_in *)&route.rt_dst)->sin_family     = AF_INET;
    ((struct sockaddr_in *)&route.rt_genmask)->sin_family = AF_INET;
    route.rt_flags  = RTF_UP | RTF_GATEWAY;
    route.rt_dev    = (char *)ifname;
    route.rt_metric = 5;

    if((ioctl(sd, SIOCADDRT, &route)) < 0) {
        close(sd);
        return -AG_ERROR;
    }

    close(sd);
    return AG_EOK;
}

/**
 * This function will get the DNS address of a specified net device.
 *
 * @param ifname the device name of netdev
 * @param mac    the DNS address buffer
 *
 * @return AG_EOK while success, 
 *         -AG_ERROR while failure.
 */
int get_local_dns(const char *ifname, char* dns_addr)
{
    struct sockaddr_in sin;

    res_state res = malloc(sizeof(struct __res_state));
    res_ninit(res);
    if (res->nscount < 1) {
        return -AG_ERROR;
    }

    memcpy(&sin, &res->nsaddr_list[0], sizeof(sin));
    snprintf(dns_addr, IP_SIZE, "%s", inet_ntoa(sin.sin_addr));

    res_nclose(res);
    if (res)
        free(res);

    return AG_EOK;
}

/**
 * This function will set the DNS address to a specified net device.
 *
 * @param ifname the device name of netdev
 * @param mac    the DNS address you want to set
 *
 * @return AG_EOK while success, 
 *         -AG_ERROR while failure.
 */
int set_local_dns(const char *ifname, const char* dns_addr)
{
    char dnsconf[100] = {0};

    if (0 == strncmp(ifname, "eth0", IP_SIZE)) {
        strncpy(netdev_attr[0].dns, dns_addr, strlen(dns_addr) + 1);
    } else if (0 == strncmp(ifname, "eth1", IP_SIZE)) {
        strncpy(netdev_attr[1].dns, dns_addr, strlen(dns_addr) + 1);
    }

    snprintf(dnsconf, sizeof(dnsconf), "echo 'nameserver %s' > /etc/resolv.conf", dns_addr);
    system(dnsconf);

    if (0 != strncmp(netdev_attr[0].dns, NONE_IP, IP_SIZE)) {
        snprintf(dnsconf, sizeof(dnsconf), "echo 'nameserver %s' >> /etc/resolv.conf", dns_addr);
        system(dnsconf);
    } else if (0 != strncmp(netdev_attr[1].dns, NONE_IP, IP_SIZE)) {
        snprintf(dnsconf, sizeof(dnsconf), "echo 'nameserver %s' >> /etc/resolv.conf", dns_addr);
        system(dnsconf);
    }

    return AG_EOK;
}

/**
 * This function will get the attribute of a specified net device.
 *
 * @param ifname the device name of netdev
 * @param mac    the attribute buffer
 *
 * @return AG_EOK while success, 
 *         -AG_ERROR while failure.
 */
int get_ip_attr(const char *ifname, netdev_attr_t *attr)
{
    if (!get_local_ip(ifname, attr->ipaddr)) {
        strncpy(attr->ipaddr, NONE_IP, IP_SIZE);
    }

    if (!get_local_netmask(ifname, attr->netmask)) {
        strncpy(attr->netmask, NONE_IP, IP_SIZE);
    }

    if (!get_local_gateway(ifname, attr->gateway)) {
        strncpy(attr->gateway, NONE_IP, IP_SIZE);
    }

    if (!get_local_dns(ifname, attr->dns)) {
        strncpy(attr->dns, NONE_IP, IP_SIZE);
    }

    return AG_EOK;
}

/**
 * This function will set the attribute to a specified net device.
 *
 * @param ifname the device name of netdev
 * @param mac    the attribute you want to set
 *
 * @return AG_EOK while success, 
 *         -AG_ERROR while failure.
 */
int set_ip_attr(const char *ifname, const netdev_attr_t *attr)
{
    set_local_ip(ifname, attr->ipaddr);
    set_local_netmask(ifname, attr->netmask);
    set_local_gateway(ifname, attr->gateway);
    set_local_dns(ifname, attr->dns);

    return AG_EOK;
}

/**
 * This function will get netdev status using SIOCETHTOOL command.
 *
 * @param ifname the device name of netdev
 *
 * @return 0 (LINK_UP) while link up, 
 *         1 (LINK_DOWN) while link down, 
 *         <0 (-AG_ERROR) while error.
 */
link_status_t get_netdev_status(const char *ifname)
{
    /* ifreq 里包含了接口的所有信息，比如接口名，地址等等 */
    struct ifreq ifr;
    struct ethtool_value edata;
    int skfd;

    if (( skfd = socket( AF_INET, SOCK_DGRAM, 0 ) ) < 0 ) {
        LOG_E("socket error");
        return -AG_ERROR;
    }

    memset(&ifr, 0, sizeof(ifr));
    edata.cmd = ETHTOOL_GLINK;

    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name)-1);
    ifr.ifr_data = (char *) &edata;

    if (ioctl(skfd, SIOCETHTOOL, &ifr) == -1) {
        LOG_E("ETHTOOL_GLINK failed: %s", strerror(errno));
        close(skfd);
        return -AG_ERROR;
    }

    close(skfd);
    return (edata.data ? LINK_UP : LINK_DOWN);
}

/**
 * This function will set netdev status using SIOCSIFFLAGS command.
 *
 * @param ifname the device name of netdev
 * @param status the status you want to set, LINK_UP or LINK_DOWN
 *
 * @return AG_EOK while success, 
 *         -AG_ERROR while failure.
 */
int set_netdev_status(const char *ifname, const link_status_t status)
{
    struct ifreq ifr;
    int sockfd;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        LOG_E("Create socket failed");
        return -AG_ERROR;
    }
    
    strcpy(ifr.ifr_name, ifname);
    if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0) {
        LOG_E("ioctl SIOCGIFFLAGS failed");
        close(sockfd);
        return -AG_ERROR;
    }

    if (status == LINK_UP) {
        ifr.ifr_flags |= IFF_UP;
    }
    else if (status == LINK_DOWN) {
        ifr.ifr_flags &= ~IFF_UP;
    }
    
    if (ioctl(sockfd, SIOCSIFFLAGS, &ifr) < 0) {
        LOG_E("ioctl SIOCSIFFLAGS failed");
        close(sockfd);
        return -AG_ERROR;
    }
    
    close(sockfd);
    return AG_EOK;
}

/**
 * This function will get local ip info (address & port) by socket fd.
 *
 * @param sockfd the socket file descriptor
 * @param ip     the ip address obtained, buffer len must greater than INET_ADDRSTRLEN.
 * @param port   the port obtained,.
 *
 * @return AG_EOK while success, 
 *         -AG_ERROR while failure.
 */
int get_local_ip_by_socket(const int sockfd, char *ip, int *port)
{
    struct sockaddr local_addr;
    socklen_t len = sizeof(struct sockaddr);

    if (0 != getsockname(sockfd, &local_addr, &len)) {
        return -AG_ERROR;
    }

    struct sockaddr_in *sin = (struct sockaddr_in *)(&local_addr);
    char addr_buffer[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(sin->sin_addr), addr_buffer, INET_ADDRSTRLEN);
    strncpy(ip, addr_buffer, INET_ADDRSTRLEN);

    *port = sin->sin_port;

    return AG_EOK;
}
