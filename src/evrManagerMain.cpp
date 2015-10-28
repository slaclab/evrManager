#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <string>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdexcept>

#include "utils.h"
#include "linux-evrma.h"
#include "linux-evr-regs.h"

namespace {

struct IoRegion {
	
	uint32_t *ptr;
	int	length;
	
	explicit IoRegion(void)
		: ptr(NULL)
		, length(0)
	{
	}
	
	void write32(int regAddr, uint32_t value)
	{
		ptr[regAddr / 4] = cpu_to_be32(value);
	}
	
	uint32_t read32(int regAddr)
	{
		return be32_to_cpu(ptr[regAddr / 4]);
	}
};

enum {
	IOCFG_INIT,
	IOCFG_TESTS = 100,
	IOCFG_TEST101 = 101
};

class EvrManager {
public:
	
	explicit EvrManager(const std::string &mngDevNodeName)
	{
		fd = open(mngDevNodeName.c_str(), O_RDWR);
		if(fd < 0) {
			throw std::runtime_error("failed mng open");
		}
		
		struct mngdev_config mngdev_config;
		
		int ret = ioctl(MNG_DEV_IOC_CONFIG, &mngdev_config);
		if(ret) {
			AERR("MNG_DEV_IOC_CONFIG failed with errno=%d", errno);
			close(fd);
			throw std::runtime_error("MNG_DEV_IOC_CONFIG failed");
		}
		
		if(mngdev_config.io_memory_length > 0) {
			int prot;
			prot = PROT_READ | PROT_WRITE;
			
			ioRegion.length = mngdev_config.io_memory_length;
			
// 			ADBG("mmaping: %d", ioRegion.length);
			
			ioRegion.ptr = (uint32_t *)mmap(NULL, ioRegion.length, 
								prot, MAP_SHARED, fd, 0);
			
			if(ioRegion.ptr != MAP_FAILED) {
// 				ADBG("IO mmap-ed pointer: 0x%x", (int)(size_t)ioRegion.ptr);
			} else {
				AERR("IO mmap failed with errno=%d", errno);
				close(fd);
				throw std::runtime_error("IO mmap failed");
			}
		}
	}
	
	virtual ~EvrManager()
	{
		if(ioRegion.ptr != NULL) {
			if(munmap(NULL, ioRegion.length)) {
				AERR("IO munmap failed, errno=%d", errno);
			}
		}
	
		close(fd);
	}
	
	// will return 0 on any error
	int getVirtDevId(const std::string &virtDevName)
	{
		struct mngdev_ioctl_vdev_ids vDevData = {
			0,
			"",
		};
		
		strncpy(vDevData.name, virtDevName.c_str(), sizeof(vDevData.name));
		
		int ret = ioctl(MNG_DEV_IOC_VIRT_DEV_FIND, &vDevData) == 0;

		if(!ret) {
			return 0;
		} else {
			return vDevData.id;
		}
	}
	
	int ioctl(unsigned long request, void *data = NULL)
	{
		int ret = ::ioctl(fd, request, data);
		if(ret < 0) {
			ADBG("IOCTL failed, errno=%d", errno);
		}
		
		return ret;
	}
	
	bool ioConfig(int what);

private:
	
