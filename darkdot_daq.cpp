/********************************************************************\

  Name:         drs_exam.cpp
  Created by:   Stefan Ritt

  Contents:     Simple example application to read out a DRS4
                evaluation board

  $Id: drs_exam.cpp 21308 2014-04-11 14:50:16Z ritt $

\********************************************************************/

#include <math.h>

#ifdef _MSC_VER

#include <windows.h>

#elif defined(OS_LINUX)

#define O_BINARY 0

#include <unistd.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <errno.h>

#define DIR_SEPARATOR '/'

#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "strlcpy.h"
#include "DRS.h"
#include <chrono>
#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>
/*------------------------------------------------------------------*/

int main()
{
   int i, nBoards;
   DRS *drs;
   DRSBoard *b;
   float time_array[8][1024];
   float wave_array[8][1024];
   float counting_time;
   float current_time;
   FILE  *f;
   int file_count=0, event_count = 0;

   
   /* do initial scan */
   drs = new DRS();

   auto start = std::chrono::steady_clock::now();
   auto duration = std::chrono::hours(1);
   /* show any found board(s) */
   for (i=0 ; i<drs->GetNumberOfBoards() ; i++) {
      b = drs->GetBoard(i);
      printf("Found DRS4 evaluation board, serial #%d, firmware revision %d\n", 
         b->GetBoardSerialNumber(), b->GetFirmwareVersion());
   }

   /* exit if no board found */
   nBoards = drs->GetNumberOfBoards();
   if (nBoards == 0) {
      printf("No DRS4 evaluation board found\n");
      return 0;
   }

   /* continue working with first board only */
   b = drs->GetBoard(0);

   /* initialize board */
   b->Init();

   /* set sampling frequency */
   b->SetFrequency(5, true);

   /* enable transparent mode needed for analog trigger */
   b->SetTranspMode(1);

   /* set input range to -0.5V ... +0.5V */
   b->SetInputRange(0);

   /* use following line to set range to 0..1V */
   //b->SetInputRange(0.5);
   
   /* use following line to turn on the internal 100 MHz clock connected to all channels  */
   //b->EnableTcal(1);

   /* use following lines to enable hardware trigger on CH1 at 50 mV positive edge */
   if (b->GetBoardType() >= 8) {        // Evaluaiton Board V4&5
      b->EnableTrigger(1, 0);           // enable hardware trigger
      b->SetTriggerConfig(1<<0);        // set CH1 as source
   } else if (b->GetBoardType() == 7) { // Evaluation Board V3
      b->EnableTrigger(0, 1);           // lemo off, analog trigger on
      b->SetTriggerConfig(1);           // use CH1 as source
   }
   b->SetTriggerLevel(-0.008);            // 0.05 V
   b->SetTriggerPolarity(false);        // positive edge

   
   /* use following lines to set individual trigger elvels */
   //b->SetIndividualTriggerLevel(1, 0.1);
   //b->SetIndividualTriggerLevel(2, 0.2);
   //b->SetIndividualTriggerLevel(3, 0.3);
   //b->SetIndividualTriggerLevel(4, 0.4);
   //b->SetTriggerSource(15);
   
   b->SetTriggerDelayNs(0);             // zero ns trigger delay
   
   /* use following lines to enable the external trigger */
   //if (b->GetBoardType() >= 8) {        // Evaluaiton Board V4&5
   //   b->EnableTrigger(1, 0);           // enable hardware trigger
   //   b->SetTriggerConfig(1<<4);        // set external trigger as source
   //} else {                             // Evaluation Board V3
   //   b->EnableTrigger(1, 0);           // lemo on, analog trigger off
   //}

   while(true) {
     auto now = std::chrono::steady_clock::now();
     auto now2 = std::chrono::system_clock::now();
     std::time_t now_time = std::chrono::system_clock::to_time_t(now2);

     std::tm* now_tm = std::localtime(&now_time);

     std::ostringstream oss;
     oss << std::put_time(now_tm, "%Y-%m-%d-%H-%M-%S");

     if (now - start >= duration) {
       break;
        }
     
     std::string text = oss.str();
     std::cout << "Current Date and Time: " << text << std::endl;
     std::string filename  = text + std::to_string(file_count) + ".txt";
     //char filename = "data.txt";
     /* open file to save waveforms */
     f = fopen(filename.c_str(), "w");
     if (f == NULL) {
       perror("ERROR: Cannot open file");
       return 1;
     }
     event_count = 0;
     while(event_count <= 10000) {  // 10k events per file
            auto now = std::chrono::steady_clock::now();
            if (now - start >= duration) {
                 break;
            }      
           /* start board (activate domino wave) */
           b->StartDomino();
     
           /* wait for trigger */
           printf("Waiting for trigger...");
      
           fflush(stdout);
           while (b->IsBusy());

           /* read all waveforms */
           b->TransferWaves(0, 8);

           /* read time (X) array of first channel in ns */
           b->GetTime(0, 0, b->GetTriggerCell(0), time_array[0]);
           current_time = b->GetTime(0, 0, b->GetTriggerCell(0), time_array[0]);

           /* decode waveform (Y) array of first channel in mV */
           b->GetWave(0, 0, wave_array[0]);
           b->GetWave(0, 3, wave_array[2]);
           b->GetWave(0, 4, wave_array[3]);

           /* read time (X) array of second channel in ns
	      Note: On the evaluation board input #1 is connected to channel 0 and 1 of
	      the DRS chip, input #2 is connected to channel 2 and 3 and so on. So to
	      get the input #2 we have to read DRS channel #2, not #1. */
           b->GetTime(0, 2, b->GetTriggerCell(0), time_array[1]);
           b->GetTime(0, 3, b->GetTriggerCell(0), time_array[2]);
           b->GetTime(0, 4, b->GetTriggerCell(0), time_array[3]);

           /* decode waveform (Y) array of second channel in mV */
           b->GetWave(0, 2, wave_array[1]);
       
           /* Save waveform: X=time_array[i], Yn=wave_array[n][i] */
           fprintf(f, "Event #%d ----------------------\n  t1[ns]  u1[mV]  t2[ns] u2[mV] t3[ns]  u3[mV]  t4[ns] u4[mV]\n", event_count);
           for (i=0 ; i<1024 ; i++)
	            fprintf(f, "%7.3f %7.1f %7.3f %7.1f %7.3f %7.1f %7.3f %7.1f\n", time_array[0][i], wave_array[0][i], time_array[1][i], wave_array[1][i], time_array[2][i], wave_array[2][i], time_array[3][i], wave_array[3][i]);

           /* print some progress indication */
           printf("\rEvent #%d read successfully\n", event_count);
           event_count++;
     }
     fclose(f);
     file_count++;
   }
   
   /* delete DRS object -> close USB connection */
   delete drs;
}
