#include "rpiInfo.h"
#include <stdio.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <sys/vfs.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <fcntl.h>
#include "st7735.h"
#include <stdlib.h>

/*
* Get the IP address of wlan0 or eth0
*/

char* get_ip_address(void)
{
    int fd;
    struct ifreq ifr;
    int symbol=0;
    if (IPADDRESS_TYPE == ETH0_ADDRESS)
    {
      fd = socket(AF_INET, SOCK_DGRAM, 0);
      /* I want to get an IPv4 IP address */
      ifr.ifr_addr.sa_family = AF_INET;
      /* I want IP address attached to "eth0" */
      strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);
      symbol=ioctl(fd, SIOCGIFADDR, &ifr);
      close(fd);
      if(symbol==0)
      {
        return inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
      }
      else
      {
        char* buffer="xxx.xxx.xxx.xxx";
        return buffer;
      }
    }
    else if (IPADDRESS_TYPE == WLAN0_ADDRESS)
    {
        fd = socket(AF_INET, SOCK_DGRAM, 0);
        /* I want to get an IPv4 IP address */
        ifr.ifr_addr.sa_family = AF_INET;
        /* I want IP address attached to "wlan0" */
        strncpy(ifr.ifr_name, "wlan0", IFNAMSIZ-1);
        symbol=ioctl(fd, SIOCGIFADDR, &ifr);
        close(fd);    
        if(symbol==0)
        {
          return inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);   
        }
        else
        {
          char* buffer="xxx.xxx.xxx.xxx";
          return buffer;
        }
    }
    else
    {
      char* buffer="xxx.xxx.xxx.xxx";
      return buffer;
    }
}

char* get_ip_address_new(void)
{
    int fd;
    struct ifreq ifr;
    int symbol=0;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    /* I want to get an IPv4 IP address */
    ifr.ifr_addr.sa_family = AF_INET;
    /* I want IP address attached to "eth0" */
    strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);
    symbol=ioctl(fd, SIOCGIFADDR, &ifr);
    close(fd);
    if(symbol==0)
    {
      return inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
    }
    else
    {
      fd = socket(AF_INET, SOCK_DGRAM, 0);
      /* I want to get an IPv4 IP address */
      ifr.ifr_addr.sa_family = AF_INET;
      /* I want IP address attached to "wlan0" */
      strncpy(ifr.ifr_name, "wlan0", IFNAMSIZ-1);
      symbol=ioctl(fd, SIOCGIFADDR, &ifr);
      close(fd);    
      if(symbol==0)
      {
        return inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);   
      }
      else
      {
        char* buffer="xxx.xxx.xxx.xxx";
        return buffer;
      }
    }
}



/*
* get ram memory
*/
void get_cpu_memory(float *Totalram,float *freeram)
{
  unsigned int value=0;
  unsigned char buffer[100]={0};
  unsigned char famer[100]={0};
  float memFree=0.0;
  int haveAvailable=0;

    FILE* fp=fopen("/proc/meminfo","r");
    if(fp==NULL)
    {
        return ;
    }
    while(fgets(buffer,sizeof(buffer),fp))
    {
        if(sscanf(buffer,"%s%u",famer,&value)!=2)
        {
        continue;
        }
        if(strcmp(famer,"MemTotal:")==0)
        {
         *Totalram=value/1000.0/1000.0;
        }
        /* MemAvailable is the kernel's own estimate of what a new workload
           could claim without swapping. MemFree is not a substitute: it
           excludes the reclaimable page cache that Linux deliberately fills
           with every spare page, so a healthy machine reads as nearly full.
           On a 32G host MemFree reports 90% used where MemAvailable
           reports 50%. */
        else if(strcmp(famer,"MemAvailable:")==0)
        {
          *freeram=value/1000.0/1000.0;
          haveAvailable=1;
        }
        else if(strcmp(famer,"MemFree:")==0)
        {
          memFree=value/1000.0/1000.0;
        }
    }
    fclose(fp);
    /* Kernels older than 3.14 do not publish MemAvailable. */
    if(!haveAvailable)
    {
      *freeram=memFree;
    }
}