	int fd;
	IoRegion ioRegion;
	
};

	
bool EvrManager::ioConfig(int what)
{
	bool ret = true;
		
	
	
	if(what == IOCFG_INIT) {

		// the dbuf is initially off; will be set by the subscriptions
		ioRegion.write32(EVR_REG_DATA_BUF_CTRL, 0); 

		ioRegion.write32(EVR_REG_FRAC_DIV, EVR_CLOCK_119000_MHZ);
		ioRegion.write32(EVR_REG_CTRL, 0x0);
		ioRegion.write32(EVR_REG_IRQEN, 0x0);
		ioRegion.write32(EVR_REG_CTRL, (1 << C_EVR_CTRL_RESET_EVENTFIFO));
		ioRegion.write32(EVR_REG_IRQFLAG, 0xFFFFFFFF);
		ioRegion.write32(EVR_REG_EV_CNT_PRESC, 1);
		
		struct mngdev_ioctl_hw_header dummyHeader = {
			-1,
			{
				{
					MODAC_RES_TYPE_NONE
				},
				{
					MODAC_RES_TYPE_NONE
				},
			}
		};
		
		int res = this->ioctl(MNG_DEV_EVR_IOC_INIT, &dummyHeader);
	
		if(res < 0) {
			AERR("MNG_DEV_EVR_IOC_INIT failed");
			return false;
		}

		uint32_t regCtrl = ioRegion.read32(EVR_REG_CTRL);		
		ioRegion.write32(EVR_REG_CTRL, regCtrl | (1 << C_EVR_CTRL_MASTER_ENABLE));
		
		// sleep a while for the card to start operating
		sleep(1);
		
	} else if(what == IOCFG_TEST101) {
		
		ioRegion.write32(EVR_REG_CTRL, 0);
		
	}
	
	return ret;
}


bool run(int argc, const char *argv[])
{
	bool ret = false;
	
	int argc_used = 1; // the command itself
	
	if(argc < argc_used + 2) {
		AERR("arg[%d, %d]->mngDevNodeName, command", argc_used, argc_used+1);
		goto LErr;
LErr:
		return 1;
	}

	std::string mngDevNodeName = argv[argc_used ++];
	std::string command = argv[argc_used ++];
	
	
	uint8_t virtNumber = 0;
	std::string virtDevName;
	
	try {
		
		EvrManager manager(mngDevNodeName);
		
		if(command == "init" || command == "sleep") {
			// no virt_DEV param
		} else {

			if(argc < argc_used + 1) {
				// all commands need this at the moment
				AERR("arg[%d]->virtDevName", argc_used);
				goto LErr;
			}

			virtDevName = argv[argc_used ++];
			
			virtNumber = manager.getVirtDevId(virtDevName);
			
			if(command == "create") {
				if(virtNumber > 0) {
					// will leave as this but will fail later because
				}
			} else {
				if(virtNumber < 1) {
					AERR("Can't proceed '%s' with invalid VIRT_DEV '%s'", command.c_str(), virtDevName.c_str());
					goto LErr;
				}
			}
		}
		
		
		if(command == "create") {

			struct mngdev_ioctl_vdev_ids vDevData = {
				virtNumber,
				"",
			};
			
			strncpy(vDevData.name, virtDevName.c_str(), sizeof(vDevData.name));
			
			ret = manager.ioctl(MNG_DEV_IOC_CREATE, &vDevData) == 0;
		
			if(!ret) {
				AERR("Virtual dev creation failed: '%s', %d", vDevData.name, vDevData.id);
				throw std::runtime_error("error");
			}
			
		} else if(command == "destroy") {

			struct mngdev_ioctl_destroy vDevData = {
				virtNumber
			};
			
			ret = manager.ioctl(MNG_DEV_IOC_DESTROY, &vDevData) == 0;
		
			if(!ret) {
				AERR("Virtual dev destruction failed: %d", vDevData.id);
				throw std::runtime_error("error");
			}
			
		} else if(command == "alloc") {
			
			if(argc < argc_used + 1) {
				AERR("arg[%d]->resName", argc_used);
				goto LErr;
			}

			std::string resName = argv[argc_used ++];
			
			struct mngdev_ioctl_res vDevData;
			
			if(resName == "pulsegen") {
			
				int prescalerLength = 0;
				int delayLength = 32;
				int widthLength = 16;
				
				if(argc < argc_used + 1) {
// 					AINFO("Could set also: arg[%d]->prescalerLength", argc_used);
				} else {
					prescalerLength = ::atoi(argv[argc_used ++]);
				}
				
				if(argc < argc_used + 1) {
// 					AINFO("Could set also: arg[%d]->delayLength", argc_used);
				} else {
					delayLength = ::atoi(argv[argc_used ++]);
				}
				
				if(argc < argc_used + 1) {
// 					AINFO("Could set also: arg[%d]->widthLength", argc_used);
				} else {
					widthLength = ::atoi(argv[argc_used ++]);
				}
				

				struct mngdev_ioctl_res vDevDataPulsegen = {
					virtNumber,
					"pulsegen",
					-1,
					{
						prescalerLength,
						delayLength,
						widthLength
					}
				};
				
				vDevData = vDevDataPulsegen;
				
			} else if(resName == "output") {
				
				int absOutputNum;
				
				if(argc < argc_used + 1) {
					AERR("arg[%d]->absOutputNum", argc_used);
					goto LErr;
				} else {
					absOutputNum = ::atoi(argv[argc_used ++]);
				}
				

				struct mngdev_ioctl_res vDevDataOutput = {
					virtNumber,
					"output",
					absOutputNum,
				};
				
				vDevData = vDevDataOutput;
				
			} else {
				AERR("Unknown resName: %s", resName.c_str());
				throw std::runtime_error("error");
			}

			int ainx = manager.ioctl(MNG_DEV_IOC_ALLOC, &vDevData);
		
			if(ainx < 0) {
				AERR("Virtual dev alloc failed: %d", vDevData.id_vdev);
				throw std::runtime_error("error");
			} else {
				ADBG("allocated abs index: %d", ainx);
				ret = true;
			}
			
		} else if(command == "output") {
			
			if(argc < argc_used + 3) {
				AERR("arg[%d, %d, %d]->outputIndex, [P/S], source", argc_used, argc_used+1, argc_used+2);
				throw std::runtime_error("error");
			}
			
			int outputIndex = ::atoi(argv[argc_used ++]);
			std::string pOrS = argv[argc_used ++];
			int source = ::atoi(argv[argc_used ++]);
			
			struct mngdev_evr_output_set outSetArgs = {
				{
					virtNumber,
					{
						{
							EVR_RES_TYPE_OUTPUT,
							outputIndex
						},
						{
							EVR_RES_TYPE_PULSEGEN,
							source
						}
					}
				},
				source
			};
			
			if(pOrS == "S") {
				outSetArgs.header.vres[1].type = MODAC_RES_TYPE_NONE;
			}

			ret = manager.ioctl(MNG_DEV_EVR_IOC_OUTSET, &outSetArgs) == 0;
		
			if(!ret) {
				AERR("MNG_DEV_EVR_IOC_OUTSET failed, errno=%d", errno);
				throw std::runtime_error("error");
			}
			
		} else if(command == "init") {
			
			ret = manager.ioConfig(IOCFG_INIT);
			
		} else if(command == "sleep") {
			
			// sleep a lot for the application to stay alive and keep the MNG_DEV open
			sleep(10000);
				
		} else if(command == "test") {
			
			int testNum;
			
			if(argc < argc_used + 1) {
				AERR("arg[%d]->testNum", argc_used);
				throw std::runtime_error("error");
			} else {
				testNum = ::atoi(argv[argc_used ++]);
			}

			if(testNum == 9997) {
				
				ADBG("test the VIRT_DEV id query");
				
				if(argc < argc_used + 1) {
					// all commands need this at the moment
					AERR("arg[%d]->virtDevName", argc_used);
					goto LErr;
				}
				
				std::string virtDevName = argv[argc_used ++];
				
				AINFO("OBTAINED ID=%d", manager.getVirtDevId(virtDevName));
				
			} else if(testNum == 9998) {

				ADBG("test the VIRT_DEV creation");
				
				if(argc < argc_used + 1) {
					// all commands need this at the moment
					AERR("arg[%d]->virtDevName", argc_used);
					goto LErr;
				}
				
				std::string virtDevName = argv[argc_used ++];
				
				struct mngdev_ioctl_vdev_ids vDevData = {
					0, // auto choose
					"",
				};
				
				strncpy(vDevData.name, virtDevName.c_str(), sizeof(vDevData.name));
				
				ret = manager.ioctl(MNG_DEV_IOC_CREATE, 
						&vDevData) == 0;
			
				if(!ret) {
					AERR("Virtual dev %s failed: '%s', %d", "creation", vDevData.name, vDevData.id);
					AERR("errno=%d", errno);
					throw std::runtime_error("error");
				}
				
			} else if(testNum >= IOCFG_TESTS) {
				ret = manager.ioConfig(testNum);
			}
			
		} else {
			AERR("Unknown cmd: %s", command.c_str());
		}
		
	} catch(std::runtime_error &e) {
		AERR("Exception: %s", e.what());
		ret = false;
	}
	
	return ret;
}

} // unnamed namespace



int main(int argc, const char *argv[])
{
	return run(argc, argv) ? 0 : 1;
}

