#pragma once


typedef u8 uint8_t;
typedef u16 uint16_t;
typedef u32 uint32_t;
typedef u64 uint64_t;

typedef u32 uint32;


typedef s8 int8_t;
typedef s16 int16_t;
typedef s32 int32_t;
typedef s64 int64_t;

#define le64_to_cpu(x) (x)
#define le32_to_cpu(x) (x)
#define le16_to_cpu(x) (x)
#define cpu_to_le64(x) (x)
#define cpu_to_le32(x) (x)
#define cpu_to_le16(x) (x)

#define cpu_to_be16(x) ( (((u8)(x))<<8) | ((u8)((x)>>8)) )

#define be16_to_cpu(x) ( (((u8)(x))<<8) | ((u8)((x)>>8)) )

#define be32_to_cpu(val) ((u32) ( \
	(((u32) (val) & (u32) 0x000000ffU) << 24) | \
	(((u32) (val) & (u32) 0x0000ff00U) <<  8) | \
	(((u32) (val) & (u32) 0x00ff0000U) >>  8) | \
	(((u32) (val) & (u32) 0xff000000U) >> 24)))

#define cpu_to_be32(val) ((u32) ( \
	(((u32) (val) & (u32) 0x000000ffU) << 24) | \
	(((u32) (val) & (u32) 0x0000ff00U) <<  8) | \
	(((u32) (val) & (u32) 0x00ff0000U) >>  8) | \
	(((u32) (val) & (u32) 0xff000000U) >> 24)))

void cpu_physical_memory_write(u32 addr,const void* buff,u32 sz);
void cpu_physical_memory_read(u32 addr,void* buff,u32 sz);

#define target_phys_addr_t u32
#define QEMUFile void*

struct PCIDevice;
struct VLANClientState;
struct QEMUTimer;
struct NICInfo
{
	uint8_t macaddr[6];
};
struct PCIBus;
void rtl8139_resete(void* p);
void qemu_send_packet(VLANClientState*,const uint8_t*, int,u32 fromaddr);
typedef u32 net_receive(void *opaque, const uint8_t *buf, int size);
typedef int net_can_receive(void *opaque);
VLANClientState * qemu_new_vlan_client(void* ,net_receive* rc,net_can_receive* cr,void* opaq);

void pci_set_irq(PCIDevice* dev,u32 duno,u32 status);

typedef void PCIConfigWriteFunc(PCIDevice *pci_dev, 
                                 uint32_t address, uint32_t data, int len); 
 typedef uint32_t PCIConfigReadFunc(PCIDevice *pci_dev, 
                                    uint32_t address, int len); 
 typedef void PCIMapIORegionFunc(PCIDevice *pci_dev, int region_num, 
                                 uint32_t addr, uint32_t size, int type); 
  
 #define PCI_ADDRESS_SPACE_MEM           0x00 
 #define PCI_ADDRESS_SPACE_IO            0x01 
 #define PCI_ADDRESS_SPACE_MEM_PREFETCH  0x08 
  
 typedef struct PCIIORegion { 
     uint32_t addr; /* current PCI mapping address. -1 means not mapped */ 
     uint32_t size; 
     uint8_t type; 
     PCIMapIORegionFunc *map_func; 
 } PCIIORegion; 
  
 #define PCI_ROM_SLOT 6 
 #define PCI_NUM_REGIONS 7 
 
 #define PCI_DEVICES_MAX 64 
  
 #define PCI_VENDOR_ID           0x00    /* 16 bits */ 
 #define PCI_DEVICE_ID           0x02    /* 16 bits */ 
 #define PCI_COMMAND             0x04    /* 16 bits */ 
 #define  PCI_COMMAND_IO         0x1     /* Enable response in I/O space */ 
 #define  PCI_COMMAND_MEMORY     0x2     /* Enable response in Memory space */ 
 #define PCI_CLASS_DEVICE        0x0a    /* Device class */ 
 #define PCI_INTERRUPT_LINE      0x3c    /* 8 bits */ 
 #define PCI_INTERRUPT_PIN       0x3d    /* 8 bits */ 
#define PCI_MIN_GNT             0x3e    /* 8 bits */ 
 #define PCI_MAX_LAT             0x3f    /* 8 bits */ 
  
 struct PCIDevice { 
     /* PCI config space */ 
     uint8_t config[256]; 
  
     /* the following fields are read only */ 
     PCIBus *bus; 
     int devfn; 
     char name[64]; 
     PCIIORegion io_regions[PCI_NUM_REGIONS]; 
  
     /* do not access the following fields */ 
     PCIConfigReadFunc *config_read; 
     PCIConfigWriteFunc *config_write; 
     /* ??? This is a PC-specific hack, and should be removed.  */ 
     int irq_index; 
  
     /* IRQ objects for the INTA-INTD pins.  */ 
   //  qemu_irq *irq; 
  
     /* Current IRQ levels.  Used internally by the generic PCI code.  */ 
     int irq_state[4]; 
 }; 
   void pci_register_io_region(PCIDevice *pci_dev, int region_num,
                            uint32_t size, int type,
                            PCIMapIORegionFunc *map_func);
 PCIDevice *pci_register_device(PCIBus *bus, const char *name, 
                                int instance_size, int devfn, 
                                PCIConfigReadFunc *config_read, 
                                PCIConfigWriteFunc *config_write);
  
 void pci_register_io_region(PCIDevice *pci_dev, int region_num, 
                             uint32_t size, int type, 
                             PCIMapIORegionFunc *map_func); 
  
 uint32_t pci_default_read_config(PCIDevice *d, 
                                  uint32_t address, int len); 
 void pci_default_write_config(PCIDevice *d, 
                               uint32_t address, uint32_t val, int len); 
 void pci_device_save(PCIDevice *s, QEMUFile *f); 
 int pci_device_load(PCIDevice *s, QEMUFile *f); 
  
 struct qemu_irq;
 typedef void (*pci_set_irq_fn)(qemu_irq *pic, int irq_num, int level); 
 typedef int (*pci_map_irq_fn)(PCIDevice *pci_dev, int irq_num); 
 PCIBus *pci_register_bus(pci_set_irq_fn set_irq, pci_map_irq_fn map_irq, 
                          qemu_irq *pic, int devfn_min, int nirq); 
  
 void pci_nic_init(PCIBus *bus, NICInfo *nd, int devfn); 
 void pci_data_write(void *opaque, uint32_t addr, uint32_t val, int len); 
 uint32_t pci_data_read(void *opaque, uint32_t addr, int len); 
 int pci_bus_num(PCIBus *s); 
 void pci_for_each_device(int bus_num, void (*fn)(PCIDevice *d)); 
  
 void pci_info(void); 
 PCIBus *pci_bridge_init(PCIBus *bus, int devfn, uint32_t id, 
                         pci_map_irq_fn map_irq, const char *name); 

PCIDevice* pci_rtl8139_init(PCIBus *bus, NICInfo *nd, int devfn);
void rtl8139_mmio_writeb(void *opaque, target_phys_addr_t addr, uint32_t val);
void rtl8139_mmio_writew(void *opaque, target_phys_addr_t addr, uint32_t val);
void rtl8139_mmio_writel(void *opaque, target_phys_addr_t addr, uint32_t val);
uint32_t rtl8139_mmio_readb(void *opaque, target_phys_addr_t addr);
uint32_t rtl8139_mmio_readw(void *opaque, target_phys_addr_t addr);
uint32_t rtl8139_mmio_readl(void *opaque, target_phys_addr_t addr);