/* Upper bound on the number of distinct filesystems we will total up. */
#define MAX_TRACKED_FS 32

typedef enum
{
  DISK_FILTER_ROOT,     /* only the filesystem mounted on "/"                */
  DISK_FILTER_NON_ROOT, /* every block-device filesystem except that one     */
  DISK_FILTER_ALL       /* every block-device filesystem                     */
} DiskFilter;

/*
* Resolve the device a path lives on, so that two mount entries backed by the
* same filesystem can be recognised as one.
*/
static int get_path_device(const char *path, dev_t *dev)
{
  struct stat st;
  if (stat(path, &st) != 0)
  {
    return -1;
  }
  *dev = st.st_dev;
  return 0;
}

/*
* Total the capacity, used and available bytes of the mounted filesystems
* selected by "filter", and return how many were counted (-1 on failure).
*
* /proc/mounts is walked instead of shelling out to df: no subprocess is
* needed, and because each filesystem is de-duplicated by its device id, a
* filesystem reachable through more than one mount entry is counted once.
* That is what makes DISK_FILTER_ROOT and DISK_FILTER_NON_ROOT disjoint, and
* therefore safe for a caller to add together, no matter whether the system
* boots from an SD card, USB or NVMe.
*
* The three byte counts mirror the columns df prints, so that a percentage
* derived from them agrees with df rather than drifting from it.
*/
static int sum_mounts(DiskFilter filter, uint64_t *totalBytes,
                      uint64_t *usedBytes, uint64_t *availBytes)
{
  FILE *fp = NULL;
  char line[512];
  char device[256];
  char mountPoint[256];
  dev_t seen[MAX_TRACKED_FS];
  int seenCount = 0;
  dev_t rootDev = 0;
  int haveRootDev = 0;

  *totalBytes = 0;
  *usedBytes = 0;
  *availBytes = 0;

  haveRootDev = (get_path_device("/", &rootDev) == 0);
  if (!haveRootDev && filter != DISK_FILTER_ALL)
  {
    /* Without knowing which filesystem is the root one we cannot honour a
       root/non-root split without risking counting it on both sides. */
    return -1;
  }

  fp = fopen("/proc/mounts", "r");
  if (fp == NULL)
  {
    return -1;
  }

  while (fgets(line, sizeof(line), fp) != NULL)
  {
    struct statfs fsInfo;
    uint64_t blockSize = 0;
    dev_t dev = 0;
    int duplicate = 0;
    int index = 0;

    if (sscanf(line, "%255s %255s", device, mountPoint) != 2)
    {
      continue;
    }

    /* Only real block devices hold user data. Loop devices are read-only
       images (snaps, mounted ISOs) that always read as 100% full, so
       including them would skew the total. */
    if (strncmp(device, "/dev/", 5) != 0 ||
        strncmp(device, "/dev/loop", 9) == 0)
    {
      continue;
    }

    if (get_path_device(mountPoint, &dev) != 0)
    {
      continue;
    }

    if (filter != DISK_FILTER_ALL)
    {
      int isRoot = (dev == rootDev);
      if (isRoot != (filter == DISK_FILTER_ROOT))
      {
        continue;
      }
    }

    for (index = 0; index < seenCount; index++)
    {
      if (seen[index] == dev)
      {
        duplicate = 1;
        break;
      }
    }
    if (duplicate)
    {
      continue;
    }
    if (seenCount >= MAX_TRACKED_FS)
    {
      /* Out of room to remember what has already been counted; skipping is
         an undercount, whereas counting on would risk a double count. */
      continue;
    }
    seen[seenCount++] = dev;

    if (statfs(mountPoint, &fsInfo) != 0 || fsInfo.f_blocks == 0)
    {
      continue;
    }

    blockSize = (uint64_t)fsInfo.f_bsize;
    /* "Size", "Used" and "Avail" as df defines them. Used counts the blocks
       reserved for root, which f_bavail excludes, so used + avail is smaller
       than the total; that gap is what df's Use% column divides by. */
    *totalBytes += blockSize * (uint64_t)fsInfo.f_blocks;
    *usedBytes += blockSize * ((uint64_t)fsInfo.f_blocks - (uint64_t)fsInfo.f_bfree);
    *availBytes += blockSize * (uint64_t)fsInfo.f_bavail;
  }

  fclose(fp);
  return seenCount;
}

