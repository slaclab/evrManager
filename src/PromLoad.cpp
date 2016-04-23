//////////////////////////////////////////////////////////////////////////////
// This file is part of 'SLAC Generic Prom Loader'.
// It is subject to the license terms in the LICENSE.txt file found in the 
// top-level directory of this distribution and at: 
//    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
// No part of 'SLAC Generic Prom Loader', including this file, 
// may be copied, modified, propagated, or distributed except according to 
// the terms contained in the LICENSE.txt file.
//////////////////////////////////////////////////////////////////////////////
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <linux/types.h>

#include <fcntl.h>
#include <sstream>
#include <string>
#include <iomanip>
#include <iostream>
#include <string.h>
#include <stdlib.h>

#include "EvrCardG2Prom.h"
#include "PromLoad.h"

using namespace std;

#define PAGE_SIZE sysconf(_SC_PAGE_SIZE)

int PromLoad (void *mapStart, string filePath) {

   EvrCardG2Prom *prom;

   if(mapStart == MAP_FAILED){
      cout << "Error: mmap() = " << dec << mapStart << endl;
      return(1);   
   }
   
   // Create the EvrCardG2Prom object
   prom = new EvrCardG2Prom(mapStart,filePath);
   
   // Check if the .mcs file exists
   if(!prom->fileExist()){
      cout << "Error opening: " << filePath << endl;
      delete prom;
      return(1);   
   }   
   
   // Get & Set the FPGA's PROM code size
   prom->setPromSize(prom->getPromSize(filePath));       
   
   // Check if the PCIe device is a generation 2 card
   if(!prom->checkFirmwareVersion()){
      delete prom;
      return(1);   
   }    
      
   // Erase the PROM
   prom->eraseBootProm();
  
   // Write the .mcs file to the PROM
   if(!prom->bufferedWriteBootProm()) {
      cout << "Error in prom->bufferedWriteBootProm() function" << endl;
      delete prom;
      return(1);     
   }   

   // Compare the .mcs file with the PROM
   if(!prom->verifyBootProm()) {
      cout << "Error in prom->verifyBootProm() function" << endl;
      delete prom;
      return(1);     
   }
      
   // Display Reminder
   cout << "\n\n\n\n\n";
   cout << "***************************************" << endl;
   cout << "***************************************" << endl;
   cout << "New data has been written into the PROM." << endl;
   cout << "To load the new PROM data into the FPGA, " << endl<< endl;
   cout << "a power cycle of the PCIe card is required " << endl;
   cout << "***************************************" << endl;
   cout << "***************************************" << endl;
   cout << "\n\n\n\n\n";
   
	// Close all the devices
   delete prom;
   return(0);
}