/*
* Total every mounted block-device filesystem, each counted exactly once.
*/
int get_disk_usage(uint64_t *totalBytes, uint64_t *usedBytes, uint64_t *availBytes)
{
  return sum_mounts(DISK_FILTER_ALL, totalBytes, usedBytes, availBytes);
}

/*
* get sd memory
*
* Reports the filesystem mounted on "/", in whole GiB. Note that despite its
* name "freesize" receives the space in use, which is what callers display.
*/
void get_sd_memory(uint32_t *MemSize, uint32_t *freesize)
{
    uint64_t totalBytes = 0;
    uint64_t usedBytes = 0;
    uint64_t availBytes = 0;

    *MemSize = 0;
    *freesize = 0;

    if (sum_mounts(DISK_FILTER_ROOT, &totalBytes, &usedBytes, &availBytes) <= 0)
    {
      return;
    }
    *MemSize = (uint32_t)(totalBytes >> 30);
    *freesize = (uint32_t)(usedBytes >> 30);
}


/*
* get hard disk memory
*
* Reports every block-device filesystem other than the one mounted on "/",
* in whole GiB, so that it never overlaps with get_sd_memory().
*/
uint8_t get_hard_disk_memory(uint16_t *diskMemSize, uint16_t *useMemSize)
{
  uint64_t totalBytes = 0;
  uint64_t usedBytes = 0;
  uint64_t availBytes = 0;

  *diskMemSize = 0;
  *useMemSize = 0;

  if (sum_mounts(DISK_FILTER_NON_ROOT, &totalBytes, &usedBytes, &availBytes) <= 0)
  {
    return 0;
  }
  *diskMemSize = (uint16_t)(totalBytes >> 30);
  *useMemSize = (uint16_t)(usedBytes >> 30);
  return 1;
}

/*
* get temperature
*/

uint8_t get_temperature(void)
{
    FILE *fd;
    unsigned int temp;
    char buff[10] = {0};
    fd = fopen("/sys/class/thermal/thermal_zone0/temp","r");
    fgets(buff,sizeof(buff),fd);
    sscanf(buff, "%d", &temp);
    fclose(fd);
    return TEMPERATURE_TYPE == FAHRENHEIT ? temp/1000*1.8+32 : temp/1000;    
}

/*
* Get cpu usage
*/
uint8_t get_cpu_message(void)
{
    FILE * fp;
    uint8_t usCpuBuff[5] = {0};
    uint8_t syCpubuff[5] = {0};
    int usCpu = 0;
    int syCpu = 0;

    fp=popen("top -bn1 | grep %Cpu | awk '{printf \"%.2f\", $(2)}'","r");    //Gets the load on the CPU
    fgets(usCpuBuff, sizeof(usCpuBuff),fp);                                    //Read the user CPU load
    pclose(fp);    

    fp=popen("top -bn1 | grep %Cpu | awk '{printf \"%.2f\", $(4)}'","r");    //Gets the load on the CPU
    fgets(syCpubuff, sizeof(syCpubuff),fp);                                    //Read the system CPU load
    pclose(fp);   
    usCpu = atoi(usCpuBuff);
    syCpu = atoi(syCpubuff);
    return usCpu+syCpu;
  
}